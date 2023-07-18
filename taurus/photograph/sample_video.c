
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>

#include "sample_comm.h"
#include "hi_mipi_tx.h"
#include "mpi_snap.h"
#include "uart.h"

#define USLEEP_TIME   1000 // 1000: usleep time, in microseconds
#define G_MBUF_LENGTH 50 // 50: length of g_mbuf
#define ALIGN_DOWN_SIZE 2
#define G_MBUF_ARRAY_SUBSCRIPT_0     0
#define G_MBUF_ARRAY_SUBSCRIPT_1     1
#define G_MBUF_ARRAY_SUBSCRIPT_2     2
#define G_MBUF_ARRAY_SUBSCRIPT_3     3
#define G_MBUF_ARRAY_SUBSCRIPT_4     4
#define G_MBUF_ARRAY_SUBSCRIPT_5     5
#define G_MBUF_ARRAY_SUBSCRIPT_6     6
#define G_MBUF_ARRAY_SUBSCRIPT_7     7
#define G_MBUF_ARRAY_SUBSCRIPT_8     8
#define G_MBUF_ARRAY_SUBSCRIPT_9     9
#define G_MBUF_ARRAY_SUBSCRIPT_10    10
#define G_MBUF_ARRAY_SUBSCRIPT_11    11
#define G_MBUF_ARRAY_SUBSCRIPT_12    12
#define G_MBUF_ARRAY_SUBSCRIPT_13    13
#define G_MBUF_ARRAY_SUBSCRIPT_14    14
#define G_MBUF_ARRAY_SUBSCRIPT_15    15
#define G_MBUF_ARRAY_SUBSCRIPT_16    16

#define LANE_ID_SUBSCRIPT_0    0
#define LANE_ID_SUBSCRIPT_1    1
#define LANE_ID_SUBSCRIPT_2    2
#define LANE_ID_SUBSCRIPT_3    3

static unsigned char g_mBuf[G_MBUF_LENGTH];

/******************************************************************************
* function : show usage
******************************************************************************/
void SAMPLE_SNAP_Usage(char* sPrgNm)
{
    printf("Usage : %s <index> \n", sPrgNm);
    printf("index:\n");
    printf("\t 0)double pipe offline, normal snap.\n");

    return;
}

HI_VOID SAMPLE_VOU_SYS_Exit(void)
{
    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();
}

HI_VOID SAMPLE_VO_GetUserLayerAttr(VO_VIDEO_LAYER_ATTR_S *pstLayerAttr, SIZE_S *pstDevSize)
{
    pstLayerAttr->bClusterMode = HI_FALSE;
    pstLayerAttr->bDoubleFrame = HI_FALSE;
    pstLayerAttr->enDstDynamicRange = DYNAMIC_RANGE_SDR8;
    pstLayerAttr->enPixFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;

    pstLayerAttr->stDispRect.s32X = 0;
    pstLayerAttr->stDispRect.s32Y = 0;
    pstLayerAttr->stDispRect.u32Height = pstDevSize->u32Height;
    pstLayerAttr->stDispRect.u32Width  = pstDevSize->u32Width;

    pstLayerAttr->stImageSize.u32Height = pstDevSize->u32Height;
    pstLayerAttr->stImageSize.u32Width = pstDevSize->u32Width;

    return;
}

HI_VOID SAMPLE_VO_GetUserChnAttr(VO_CHN_ATTR_S *pstChnAttr, SIZE_S *pstDevSize, HI_S32 VoChnNum)
{
    HI_S32 i;
    for (i = 0; i < VoChnNum; i++) {
        pstChnAttr[i].bDeflicker = HI_FALSE;
        pstChnAttr[i].u32Priority = 0;
        pstChnAttr[i].stRect.s32X = 0;
        pstChnAttr[i].stRect.s32Y = 0;
        pstChnAttr[i].stRect.u32Height = pstDevSize->u32Height;
        pstChnAttr[i].stRect.u32Width = pstDevSize->u32Width;
        }

    return;
}

/* open mipi_tx device */
HI_S32 SampleOpenMipiTxFd(HI_VOID)
{
    HI_S32 fd;

    fd = open("/dev/hi_mipi_tx", O_RDWR);
    if (fd < 0) {
        printf("open hi_mipi_tx dev failed\n");
    }
    return fd;
}

/* close mipi_tx device */
HI_VOID SampleCloseMipiTxFd(HI_S32 fd)
{
    close(fd);
    return;
}

/* get mipi tx config information */
HI_VOID SAMPLE_GetMipiTxConfig(combo_dev_cfg_t *pstMipiTxConfig)
{
    /* USER NEED SET MIPI DEV CONFIG */
    pstMipiTxConfig->devno = 0;
    pstMipiTxConfig->lane_id[LANE_ID_SUBSCRIPT_0] = 0;
    pstMipiTxConfig->lane_id[LANE_ID_SUBSCRIPT_1] = 1;
    // -1: 2 lane mode configuration,lane_id[4] = {0, 1, -1, -1}
    pstMipiTxConfig->lane_id[LANE_ID_SUBSCRIPT_2] = -1;
    // -1: 2 lane mode configuration,lane_id[4] = {0, 1, -1, -1}
    pstMipiTxConfig->lane_id[LANE_ID_SUBSCRIPT_3] = -1;
    pstMipiTxConfig->output_mode = OUTPUT_MODE_DSI_VIDEO;
    pstMipiTxConfig->output_format = OUT_FORMAT_RGB_24_BIT;
    pstMipiTxConfig->video_mode = BURST_MODE;
    pstMipiTxConfig->sync_info.vid_pkt_size = 480; // 480: received packet size
    pstMipiTxConfig->sync_info.vid_hsa_pixels = 10; // 10: The number of pixels in the input line sync pulse area
    pstMipiTxConfig->sync_info.vid_hbp_pixels = 50; // 50: Number of pixels in blanking area after input
    pstMipiTxConfig->sync_info.vid_hline_pixels = 590; // 590: The total number of pixels detected per line
    pstMipiTxConfig->sync_info.vid_vsa_lines = 4; // 4: Number of frame sync pulse lines detected
    pstMipiTxConfig->sync_info.vid_vbp_lines = 20; // 20: Number of blanking area lines after frame sync pulse
    pstMipiTxConfig->sync_info.vid_vfp_lines = 20; // 20：Number of blanking area lines before frame sync pulse
    pstMipiTxConfig->sync_info.vid_active_lines = 800; // 800: VACTIVE rows
    pstMipiTxConfig->sync_info.edpi_cmd_size = 0; // 0: Write memory command bytes
    pstMipiTxConfig->phy_data_rate = 359; // 359: MIPI Tx output rate
    pstMipiTxConfig->pixel_clk = 29878; // 29878: pixel clock. The unit is KHz

    return;
}

/* set mipi tx config information */
HI_S32 SAMPLE_SetMipiTxConfig(HI_S32 fd, combo_dev_cfg_t *pstMipiTxConfig)
{
    HI_S32 s32Ret;
    s32Ret = ioctl(fd, HI_MIPI_TX_SET_DEV_CFG, pstMipiTxConfig);
    if (s32Ret != HI_SUCCESS) {
        printf("MIPI_TX SET_DEV_CONFIG failed\n");
        SampleCloseMipiTxFd(fd);
        return s32Ret;
    }
    return s32Ret;
}

