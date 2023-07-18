/*
 * Copyright (c) 2022 HiSilicon (Shanghai) Technologies CO., LIMITED.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>
#include <limits.h>
#include "sample_comm.h"
#include "autoconf.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#define TEMP_BUF_LEN    8
#define PER_UNIT        1024
#define MAX_THM_SIZE    (64 * 1024)

static pthread_t gs_VencPid;
static pthread_t gs_VencQpmapPid;
static SAMPLE_VENC_GETSTREAM_PARA_S gs_stPara;
static SAMPLE_VENC_QPMAP_SENDFRAME_PARA_S stQpMapSendFramePara;
static HI_S32 gs_s32SnapCnt = 0;
HI_CHAR *DstBuf = NULL;

static HI_S32 FileTrans_GetThmFromJpg(const HI_CHAR *JPGPath, HI_S32 *DstSize)
{
    HI_S32 bufpos = 0;
    HI_CHAR temp_read = 0;
    HI_CHAR preflag = 0xff;
    HI_CHAR startflag = 0xd8;
    HI_CHAR endflag = 0xd9;
    HI_S32 startpos = 0;
    HI_S32 endpos = 0;
    HI_S32 dcf_len = 0;
    HI_BOOL startflag_match = HI_FALSE;
    HI_CHAR real_path[PATH_MAX] = {0};

    HI_S32 fd = 0;
    struct stat stStat = { 0 };
    FILE *fpJpg = NULL;

    if (realpath(JPGPath, real_path) == HI_NULL) {
        printf("file %s error!\n", real_path);
        return HI_FAILURE;
    }

    fd = open(real_path, O_RDONLY, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        printf("file %s not exist!\n", real_path);
        return HI_FAILURE;
    } else {
        fpJpg = fdopen(fd, "rb");
        if (fpJpg == HI_NULL) {
            close(fd);
            printf("fdopen fail!\n");
            return HI_FAILURE;
        }
        fstat(fd, &stStat);
        while (feof(fpJpg) == 0) {
            if (temp_read == preflag) {
                temp_read = getc(fpJpg);
                if (temp_read == startflag) {
                    startpos = ftell(fpJpg) - 2; /* 2 byte */
                    if (startpos < 0) {
                        startpos = 0;
                    }
                    startflag_match = HI_TRUE;
                }
                if (temp_read == endflag) {
                    endpos = ftell(fpJpg);
                    if (startflag_match == HI_TRUE) {
                        break;
                    }
                }
            } else {
                temp_read = getc(fpJpg);
            }
            bufpos++;
        }
    }

    if (startflag_match != HI_TRUE) {
        printf("%s, %d: no start flag, get .thm fail!\n", __FUNCTION__, __LINE__);
        (HI_VOID)fclose(fpJpg);
        return HI_FAILURE;
    }

    dcf_len = endpos - startpos;
    if (dcf_len <= 0 || dcf_len > MAX_THM_SIZE || dcf_len >= stStat.st_size) {
        printf("%s, %d: length is %d, get .thm fail!\n", __FUNCTION__, __LINE__, endpos - startpos);
        (HI_VOID)fclose(fpJpg);
        return HI_FAILURE;
    }

    HI_CHAR *cDstBuf = (HI_CHAR *)malloc(endpos - startpos);
    if (cDstBuf == NULL) {
        printf("memory malloc fail!\n");
        (HI_VOID)fclose(fpJpg);
        return HI_FAILURE;
    }

    (HI_VOID)fseek(fpJpg, (long)startpos, SEEK_SET);
    *DstSize = fread(cDstBuf, 1, endpos - startpos, fpJpg);
    if (*DstSize != (endpos - startpos)) {
        free(cDstBuf);
        printf("fread fail!\n");
        (HI_VOID)fclose(fpJpg);
        return HI_FAILURE;
    }

    DstBuf = cDstBuf;
    (HI_VOID)fclose(fpJpg);

    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_VENC_MemConfig(HI_VOID)
{
    HI_S32 i = 0;
    HI_S32 s32Ret;
    HI_CHAR *pcMmzName = HI_NULL;
    MPP_CHN_S stMppChnVENC;

    for (i = 0; i < 64; i++) { /* group, venc max chn is 64 */
        stMppChnVENC.enModId = HI_ID_VENC;
        stMppChnVENC.s32DevId = 0;
        stMppChnVENC.s32ChnId = i;
        pcMmzName = NULL;

        s32Ret = HI_MPI_SYS_SetMemConfig(&stMppChnVENC, pcMmzName);
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("HI_MPI_SYS_SetMemConfig with %#x!\n", s32Ret);
            return HI_FAILURE;
        }
    }
    return HI_SUCCESS;
}

