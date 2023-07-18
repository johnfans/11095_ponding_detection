#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>

#include "rtspservice.h"
#include "rtputils.h"
#include "rtsputils.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    /* byte 0 */
    unsigned char u4CSrcLen:4;      /**//* expect 0 */
    unsigned char u1Externsion:1;   /**//* expect 1, see RTP_OP below */
    unsigned char u1Padding:1;      /**//* expect 0 */
    unsigned char u2Version:2;      /**//* expect 2 */
    /**//* byte 1 */
    unsigned char u7Payload:7;      /**//* RTP_PAYLOAD_RTSP */
    unsigned char u1Marker:1;       /**//* expect 1 */
    /**//* bytes 2, 3 */
    unsigned short u16SeqNum;
    /**//* bytes 4-7 */
    unsigned long long u32TimeStamp;
    /**//* bytes 8-11 */
    unsigned long u32SSrc;          /**//* stream number is used here. */
} StRtpFixedHdr;

typedef struct
{
    //byte 0
    unsigned char u5Type:5;
    unsigned char u2Nri:2;
    unsigned char u1F:1;
} StNaluHdr; /**/ /* 1 BYTES */

typedef struct
{
    //byte 0
    unsigned char u5Type:5;
    unsigned char u2Nri:2;
    unsigned char u1F:1;
} StFuIndicator; /**/ /* 1 BYTES */

typedef struct
{
    //byte 0
    unsigned char u5Type:5;
    unsigned char u1R:1;
    unsigned char u1E:1;
    unsigned char u1S:1;
} StFuHdr; /**/ /* 1 BYTES */

typedef struct _tagStRtpHandle
{
    int                 s32Sock;
    struct sockaddr_in  stServAddr;
    unsigned short      u16SeqNum;
    unsigned long long        u32TimeStampInc;
    unsigned long long        u32TimeStampCurr;
    unsigned long long      u32CurrTime;
    unsigned long long      u32PrevTime;
    unsigned int        u32SSrc;
    StRtpFixedHdr       *pRtpFixedHdr;
    StNaluHdr           *pNaluHdr;
    StFuIndicator       *pFuInd;
    StFuHdr             *pFuHdr;
    EmRtpPayload        emPayload;
#ifdef SAVE_NALU
    FILE                *pNaluFile;
#endif
} StRtpObj, *HndRtp;