/* set mipi tx device config */
HI_S32 SampleSetMipiTxDevAttr(HI_S32 fd)
{
    HI_S32 s32Ret;
    combo_dev_cfg_t stMipiTxConfig;

    /* USER SET MIPI DEV CONFIG */
    SAMPLE_GetMipiTxConfig(&stMipiTxConfig);

    /* USER SET MIPI DEV CONFIG */
    s32Ret = SAMPLE_SetMipiTxConfig(fd, &stMipiTxConfig);

    return s32Ret;
}

/* init mipi tx device */
HI_S32 SAMPLE_USER_INIT_MIPITx(HI_S32 fd, cmd_info_t *pcmd_info)
{
    HI_S32 s32Ret;

    s32Ret = ioctl(fd, HI_MIPI_TX_SET_CMD, pcmd_info);
    if (s32Ret !=  HI_SUCCESS) {
        printf("MIPI_TX SET CMD failed\n");
        SampleCloseMipiTxFd(fd);
        return s32Ret;
    }

    return HI_SUCCESS;
}

/* set mipi tx device init screen */
HI_S32 SampleVoInitMipiTxScreen(HI_S32 fd)
{
    HI_S32 s32Ret;
    cmd_info_t cmd_info;
	memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x77;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x01;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x13;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 6; // 6: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x08ef;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x77;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x01;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x10;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 6; // 6: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xC0;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x63;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x00;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 3; // 3: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xC1;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x10;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x02;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 3; // 3: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xC2;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x01;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x08;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 3; // 3: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x18CC;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xB0;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x40;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0xC9;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x8F;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x0D;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x11;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0x07;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_7] = 0x02;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_8] = 0x09;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_9] = 0x09;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_10] = 0x1F;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_11] = 0x04;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_12] = 0x50;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_13] = 0x0F;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_14] = 0xE4;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_15] = 0x29;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_16] = 0xDF;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 17; // 17: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xB1;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x40;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0xCB;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0xD3;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x11;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x8F;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0x04;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_7] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_8] = 0x08;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_9] = 0x07;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_10] = 0x1C;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_11] = 0x06;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_12] = 0x53;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_13] = 0x12;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_14] = 0x63;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_15] = 0xEB;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_16] = 0xDF;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 17; // 17: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x77;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x01;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x11;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 6; // 6: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x65b0;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x34b1;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x87b2;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x80b3;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x49b5;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x85b7;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x20b8;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x10b9;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x78c1;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x78c2;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x88d0;
    cmd_info.data_type = 0x23;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);
    usleep(100000);  // 100000: The process hangs for a period of time, in microseconds

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE0;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x19;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x02;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 4; // 4: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE1;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x05;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0xA0;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x07;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0xA0;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x04;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0xA0;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_7] = 0x06;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_8] = 0xA0;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_9] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_10] = 0x44;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_11] = 0x44;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 12; // 12: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE2;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_7] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_8] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_9] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_10] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_11] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_12] = 0x00;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 13; // 13: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE3;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x33;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x33;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 5; // 5: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE4;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x44;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x44;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 3; // 3: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE5;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x0D;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x31;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0xC8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0xAF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x0F;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0x33;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_7] = 0xC8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_8] = 0xAF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_9] = 0x09;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_10] = 0x2D;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_11] = 0xC8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_12] = 0xAF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_13] = 0x0B;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_14] = 0x2F;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_15] = 0xC8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_16] = 0xAF;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 17; // 17: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE6;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x33;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x33;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 5; // 5: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE7;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x44;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x44;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 3; // 3: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x0C;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x30;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0xC8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0xAF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x0E;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0x32;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_7] = 0xC8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_8] = 0xAF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_9] = 0x08;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_10] = 0x2C;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_11] = 0xC8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_12] = 0xAF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_13] = 0x0A;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_14] = 0x2E;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_15] = 0xC8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_16] = 0xAF;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 17; // 17: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xEB;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x02;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0xE4;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0xE4;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x44;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_7] = 0x40;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 8; // 8: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xEC;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x3C;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x00;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 3; // 3: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xED;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0xAB;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x89;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x76;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x54;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x01;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_7] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_8] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_9] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_10] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_11] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_12] = 0x10;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_13] = 0x45;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_14] = 0x67;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_15] = 0x98;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_16] = 0xBA;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 17; // 17: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xEF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x08;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x08;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x08;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x45;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x3F;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_6] = 0x54;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 7; // 7: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x77;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x01;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x00;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 6; // 6: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x77;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x01;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x13;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 6; // 6: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x0E;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x11;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 4; // 4: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);
    usleep(120000); // 120000: The process hangs for a period of time, in microseconds

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x0C;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 3; // 3: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);
    usleep(10000); // 10000: The process hangs for a period of time, in microseconds

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xE8;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x00;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 3; // 3: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);
    usleep(10000); // 10000: The process hangs for a period of time, in microseconds

    memset_s(g_mBuf, sizeof(g_mBuf), 0, G_MBUF_LENGTH);
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_0] = 0xFF;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_1] = 0x77;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_2] = 0x01;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_3] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_4] = 0x00;
    g_mBuf[G_MBUF_ARRAY_SUBSCRIPT_5] = 0x00;
    cmd_info.devno = 0;
    cmd_info.cmd_size = 6; // 6: command data size
    cmd_info.data_type = 0x29;
    cmd_info.cmd = g_mBuf;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);
    usleep(10000); // 10000: The process hangs for a period of time, in microseconds

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x11;
    cmd_info.data_type = 0x05;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);
    usleep(150000); // 150000: The process hangs for a period of time, in microseconds

    cmd_info.devno = 0;
    cmd_info.cmd_size = 0x29;
    cmd_info.data_type = 0x05;
    cmd_info.cmd = NULL;
    s32Ret = SAMPLE_USER_INIT_MIPITx(fd, &cmd_info);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(USLEEP_TIME);
    usleep(50000); // 50000: The process hangs for a period of time, in microseconds

    return HI_SUCCESS;
}

/* enable mipi tx */
HI_S32 SAMPLE_VO_ENABLE_MIPITx(HI_S32 fd)
{
    HI_S32 s32Ret;

    s32Ret = ioctl(fd, HI_MIPI_TX_ENABLE);
    if (s32Ret != HI_SUCCESS) {
        printf("MIPI_TX enable failed\n");
        return s32Ret;
    }

    return s32Ret;
}

/* disable mipi tx */
HI_S32 SAMPLE_VO_DISABLE_MIPITx(HI_S32 fd)
{
    HI_S32 s32Ret;
    s32Ret = ioctl(fd, HI_MIPI_TX_DISABLE);
    if (s32Ret != HI_SUCCESS) {
        printf("MIPI_TX disable failed\n");
        return s32Ret;
    }

    return s32Ret;
}

/* onfig mipi */
HI_S32 SAMPLE_VO_CONFIG_MIPI(HI_S32* mipiFD)
{
    HI_S32 s32Ret;
    /* SET MIPI BAKCLIGHT */
    HI_S32  fd;
    /* CONFIG MIPI PINUMX */

    /* Reset MIPI */
    /* OPEN MIPI FD */
    fd = SampleOpenMipiTxFd();
    if (fd < 0) {
        return HI_FAILURE;
    }
	*mipiFD = fd;

    /* SET MIPI Tx Dev ATTR */
    s32Ret = SampleSetMipiTxDevAttr(fd);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }
    usleep(10000); // 10000: The process hangs for a period of time, in microseconds
    system("cd /sys/class/gpio/;echo 55 > export;echo out > gpio55/direction;echo 1 > gpio55/value");
    system("cd /sys/class/gpio/;echo 5 > export;echo out > gpio5/direction;echo 1 > gpio5/value");
    usleep(200000); // 200000: The process hangs for a period of time, in microseconds
    system("echo 0 > /sys/class/gpio/gpio5/value");
    usleep(200000); // 200000: The process hangs for a period of time, in microseconds
    system("echo 1 > /sys/class/gpio/gpio5/value");
    usleep(20000); // 20000: The process hangs for a period of time, in microseconds

    /* CONFIG MIPI Tx INITIALIZATION SEQUENCE */
    s32Ret = SampleVoInitMipiTxScreen(fd);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }

    /* ENABLE MIPI Tx DEV */
    s32Ret = SAMPLE_VO_ENABLE_MIPITx(fd);
    if (s32Ret != HI_SUCCESS) {
        return s32Ret;
    }

    return s32Ret;
}