static HI_S32 SAMPLE_COMM_VENC_GetFilePostfix(PAYLOAD_TYPE_E enPayload, HI_CHAR *szFilePostfix, HI_U8 len)
{
    if (szFilePostfix == NULL) {
        SAMPLE_PRT("null pointer\n");
        return HI_FAILURE;
    }

    if (PT_H264 == enPayload) {
        if (strcpy_s(szFilePostfix, len, ".h264") != EOK) {
            return HI_FAILURE;
        }
    } else if (PT_H265 == enPayload) {
        if (strcpy_s(szFilePostfix, len, ".h265") != EOK) {
            return HI_FAILURE;
        }
    } else if (PT_JPEG == enPayload) {
        if (strcpy_s(szFilePostfix, len, ".jpg") != EOK) {
            return HI_FAILURE;
        }
    } else if (PT_MJPEG == enPayload) {
        if (strcpy_s(szFilePostfix, len, ".mjp") != EOK) {
            return HI_FAILURE;
        }
    } else {
        SAMPLE_PRT("payload type err!\n");
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_VENC_GetGopAttr(VENC_GOP_MODE_E enGopMode, VENC_GOP_ATTR_S *pstGopAttr)
{
    if (pstGopAttr == NULL) {
        SAMPLE_PRT("null pointer\n");
        return HI_FAILURE;
    }
    switch (enGopMode) {
        case VENC_GOPMODE_NORMALP:
            pstGopAttr->enGopMode = VENC_GOPMODE_NORMALP;
            pstGopAttr->stNormalP.s32IPQpDelta = 2; /* set 2 */
            break;
        case VENC_GOPMODE_SMARTP:
            pstGopAttr->enGopMode = VENC_GOPMODE_SMARTP;
            pstGopAttr->stSmartP.s32BgQpDelta = 4; /* set 4 */
            pstGopAttr->stSmartP.s32ViQpDelta = 2; /* set 2 */
            pstGopAttr->stSmartP.u32BgInterval = 90; /* set 90 */
            break;

        case VENC_GOPMODE_DUALP:
            pstGopAttr->enGopMode = VENC_GOPMODE_DUALP;
            pstGopAttr->stDualP.s32IPQpDelta = 4; /* set 4 */
            pstGopAttr->stDualP.s32SPQpDelta = 2; /* set 2 */
            pstGopAttr->stDualP.u32SPInterval = 3; /* set 3 */
            break;

        case VENC_GOPMODE_BIPREDB:
            pstGopAttr->enGopMode = VENC_GOPMODE_BIPREDB;
            pstGopAttr->stBipredB.s32BQpDelta = -2; /* set -2 */
            pstGopAttr->stBipredB.s32IPQpDelta = 3; /* set 3 */
            pstGopAttr->stBipredB.u32BFrmNum = 2; /* set 2 */
            break;

        default:
            SAMPLE_PRT("not support the gop mode !\n");
            return HI_FAILURE;
            break;
    }
    return HI_SUCCESS;
}

static HI_S32 SAMPLE_COMM_VENC_Getdcfinfo(const char *SrcJpgPath, const char *DstThmPath)
{
    HI_S32 s32RtnVal = HI_SUCCESS;
    HI_CHAR JPGSrcPath[FILE_NAME_LEN] = {0};
    HI_CHAR JPGDesPath[FILE_NAME_LEN] = {0};
    HI_S32 DstSize = 0;
    HI_S32 fd = -1;
    HI_S32 u32WritenSize = 0;

    if (snprintf_s(JPGSrcPath, sizeof(JPGSrcPath), sizeof(JPGSrcPath) - 1, "%s", SrcJpgPath) < 0) {
        return HI_FAILURE;
    }
    if (snprintf_s(JPGDesPath, sizeof(JPGDesPath), sizeof(JPGDesPath) - 1, "%s", DstThmPath) < 0) {
        return HI_FAILURE;
    }

    s32RtnVal = FileTrans_GetThmFromJpg(JPGSrcPath, &DstSize);
    if ((HI_SUCCESS != s32RtnVal) || (DstSize == 0)) {
        printf("fail to get thm\n");
        return HI_FAILURE;
    }

    fd = open(JPGDesPath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        printf("file to create file %s\n", JPGDesPath);
        return HI_FAILURE;
    }

    FILE *fpTHM = fdopen(fd, "w");
    if (fpTHM == HI_NULL) {
        printf("fdopen fail!\n");
        close(fd);
        return HI_FAILURE;
    }

    while (u32WritenSize < DstSize) {
        s32RtnVal = fwrite(DstBuf + u32WritenSize, 1, DstSize, fpTHM);
        if (s32RtnVal <= 0) {
            printf("fail to write file, rtn=%d\n", s32RtnVal);
            break;
        }

        u32WritenSize += s32RtnVal;
    }

    if (fpTHM != HI_NULL) {
        (HI_VOID)fclose(fpTHM);
        fpTHM = 0;
    }

    if (DstBuf != NULL) {
        free(DstBuf);
        DstBuf = NULL;
    }

    return 0;
}

HI_S32 SAMPLE_COMM_VENC_SaveStream(FILE *pFd, VENC_STREAM_S *pstStream)
{
    HI_U32 i;

    if ((pFd == NULL) || (pstStream == NULL)) {
        SAMPLE_PRT("null pointer\n");
        return HI_FAILURE;
    }
    for (i = 0; i < pstStream->u32PackCount; i++) {
        (HI_VOID)fwrite(pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset,
            pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset, 1, pFd);

        (HI_VOID)fflush(pFd);
    }

    return HI_SUCCESS;
}

static HI_S32 SAMPLE_COMM_VENC_CloseReEncode(VENC_CHN VencChn)
{
    HI_S32 s32Ret;
    VENC_RC_PARAM_S stRcParam;
    VENC_CHN_ATTR_S stChnAttr;

    s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stChnAttr);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("GetChnAttr failed!\n");
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VENC_GetRcParam(VencChn, &stRcParam);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("GetRcParam failed!\n");
        return HI_FAILURE;
    }

    if (VENC_RC_MODE_H264CBR == stChnAttr.stRcAttr.enRcMode) {
        stRcParam.stParamH264Cbr.s32MaxReEncodeTimes = 0;
    } else if (VENC_RC_MODE_H264VBR == stChnAttr.stRcAttr.enRcMode) {
        stRcParam.stParamH264Vbr.s32MaxReEncodeTimes = 0;
    } else if (VENC_RC_MODE_H265CBR == stChnAttr.stRcAttr.enRcMode) {
        stRcParam.stParamH265Cbr.s32MaxReEncodeTimes = 0;
    } else if (VENC_RC_MODE_H265VBR == stChnAttr.stRcAttr.enRcMode) {
        stRcParam.stParamH265Vbr.s32MaxReEncodeTimes = 0;
    } else {
        return HI_SUCCESS;
    }
    s32Ret = HI_MPI_VENC_SetRcParam(VencChn, &stRcParam);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("SetRcParam failed!\n");
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_VENC_Create(VENC_CHN VencChn, PAYLOAD_TYPE_E enType, PIC_SIZE_E enSize, SAMPLE_RC_E enRcMode,
    HI_U32 u32Profile, HI_BOOL bRcnRefShareBuf, VENC_GOP_ATTR_S *pstGopAttr)
{
    HI_S32 s32Ret;
    SIZE_S stPicSize;
    VENC_CHN_ATTR_S stVencChnAttr;
    VENC_ATTR_JPEG_S stJpegAttr;
    SAMPLE_VI_CONFIG_S stViConfig;
    HI_U32 u32FrameRate;
    HI_U32 u32StatTime;
    HI_U32 u32Gop = 30; /* default set 30 */

    if (pstGopAttr == NULL) {
        SAMPLE_PRT("pstGopAttr is null!\n");
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSize, &stPicSize);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("Get picture size failed!\n");
        return HI_FAILURE;
    }

    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    if (stViConfig.astViInfo[0].stSnsInfo.enSnsType == SAMPLE_SNS_TYPE_BUTT) {
        SAMPLE_PRT("Not set SENSOR%d_TYPE !\n", 0);
        return HI_FAILURE;
    }
    s32Ret = SAMPLE_COMM_VI_GetFrameRateBySensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType, &u32FrameRate);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetFrameRateBySensor failed!\n");
        return s32Ret;
    }

    /* step 1:  Create Venc Channel */
    stVencChnAttr.stVencAttr.enType = enType;
    stVencChnAttr.stVencAttr.u32MaxPicWidth = stPicSize.u32Width;
    stVencChnAttr.stVencAttr.u32MaxPicHeight = stPicSize.u32Height;
    stVencChnAttr.stVencAttr.u32PicWidth = stPicSize.u32Width;   /* the picture width */
    stVencChnAttr.stVencAttr.u32PicHeight = stPicSize.u32Height; /* the picture height */

    if (enType == PT_MJPEG || enType == PT_JPEG) {
        stVencChnAttr.stVencAttr.u32BufSize =
            HI_ALIGN_UP(stPicSize.u32Width, 16) * HI_ALIGN_UP(stPicSize.u32Height, 16); /* 16 align */
    } else {
        stVencChnAttr.stVencAttr.u32BufSize =
            HI_ALIGN_UP(stPicSize.u32Width * stPicSize.u32Height * 3 / 4, 64); /* * 3 / 4, 64 align */
    }
    stVencChnAttr.stVencAttr.u32Profile = u32Profile;
    stVencChnAttr.stVencAttr.bByFrame = HI_TRUE; /* get stream mode is slice mode or frame mode? */

    if (pstGopAttr->enGopMode == VENC_GOPMODE_SMARTP) {
        u32StatTime = pstGopAttr->stSmartP.u32BgInterval / u32Gop;
    } else {
        u32StatTime = 1;
    }

    switch (enType) {
        case PT_H265: {
            if (enRcMode == SAMPLE_RC_CBR) {
                VENC_H265_CBR_S stH265Cbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
                stH265Cbr.u32Gop = u32Gop;
                stH265Cbr.u32StatTime = u32StatTime;       /* stream rate statics time(s) */
                stH265Cbr.u32SrcFrameRate = u32FrameRate;  /* input (vi) frame rate */
                stH265Cbr.fr32DstFrameRate = u32FrameRate; /* target frame rate */
                switch (enSize) {
                    case PIC_720P:
                        stH265Cbr.u32BitRate = PER_UNIT * 2 + PER_UNIT * u32FrameRate / FPS_30; /* 2M + 1M */
                        break;
                    case PIC_1080P:
                        stH265Cbr.u32BitRate = PER_UNIT * 2 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 2M + 2M */
                        break;
                    case PIC_2592x1944:
                        stH265Cbr.u32BitRate = PER_UNIT * 3 + PER_UNIT * 3 * u32FrameRate / FPS_30; /* 3M + 3M */
                        break;
                    case PIC_3840x2160:
                        stH265Cbr.u32BitRate = PER_UNIT * 5 + PER_UNIT * 5 * u32FrameRate / FPS_30; /* 5M + 5M */
                        break;
                    default:
                        stH265Cbr.u32BitRate = PER_UNIT * 15 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 15M + 2M */
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH265Cbr, sizeof(VENC_H265_CBR_S), &stH265Cbr,
                    sizeof(VENC_H265_CBR_S));
            } else if (enRcMode == SAMPLE_RC_FIXQP) {
                VENC_H265_FIXQP_S stH265FixQp;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265FIXQP;
                stH265FixQp.u32Gop = u32Gop;
                stH265FixQp.u32SrcFrameRate = u32FrameRate;
                stH265FixQp.fr32DstFrameRate = u32FrameRate;
                stH265FixQp.u32IQp = 25; /* set 25 */
                stH265FixQp.u32PQp = 30; /* set 30 */
                stH265FixQp.u32BQp = 32; /* set 32 */
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH265FixQp, sizeof(VENC_H265_FIXQP_S), &stH265FixQp,
                    sizeof(VENC_H265_FIXQP_S));
            } else if (enRcMode == SAMPLE_RC_VBR) {
                VENC_H265_VBR_S stH265Vbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265VBR;
                stH265Vbr.u32Gop = u32Gop;
                stH265Vbr.u32StatTime = u32StatTime;
                stH265Vbr.u32SrcFrameRate = u32FrameRate;
                stH265Vbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_720P:
                        stH265Vbr.u32MaxBitRate = PER_UNIT * 2 + PER_UNIT * u32FrameRate / FPS_30; /* 2M + 1M */
                        break;
                    case PIC_1080P:
                        stH265Vbr.u32MaxBitRate = PER_UNIT * 2 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 2M + 2M */
                        break;
                    case PIC_2592x1944:
                        stH265Vbr.u32MaxBitRate = PER_UNIT * 3 + PER_UNIT * 3 * u32FrameRate / FPS_30; /* 3M + 3M */
                        break;
                    case PIC_3840x2160:
                        stH265Vbr.u32MaxBitRate = PER_UNIT * 5 + PER_UNIT * 5 * u32FrameRate / FPS_30; /* 5M + 5M */
                        break;
                    default:
                        stH265Vbr.u32MaxBitRate = PER_UNIT * 15 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 15M + 2M */
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH265Vbr, sizeof(VENC_H265_VBR_S), &stH265Vbr,
                    sizeof(VENC_H265_VBR_S));
            } else if (enRcMode == SAMPLE_RC_AVBR) {
                VENC_H265_AVBR_S stH265AVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265AVBR;
                stH265AVbr.u32Gop = u32Gop;
                stH265AVbr.u32StatTime = u32StatTime;
                stH265AVbr.u32SrcFrameRate = u32FrameRate;
                stH265AVbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_720P:
                        stH265AVbr.u32MaxBitRate = PER_UNIT * 2 + PER_UNIT * u32FrameRate / FPS_30; /* 2M + 1M */
                        break;
                    case PIC_1080P:
                        stH265AVbr.u32MaxBitRate = PER_UNIT * 2 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 2M + 2M */
                        break;
                    case PIC_2592x1944:
                        stH265AVbr.u32MaxBitRate = PER_UNIT * 3 + PER_UNIT * 3 * u32FrameRate / FPS_30; /* 3M + 3M */
                        break;
                    case PIC_3840x2160:
                        stH265AVbr.u32MaxBitRate = PER_UNIT * 5 + PER_UNIT * 5 * u32FrameRate / FPS_30; /* 5M + 5M */
                        break;
                    default:
                        stH265AVbr.u32MaxBitRate = PER_UNIT * 15 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 15M + 2M */
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH265AVbr, sizeof(VENC_H265_AVBR_S), &stH265AVbr,
                    sizeof(VENC_H265_AVBR_S));
            } else if (enRcMode == SAMPLE_RC_QVBR) {
                VENC_H265_QVBR_S stH265QVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265QVBR;
                stH265QVbr.u32Gop = u32Gop;
                stH265QVbr.u32StatTime = u32StatTime;
                stH265QVbr.u32SrcFrameRate = u32FrameRate;
                stH265QVbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_720P:
                        stH265QVbr.u32TargetBitRate =
                            PER_UNIT * 2 + PER_UNIT * u32FrameRate / FPS_30; /* 2M + 1M */
                        break;
                    case PIC_1080P:
                        stH265QVbr.u32TargetBitRate =
                            PER_UNIT * 2 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 2M + 2M */
                        break;
                    case PIC_2592x1944:
                        stH265QVbr.u32TargetBitRate =
                            PER_UNIT * 3 + PER_UNIT * 3 * u32FrameRate / FPS_30; /* 3M + 3M */
                        break;
                    case PIC_3840x2160:
                        stH265QVbr.u32TargetBitRate =
                            PER_UNIT * 5 + PER_UNIT * 5  * u32FrameRate / FPS_30; /* 5M + 5M */
                        break;
                    default:
                        stH265QVbr.u32TargetBitRate =
                            PER_UNIT * 15 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 15M + 2M */
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH265QVbr, sizeof(VENC_H265_QVBR_S), &stH265QVbr,
                    sizeof(VENC_H265_QVBR_S));
            } else if (enRcMode == SAMPLE_RC_CVBR) {
                VENC_H265_CVBR_S stH265CVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CVBR;
                stH265CVbr.u32Gop = u32Gop;
                stH265CVbr.u32StatTime = u32StatTime;
                stH265CVbr.u32SrcFrameRate = u32FrameRate;
                stH265CVbr.fr32DstFrameRate = u32FrameRate;
                stH265CVbr.u32LongTermStatTime = 1;
                stH265CVbr.u32ShortTermStatTime = u32StatTime;
                switch (enSize) {
                    case PIC_720P:
                        stH265CVbr.u32MaxBitRate = PER_UNIT * 3 + PER_UNIT * u32FrameRate / FPS_30; /* 3M + 1M */
                        stH265CVbr.u32LongTermMaxBitrate =
                            PER_UNIT * 2 + PER_UNIT * u32FrameRate / FPS_30; /* 2M + 1M */
                        stH265CVbr.u32LongTermMinBitrate = 512; /* 512kbps */
                        break;
                    case PIC_1080P:
                        stH265CVbr.u32MaxBitRate = PER_UNIT * 2 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 2M + 2M */
                        stH265CVbr.u32LongTermMaxBitrate =
                            PER_UNIT * 2 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 2M + 2M */
                        stH265CVbr.u32LongTermMinBitrate = PER_UNIT;
                        break;
                    case PIC_2592x1944:
                        stH265CVbr.u32MaxBitRate = PER_UNIT * 4 + PER_UNIT * 3 * u32FrameRate / FPS_30; /* 4M + 3M */
                        stH265CVbr.u32LongTermMaxBitrate =
                            PER_UNIT * 3 + PER_UNIT * 3 * u32FrameRate / FPS_30; /* 3M + 3M */
                        stH265CVbr.u32LongTermMinBitrate = PER_UNIT * 2; /* 2M */
                        break;
                    case PIC_3840x2160:
                        stH265CVbr.u32MaxBitRate = PER_UNIT * 8 + PER_UNIT * 5 * u32FrameRate / FPS_30; /* 8M + 5M */
                        stH265CVbr.u32LongTermMaxBitrate =
                            PER_UNIT * 5 + PER_UNIT * 5 * u32FrameRate / FPS_30; /* 5M + 5M */
                        stH265CVbr.u32LongTermMinBitrate = PER_UNIT * 3; /* 3M */
                        break;
                    default:
                        stH265CVbr.u32MaxBitRate = PER_UNIT * 24 + PER_UNIT * 5 * u32FrameRate / FPS_30; /* 24M + 5M */
                        stH265CVbr.u32LongTermMaxBitrate =
                            PER_UNIT * 15 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 15M + 2M */
                        stH265CVbr.u32LongTermMinBitrate = PER_UNIT * 5; /* 5M */
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH265CVbr, sizeof(VENC_H265_CVBR_S), &stH265CVbr,
                    sizeof(VENC_H265_CVBR_S));
            } else if (enRcMode == SAMPLE_RC_QPMAP) {
                VENC_H265_QPMAP_S stH265QpMap;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265QPMAP;
                stH265QpMap.u32Gop = u32Gop;
                stH265QpMap.u32StatTime = u32StatTime;
                stH265QpMap.u32SrcFrameRate = u32FrameRate;
                stH265QpMap.fr32DstFrameRate = u32FrameRate;
                stH265QpMap.enQpMapMode = VENC_RC_QPMAP_MODE_MEANQP;
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH265QpMap, sizeof(VENC_H265_QPMAP_S), &stH265QpMap,
                    sizeof(VENC_H265_QPMAP_S));
            } else {
                SAMPLE_PRT("%s,%d,enRcMode(%d) not support\n", __FUNCTION__, __LINE__, enRcMode);
                return HI_FAILURE;
            }
            stVencChnAttr.stVencAttr.stAttrH265e.bRcnRefShareBuf = bRcnRefShareBuf;
            break;
        }
        case PT_H264: {
            if (enRcMode == SAMPLE_RC_CBR) {
                VENC_H264_CBR_S stH264Cbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
                stH264Cbr.u32Gop = u32Gop;                 /* the interval of IFrame */
                stH264Cbr.u32StatTime = u32StatTime;       /* stream rate statics time(s) */
                stH264Cbr.u32SrcFrameRate = u32FrameRate;  /* input (vi) frame rate */
                stH264Cbr.fr32DstFrameRate = u32FrameRate; /* target frame rate */
                switch (enSize) {
                    case PIC_720P:
                        stH264Cbr.u32BitRate = PER_UNIT * 3 + PER_UNIT * u32FrameRate / FPS_30; /* 3M + 1M */
                        break;
                    case PIC_1080P:
                        stH264Cbr.u32BitRate = PER_UNIT * 2 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 2M + 2M */
                        break;
                    case PIC_2592x1944:
                        stH264Cbr.u32BitRate = PER_UNIT * 4 + PER_UNIT * 3 * u32FrameRate / FPS_30; /* 4M + 3M */
                        break;
                    case PIC_3840x2160:
                        stH264Cbr.u32BitRate = PER_UNIT * 8 + PER_UNIT * 5 * u32FrameRate / FPS_30; /* 8M + 5M */
                        break;
                    default:
                        stH264Cbr.u32BitRate = PER_UNIT * 24 + PER_UNIT * 5 * u32FrameRate / FPS_30; /* 24M + 5M */
                        break;
                }

                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH264Cbr, sizeof(VENC_H264_CBR_S), &stH264Cbr,
                    sizeof(VENC_H264_CBR_S));
            } else if (enRcMode == SAMPLE_RC_FIXQP) {
                VENC_H264_FIXQP_S stH264FixQp;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264FIXQP;
                stH264FixQp.u32Gop = u32Gop;
                stH264FixQp.u32SrcFrameRate = u32FrameRate;
                stH264FixQp.fr32DstFrameRate = u32FrameRate;
                stH264FixQp.u32IQp = 25; /* set 25 */
                stH264FixQp.u32PQp = 30; /* set 30 */
                stH264FixQp.u32BQp = 32; /* set 32 */
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH264FixQp, sizeof(VENC_H264_FIXQP_S), &stH264FixQp,
                    sizeof(VENC_H264_FIXQP_S));
            } else if (enRcMode == SAMPLE_RC_VBR) {
                VENC_H264_VBR_S stH264Vbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264VBR;
                stH264Vbr.u32Gop = u32Gop;
                stH264Vbr.u32StatTime = u32StatTime;
                stH264Vbr.u32SrcFrameRate = u32FrameRate;
                stH264Vbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_360P:
                        stH264Vbr.u32MaxBitRate = PER_UNIT * 2 + PER_UNIT * u32FrameRate / FPS_30; /* 2M + 1M */
                        break;
                    case PIC_720P:
                        stH264Vbr.u32MaxBitRate = PER_UNIT * 2 + PER_UNIT * u32FrameRate / FPS_30; /* 2M + 1M */
                        break;
                    case PIC_1080P:
                        stH264Vbr.u32MaxBitRate = PER_UNIT * 2 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 2M + 2M */
                        break;
                    case PIC_2592x1944:
                        stH264Vbr.u32MaxBitRate = PER_UNIT * 3 + PER_UNIT * 3 * u32FrameRate / FPS_30; /* 3M + 3M */
                        break;
                    case PIC_3840x2160:
                        stH264Vbr.u32MaxBitRate = PER_UNIT * 5 + PER_UNIT * 5 * u32FrameRate / FPS_30; /* 5M + 5M */
                        break;
                    default:
                        stH264Vbr.u32MaxBitRate = PER_UNIT * 15 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 15M + 2M */
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH264Vbr, sizeof(VENC_H264_VBR_S), &stH264Vbr,
                    sizeof(VENC_H264_VBR_S));
            } else if (enRcMode == SAMPLE_RC_AVBR) {
                VENC_H264_VBR_S stH264AVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264AVBR;
                stH264AVbr.u32Gop = u32Gop;
                stH264AVbr.u32StatTime = u32StatTime;
                stH264AVbr.u32SrcFrameRate = u32FrameRate;
                stH264AVbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_360P:
                        stH264AVbr.u32MaxBitRate = PER_UNIT * 2 + PER_UNIT * u32FrameRate / FPS_30; /* 2M + 1M */
                        break;
                    case PIC_720P:
                        stH264AVbr.u32MaxBitRate = PER_UNIT * 2 + PER_UNIT * u32FrameRate / FPS_30; /* 2M + 1M */
                        break;
                    case PIC_1080P:
                        stH264AVbr.u32MaxBitRate = PER_UNIT * 2 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 2M + 2M */
                        break;
                    case PIC_2592x1944:
                        stH264AVbr.u32MaxBitRate = PER_UNIT * 3 + PER_UNIT * 3 * u32FrameRate / FPS_30; /* 3M + 3M */
                        break;
                    case PIC_3840x2160:
                        stH264AVbr.u32MaxBitRate = PER_UNIT * 5 + PER_UNIT * 5 * u32FrameRate / FPS_30; /* 5M + 5M */
                        break;
                    default:
                        stH264AVbr.u32MaxBitRate = PER_UNIT * 15 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 15M + 2M */
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH264AVbr, sizeof(VENC_H264_AVBR_S), &stH264AVbr,
                    sizeof(VENC_H264_AVBR_S));
            } else if (enRcMode == SAMPLE_RC_QVBR) {
                VENC_H264_QVBR_S stH264QVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264QVBR;
                stH264QVbr.u32Gop = u32Gop;
                stH264QVbr.u32StatTime = u32StatTime;
                stH264QVbr.u32SrcFrameRate = u32FrameRate;
                stH264QVbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_360P:
                        stH264QVbr.u32TargetBitRate =
                            PER_UNIT * 2 + PER_UNIT * u32FrameRate / FPS_30; /* 2M + 1M */
                        break;
                    case PIC_720P:
                        stH264QVbr.u32TargetBitRate =
                            PER_UNIT * 2 + PER_UNIT * u32FrameRate / FPS_30; /* 2M + 1M */
                        break;
                    case PIC_1080P:
                        stH264QVbr.u32TargetBitRate =
                            PER_UNIT * 2 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 2M + 2M */
                        break;
                    case PIC_2592x1944:
                        stH264QVbr.u32TargetBitRate =
                            PER_UNIT * 3 + PER_UNIT * 3 * u32FrameRate / FPS_30; /* 3M + 3M */
                        break;
                    case PIC_3840x2160:
                        stH264QVbr.u32TargetBitRate =
                            PER_UNIT * 5 + PER_UNIT * 5 * u32FrameRate / FPS_30; /* 5M + 5M */
                        break;
                    default:
                        stH264QVbr.u32TargetBitRate =
                            PER_UNIT * 15 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 15M + 2M */
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH264QVbr, sizeof(VENC_H264_QVBR_S), &stH264QVbr,
                    sizeof(VENC_H264_QVBR_S));
            } else if (enRcMode == SAMPLE_RC_CVBR) {
                VENC_H264_CVBR_S stH264CVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CVBR;
                stH264CVbr.u32Gop = u32Gop;
                stH264CVbr.u32StatTime = u32StatTime;
                stH264CVbr.u32SrcFrameRate = u32FrameRate;
                stH264CVbr.fr32DstFrameRate = u32FrameRate;
                stH264CVbr.u32LongTermStatTime = 1;
                stH264CVbr.u32ShortTermStatTime = u32StatTime;
                switch (enSize) {
                    case PIC_720P:
                        stH264CVbr.u32MaxBitRate = PER_UNIT * 3 + PER_UNIT * u32FrameRate / FPS_30; /* 3M + 2M */
                        stH264CVbr.u32LongTermMaxBitrate = PER_UNIT * 2 + PER_UNIT * u32FrameRate / FPS_30;
                        stH264CVbr.u32LongTermMinBitrate = 512; /* 512kbps */
                        break;
                    case PIC_1080P:
                        stH264CVbr.u32MaxBitRate = PER_UNIT * 2 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 2M + 2M */
                        stH264CVbr.u32LongTermMaxBitrate = PER_UNIT * 2 + PER_UNIT * 2 * u32FrameRate / FPS_30;
                        stH264CVbr.u32LongTermMinBitrate = PER_UNIT;
                        break;
                    case PIC_2592x1944:
                        stH264CVbr.u32MaxBitRate = PER_UNIT * 4 + PER_UNIT * 3 * u32FrameRate / FPS_30; /* 4M + 3M */
                        stH264CVbr.u32LongTermMaxBitrate = PER_UNIT * 3 + PER_UNIT * 3 * u32FrameRate / FPS_30;
                        stH264CVbr.u32LongTermMinBitrate = PER_UNIT * 2; /* 2M */
                        break;
                    case PIC_3840x2160:
                        stH264CVbr.u32MaxBitRate = PER_UNIT * 8 + PER_UNIT * 5 * u32FrameRate / FPS_30; /* 8M + 5M */
                        stH264CVbr.u32LongTermMaxBitrate = PER_UNIT * 5 + PER_UNIT * 5 * u32FrameRate / FPS_30;
                        stH264CVbr.u32LongTermMinBitrate = PER_UNIT * 3; /* 3M */
                        break;
                    default:
                        stH264CVbr.u32MaxBitRate = PER_UNIT * 24 + PER_UNIT * 5 * u32FrameRate / FPS_30; /* 24M + 5M */
                        stH264CVbr.u32LongTermMaxBitrate =
                            PER_UNIT * 15 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 15M + 2M */
                        stH264CVbr.u32LongTermMinBitrate = PER_UNIT * 5; /* 5M */
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH264CVbr, sizeof(VENC_H264_CVBR_S), &stH264CVbr,
                    sizeof(VENC_H264_CVBR_S));
            } else if (enRcMode == SAMPLE_RC_QPMAP) {
                VENC_H264_QPMAP_S stH264QpMap;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264QPMAP;
                stH264QpMap.u32Gop = u32Gop;
                stH264QpMap.u32StatTime = u32StatTime;
                stH264QpMap.u32SrcFrameRate = u32FrameRate;
                stH264QpMap.fr32DstFrameRate = u32FrameRate;
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH264QpMap, sizeof(VENC_H264_QPMAP_S), &stH264QpMap,
                    sizeof(VENC_H264_QPMAP_S));
            } else {
                SAMPLE_PRT("%s,%d,enRcMode(%d) not support\n", __FUNCTION__, __LINE__, enRcMode);
                return HI_FAILURE;
            }
            stVencChnAttr.stVencAttr.stAttrH264e.bRcnRefShareBuf = bRcnRefShareBuf;
                break;
        }
        case PT_MJPEG: {
            if (enRcMode == SAMPLE_RC_FIXQP) {
                VENC_MJPEG_FIXQP_S stMjpegeFixQp;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGFIXQP;
                stMjpegeFixQp.u32Qfactor = 95; /* set 95 */
                stMjpegeFixQp.u32SrcFrameRate = u32FrameRate;
                stMjpegeFixQp.fr32DstFrameRate = u32FrameRate;

                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stMjpegFixQp, sizeof(VENC_MJPEG_FIXQP_S), &stMjpegeFixQp,
                    sizeof(VENC_MJPEG_FIXQP_S));
            } else if (enRcMode == SAMPLE_RC_CBR) {
                VENC_MJPEG_CBR_S stMjpegeCbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
                stMjpegeCbr.u32StatTime = u32StatTime;
                stMjpegeCbr.u32SrcFrameRate = u32FrameRate;
                stMjpegeCbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_360P:
                        stMjpegeCbr.u32BitRate = PER_UNIT * 3 + PER_UNIT * u32FrameRate / FPS_30; /* 3M + 1M */
                        break;
                    case PIC_720P:
                        stMjpegeCbr.u32BitRate = PER_UNIT * 5 + PER_UNIT * u32FrameRate / FPS_30; /* 5M + 1M */
                        break;
                    case PIC_1080P:
                        stMjpegeCbr.u32BitRate = PER_UNIT * 8 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 8M + 2M */
                        break;
                    case PIC_2592x1944:
                        stMjpegeCbr.u32BitRate = PER_UNIT * 20 + PER_UNIT * 3 * u32FrameRate / FPS_30; /* 20M + 3M */
                        break;
                    case PIC_3840x2160:
                        stMjpegeCbr.u32BitRate = PER_UNIT * 25 + PER_UNIT * 5 * u32FrameRate / FPS_30; /* 25M + 5M */
                        break;
                    default:
                        stMjpegeCbr.u32BitRate = PER_UNIT * 20 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 20M + 2M */
                        break;
                }

                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stMjpegCbr, sizeof(VENC_MJPEG_CBR_S), &stMjpegeCbr,
                    sizeof(VENC_MJPEG_CBR_S));
            } else if ((enRcMode == SAMPLE_RC_VBR) || (enRcMode == SAMPLE_RC_AVBR) || (enRcMode == SAMPLE_RC_QVBR) ||
                (enRcMode == SAMPLE_RC_CVBR)) {
                VENC_MJPEG_VBR_S stMjpegVbr;

                if (enRcMode == SAMPLE_RC_AVBR) {
                    SAMPLE_PRT("Mjpege not support AVBR, so change rcmode to VBR!\n");
                }

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGVBR;
                stMjpegVbr.u32StatTime = u32StatTime;
                stMjpegVbr.u32SrcFrameRate = u32FrameRate;
                stMjpegVbr.fr32DstFrameRate = 5; /* output 5fps */

                switch (enSize) {
                    case PIC_360P:
                        stMjpegVbr.u32MaxBitRate = PER_UNIT * 3 + PER_UNIT * u32FrameRate / FPS_30; /* 3M + 1M */
                        break;
                    case PIC_720P:
                        stMjpegVbr.u32MaxBitRate = PER_UNIT * 5 + PER_UNIT * u32FrameRate / FPS_30; /* 5M + 1M */
                        break;
                    case PIC_1080P:
                        stMjpegVbr.u32MaxBitRate = PER_UNIT * 8 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 8M + 2M */
                        break;
                    case PIC_2592x1944:
                        stMjpegVbr.u32MaxBitRate = PER_UNIT * 20 + PER_UNIT * 3 * u32FrameRate / FPS_30; /* 20M + 3M */
                        break;
                    case PIC_3840x2160:
                        stMjpegVbr.u32MaxBitRate = PER_UNIT * 25 + PER_UNIT * 5 * u32FrameRate / FPS_30; /* 25M + 5M */
                        break;
                    default:
                        stMjpegVbr.u32MaxBitRate = PER_UNIT * 20 + PER_UNIT * 2 * u32FrameRate / FPS_30; /* 20M + 2M */
                        break;
                }

                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stMjpegVbr, sizeof(VENC_MJPEG_VBR_S), &stMjpegVbr,
                    sizeof(VENC_MJPEG_VBR_S));
            } else {
                SAMPLE_PRT("can't support other mode(%d) in this version!\n", enRcMode);
                return HI_FAILURE;
            }
            break;
        }
        case PT_JPEG:
            stJpegAttr.bSupportDCF = HI_FALSE;
            stJpegAttr.stMPFCfg.u8LargeThumbNailNum = 0;
            stJpegAttr.enReceiveMode = VENC_PIC_RECEIVE_SINGLE;
            (hi_void)memcpy_s(&stVencChnAttr.stVencAttr.stAttrJpege, sizeof(VENC_ATTR_JPEG_S), &stJpegAttr,
                sizeof(VENC_ATTR_JPEG_S));
            break;
        default:
            SAMPLE_PRT("can't support this enType (%d) in this version!\n", enType);
            return HI_ERR_VENC_NOT_SUPPORT;
    }

    if (enType == PT_MJPEG || enType == PT_JPEG) {
        stVencChnAttr.stGopAttr.enGopMode = VENC_GOPMODE_NORMALP;
        stVencChnAttr.stGopAttr.stNormalP.s32IPQpDelta = 0;
    } else {
        (hi_void)memcpy_s(&stVencChnAttr.stGopAttr, sizeof(VENC_GOP_ATTR_S), pstGopAttr, sizeof(VENC_GOP_ATTR_S));
        if ((pstGopAttr->enGopMode == VENC_GOPMODE_BIPREDB) && (enType == PT_H264)) {
            if (stVencChnAttr.stVencAttr.u32Profile == 0) {
                stVencChnAttr.stVencAttr.u32Profile = 1;
                SAMPLE_PRT("H.264 base profile not support BIPREDB, so change profile to main profile!\n");
            }
        }

        if ((stVencChnAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264QPMAP) ||
            (stVencChnAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265QPMAP)) {
            if (pstGopAttr->enGopMode == VENC_GOPMODE_ADVSMARTP) {
                stVencChnAttr.stGopAttr.enGopMode = VENC_GOPMODE_SMARTP;
                SAMPLE_PRT("advsmartp not support QPMAP, so change gopmode to smartp!\n");
            }
        }
    }

    s32Ret = HI_MPI_VENC_CreateChn(VencChn, &stVencChnAttr);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_VENC_CreateChn [%d] failed with %#x! ===\n", VencChn, s32Ret);
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VENC_CloseReEncode(VencChn);
    if (s32Ret != HI_SUCCESS) {
        HI_MPI_VENC_DestroyChn(VencChn);
        return s32Ret;
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_VENC_Start(VENC_CHN VencChn, PAYLOAD_TYPE_E enType, PIC_SIZE_E enSize, SAMPLE_RC_E enRcMode,
    HI_U32 u32Profile, HI_BOOL bRcnRefShareBuf, VENC_GOP_ATTR_S *pstGopAttr)
{
    HI_S32 s32Ret;
    VENC_RECV_PIC_PARAM_S stRecvParam;

    s32Ret = SAMPLE_COMM_VENC_Create(VencChn, enType, enSize, enRcMode, u32Profile, bRcnRefShareBuf, pstGopAttr);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("SAMPLE_COMM_VENC_Create failed with%#x! \n", s32Ret);
        return HI_FAILURE;
    }

    stRecvParam.s32RecvPicNum = -1;
    s32Ret = HI_MPI_VENC_StartRecvFrame(VencChn, &stRecvParam);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_VENC_StartRecvPic failed with%#x! \n", s32Ret);
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_VENC_Stop(VENC_CHN VencChn)
{
    HI_S32 s32Ret;

    s32Ret = HI_MPI_VENC_StopRecvFrame(VencChn);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_VENC_StopRecvPic vechn[%d] failed with %#x!\n", VencChn, s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VENC_DestroyChn(VencChn);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_VENC_DestroyChn vechn[%d] failed with %#x!\n", VencChn, s32Ret);
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_VENC_SnapStart(VENC_CHN VencChn, SIZE_S *pstSize, HI_BOOL bSupportDCF)
{
    HI_S32 s32Ret;
    VENC_CHN_ATTR_S stVencChnAttr;

    if (pstSize == NULL) {
        SAMPLE_PRT("null pointer\n");
        return HI_FAILURE;
    }
    stVencChnAttr.stVencAttr.enType = PT_JPEG;
    stVencChnAttr.stVencAttr.u32Profile = 0;
    stVencChnAttr.stVencAttr.u32MaxPicWidth = pstSize->u32Width;
    stVencChnAttr.stVencAttr.u32MaxPicHeight = pstSize->u32Height;
    stVencChnAttr.stVencAttr.u32PicWidth = pstSize->u32Width;
    stVencChnAttr.stVencAttr.u32PicHeight = pstSize->u32Height;
    stVencChnAttr.stVencAttr.u32BufSize =
        HI_ALIGN_UP(pstSize->u32Width, 16) * HI_ALIGN_UP(pstSize->u32Height, 16); /* 16 align */
    stVencChnAttr.stVencAttr.bByFrame = HI_TRUE; /* get stream mode is field mode  or frame mode */
    stVencChnAttr.stVencAttr.stAttrJpege.bSupportDCF = bSupportDCF;
    stVencChnAttr.stVencAttr.stAttrJpege.stMPFCfg.u8LargeThumbNailNum = 0;
    stVencChnAttr.stVencAttr.stAttrJpege.enReceiveMode = VENC_PIC_RECEIVE_SINGLE;
    s32Ret = HI_MPI_VENC_CreateChn(VencChn, &stVencChnAttr);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_VENC_CreateChn [%d] failed with %#x!\n", VencChn, s32Ret);
        return s32Ret;
    }
    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_VENC_SnapStop(VENC_CHN VencChn)
{
    HI_S32 s32Ret;
    s32Ret = HI_MPI_VENC_StopRecvFrame(VencChn);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_VENC_StopRecvPic vechn[%d] failed with %#x!\n", VencChn, s32Ret);
        return HI_FAILURE;
    }
    s32Ret = HI_MPI_VENC_DestroyChn(VencChn);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_VENC_DestroyChn vechn[%d] failed with %#x!\n", VencChn, s32Ret);
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_VENC_SnapProcess(VENC_CHN VencChn, HI_U32 SnapCnt, HI_BOOL bSaveJpg, HI_BOOL bSaveThm)
{
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_S32 s32VencFd;
    HI_S32 fd = -1;
    VENC_CHN_STATUS_S stStat;
    VENC_STREAM_S stStream;
    HI_S32 s32Ret;
    VENC_RECV_PIC_PARAM_S stRecvParam;
    HI_U32 i;
#if defined(__HuaweiLite__) && (!defined(__OHOS__))
    VENC_STREAM_BUF_INFO_S stStreamBufInfo;
#endif

    stRecvParam.s32RecvPicNum = SnapCnt;
    s32Ret = HI_MPI_VENC_StartRecvFrame(VencChn, &stRecvParam);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_VENC_StartRecvPic failed with%#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32VencFd = HI_MPI_VENC_GetFd(VencChn);
    if (s32VencFd < 0) {
        SAMPLE_PRT("HI_MPI_VENC_GetFd failed with%#x!\n", s32VencFd);
        return HI_FAILURE;
    }

    for (i = 0; i < SnapCnt; i++) {
        FD_ZERO(&read_fds);
        FD_SET(s32VencFd, &read_fds);
        TimeoutVal.tv_sec = 10; /* 10 s */
        TimeoutVal.tv_usec = 0;
        s32Ret = select(s32VencFd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0) {
            SAMPLE_PRT("snap select failed!\n");
            return HI_FAILURE;
        } else if (s32Ret == 0) {
            SAMPLE_PRT("snap time out!\n");
            return HI_FAILURE;
        } else {
            if (FD_ISSET(s32VencFd, &read_fds)) {
                s32Ret = HI_MPI_VENC_QueryStatus(VencChn, &stStat);
                if (s32Ret != HI_SUCCESS) {
                    SAMPLE_PRT("HI_MPI_VENC_QueryStatus failed with %#x!\n", s32Ret);
                    return HI_FAILURE;
                }
                if (stStat.u32CurPacks == 0) {
                    SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                    return HI_SUCCESS;
                }
                stStream.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                if (stStream.pstPack == NULL) {
                    SAMPLE_PRT("malloc memory failed!\n");
                    return HI_FAILURE;
                }
                stStream.u32PackCount = stStat.u32CurPacks;
                s32Ret = HI_MPI_VENC_GetStream(VencChn, &stStream, -1);
                if (s32Ret != HI_SUCCESS) {
                    SAMPLE_PRT("HI_MPI_VENC_GetStream failed with %#x!\n", s32Ret);

                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    return HI_FAILURE;
                }
                if (bSaveJpg || bSaveThm) {
                    char acFile[FILE_NAME_LEN] = {0};
                    FILE *pFile = HI_NULL;

                    if (snprintf_s(acFile, FILE_NAME_LEN, FILE_NAME_LEN - 1, "snap_%d.jpg", gs_s32SnapCnt) < 0) {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        return HI_FAILURE;
                    }
                    fd = open(acFile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                    if (fd < 0) {
                        SAMPLE_PRT("open file err\n");
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        return HI_FAILURE;
                    }

                    pFile = fdopen(fd, "wb");
                    if (pFile == NULL) {
                        SAMPLE_PRT("fdopen err\n");
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        close(fd);
                        return HI_FAILURE;
                    }
                    s32Ret = SAMPLE_COMM_VENC_SaveStream(pFile, &stStream);
                    if (s32Ret != HI_SUCCESS) {
                        SAMPLE_PRT("save snap picture failed!\n");

                        free(stStream.pstPack);
                        stStream.pstPack = NULL;

                        (HI_VOID)fclose(pFile);
                        pFile = HI_NULL;
                        return HI_FAILURE;
                    }
                    if (bSaveThm) {
                        char acFile_dcf[FILE_NAME_LEN] = {0};
                        if (snprintf_s(acFile_dcf, FILE_NAME_LEN, FILE_NAME_LEN - 1, "snap_thm_%d.jpg", gs_s32SnapCnt) <
                            0) {
                            free(stStream.pstPack);
                            stStream.pstPack = NULL;

                            (HI_VOID)fclose(pFile);
                            pFile = HI_NULL;
                            return HI_FAILURE;
                        }

                        s32Ret = SAMPLE_COMM_VENC_Getdcfinfo(acFile, acFile_dcf);
                        if (s32Ret != HI_SUCCESS) {
                            SAMPLE_PRT("save thm picture failed!\n");

                            free(stStream.pstPack);
                            stStream.pstPack = NULL;

                            (HI_VOID)fclose(pFile);
                            pFile = HI_NULL;
                            return HI_FAILURE;
                        }
                    }
                    (HI_VOID)fclose(pFile);
                    pFile = HI_NULL;
                    gs_s32SnapCnt++;
                }

                s32Ret = HI_MPI_VENC_ReleaseStream(VencChn, &stStream);
                if (s32Ret != HI_SUCCESS) {
                    SAMPLE_PRT("HI_MPI_VENC_ReleaseStream failed with %#x!\n", s32Ret);
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    return HI_FAILURE;
                }
                free(stStream.pstPack);
                stStream.pstPack = NULL;
            }
        }
    }
    s32Ret = HI_MPI_VENC_StopRecvFrame(VencChn);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_VENC_StopRecvPic failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_VENC_SaveJpeg(VENC_CHN VencChn, HI_U32 SnapCnt)
{
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_S32 s32VencFd;
    HI_S32 fd = -1;
    VENC_CHN_STATUS_S stStat;
    VENC_STREAM_S stStream;
    HI_S32 s32Ret;
    HI_U32 i;

#if defined(__HuaweiLite__) && (!defined(__OHOS__))
    VENC_STREAM_BUF_INFO_S stStreamBufInfo;
#endif

    s32VencFd = HI_MPI_VENC_GetFd(VencChn);
    if (s32VencFd < 0) {
        SAMPLE_PRT("HI_MPI_VENC_GetFd failed with%#x!\n", s32VencFd);
        return HI_FAILURE;
    }

    for (i = 0; i < SnapCnt; i++) {
        FD_ZERO(&read_fds);
        FD_SET(s32VencFd, &read_fds);
        TimeoutVal.tv_sec = 10; /* 10 s */
        TimeoutVal.tv_usec = 0;
        s32Ret = select(s32VencFd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0) {
            SAMPLE_PRT("snap select failed!\n");
            return HI_FAILURE;
        } else if (s32Ret == 0) {
            SAMPLE_PRT("snap time out!\n");
            return HI_FAILURE;
        } else {
            if (FD_ISSET(s32VencFd, &read_fds)) {
                s32Ret = HI_MPI_VENC_QueryStatus(VencChn, &stStat);
                if (s32Ret != HI_SUCCESS) {
                    SAMPLE_PRT("HI_MPI_VENC_QueryStatus failed with %#x!\n", s32Ret);
                    return HI_FAILURE;
                }
                if (stStat.u32CurPacks == 0) {
                    SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                    return HI_SUCCESS;
                }
                stStream.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                if (stStream.pstPack == NULL) {
                    SAMPLE_PRT("malloc memory failed!\n");
                    return HI_FAILURE;
                }
                stStream.u32PackCount = stStat.u32CurPacks;
                s32Ret = HI_MPI_VENC_GetStream(VencChn, &stStream, -1);
                if (s32Ret != HI_SUCCESS) {
                    SAMPLE_PRT("HI_MPI_VENC_GetStream failed with %#x!\n", s32Ret);

                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    return HI_FAILURE;
                }
                if (1) {
                    char acFile[FILE_NAME_LEN] = {0};
                    FILE *pFile;

                    if (snprintf_s(acFile, FILE_NAME_LEN, FILE_NAME_LEN - 1, "snap_%d.jpg", gs_s32SnapCnt) < 0) {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        return HI_FAILURE;
                    }

                    fd = open(acFile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                    if (fd < 0) {
                        SAMPLE_PRT("open file err\n");
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        return HI_FAILURE;
                    }
                    pFile = fdopen(fd, "wb");
                    if (pFile == NULL) {
                        SAMPLE_PRT("fdopen err\n");
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        close(fd);
                        return HI_FAILURE;
                    }
#if (!defined(__HuaweiLite__)) || defined(__OHOS__)
                    s32Ret = SAMPLE_COMM_VENC_SaveStream(pFile, &stStream);
                    if (s32Ret != HI_SUCCESS) {
                        SAMPLE_PRT("save snap picture failed!\n");
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        (HI_VOID)fclose(pFile);
                        return HI_FAILURE;
                    }

#else
                    s32Ret = HI_MPI_VENC_GetStreamBufInfo(VencChn, &stStreamBufInfo);
                    if (s32Ret != HI_SUCCESS) {
                        SAMPLE_PRT("HI_MPI_VENC_GetStreamBufInfo failed with %#x!\n", s32Ret);
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        (HI_VOID)fclose(pFile);
                        return HI_FAILURE;
                    }

                    s32Ret = SAMPLE_COMM_VENC_SaveStream_PhyAddr(pFile, &stStreamBufInfo, &stStream);
                    if (s32Ret != HI_SUCCESS) {
                        SAMPLE_PRT("save snap picture failed!\n");
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        (HI_VOID)fclose(pFile);
                        return HI_FAILURE;
                    }
#endif

                    (HI_VOID)fclose(pFile);
                    gs_s32SnapCnt++;
                }

                s32Ret = HI_MPI_VENC_ReleaseStream(VencChn, &stStream);
                if (s32Ret != HI_SUCCESS) {
                    SAMPLE_PRT("HI_MPI_VENC_ReleaseStream failed with %#x!\n", s32Ret);
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    return HI_FAILURE;
                }
                free(stStream.pstPack);
                stStream.pstPack = NULL;
            }
        }
    }
    return HI_SUCCESS;
}

#define QpMapBufNum 8
static HI_VOID *SAMPLE_COMM_QpmapSendFrameProc(HI_VOID *p)
{
    HI_U32 i, j;
    HI_S32 s32Ret, VeChnCnt;
    VIDEO_FRAME_INFO_S *pstVideoFrame = HI_NULL;
    USER_FRAME_INFO_S stFrame[QpMapBufNum];
    SAMPLE_VENC_QPMAP_SENDFRAME_PARA_S *pstPara = HI_NULL;

    HI_U32 u32QpMapSize;
    HI_U64 u64QpMapPhyAddr[QpMapBufNum];
    HI_VOID* pQpMapVirAddr[QpMapBufNum] = {HI_NULL};
    HI_U32 u32QpMapSizeHeight;
    HI_U32 u32QpMapSizeWidth;
    HI_U8 *pVirAddr = HI_NULL;
    HI_U64 u64PhyAddr;
    HI_U8 *pVirAddrTemp = HI_NULL;

    HI_U32 u32SkipWeightHeight_H264;
    HI_U32 u32SkipWeightWidth_H264;
    HI_U32 u32SkipWeightSize_H264;
    HI_U64 u64SkipWeightPhyAddr_H264[QpMapBufNum];
    HI_VOID* pSkipWeightVirAddr_H264[QpMapBufNum] = {HI_NULL};

    HI_U32 u32SkipWeightHeight_H265;
    HI_U32 u32SkipWeightWidth_H265;
    HI_U32 u32SkipWeightSize_H265;
    HI_U64 u64SkipWeightPhyAddr_H265[QpMapBufNum];
    HI_VOID* pSkipWeightVirAddr_H265[QpMapBufNum] = {HI_NULL};

    VPSS_CHN_ATTR_S stVpssChnAttr;

    pstPara = (SAMPLE_VENC_QPMAP_SENDFRAME_PARA_S *)p;

    /* qpmap */
    u32QpMapSizeWidth = (pstPara->stSize.u32Width + 512 - 1) / 512 * 32; /* 512 align * 32 */
    u32QpMapSizeHeight = (pstPara->stSize.u32Height + 16 - 1) / 16; /* 16 align */
    u32QpMapSize = u32QpMapSizeWidth * u32QpMapSizeHeight;
    s32Ret = HI_MPI_SYS_MmzAlloc(&u64PhyAddr, (void **)&pVirAddr, NULL, HI_NULL, u32QpMapSize * QpMapBufNum);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_SYS_MmzAlloc err:0x%x", s32Ret);
        return NULL;
    }

    for (i = 0; i < QpMapBufNum; i++) {
        u64QpMapPhyAddr[i] = u64PhyAddr + i * u32QpMapSize;
        pQpMapVirAddr[i] = pVirAddr + i * u32QpMapSize;
    }

    /* skipweight h.264 */
    u32SkipWeightWidth_H264 = (pstPara->stSize.u32Width + 512 - 1) / 512 * 16; /* 512 align * 16 */
    u32SkipWeightHeight_H264 = (pstPara->stSize.u32Height + 16 - 1) / 16; /* 16 align */
    u32SkipWeightSize_H264 = u32SkipWeightWidth_H264 * u32SkipWeightHeight_H264;
    s32Ret = HI_MPI_SYS_MmzAlloc(&u64PhyAddr, (void **)&pVirAddr, NULL, HI_NULL, u32SkipWeightSize_H264 * QpMapBufNum);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_SYS_MmzAlloc err:0x%x", s32Ret);
        HI_MPI_SYS_MmzFree(u64QpMapPhyAddr[0], pQpMapVirAddr[0]);
        return NULL;
    }

    for (i = 0; i < QpMapBufNum; i++) {
        u64SkipWeightPhyAddr_H264[i] = u64PhyAddr + i * u32SkipWeightSize_H264;
        pSkipWeightVirAddr_H264[i] = pVirAddr + i * u32SkipWeightSize_H264;
    }

    /* skipweight h.265 */
    u32SkipWeightWidth_H265 = (pstPara->stSize.u32Width + 2048 - 1) / 2048 * 16; /* 2048 align * 16 */
    u32SkipWeightHeight_H265 = (pstPara->stSize.u32Height + 64 - 1) / 64; /* 64 align */
    u32SkipWeightSize_H265 = u32SkipWeightWidth_H265 * u32SkipWeightHeight_H265;
    s32Ret = HI_MPI_SYS_MmzAlloc(&u64PhyAddr, (void **)&pVirAddr, NULL, HI_NULL, u32SkipWeightSize_H265 * QpMapBufNum);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_SYS_MmzAlloc err:0x%x", s32Ret);
        HI_MPI_SYS_MmzFree(u64QpMapPhyAddr[0], pQpMapVirAddr[0]);
        HI_MPI_SYS_MmzFree(u64SkipWeightPhyAddr_H264[0], pSkipWeightVirAddr_H264[0]);
        return NULL;
    }
    for (i = 0; i < QpMapBufNum; i++) {
        u64SkipWeightPhyAddr_H265[i] = u64PhyAddr + i * u32SkipWeightSize_H265;
        pSkipWeightVirAddr_H265[i] = pVirAddr + i * u32SkipWeightSize_H265;
    }

    s32Ret = HI_MPI_VPSS_GetChnAttr(pstPara->VpssGrp, pstPara->VpssChn, &stVpssChnAttr);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_VPSS_GetChnAttr err:0x%x", s32Ret);

        return NULL;
    }

    stVpssChnAttr.u32Depth = 3; /* set depth 3 */
    s32Ret = HI_MPI_VPSS_SetChnAttr(pstPara->VpssGrp, pstPara->VpssChn, &stVpssChnAttr);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_VPSS_SetChnAttr err:0x%x", s32Ret);

        return NULL;
    }

    i = 0;
    while (HI_TRUE == pstPara->bThreadStart) {
        pstVideoFrame = &stFrame[i].stUserFrame;
        s32Ret = HI_MPI_VPSS_GetChnFrame(pstPara->VpssGrp, pstPara->VpssChn, pstVideoFrame, 1000); /* 1000us timeout */
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("HI_MPI_VPSS_GetChnFrame err:0x%x\n", s32Ret);
            continue;
        }

        pVirAddrTemp = (HI_U8 *)pQpMapVirAddr[i];
        for (j = 0; j < u32QpMapSize; j++) {
            *pVirAddrTemp = 0x5E;
            pVirAddrTemp++;
        }

        pVirAddrTemp = (HI_U8 *)pSkipWeightVirAddr_H264[i];
        for (j = 0; j < u32SkipWeightSize_H264; j++) {
            *pVirAddrTemp = 0x88;
            pVirAddrTemp++;
        }

        pVirAddrTemp = (HI_U8 *)pSkipWeightVirAddr_H265[i];
        for (j = 0; j < u32SkipWeightSize_H265; j++) {
            *pVirAddrTemp = 0x88;
            pVirAddrTemp++;
        }

        for (VeChnCnt = 0; VeChnCnt < pstPara->s32Cnt; VeChnCnt++) {
            VENC_CHN_ATTR_S stChnAttr;
            HI_MPI_VENC_GetChnAttr(pstPara->VeChn[VeChnCnt], &stChnAttr);
            if (PT_H264 == stChnAttr.stVencAttr.enType) {
                stFrame[i].stUserRcInfo.bSkipWeightValid = 1;
                stFrame[i].stUserRcInfo.u64SkipWeightPhyAddr = u64SkipWeightPhyAddr_H264[i];
            } else if (PT_H265 == stChnAttr.stVencAttr.enType) {
                stFrame[i].stUserRcInfo.bSkipWeightValid = 1;
                stFrame[i].stUserRcInfo.u64SkipWeightPhyAddr = u64SkipWeightPhyAddr_H265[i];
            } else {
                continue;
            }

            stFrame[i].stUserRcInfo.bQpMapValid = 1;
            stFrame[i].stUserRcInfo.u64QpMapPhyAddr = u64QpMapPhyAddr[i];
            stFrame[i].stUserRcInfo.u32BlkStartQp = FPS_30;
            stFrame[i].stUserRcInfo.enFrameType = VENC_FRAME_TYPE_NONE;

            s32Ret = HI_MPI_VENC_SendFrameEx(pstPara->VeChn[VeChnCnt], &stFrame[i], -1);
            if (s32Ret != HI_SUCCESS) {
                SAMPLE_PRT("HI_MPI_VENC_SendFrame err:0x%x\n", s32Ret);
                break;
            }
        }
        if (s32Ret != HI_SUCCESS) {
            s32Ret = HI_MPI_VPSS_ReleaseChnFrame(pstPara->VpssGrp, pstPara->VpssChn, pstVideoFrame);
            if (s32Ret != HI_SUCCESS) {
                SAMPLE_PRT("HI_MPI_VPSS_ReleaseChnFrame err:0x%x", s32Ret);
                goto err_out;
            }
            continue;
        }

        s32Ret = HI_MPI_VPSS_ReleaseChnFrame(pstPara->VpssGrp, pstPara->VpssChn, pstVideoFrame);
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("HI_MPI_VPSS_ReleaseChnFrame err:0x%x", s32Ret);
            goto err_out;
        }

        i++;
        if (i >= QpMapBufNum) {
            i = 0;
        }
    }