unsigned int RtpCreate(unsigned int u32IP, int s32Port, EmRtpPayload emPayload)
{
    HndRtp hRtp = NULL;
    struct timeval stTimeval;
    int s32Broadcast = 1;

    hRtp = (HndRtp)calloc(1, sizeof(StRtpObj));
    if(NULL == hRtp)
    {
        printf("Failed to create RTP handle\n");
        goto cleanup;
    }
    hRtp->s32Sock = -1;
    if((hRtp->s32Sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        printf("Failed to create socket\n");
        goto cleanup;
    }

    if(0xFF000000 == (u32IP & 0xFF000000))
    {
        if(-1 == setsockopt(hRtp->s32Sock, SOL_SOCKET, SO_BROADCAST, (char *)&s32Broadcast, sizeof(s32Broadcast)))
        {
            printf("Failed to set socket\n");
            goto cleanup;
        }
    }

    hRtp->stServAddr.sin_family = AF_INET;
    hRtp->stServAddr.sin_port = htons(s32Port);
    hRtp->stServAddr.sin_addr.s_addr = u32IP;
    bzero(&(hRtp->stServAddr.sin_zero), 8);
    hRtp->u16SeqNum = 0;
    hRtp->u32TimeStampInc = 0;
    hRtp->u32TimeStampCurr = 0;

    if(gettimeofday(&stTimeval, NULL) == -1) {
        printf("Failed to get os time\n");
        goto cleanup;
    }
    hRtp->u32PrevTime = stTimeval.tv_sec * 1000 + stTimeval.tv_usec / 1000;
    hRtp->emPayload = emPayload;
    printf("<><><><>success creat RTP<><><><>\n");
    return (unsigned int)hRtp;
cleanup:
    if(hRtp) {
        if(hRtp->s32Sock >= 0) {
            close(hRtp->s32Sock);
        }
        free(hRtp);
    }

    return 0;
}

void RtpDelete(unsigned int u32Rtp)
{
    HndRtp hRtp = (HndRtp)u32Rtp;

    if(hRtp)
    {
#ifdef SAVE_NALU
        if(hRtp->pNaluFile)
        {
            fclose(hRtp->pNaluFile);
        }
#endif

        if(hRtp->s32Sock >= 0)
        {
            close(hRtp->s32Sock);
        }

        free(hRtp);
    }
}

static int SendNalu264(HndRtp hRtp, char *pNalBuf, int s32NalBufSize)
{
    char *pNaluPayload;
    char *pSendBuf;
    int s32Bytes = 0;
    int s32Ret = 0;
    struct timeval stTimeval;
    char *pNaluCurr;
    int s32NaluRemain;
    unsigned char u8NaluBytes;

    pSendBuf = (char *)calloc(MAX_RTP_PKT_LENGTH + 100, sizeof(char));
    if(NULL == pSendBuf)
    {
        s32Ret = -1;
        goto cleanup;
    }

    hRtp->pRtpFixedHdr = (StRtpFixedHdr *)pSendBuf;
    hRtp->pRtpFixedHdr->u7Payload   = H264;
    hRtp->pRtpFixedHdr->u2Version   = 2;
    hRtp->pRtpFixedHdr->u1Marker    = 0;
    hRtp->pRtpFixedHdr->u32SSrc     = hRtp->u32SSrc;
    hRtp->pRtpFixedHdr->u32TimeStamp = htonl(hRtp->u32TimeStampCurr * (90000 / 1000));
    //printf("sendnalu264 timestamp:%lld\n",hRtp->u32TimeStampCurr);
    if(gettimeofday(&stTimeval, NULL) == -1)
    {
        printf("Failed to get os time\n");
        s32Ret = -1;
        goto cleanup;
    }

    u8NaluBytes = *pNalBuf;
    pNaluCurr = pNalBuf + 1;
    s32NaluRemain = s32NalBufSize - 1;
    if(s32NaluRemain <= MAX_RTP_PKT_LENGTH) {
        hRtp->pRtpFixedHdr->u1Marker    = 1;
        hRtp->pRtpFixedHdr->u16SeqNum   = htons(hRtp->u16SeqNum ++);
        hRtp->pNaluHdr                  = (StNaluHdr *)(pSendBuf + 12);
        hRtp->pNaluHdr->u1F             = (u8NaluBytes & 0x80) >> 7;
        hRtp->pNaluHdr->u2Nri           = (u8NaluBytes & 0x60) >> 5;
        hRtp->pNaluHdr->u5Type          = u8NaluBytes & 0x1f;
        pNaluPayload = (pSendBuf + 13);
        memcpy(pNaluPayload, pNaluCurr, s32NaluRemain);
        s32Bytes = s32NaluRemain + 13;
        if(sendto(hRtp->s32Sock, pSendBuf, s32Bytes, 0, (struct sockaddr *)&hRtp->stServAddr, sizeof(hRtp->stServAddr)) < 0) {
            s32Ret = -1;
            goto cleanup;
        }
    } else {
        hRtp->pFuInd            = (StFuIndicator *)(pSendBuf + 12);
        hRtp->pFuInd->u1F       = (u8NaluBytes & 0x80) >> 7;
        hRtp->pFuInd->u2Nri     = (u8NaluBytes & 0x60) >> 5;
        hRtp->pFuInd->u5Type    = 28;
        hRtp->pFuHdr            = (StFuHdr *)(pSendBuf + 13);
        hRtp->pFuHdr->u1R       = 0;
        hRtp->pFuHdr->u5Type    = u8NaluBytes & 0x1f;
        pNaluPayload = (pSendBuf + 14);
        while(s32NaluRemain > 0) {
            hRtp->pRtpFixedHdr->u16SeqNum = htons(hRtp->u16SeqNum ++);
            hRtp->pRtpFixedHdr->u1Marker = (s32NaluRemain <= MAX_RTP_PKT_LENGTH) ? 1 : 0;
            hRtp->pFuHdr->u1E = (s32NaluRemain <= MAX_RTP_PKT_LENGTH) ? 1 : 0;
            hRtp->pFuHdr->u1S = (s32NaluRemain == (s32NalBufSize - 1)) ? 1 : 0;
            s32Bytes = (s32NaluRemain < MAX_RTP_PKT_LENGTH) ? s32NaluRemain : MAX_RTP_PKT_LENGTH;
            memcpy(pNaluPayload, pNaluCurr, s32Bytes);
            s32Bytes = s32Bytes + 14;
            if(sendto(hRtp->s32Sock, pSendBuf, s32Bytes, 0, (struct sockaddr *)&hRtp->stServAddr, sizeof(hRtp->stServAddr)) < 0) {
                s32Ret = -1;
                goto cleanup;
            }
            pNaluCurr += MAX_RTP_PKT_LENGTH;
            s32NaluRemain -= MAX_RTP_PKT_LENGTH;
        }
    }
cleanup:
    if (pSendBuf) {
        free((void *)pSendBuf);
    }
    return s32Ret;
}

static int SendNalu711(HndRtp hRtp, char *buf, int bufsize)
{
    char *pSendBuf;
    int s32Bytes = 0;
    int s32Ret = 0;

    pSendBuf = (char *)calloc(MAX_RTP_PKT_LENGTH + 100, sizeof(char));
    if(NULL == pSendBuf)
    {
        s32Ret = -1;
        goto cleanup;
    }
    hRtp->pRtpFixedHdr = (StRtpFixedHdr *)pSendBuf;
    hRtp->pRtpFixedHdr->u7Payload     = G711;
    hRtp->pRtpFixedHdr->u2Version     = 2;
    hRtp->pRtpFixedHdr->u1Marker = 1;
    hRtp->pRtpFixedHdr->u32SSrc = hRtp->u32SSrc;
    hRtp->pRtpFixedHdr->u16SeqNum  = htons(hRtp->u16SeqNum ++);
    memcpy(pSendBuf + 12, buf, bufsize);
    hRtp->pRtpFixedHdr->u32TimeStamp = htonl(hRtp->u32TimeStampCurr);
    s32Bytes = bufsize + 12;
    if(sendto(hRtp->s32Sock, pSendBuf, s32Bytes, 0, (struct sockaddr *)&hRtp->stServAddr, sizeof(hRtp->stServAddr)) < 0) {
        printf("Failed to send!");
        s32Ret = -1;
        goto cleanup;
    }
cleanup:
    if(pSendBuf) {
        free((void *)pSendBuf);
    }
    return s32Ret;
}

unsigned int RtpSend(unsigned int u32Rtp, char *pData, int s32DataSize, unsigned int u32TimeStamp)
{
    int s32NalSize = 0;
    char *pNalBuf, *pDataEnd;
    HndRtp hRtp = (HndRtp)u32Rtp;
    unsigned int u32NaluToken;
    hRtp->u32TimeStampCurr = u32TimeStamp;
    if (_h264 == hRtp->emPayload) {
        pDataEnd = pData + s32DataSize;
        for(; pData < pDataEnd-5; pData ++) {
            memcpy(&u32NaluToken, pData, 4 * sizeof(char));
            if(0x01000000 == u32NaluToken) {
                pData += 4;
                pNalBuf = pData;
                break;
            }
        }
        for (; pData < pDataEnd-5; pData ++) {
            memcpy(&u32NaluToken, pData, 4 * sizeof(char));
            if(0x01000000 == u32NaluToken) {
                s32NalSize = (int)(pData - pNalBuf);
                if (SendNalu264(hRtp, pNalBuf, s32NalSize) == -1) {
                    return -1;
                }
                pData += 4;
                pNalBuf = pData;
            }
        }
        if (pData > pNalBuf) {
            s32NalSize = (int)(pData - pNalBuf);
            if(SendNalu264(hRtp, pNalBuf, s32NalSize) == -1) {
                return -1;
            }
        }
    } else if(_h264nalu == hRtp->emPayload) {
        if(SendNalu264(hRtp, pData, s32DataSize) == -1) {
            return -1;
        }
    } else if(_g711 == hRtp->emPayload) {
        if (SendNalu711(hRtp, pData, s32DataSize) == -1) {
            return -1;
        }
    } else {
        return -1;
    }
    return 0;
}
#ifdef __cplusplus
}
#endif