/* get mipi device Height and width */
HI_S32 SampleCommVoGetWhMipi(VO_INTF_SYNC_E enIntfSync, HI_U32* pu32W, HI_U32* pu32H, HI_U32* pu32Frm)
{
    switch (enIntfSync) {
        case VO_OUTPUT_PAL:
            *pu32W = 720; // 720: VO_OUTPUT_PAL-Width
            *pu32H = 576; // 576: VO_OUTPUT_PAL-Height
            *pu32Frm = 25; // 25: VO_OUTPUT_PAL-Frame rate
            break;
        case VO_OUTPUT_NTSC:
            *pu32W = 720; // 720: VO_OUTPUT_NTSC-Width
            *pu32H = 480; // 480: VO_OUTPUT_NTSC-Height
            *pu32Frm = 30; // 30: VO_OUTPUT_NTSC-Frame rate
            break;
        case VO_OUTPUT_1080P24:
            *pu32W = 1920; // 1920: VO_OUTPUT_1080P24-Width
            *pu32H = 1080; // 1080: VO_OUTPUT_1080P24-Height
            *pu32Frm = 24; // 24: VO_OUTPUT_1080P24-Frame rate
            break;
        case VO_OUTPUT_1080P25:
            *pu32W = 1920; // 1920: VO_OUTPUT_1080P25-Width
            *pu32H = 1080; // 1080: VO_OUTPUT_1080P25-Height
            *pu32Frm = 25; // 25: VO_OUTPUT_1080P25-Frame rate
            break;
        case VO_OUTPUT_1080P30:
            *pu32W = 1920; // 1920: VO_OUTPUT_1080P30-Width
            *pu32H = 1080; // 1080: VO_OUTPUT_1080P30-Height
            *pu32Frm = 30; // 30: VO_OUTPUT_1080P30-Frame rate
            break;
        case VO_OUTPUT_720P50:
            *pu32W = 1280; // 1280: VO_OUTPUT_720P50-Width
            *pu32H = 720; // 720: VO_OUTPUT_720P50-Height
            *pu32Frm = 50; // 50: VO_OUTPUT_720P50-Frame rate
            break;
        case VO_OUTPUT_720P60:
            *pu32W = 1280; // 1280: VO_OUTPUT_720P60-Width
            *pu32H = 720; // 720: VO_OUTPUT_720P60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_720P60-Frame rate
            break;
        case VO_OUTPUT_1080I50:
            *pu32W = 1920; // 1920: VO_OUTPUT_1080I50-Width
            *pu32H = 1080; // 1080: VO_OUTPUT_1080I50-Height
            *pu32Frm = 50; // 50: VO_OUTPUT_1080I50-Frame rate
            break;
        case VO_OUTPUT_1080I60:
            *pu32W = 1920; // 1920: VO_OUTPUT_1080I60-Width
            *pu32H = 1080; // 1080: VO_OUTPUT_1080I60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_1080I60-Frame rate
            break;
        case VO_OUTPUT_1080P50:
            *pu32W = 1920; // 1920: VO_OUTPUT_1080P50-Width
            *pu32H = 1080; // 1080: VO_OUTPUT_1080P50-Height
            *pu32Frm = 50; // 50: VO_OUTPUT_1080P50-Frame rate
            break;
        case VO_OUTPUT_1080P60:
            *pu32W = 1920; // 1920: VO_OUTPUT_1080P60-Width
            *pu32H = 1080; // 1080: VO_OUTPUT_1080P60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_1080P60-Frame rate
            break;
        case VO_OUTPUT_576P50:
            *pu32W = 720; // 720: VO_OUTPUT_576P50-Width
            *pu32H = 576; // 576: VO_OUTPUT_576P50-Height
            *pu32Frm = 50; // 50: VO_OUTPUT_576P50-Frame rate
            break;
        case VO_OUTPUT_480P60:
            *pu32W = 720; // 720: VO_OUTPUT_480P60-Width
            *pu32H = 480; // 480: VO_OUTPUT_480P60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_480P60-Frame rate
            break;
        case VO_OUTPUT_800x600_60:
            *pu32W = 800; // 800: VO_OUTPUT_800x600_60-Width
            *pu32H = 600; // 600: VO_OUTPUT_800x600_60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_800x600_60-Frame rate
            break;
        case VO_OUTPUT_1024x768_60:
            *pu32W = 1024; // 1024: VO_OUTPUT_1024x768_60-Width
            *pu32H = 768; // 768: VO_OUTPUT_1024x768_60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_1024x768_60-Frame rate
            break;
        case VO_OUTPUT_1280x1024_60:
            *pu32W = 1280; // 1280: VO_OUTPUT_1280x1024_60-Width
            *pu32H = 1024; // 1024: VO_OUTPUT_1280x1024_60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_1280x1024_60-Frame rate
            break;
        case VO_OUTPUT_1366x768_60:
            *pu32W = 1366; // 1366: VO_OUTPUT_1366x768_60-Width
            *pu32H = 768; // 768: VO_OUTPUT_1366x768_60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_1366x768_60-Frame rate
            break;
        case VO_OUTPUT_1440x900_60:
            *pu32W = 1440; // 1440: VO_OUTPUT_1440x900_60-Width
            *pu32H = 900; // 900: VO_OUTPUT_1440x900_60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_1440x900_60-Frame rate
            break;
        case VO_OUTPUT_1280x800_60:
            *pu32W = 1280; // 1280: VO_OUTPUT_1280x800_60-Width
            *pu32H = 800; // 800: VO_OUTPUT_1280x800_60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_1280x800_60-Frame rate
            break;
        case VO_OUTPUT_1600x1200_60:
            *pu32W = 1600; // 1600: VO_OUTPUT_1600x1200_60-Width
            *pu32H = 1200; // 1200: VO_OUTPUT_1600x1200_60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_1600x1200_60-Frame rate
            break;
        case VO_OUTPUT_1680x1050_60:
            *pu32W = 1680; // 1680: VO_OUTPUT_1680x1050_60-Width
            *pu32H = 1050; // 1050: VO_OUTPUT_1680x1050_60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_1680x1050_60-Frame rate
            break;
        case VO_OUTPUT_1920x1200_60:
            *pu32W = 1920; // 1920: VO_OUTPUT_1920x1200_60-Width
            *pu32H = 1200; // 1200: VO_OUTPUT_1920x1200_60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_1920x1200_60-Frame rate
            break;
        case VO_OUTPUT_640x480_60:
            *pu32W = 640; // 640: VO_OUTPUT_640x480_60-Width
            *pu32H = 480; // 480: VO_OUTPUT_640x480_60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_640x480_60-Frame rate
            break;
        case VO_OUTPUT_960H_PAL:
            *pu32W = 960; // 960: VO_OUTPUT_960H_PAL-Width
            *pu32H = 576; // 576: VO_OUTPUT_960H_PAL-Height
            *pu32Frm = 50; // 50: VO_OUTPUT_960H_PAL-Frame rate
            break;
        case VO_OUTPUT_960H_NTSC:
            *pu32W = 960; // 960: VO_OUTPUT_960H_NTSC-Width
            *pu32H = 480; // 480: VO_OUTPUT_960H_NTSC-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_960H_NTSC-Frame rate
            break;
        case VO_OUTPUT_1920x2160_30:
            *pu32W = 1920; // 1920: VO_OUTPUT_1920x2160_30-Width
            *pu32H = 2160; // 2160: VO_OUTPUT_1920x2160_30-Height
            *pu32Frm = 30; // 30: VO_OUTPUT_1920x2160_30-Frame rate
            break;
        case VO_OUTPUT_2560x1440_30:
            *pu32W = 2560; // 2560: VO_OUTPUT_2560x1440_30-Width
            *pu32H = 1440; // 1440: VO_OUTPUT_2560x1440_30-Height
            *pu32Frm = 30; // 30: VO_OUTPUT_2560x1440_30-Frame rate
            break;
        case VO_OUTPUT_2560x1600_60:
            *pu32W = 2560; // 2560: VO_OUTPUT_2560x1600_60-Width
            *pu32H = 1600; // 1600: VO_OUTPUT_2560x1600_60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_2560x1600_60-Frame rate
            break;
        case VO_OUTPUT_3840x2160_30:
            *pu32W = 3840; // 3840: VO_OUTPUT_3840x2160_30-Width
            *pu32H = 2160; // 2160: VO_OUTPUT_3840x2160_30-Height
            *pu32Frm = 30; // 30: VO_OUTPUT_3840x2160_30-Frame rate
            break;
        case VO_OUTPUT_3840x2160_60:
            *pu32W = 3840; // 3840: VO_OUTPUT_3840x2160_60-Width
            *pu32H = 2160; // 2160: VO_OUTPUT_3840x2160_60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_3840x2160_60-Frame rate
            break;
        case VO_OUTPUT_320x240_60:
            *pu32W = 320; // 320: VO_OUTPUT_320x240_60-Width
            *pu32H = 240; // 240: VO_OUTPUT_320x240_60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_320x240_60-Frame rate
            break;
        case VO_OUTPUT_320x240_50:
            *pu32W = 320; // 320: VO_OUTPUT_320x240_50-Width
            *pu32H = 240; // 240: VO_OUTPUT_320x240_50-Height
            *pu32Frm = 50; // 50: VO_OUTPUT_320x240_50-Frame rate
            break;
        case VO_OUTPUT_240x320_50:
            *pu32W = 240; // 240: VO_OUTPUT_240x320_50-Width
            *pu32H = 320; // 320: VO_OUTPUT_240x320_50-Height
            *pu32Frm = 50; // 50: VO_OUTPUT_240x320_50-Frame rate
            break;
        case VO_OUTPUT_240x320_60:
            *pu32W = 240; // 240: VO_OUTPUT_240x320_60-Width
            *pu32H = 320; // 320: VO_OUTPUT_240x320_60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_240x320_60-Frame rate
            break;
        case VO_OUTPUT_800x600_50:
            *pu32W = 800; // 800: VO_OUTPUT_800x600_50-Width
            *pu32H = 600; // 600: VO_OUTPUT_800x600_50-Height
            *pu32Frm = 50; // 50: VO_OUTPUT_800x600_50-Frame rate
            break;
        case VO_OUTPUT_720x1280_60:
            *pu32W = 720; // 720: VO_OUTPUT_720x1280_60-Width
            *pu32H = 1280; // 1280: VO_OUTPUT_720x1280_60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_720x1280_60-Frame rate
            break;
        case VO_OUTPUT_1080x1920_60:
            *pu32W = 1080; // 1080: VO_OUTPUT_1080x1920_60-Width
            *pu32H = 1920; // 1920: VO_OUTPUT_1080x1920_60-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_1080x1920_60-Frame rate
            break;
        case VO_OUTPUT_7680x4320_30:
            *pu32W = 7680; // 7680: VO_OUTPUT_7680x4320_30-Width
            *pu32H = 4320; // 4320: VO_OUTPUT_7680x4320_30-Height
            *pu32Frm = 30; // 30: VO_OUTPUT_7680x4320_30-Frame rate
            break;
        case VO_OUTPUT_USER:
            *pu32W = 800; // 800: VO_OUTPUT_USER-Width
            *pu32H = 480; // 480: VO_OUTPUT_USER-Height
            *pu32Frm = 60; // 60: VO_OUTPUT_USER-Frame rate
            break;
        default:
            SAMPLE_PRT("vo enIntfSync %d not support!\n", enIntfSync);
            return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 SampleCommVoStartDevMipi(VO_DEV VoDev, VO_PUB_ATTR_S* pstPubAttr)
{
    HI_S32 s32Ret;
    VO_USER_INTFSYNC_INFO_S stUserInfo = {0};

    stUserInfo.bClkReverse = HI_TRUE;
    stUserInfo.u32DevDiv = 1;
    stUserInfo.u32PreDiv = 1;
    stUserInfo.stUserIntfSyncAttr.enClkSource = VO_CLK_SOURCE_PLL;
    stUserInfo.stUserIntfSyncAttr.stUserSyncPll.u32Fbdiv = 244; // 244: PLL integer frequency multiplier coefficient
    stUserInfo.stUserIntfSyncAttr.stUserSyncPll.u32Frac = 0x1A36;
    stUserInfo.stUserIntfSyncAttr.stUserSyncPll.u32Refdiv = 4; // 4: PLL reference clock frequency division coefficient
    // 7: PLL first stage output frequency division coefficient
    stUserInfo.stUserIntfSyncAttr.stUserSyncPll.u32Postdiv1 = 7;
    // 7: PLL second stage output frequency division coefficient
    stUserInfo.stUserIntfSyncAttr.stUserSyncPll.u32Postdiv2 = 7;
    HI_U32 u32Framerate = 60; // 60: device frame rate

    /* Set the common properties of the video output device */
    s32Ret = HI_MPI_VO_SetPubAttr(VoDev, pstPubAttr);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    /* Set the device frame rate under the device user timing */
    s32Ret = HI_MPI_VO_SetDevFrameRate(VoDev, u32Framerate);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    /* Set user interface timing information */
    s32Ret = HI_MPI_VO_SetUserIntfSyncInfo(VoDev, &stUserInfo);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    /* Enable video output device */
    s32Ret = HI_MPI_VO_Enable(VoDev);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}

HI_S32 SampleCommVoStartChnMipi(VO_LAYER VoLayer, SAMPLE_VO_MODE_E enMode)
{
    HI_S32 i;
    HI_S32 s32Ret    = HI_SUCCESS;
    HI_U32 u32WndNum = 0;
    HI_U32 u32Square = 0;
    HI_U32 u32Row    = 0;
    HI_U32 u32Col    = 0;
    HI_U32 u32Width  = 0;
    HI_U32 u32Height = 0;
    VO_CHN_ATTR_S         stChnAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;

    switch (enMode) {
        case VO_MODE_1MUX:
            u32WndNum = 1;
            u32Square = 1;
            break;
        case VO_MODE_2MUX:
            u32WndNum = 2; // 2: 2MUX-WndNum
            u32Square = 2; // 2: 2MUX-Square
            break;
        case VO_MODE_4MUX:
            u32WndNum = 4; // 4: 4MUX-WndNum
            u32Square = 2; // 2: 4MUX-Square
            break;
        case VO_MODE_8MUX:
            u32WndNum = 8; // 8: 8MUX-WndNum
            u32Square = 3; // 3: 8MUX-Square
            break;
        case VO_MODE_9MUX:
            u32WndNum = 9; // 9: 9MUX-WndNum
            u32Square = 3; // 3: 9MUX-Square
            break;
        case VO_MODE_16MUX:
            u32WndNum = 16; // 16: 16MUX-WndNum
            u32Square = 4; // 4: 16MUX-Square
            break;
        case VO_MODE_25MUX:
            u32WndNum = 25; // 25: 25MUX-WndNum
            u32Square = 5; // 5: 25MUX-Square
            break;
        case VO_MODE_36MUX:
            u32WndNum = 36; // 36: 36MUX-WndNum
            u32Square = 6; // 6: 36MUX-Square
            break;
        case VO_MODE_49MUX:
            u32WndNum = 49; // 49: 49MUX-WndNum
            u32Square = 7; // 7: 49MUX-Square
            break;
        case VO_MODE_64MUX:
            u32WndNum = 64; // 64: 64MUX-WndNum
            u32Square = 8; // 8: 64MUX-Square
            break;
        case VO_MODE_2X4:
            u32WndNum = 8; // 8: 2X4-WndNum
            u32Square = 3; // 3: 2X4-Square
            u32Row    = 4; // 4: 2X4-Row
            u32Col    = 2; // 2: 2X4-Col
            break;
        default:
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
    }

    /* Get video layer properties */
    s32Ret = HI_MPI_VO_GetVideoLayerAttr(VoLayer, &stLayerAttr);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }
    u32Width  = stLayerAttr.stImageSize.u32Width;
    u32Height = stLayerAttr.stImageSize.u32Height;
    SAMPLE_PRT("u32Width:%d, u32Height:%d, u32Square:%d\n", u32Width, u32Height, u32Square);
    for (i = 0; i < u32WndNum; i++) {
        if (enMode == VO_MODE_1MUX  ||
            enMode == VO_MODE_2MUX  ||
            enMode == VO_MODE_4MUX  ||
            enMode == VO_MODE_8MUX  ||
            enMode == VO_MODE_9MUX  ||
            enMode == VO_MODE_16MUX ||
            enMode == VO_MODE_25MUX ||
            enMode == VO_MODE_36MUX ||
            enMode == VO_MODE_49MUX ||
            enMode == VO_MODE_64MUX) {
            stChnAttr.stRect.s32X       = ALIGN_DOWN((u32Width / u32Square) * (i % u32Square), ALIGN_DOWN_SIZE);
            stChnAttr.stRect.s32Y       = ALIGN_DOWN((u32Height / u32Square) * (i / u32Square), ALIGN_DOWN_SIZE);
            stChnAttr.stRect.u32Width   = ALIGN_DOWN(u32Width / u32Square, ALIGN_DOWN_SIZE);
            stChnAttr.stRect.u32Height  = ALIGN_DOWN(u32Height / u32Square, ALIGN_DOWN_SIZE);
            stChnAttr.u32Priority       = 0;
            stChnAttr.bDeflicker        = HI_FALSE;
        } else if (enMode == VO_MODE_2X4) {
            stChnAttr.stRect.s32X       = ALIGN_DOWN((u32Width / u32Col) * (i % u32Col), ALIGN_DOWN_SIZE);
            stChnAttr.stRect.s32Y       = ALIGN_DOWN((u32Height / u32Row) * (i / u32Col), ALIGN_DOWN_SIZE);
            stChnAttr.stRect.u32Width   = ALIGN_DOWN(u32Width / u32Col, ALIGN_DOWN_SIZE);
            stChnAttr.stRect.u32Height  = ALIGN_DOWN(u32Height / u32Row, ALIGN_DOWN_SIZE);
            stChnAttr.u32Priority       = 0;
            stChnAttr.bDeflicker        = HI_FALSE;
        }

        /* Set properties for the specified video output channel */
        s32Ret = HI_MPI_VO_SetChnAttr(VoLayer, i, &stChnAttr);
        if (s32Ret != HI_SUCCESS) {
            printf("%s(%d):failed with %#x!\n", \
                   __FUNCTION__, __LINE__,  s32Ret);
            return HI_FAILURE;
        }

        /* Set video output channel rotation angle */
		s32Ret = HI_MPI_VO_SetChnRotation(VoLayer, i, ROTATION_90);
        if (s32Ret != HI_SUCCESS) {
            printf("%s(%d):failed with %#x!\n", \
                   __FUNCTION__, __LINE__,  s32Ret);
            return HI_FAILURE;
        }

        /* Enables the specified video output channel */
        s32Ret = HI_MPI_VO_EnableChn(VoLayer, i);
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }
    }

    return HI_SUCCESS;
}