err_out:
    s32Ret = HI_MPI_SYS_MmzFree(u64QpMapPhyAddr[0], pQpMapVirAddr[0]);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_SYS_MmzFree err:0x%x", s32Ret);
        return NULL;
    }

    s32Ret = HI_MPI_SYS_MmzFree(u64SkipWeightPhyAddr_H264[0], pSkipWeightVirAddr_H264[0]);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_SYS_MmzFree err:0x%x", s32Ret);
        return NULL;
    }

    s32Ret = HI_MPI_SYS_MmzFree(u64SkipWeightPhyAddr_H265[0], pSkipWeightVirAddr_H265[0]);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_MPI_SYS_MmzFree err:0x%x", s32Ret);
        return NULL;
    }

    return NULL;
}
HI_S32 SAMPLE_COMM_VENC_QpmapSendFrame(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VENC_CHN VeChn[], HI_S32 s32Cnt,
    SIZE_S stSize)
{
    HI_S32 i;

    CHECK_NULL_PTR(VeChn);
    stQpMapSendFramePara.bThreadStart = HI_TRUE;
    stQpMapSendFramePara.VpssGrp = VpssGrp;
    stQpMapSendFramePara.VpssChn = VpssChn;
    stQpMapSendFramePara.s32Cnt = s32Cnt;
    stQpMapSendFramePara.stSize = stSize;
    for (i = 0; i < s32Cnt; i++) {
        stQpMapSendFramePara.VeChn[i] = VeChn[i];
    }

    return pthread_create(&gs_VencQpmapPid, 0, SAMPLE_COMM_QpmapSendFrameProc, (HI_VOID *)&stQpMapSendFramePara);
}