/* start vo to mipi lcd */
HI_S32 SampleCommVoStartMipi(SAMPLE_VO_CONFIG_S *pstVoConfig)
{
    RECT_S                 stDefDispRect  = {0, 0, 800, 480};
    SIZE_S                 stDefImageSize = {800, 480};

    /*******************************************
    * VO device VoDev# information declaration.
    ********************************************/
    VO_DEV                 VoDev          = 0;
    VO_LAYER               VoLayer;
    SAMPLE_VO_MODE_E       enVoMode       = 0;
    VO_INTF_TYPE_E         enVoIntfType   = VO_INTF_HDMI;
    VO_PART_MODE_E         enVoPartMode   = VO_PART_MODE_SINGLE;
    VO_PUB_ATTR_S          stVoPubAttr    = {0};
    VO_VIDEO_LAYER_ATTR_S  stLayerAttr    = {0};
    VO_CSC_S               stVideoCSC     = {0};
    HI_S32                 s32Ret;

    if (NULL == pstVoConfig) {
        SAMPLE_PRT("Error:argument can not be NULL\n");
        return HI_FAILURE;
    }
    VoDev          = pstVoConfig->VoDev;
    VoLayer        = pstVoConfig->VoDev;
    enVoMode       = pstVoConfig->enVoMode;
    enVoIntfType   = pstVoConfig->enVoIntfType;
    enVoPartMode   = pstVoConfig->enVoPartMode;

    /********************************
    * Set and start VO device VoDev#.
    *********************************/
    stVoPubAttr.enIntfType  = VO_INTF_MIPI;
    stVoPubAttr.enIntfSync  = VO_OUTPUT_USER;
    stVoPubAttr.stSyncInfo.bSynm = 0;
    stVoPubAttr.stSyncInfo.bIop = 1;
    stVoPubAttr.stSyncInfo.u8Intfb = 0;

    stVoPubAttr.stSyncInfo.u16Hmid = 1;
    stVoPubAttr.stSyncInfo.u16Bvact = 1;
    stVoPubAttr.stSyncInfo.u16Bvbb = 1;
    stVoPubAttr.stSyncInfo.u16Bvfb = 1;

    stVoPubAttr.stSyncInfo.bIdv = 0;
    stVoPubAttr.stSyncInfo.bIhs = 0;
    stVoPubAttr.stSyncInfo.bIvs = 0;

    stVoPubAttr.stSyncInfo.u16Hact = 480; // 480: Horizontal effective area. Unit: pixel
    stVoPubAttr.stSyncInfo.u16Hbb = 60; // 60: Horizontal blanking of the rear shoulder. Unit: pixel
    stVoPubAttr.stSyncInfo.u16Hfb = 50; // 50: Horizontal blanking of the front shoulder. Unit: pixel
    stVoPubAttr.stSyncInfo.u16Hpw = 10; // 10: The width of the horizontal sync signal. Unit: pixel
    stVoPubAttr.stSyncInfo.u16Vact = 800; // 800: Vertical effective area. Unit: line
    stVoPubAttr.stSyncInfo.u16Vbb = 24; // 24: Vertical blanking of the rear shoulder.  Unit: line
    stVoPubAttr.stSyncInfo.u16Vfb = 20; // 20: Vertical blanking of the front shoulder.  Unit: line
    stVoPubAttr.stSyncInfo.u16Vpw = 4; // 4: The width of the vertical sync signal. Unit: line
    stVoPubAttr.u32BgColor  = pstVoConfig->u32BgColor;

    s32Ret = SampleCommVoStartDevMipi(VoDev, &stVoPubAttr);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SampleCommVoStartDevMipi failed!\n");
        return s32Ret;
    }

    /******************************
    * Set and start layer VoDev#.
    ********************************/
    s32Ret = SampleCommVoGetWhMipi(stVoPubAttr.enIntfSync,
        &stLayerAttr.stDispRect.u32Width, &stLayerAttr.stDispRect.u32Height, &stLayerAttr.u32DispFrmRt);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SampleCommVoGetWhMipi failed!\n");
        SAMPLE_COMM_VO_StopDev(VoDev);
        return s32Ret;
    }
    stLayerAttr.bClusterMode     = HI_FALSE;
    stLayerAttr.bDoubleFrame    = HI_FALSE;
    stLayerAttr.enPixFormat       = pstVoConfig->enPixFormat;

    stLayerAttr.stDispRect.s32X = 0;
    stLayerAttr.stDispRect.s32Y = 0;

    /******************************
    Set display rectangle if changed.
    ********************************/
    if (memcmp(&pstVoConfig->stDispRect, &stDefDispRect, sizeof(RECT_S)) != 0) {
        memcpy_s(&stLayerAttr.stDispRect, sizeof(stLayerAttr.stDispRect),
            &pstVoConfig->stDispRect, sizeof(RECT_S));
    }

    /******************************
    Set image size if changed.
    ********************************/
    if (memcmp(&pstVoConfig->stImageSize, &stDefImageSize, sizeof(SIZE_S)) != 0) {
        memcpy_s(&stLayerAttr.stImageSize, sizeof(stLayerAttr.stImageSize),
            &pstVoConfig->stImageSize, sizeof(SIZE_S));
    }
    stLayerAttr.stImageSize.u32Width  = stLayerAttr.stDispRect.u32Width = 480; // 480: video layer canvas Width
    stLayerAttr.stImageSize.u32Height = stLayerAttr.stDispRect.u32Height = 800; // 800: video layer canvas Height
    stLayerAttr.enDstDynamicRange     = pstVoConfig->enDstDynamicRange;

    if (pstVoConfig->u32DisBufLen) {
        /* Set buffer length */
        s32Ret = HI_MPI_VO_SetDisplayBufLen(VoLayer, pstVoConfig->u32DisBufLen);
        if (HI_SUCCESS != s32Ret) {
            SAMPLE_PRT("HI_MPI_VO_SetDisplayBufLen failed with %#x!\n", s32Ret);
            SAMPLE_COMM_VO_StopDev(VoDev);
            return s32Ret;
        }
    }
    if (VO_PART_MODE_MULTI == enVoPartMode) {
        /* Set the segmentation mode of the video layer */
        s32Ret = HI_MPI_VO_SetVideoLayerPartitionMode(VoLayer, enVoPartMode);
        if (HI_SUCCESS != s32Ret) {
            SAMPLE_PRT("HI_MPI_VO_SetVideoLayerPartitionMode failed!\n");
            SAMPLE_COMM_VO_StopDev(VoDev);
            return s32Ret;
        }
    }

    /* start layer */
    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_VO_Start video layer failed!\n");
        SAMPLE_COMM_VO_StopDev(VoDev);
        return s32Ret;
    }

    if (VO_INTF_MIPI == enVoIntfType) {
        /* get video layerCSC */
        s32Ret = HI_MPI_VO_GetVideoLayerCSC(VoLayer, &stVideoCSC);
        if (HI_SUCCESS != s32Ret) {
            SAMPLE_PRT("HI_MPI_VO_GetVideoLayerCSC failed!\n");
            SAMPLE_COMM_VO_StopDev(VoDev);
            return s32Ret;
        }
        stVideoCSC.enCscMatrix = VO_CSC_MATRIX_BT709_TO_RGB_PC;
        /* Set video layer CSC */
        s32Ret = HI_MPI_VO_SetVideoLayerCSC(VoLayer, &stVideoCSC);
        if (HI_SUCCESS != s32Ret) {
            SAMPLE_PRT("HI_MPI_VO_SetVideoLayerCSC failed!\n");
            SAMPLE_COMM_VO_StopDev(VoDev);
            return s32Ret;
        }
    }

    /******************************
    * start vo channels.
    ********************************/
    s32Ret = SampleCommVoStartChnMipi(VoLayer, enVoMode);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartChn failed!\n");
        SAMPLE_COMM_VO_StopLayer(VoLayer);
        SAMPLE_COMM_VO_StopDev(VoDev);
        return s32Ret;
    }

    return HI_SUCCESS;
}