static HI_VOID *SAMPLE_COMM_VENC_GetVencStreamProc(HI_VOID *p)
{
    HI_S32 i;
    HI_S32 s32ChnTotal;
    VENC_CHN_ATTR_S stVencChnAttr;
    SAMPLE_VENC_GETSTREAM_PARA_S *pstPara = HI_NULL;
    HI_S32 maxfd = 0;
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_U32 u32PictureCnt[VENC_MAX_CHN_NUM] = {0};
    HI_S32 VencFd[VENC_MAX_CHN_NUM];
    HI_CHAR aszFileName[VENC_MAX_CHN_NUM][FILE_NAME_LEN];
    HI_CHAR real_file_name[VENC_MAX_CHN_NUM][PATH_MAX];
    FILE* pFile[VENC_MAX_CHN_NUM] = {HI_NULL};
    HI_S32 fd[VENC_MAX_CHN_NUM] = {0};
    char szFilePostfix[10]; /* length set 10 */
    VENC_CHN_STATUS_S stStat;
    VENC_STREAM_S stStream;
    HI_S32 s32Ret;
    VENC_CHN VencChn;
    PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];
    VENC_STREAM_BUF_INFO_S stStreamBufInfo[VENC_MAX_CHN_NUM];

    prctl(PR_SET_NAME, "GetVencStream", 0, 0, 0);

    pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S *)p;
    s32ChnTotal = pstPara->s32Cnt;
    if (s32ChnTotal >= VENC_MAX_CHN_NUM) {
        SAMPLE_PRT("input count invalid\n");
        return NULL;
    }
    for (i = 0; i < s32ChnTotal; i++) {
        /* decide the stream file name, and open file to save stream */
        VencChn = pstPara->VeChn[i];
        s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", VencChn, s32Ret);
            return NULL;
        }
        enPayLoadType[i] = stVencChnAttr.stVencAttr.enType;

        s32Ret = SAMPLE_COMM_VENC_GetFilePostfix(enPayLoadType[i], szFilePostfix, sizeof(szFilePostfix));
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("SAMPLE_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n", stVencChnAttr.stVencAttr.enType,
                s32Ret);
            return NULL;
        }
        // if (PT_JPEG != enPayLoadType[i]) {
#ifndef OHOS_CONFIG_WITHOUT_SAVE_STREAM
            // if (snprintf_s(aszFileName[i], FILE_NAME_LEN, FILE_NAME_LEN - 1, "./") < 0) {
            //     return NULL;
            // }
            // if (realpath(aszFileName[i], real_file_name[i]) == NULL) {
            //     SAMPLE_PRT("chn %d stream path error\n", VencChn);
            // }

            // if (snprintf_s(real_file_name[i], FILE_NAME_LEN, FILE_NAME_LEN - 1, "stream_chn%d%s", i, szFilePostfix) <
            //     0) {
            //     return NULL;
            // }

            // fd[i] = open(real_file_name[i], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            // if (fd[i] < 0) {
            //     SAMPLE_PRT("open file[%s] failed!\n", real_file_name[i]);
            //     return NULL;
            // }

            // pFile[i] = fdopen(fd[i], "wb");
            // if (pFile[i] == HI_NULL) {
            //     SAMPLE_PRT("fdopen error!\n");
            //     close(fd[i]);
            //     return NULL;
            // }
#endif
        // }
        VencFd[i] = HI_MPI_VENC_GetFd(VencChn);
        if (VencFd[i] < 0) {
            SAMPLE_PRT("HI_MPI_VENC_GetFd failed with %#x!\n", VencFd[i]);
            return NULL;
        }
        if (maxfd <= VencFd[i]) {
            maxfd = VencFd[i];
        }

        s32Ret = HI_MPI_VENC_GetStreamBufInfo(VencChn, &stStreamBufInfo[i]);
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("HI_MPI_VENC_GetStreamBufInfo failed with %#x!\n", s32Ret);
            return (void *)HI_FAILURE;
        }
    }

    while (HI_TRUE == pstPara->bThreadStart) {
#ifndef CONFIG_USER_SPACE
        FD_ZERO(&read_fds);
        for (i = 0; i < s32ChnTotal; i++) {
            FD_SET(VencFd[i], &read_fds);
        }

        TimeoutVal.tv_sec = 2; /* 2 s */
        TimeoutVal.tv_usec = 0;
        s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0) {
            SAMPLE_PRT("select failed!\n");
            break;
        } else if (s32Ret == 0) {
            SAMPLE_PRT("get venc stream time out, exit thread\n");
            continue;
        } else {
            for (i = 0; i < s32ChnTotal; i++) {
                if (FD_ISSET(VencFd[i], &read_fds)) {
#else
                for (i = 0; i < s32ChnTotal; i++) {
#endif
                    (HI_VOID)memset_s(&stStream, sizeof(stStream), 0, sizeof(stStream));

                    VencChn = pstPara->VeChn[i];

                    s32Ret = HI_MPI_VENC_QueryStatus(VencChn, &stStat);
                    if (s32Ret != HI_SUCCESS) {
                        SAMPLE_PRT("HI_MPI_VENC_QueryStatus chn[%d] failed with %#x!\n", VencChn, s32Ret);
                        break;
                    }
                    if (stStat.u32CurPacks == 0) {
#ifdef CONFIG_USER_SPACE
                        usleep(10 * 1000);
#endif
                        continue;
                    }
                    stStream.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (stStream.pstPack == NULL) {
                        SAMPLE_PRT("malloc stream pack failed!\n");
                        break;
                    }

                    stStream.u32PackCount = stStat.u32CurPacks;
                    s32Ret = HI_MPI_VENC_GetStream(VencChn, &stStream, HI_TRUE);
                    if (s32Ret != HI_SUCCESS) {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        SAMPLE_PRT("HI_MPI_VENC_GetStream failed with %#x!\n", s32Ret);
                        break;
                    }
                    HisiPutH264DataToBuffer(&stStream);
#ifdef OHOS_CONFIG_WITHOUT_SAVE_STREAM
                    static int once = 0;
                    if (once == 0) {
                        printf("get first frame pts:%llu!\n", stStream.pstPack[0].u64PTS);
                        once = 1;
                    }
#endif
                    // if (PT_JPEG == enPayLoadType[i]) {
                    //     if (strcpy_s(szFilePostfix, sizeof(szFilePostfix), ".jpg") != EOK) {
                    //         free(stStream.pstPack);
                    //         stStream.pstPack = NULL;
                    //         return HI_NULL;
                    //     }
                    //     if (snprintf_s(aszFileName[i], FILE_NAME_LEN, FILE_NAME_LEN - 1, "./") < 0) {
                    //         free(stStream.pstPack);
                    //         stStream.pstPack = NULL;
                    //         return NULL;
                    //     }
                    //     if (realpath(aszFileName[i], real_file_name[i]) == NULL) {
                    //         SAMPLE_PRT("chn %d stream path error\n", VencChn);
                    //         free(stStream.pstPack);
                    //         stStream.pstPack = NULL;
                    //         return NULL;
                    //     }

                    //     if (snprintf_s(real_file_name[i], FILE_NAME_LEN, FILE_NAME_LEN - 1, "stream_chn%d_%d%s",
                    //         VencChn, u32PictureCnt[i], szFilePostfix) < 0) {
                    //         free(stStream.pstPack);
                    //         stStream.pstPack = NULL;
                    //         return NULL;
                    //     }

                    //     fd[i] = open(real_file_name[i], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                    //     if (fd[i] < 0) {
                    //         SAMPLE_PRT("open file[%s] failed!\n", real_file_name[i]);
                    //         free(stStream.pstPack);
                    //         stStream.pstPack = NULL;
                    //         return NULL;
                    //     }

                    //     pFile[i] = fdopen(fd[i], "wb");
                    //     if (!pFile[i]) {
                    //         free(stStream.pstPack);
                    //         stStream.pstPack = NULL;
                    //         SAMPLE_PRT("fdopen err!\n");
                    //         close(fd[i]);
                    //         return NULL;
                    //     }
                    // }
#ifndef OHOS_CONFIG_WITHOUT_SAVE_STREAM
#if (!defined(__HuaweiLite__)) || defined(__OHOS__)
                    // s32Ret = SAMPLE_COMM_VENC_SaveStream(pFile[i], &stStream);
#else
                    // s32Ret = SAMPLE_COMM_VENC_SaveStream_PhyAddr(pFile[i], &stStreamBufInfo[i], &stStream);
#endif
                    // if (s32Ret != HI_SUCCESS) {
                    //     free(stStream.pstPack);
                    //     stStream.pstPack = NULL;
                    //     SAMPLE_PRT("save stream failed!\n");
                    //     break;
                    // }
#endif
                    s32Ret = HI_MPI_VENC_ReleaseStream(VencChn, &stStream);
                    if (s32Ret != HI_SUCCESS) {
                        SAMPLE_PRT("HI_MPI_VENC_ReleaseStream failed!\n");
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        break;
                    }

                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    u32PictureCnt[i]++;
                    // if (PT_JPEG == enPayLoadType[i]) {
                    //     (HI_VOID)fclose(pFile[i]);
                    // }
                }
#ifndef CONFIG_USER_SPACE
            }
        }
#endif
    }