/******************************************************************************
* function : to process abnormal case
******************************************************************************/
void SAMPLE_SNAP_HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTERM == signo)
    {
        SAMPLE_COMM_All_ISP_Stop();
        SAMPLE_COMM_VO_HdmiStop();
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    exit(-1);
}

HI_S32 SAMPLE_SNAP_DoublePipeOffline(HI_VOID)
{
    HI_S32                  s32Ret              = HI_SUCCESS;
    HI_S32             fd = 0;
    VI_DEV                  ViDev0              = 0;
    VI_PIPE                 VideoPipe           = 0;
    VI_PIPE                 SnapPipe            = 1;
    VI_CHN                  ViChn               = 0;
    HI_S32                  s32ViCnt            = 1;
    VPSS_GRP                VpssGrp0            = VideoPipe;
    VPSS_GRP                VpssGrp1            = SnapPipe;
    VPSS_CHN                VpssChn[4]          = {VPSS_CHN0, VPSS_CHN1, VPSS_CHN2, VPSS_CHN3};
    VPSS_GRP_ATTR_S         stVpssGrpAttr       = {0};
    VPSS_CHN_ATTR_S         stVpssChnAttr[VPSS_MAX_PHY_CHN_NUM] = {0};
    HI_BOOL                 abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {0};
    VO_CHN                  VoChn               = 0;
    PIC_SIZE_E              enPicSize           = PIC_3840x2160;
    WDR_MODE_E              enWDRMode           = WDR_MODE_NONE;
    DYNAMIC_RANGE_E         enDynamicRange      = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E          enPixFormat         = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VIDEO_FORMAT_E          enVideoFormat       = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E         enCompressMode      = COMPRESS_MODE_NONE;
    VI_VPSS_MODE_E          enVideoPipeMode     = VI_OFFLINE_VPSS_OFFLINE;
    VI_VPSS_MODE_E          enSnapPipeMode      = VI_OFFLINE_VPSS_OFFLINE;
    SIZE_S                  stSize;
    HI_U32                  u32BlkSize;
    VB_CONFIG_S             stVbConf;
    SAMPLE_VI_CONFIG_S      stViConfig;
    SAMPLE_VO_CONFIG_S      stVoConfig;
    HI_U32 u32SupplementConfig = VB_SUPPLEMENT_JPEG_MASK;
    VENC_CHN VencChn = 0;
    VPSS_GRP_NRX_PARAM_S stNrxParam;

    /************************************************
    step 1:  Get all sensors information, need two vi
        ,and need two mipi --
    *************************************************/
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    stViConfig.s32WorkingViNum                           = s32ViCnt;

    stViConfig.as32WorkingViId[0]                        = 0;
    stViConfig.astViInfo[0].stSnsInfo.MipiDev            = ViDev0;
    stViConfig.astViInfo[0].stSnsInfo.s32BusId           = 0;

    stViConfig.astViInfo[0].stDevInfo.ViDev              = ViDev0;
    stViConfig.astViInfo[0].stDevInfo.enWDRMode          = enWDRMode;

    stViConfig.astViInfo[0].stPipeInfo.enMastPipeMode    = enVideoPipeMode;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[0]          = VideoPipe;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[1]          = SnapPipe;
    //stViConfig.astViInfo[0].stPipeInfo.aPipe[1]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[2]          = -1;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[3]          = -1;

    stViConfig.astViInfo[0].stChnInfo.ViChn              = ViChn;
    stViConfig.astViInfo[0].stChnInfo.enPixFormat        = enPixFormat;
    stViConfig.astViInfo[0].stChnInfo.enDynamicRange     = enDynamicRange;
    stViConfig.astViInfo[0].stChnInfo.enVideoFormat      = enVideoFormat;
    stViConfig.astViInfo[0].stChnInfo.enCompressMode     = enCompressMode;

    stViConfig.astViInfo[0].stSnapInfo.bSnap = HI_TRUE;
    stViConfig.astViInfo[0].stSnapInfo.bDoublePipe = HI_TRUE;
    stViConfig.astViInfo[0].stSnapInfo.VideoPipe = VideoPipe;
    stViConfig.astViInfo[0].stSnapInfo.SnapPipe = SnapPipe;
    stViConfig.astViInfo[0].stSnapInfo.enVideoPipeMode = enVideoPipeMode;
    stViConfig.astViInfo[0].stSnapInfo.enSnapPipeMode = enSnapPipeMode;

    /************************************************
    step 2:  Get  input size
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType, &enPicSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed with %d!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed with %d!\n", s32Ret);
        return s32Ret;
    }

    /************************************************
    step3:  Init SYS and common VB
    *************************************************/
    memset(&stVbConf, 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt              = 2;

    u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_8, enCompressMode, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt   = 10;

    u32BlkSize = VI_GetRawBufferSize(stSize.u32Width, stSize.u32Height, PIXEL_FORMAT_RGB_BAYER_16BPP, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize  = u32BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt   = 4;

    s32Ret = SAMPLE_COMM_SYS_InitWithVbSupplement(&stVbConf, u32SupplementConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        goto EXIT;
    }

    /* set VO config to mipi, get mipi device */
    s32Ret = SAMPLE_VO_CONFIG_MIPI(&fd);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("CONFIG MIPI failed.s32Ret:0x%x !\n", s32Ret);
        goto EXIT;
    }

    s32Ret = SAMPLE_COMM_VI_SetParam(&stViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_SetParam failed with %d!\n", s32Ret);
        goto EXIT;
    }


    /************************************************
    step 4: start VI
    *************************************************/
    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);

    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_StartVi failed with %d!\n", s32Ret);
        goto EXIT1;
    }

    /************************************************
    step 5: start VPSS, need two grp
    *************************************************/
    stVpssGrpAttr.u32MaxW                        = stSize.u32Width;
    stVpssGrpAttr.u32MaxH                        = stSize.u32Height;
    stVpssGrpAttr.enPixelFormat                  = enPixFormat;
    stVpssGrpAttr.enDynamicRange                 = enDynamicRange;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate    = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate    = -1;
    stVpssGrpAttr.bNrEn                          = HI_TRUE;
    stVpssGrpAttr.stNrAttr.enNrType              = VPSS_NR_TYPE_VIDEO;
    stVpssGrpAttr.stNrAttr.enCompressMode        = COMPRESS_MODE_FRAME;
    stVpssGrpAttr.stNrAttr.enNrMotionMode        = NR_MOTION_MODE_NORMAL;

    abChnEnable[0]                               = HI_TRUE;
    stVpssChnAttr[0].u32Width                    = stSize.u32Width;
    stVpssChnAttr[0].u32Height                   = stSize.u32Height;
    stVpssChnAttr[0].enChnMode                   = VPSS_CHN_MODE_USER;
    stVpssChnAttr[0].enCompressMode              = enCompressMode;
    stVpssChnAttr[0].enDynamicRange              = enDynamicRange;
    stVpssChnAttr[0].enPixelFormat               = enPixFormat;
    stVpssChnAttr[0].enVideoFormat               = enVideoFormat;
    stVpssChnAttr[0].stFrameRate.s32SrcFrameRate = -1;
    stVpssChnAttr[0].stFrameRate.s32DstFrameRate = -1;
    stVpssChnAttr[0].u32Depth                    = 1;
    stVpssChnAttr[0].bMirror                     = HI_FALSE;
    stVpssChnAttr[0].bFlip                       = HI_FALSE;
    stVpssChnAttr[0].stAspectRatio.enMode        = ASPECT_RATIO_NONE;
    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp0, abChnEnable, &stVpssGrpAttr, stVpssChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VPSS_Start Grp0 failed with %d!\n", s32Ret);
        goto EXIT1;
    }

    stVpssGrpAttr.stNrAttr.enNrType = VPSS_NR_TYPE_SNAP;
    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp1, abChnEnable, &stVpssGrpAttr, stVpssChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VPSS_Start Grp1 failed with %d!\n", s32Ret);
        goto EXIT2;
    }

    memset(&stNrxParam, 0, sizeof(VPSS_GRP_NRX_PARAM_S));
    stNrxParam.enNRVer = VPSS_NR_V2;
    s32Ret = HI_MPI_VPSS_GetGrpNRXParam(VpssGrp0, &stNrxParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VPSS_GetGrpNRXParam failed with %d!\n", s32Ret);
        goto EXIT3;
    }

    stNrxParam.stNRXParam_V2.enOptMode = OPERATION_MODE_MANUAL;
    stNrxParam.stNRXParam_V2.stNRXManual.stNRXParam.MDy[0].MATH0= 200;
    stNrxParam.stNRXParam_V2.stNRXManual.stNRXParam.MDy[1].MATH0 = 200;
    s32Ret = HI_MPI_VPSS_SetGrpNRXParam(VpssGrp0, &stNrxParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VPSS_SetGrpNRXParam failed with %d!\n", s32Ret);
        goto EXIT3;
    }

    memset(&stNrxParam, 0, sizeof(VPSS_GRP_NRX_PARAM_S));
    stNrxParam.enNRVer = VPSS_NR_V2;
    s32Ret = HI_MPI_VPSS_GetGrpNRXParam(VpssGrp1, &stNrxParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VPSS_GetGrpNRXParam failed with %d!\n", s32Ret);
        goto EXIT3;
    }

    stNrxParam.stNRXParam_V2.enOptMode = OPERATION_MODE_MANUAL;
    stNrxParam.stNRXParam_V2.stNRXManual.stNRXParam.MDy[0].MATH0 = 200;
    stNrxParam.stNRXParam_V2.stNRXManual.stNRXParam.MDy[1].MATH0 = 200;
    s32Ret = HI_MPI_VPSS_SetGrpNRXParam(VpssGrp1, &stNrxParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VPSS_SetGrpNRXParam failed with %d!\n", s32Ret);
        goto EXIT3;
    }

    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(VideoPipe, ViChn, VpssGrp0);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_Bind_VPSS failed with %d!\n", s32Ret);
        goto EXIT3;
    }
    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(SnapPipe, ViChn, VpssGrp1);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_Bind_VPSS failed with %d!\n", s32Ret);
        goto EXIT3;
    }

    /************************************************
    step 6:  start VO
    *************************************************/
    s32Ret = SAMPLE_COMM_VO_GetDefConfig(&stVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartVO failed with %d!\n", s32Ret);
        goto EXIT3;
    }
    stVoConfig.enDstDynamicRange = enDynamicRange;

    stVoConfig.enVoIntfType = VO_INTF_MIPI; /* set VO int type */
    stVoConfig.enIntfSync = VO_OUTPUT_USER; /* set VO output information */

    stVoConfig.enPicSize = enPicSize;

        /* start vo */
    s32Ret = SampleCommVoStartMipi(&stVoConfig);
    if (HI_SUCCESS != s32Ret) {
        SAMPLE_PRT("start vo failed. s32Ret: 0x%x !\n", s32Ret);
        goto EXIT4;
    }

    // s32Ret = SAMPLE_COMM_VO_StartVO(&stVoConfig);
    // if (HI_SUCCESS != s32Ret)
    // {
    //     SAMPLE_PRT("SAMPLE_COMM_VO_StartVO failed with %d!\n", s32Ret);
    //     goto EXIT3;
    // }

    s32Ret = SAMPLE_COMM_VPSS_Bind_VO(VpssGrp0, VpssChn[0], stVoConfig.VoDev, VoChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VPSS_Bind_VO Grp0 failed with %d!\n", s32Ret);
        goto EXIT4;
    }

    /************************************************
    step 7:  start VENC
    *************************************************/
    s32Ret = SAMPLE_COMM_VENC_SnapStart(VencChn, &stSize, HI_TRUE);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VENC_SnapStart failed witfh %d\n", s32Ret);
        goto EXIT4;
    }

    s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(VpssGrp1, VpssChn[0], VencChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VPSS_Bind_VENC failed with %d!\n", s32Ret);
        goto EXIT5;
    }

    /************************************************
    step 8:  snap
    *************************************************/
    SNAP_ATTR_S stSnapAttr;
    stSnapAttr.enSnapType = SNAP_TYPE_NORMAL;
    stSnapAttr.bLoadCCM = HI_TRUE;
    stSnapAttr.stNormalAttr.u32FrameCnt = 1;
    stSnapAttr.stNormalAttr.u32RepeatSendTimes = 1;
    stSnapAttr.stNormalAttr.bZSL = 0;
    s32Ret = HI_MPI_SNAP_SetPipeAttr(SnapPipe, &stSnapAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_SNAP_SetPipeAttr failed with %#x!\n", s32Ret);
        goto EXIT5;
    }

    s32Ret = HI_MPI_SNAP_EnablePipe(SnapPipe);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_SNAP_EnablePipe failed with %#x!\n", s32Ret);
        goto EXIT5;
    }
    UartOpenInit();
        char *gestureName = NULL;
        int uartFd = 0;
        unsigned char uartReadBuff[2048] = {0}; 
        unsigned int RecvLen = strlen(gestureName);
        unsigned int ret = 0;
        int readLen = 0;

    printf("=======press any key to trigger=====\n");
    while (1) {
        
        readLen = UartRead(uartFd, uartReadBuff, RecvLen, 1000); /* 1000 :time out */
        if (readLen > 0) {
            printf("Uart read data:%s\r\n", uartReadBuff);
        }
    
        uartFd = 0;
    
        

        s32Ret = HI_MPI_SNAP_TriggerPipe(SnapPipe);
        if (HI_SUCCESS != s32Ret) {
            SAMPLE_PRT("HI_MPI_SNAP_TriggerPipe failed with %#x!\n", s32Ret);
            goto EXIT6;
    }

        s32Ret = SAMPLE_COMM_VENC_SnapProcess(VencChn, stSnapAttr.stNormalAttr.u32FrameCnt, HI_TRUE, HI_TRUE);
        if (HI_SUCCESS != s32Ret) {
            printf("%s: sanp process failed!\n", __FUNCTION__);
            goto EXIT6;
        }
        printf("snap success!\n");
    }
    

    PAUSE();
	SAMPLE_VO_DISABLE_MIPITx(fd);
	SampleCloseMipiTxFd(fd);
	system("echo 0 > /sys/class/gpio/gpio55/value");


    //SAMPLE_COMM_VPSS_UnBind_VO(VpssGrp1, VpssChn[0], stVoConfig.VoDev, VoChn);