#ifndef OHOS_CONFIG_WITHOUT_SAVE_STREAM
    // for (i = 0; i < s32ChnTotal; i++) {
    //     if (PT_JPEG != enPayLoadType[i]) {
    //         (HI_VOID)fclose(pFile[i]);
    //     }
    // }
#endif
    return NULL;
}

static HI_VOID *SAMPLE_COMM_VENC_GetVencStreamProc_Svc_t(void *p)
{
    HI_S32 i = 0;
    HI_S32 s32Cnt = 0;
    HI_S32 s32ChnTotal;
    VENC_CHN_ATTR_S stVencChnAttr;
    SAMPLE_VENC_GETSTREAM_PARA_S *pstPara = HI_NULL;
    HI_S32 maxfd = 0;
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_S32 VencFd[VENC_MAX_CHN_NUM];
    HI_S32 fd[VENC_MAX_CHN_NUM] = {0};
    HI_CHAR aszFileName[VENC_MAX_CHN_NUM][FILE_NAME_LEN];
    HI_CHAR real_file_name[VENC_MAX_CHN_NUM][FILE_NAME_LEN];
    FILE* pFile[VENC_MAX_CHN_NUM] = {HI_NULL};
    char szFilePostfix[10];
    VENC_CHN_STATUS_S stStat;
    VENC_STREAM_S stStream;
    HI_S32 s32Ret;
    VENC_CHN VencChn;
    PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];
    VENC_STREAM_BUF_INFO_S stStreamBufInfo[VENC_MAX_CHN_NUM];

    pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S *)p;
    s32ChnTotal = pstPara->s32Cnt;

    if (s32ChnTotal >= VENC_MAX_CHN_NUM) {
        SAMPLE_PRT("input count invalid\n");
        return NULL;
    }
    for (i = 0; i < s32ChnTotal; i++) {
        /* decide the stream file name, and open file to save stream */
        VencChn = i;
        s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", VencChn, s32Ret);
            return NULL;
        }
        enPayLoadType[i] = stVencChnAttr.stVencAttr.enType;

        s32Ret = SAMPLE_COMM_VENC_GetFilePostfix(enPayLoadType[i], szFilePostfix, sizeof(szFilePostfix));
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("SAMPLE_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n", stVencChnAttr.stVencAttr.enType,
                s32Ret);
            return NULL;
        }

        for (s32Cnt = 0; s32Cnt < 3; s32Cnt++) { /* 3 file */
            if (snprintf_s(aszFileName[i + s32Cnt], FILE_NAME_LEN, FILE_NAME_LEN - 1, "./") < 0) {
                return HI_NULL;
            }
            if (realpath(aszFileName[i + s32Cnt], real_file_name[i + s32Cnt]) == HI_NULL) {
                printf("file path error\n");
                return HI_NULL;
            }
            if (snprintf_s(real_file_name[i + s32Cnt], FILE_NAME_LEN, FILE_NAME_LEN - 1, "Tid%d%s", i + s32Cnt,
                szFilePostfix) < 0) {
                return HI_NULL;
            }

            fd[i + s32Cnt] = open(real_file_name[i + s32Cnt], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            if (fd[i] < 0) {
                SAMPLE_PRT("open file[%s] failed!\n", real_file_name[i + s32Cnt]);
                return NULL;
            }
            pFile[i + s32Cnt] = fdopen(fd[i + s32Cnt], "wb");

            if (!pFile[i + s32Cnt]) {
                SAMPLE_PRT("fdopen error!\n");
                close(fd[i + s32Cnt]);
                return NULL;
            }
        }

        /* Set Venc Fd. */
        VencFd[i] = HI_MPI_VENC_GetFd(i);
        if (VencFd[i] < 0) {
            SAMPLE_PRT("HI_MPI_VENC_GetFd failed with %#x!\n", VencFd[i]);
            return NULL;
        }
        if (maxfd <= VencFd[i]) {
            maxfd = VencFd[i];
        }
        s32Ret = HI_MPI_VENC_GetStreamBufInfo(i, &stStreamBufInfo[i]);
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("HI_MPI_VENC_GetStreamBufInfo failed with %#x!\n", s32Ret);
            return NULL;
        }
    }

    while (HI_TRUE == pstPara->bThreadStart) {
        FD_ZERO(&read_fds);
        for (i = 0; i < s32ChnTotal; i++) {
            FD_SET(VencFd[i], &read_fds);
        }
        TimeoutVal.tv_sec = 2; /* 2 s */
        TimeoutVal.tv_usec = 0;
        s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0) {
            SAMPLE_PRT("select failed!\n");
            break;
        } else if (s32Ret == 0) {
            SAMPLE_PRT("get venc stream time out, exit thread\n");
            continue;
        } else {
            for (i = 0; i < s32ChnTotal; i++) {
                if (FD_ISSET(VencFd[i], &read_fds)) {
                    (HI_VOID)memset_s(&stStream, sizeof(stStream), 0, sizeof(stStream));
                    s32Ret = HI_MPI_VENC_QueryStatus(i, &stStat);
                    if (s32Ret != HI_SUCCESS) {
                        SAMPLE_PRT("HI_MPI_VENC_Query chn[%d] failed with %#x!\n", i, s32Ret);
                        break;
                    }

                    if (stStat.u32CurPacks == 0) {
                        SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                        continue;
                    }

                    stStream.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (stStream.pstPack == NULL) {
                        SAMPLE_PRT("malloc stream pack failed!\n");
                        break;
                    }

                    stStream.u32PackCount = stStat.u32CurPacks;
                    s32Ret = HI_MPI_VENC_GetStream(i, &stStream, HI_TRUE);
                    if (s32Ret != HI_SUCCESS) {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        SAMPLE_PRT("HI_MPI_VENC_GetStream failed with %#x!\n", s32Ret);
                        break;
                    }

                    for (s32Cnt = 0; s32Cnt < 3; s32Cnt++) { /* loop 3 file */
                        switch (s32Cnt) {
                            case 0: /* 0: save BASE_PSLICE_REFBYBASE frame */
                                if (BASE_IDRSLICE == stStream.stH264Info.enRefType ||
                                    BASE_PSLICE_REFBYBASE == stStream.stH264Info.enRefType) {
#if (!defined(__HuaweiLite__)) || defined(__OHOS__)
                                    s32Ret = SAMPLE_COMM_VENC_SaveStream(pFile[i + s32Cnt], &stStream);
#else
                                    s32Ret = SAMPLE_COMM_VENC_SaveStream_PhyAddr(pFile[i + s32Cnt], &stStreamBufInfo[i],
                                        &stStream);
#endif
                                }
                                break;
                            case 1: /* 1: save BASE_PSLICE_REFBYBASE and BASE_PSLICE_REFBYENHANCE frame */
                                if (BASE_IDRSLICE == stStream.stH264Info.enRefType ||
                                    BASE_PSLICE_REFBYBASE == stStream.stH264Info.enRefType ||
                                    BASE_PSLICE_REFBYENHANCE == stStream.stH264Info.enRefType) {
#if (!defined(__HuaweiLite__)) || defined(__OHOS__)
                                    s32Ret = SAMPLE_COMM_VENC_SaveStream(pFile[i + s32Cnt], &stStream);
#else
                                    s32Ret = SAMPLE_COMM_VENC_SaveStream_PhyAddr(pFile[i + s32Cnt], &stStreamBufInfo[i],
                                        &stStream);
#endif
                                }
                                break;
                            case 2: /* 2: save all frame */
#if (!defined(__HuaweiLite__)) || defined(__OHOS__)
                                s32Ret = SAMPLE_COMM_VENC_SaveStream(pFile[i + s32Cnt], &stStream);
#else
                                s32Ret = SAMPLE_COMM_VENC_SaveStream_PhyAddr(pFile[i + s32Cnt], &stStreamBufInfo[i],
                                    &stStream);
#endif
                                break;
                        }
                        if (s32Ret != HI_SUCCESS) {
                            free(stStream.pstPack);
                            stStream.pstPack = NULL;
                            SAMPLE_PRT("save stream failed!\n");
                            break;
                        }
                    }

                    s32Ret = HI_MPI_VENC_ReleaseStream(i, &stStream);
                    if (s32Ret != HI_SUCCESS) {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        break;
                    }

                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                }
            }
        }
    }

    for (i = 0; i < s32ChnTotal; i++) {
        for (s32Cnt = 0; s32Cnt < 3; s32Cnt++) { /* loop 3 file */
            if (pFile[i + s32Cnt]) {
                (HI_VOID)fclose(pFile[i + s32Cnt]);
            }
        }
    }
    return NULL;
}

HI_S32 SAMPLE_COMM_VENC_StartGetStream(VENC_CHN VeChn[], HI_S32 s32Cnt)
{
    HI_S32 i;

    if (VeChn == NULL) {
        SAMPLE_PRT("null pointer\n");
        return HI_FAILURE;
    }
    gs_stPara.bThreadStart = HI_TRUE;
    gs_stPara.s32Cnt = s32Cnt;
    for (i = 0; i < s32Cnt && i < VENC_MAX_CHN_NUM; i++) {
        gs_stPara.VeChn[i] = VeChn[i];
    }
    return pthread_create(&gs_VencPid, 0, SAMPLE_COMM_VENC_GetVencStreamProc, (HI_VOID *)&gs_stPara);
}

HI_S32 SAMPLE_COMM_VENC_StartGetStream_Svc_t(HI_S32 s32Cnt)
{
    gs_stPara.bThreadStart = HI_TRUE;
    gs_stPara.s32Cnt = s32Cnt;
    return pthread_create(&gs_VencPid, 0, SAMPLE_COMM_VENC_GetVencStreamProc_Svc_t, (HI_VOID *)&gs_stPara);
}

HI_S32 SAMPLE_COMM_VENC_StopGetStream(void)
{
    if (HI_TRUE == gs_stPara.bThreadStart) {
        gs_stPara.bThreadStart = HI_FALSE;
        pthread_join(gs_VencPid, 0);
    }
    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_VENC_StopSendQpmapFrame(void)
{
    if (HI_TRUE == stQpMapSendFramePara.bThreadStart) {
        stQpMapSendFramePara.bThreadStart = HI_FALSE;
        pthread_join(gs_VencQpmapPid, 0);
    }
    return HI_SUCCESS;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