EXIT6:
    HI_MPI_SNAP_DisablePipe(SnapPipe);
    SAMPLE_COMM_VPSS_UnBind_VENC(VpssGrp1, VpssChn[0], VencChn);
EXIT5:
    SAMPLE_COMM_VENC_Stop(VencChn);
    SAMPLE_COMM_VPSS_UnBind_VO(VpssGrp0, VpssChn[0], stVoConfig.VoDev, VoChn);
EXIT4:
    SAMPLE_COMM_VO_StopVO(&stVoConfig);
EXIT3:
    SAMPLE_COMM_VPSS_Stop(VpssGrp1, abChnEnable);
EXIT2:
    SAMPLE_COMM_VPSS_Stop(VpssGrp0, abChnEnable);
    SAMPLE_COMM_VI_UnBind_VPSS(VideoPipe, ViChn, VpssGrp0);
    SAMPLE_COMM_VI_UnBind_VPSS(SnapPipe, ViChn, VpssGrp1);
EXIT1:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
EXIT:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}




/******************************************************************************
* function    : main()
* Description : main
******************************************************************************/
#ifdef __HuaweiLite__
int app_main(int argc, char *argv[])
#else
int main(int argc, char* argv[])
#endif
{
    HI_S32 s32Ret = HI_FAILURE;
    HI_S32 s32Index;

    if (argc < 2 || argc > 2)
    {
        SAMPLE_SNAP_Usage(argv[0]);
        return HI_FAILURE;
    }

    if (!strncmp(argv[1], "-h", 2))
    {
        SAMPLE_SNAP_Usage(argv[0]);
        return HI_SUCCESS;
    }

#ifndef __HuaweiLite__
    signal(SIGINT, SAMPLE_SNAP_HandleSig);
    signal(SIGTERM, SAMPLE_SNAP_HandleSig);
#endif

    s32Index = atoi(argv[1]);
    switch (s32Index)
    {
        case 0:
            s32Ret = SAMPLE_SNAP_DoublePipeOffline();
            break;

        default:
            SAMPLE_PRT("the index %d is invaild!\n",s32Index);
            SAMPLE_SNAP_Usage(argv[0]);
            return HI_FAILURE;
    }

    if (HI_SUCCESS == s32Ret)
    {
        SAMPLE_PRT("program exit normally!\n");
    }
    else
    {
        SAMPLE_PRT("program exit abnormally!\n");
    }

    return (s32Ret);
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

