#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>

#include "rtsputils.h"
#include "rtspservice.h"
#include "rtputils.h"
#include "ringfifo.h"

struct profileid_sps_pps{
     char base64profileid[10];
     char base64sps[524];
     char base64pps[524];
};

pthread_mutex_t mut;

#define MAX_SIZE 1024*1024*200
#define SDP_EL "\r\n"
#define RTSP_RTP_AVP "RTP/AVP"
struct profileid_sps_pps psp;

StServPrefs stPrefs;
extern int num_conn;
int g_s32Maxfd = 0;
int g_s32DoPlay = 0;

uint32_t s_u32StartPort=RTP_DEFAULT_PORT;
uint32_t s_uPortPool[MAX_CONNECTION];
extern int stop_schedule;
int g_s32Quit = 0;

int UpdateSpsOrPps(unsigned char *data,int frame_type,int len);

void PrefsInit()
{
    stPrefs.port = SERVER_RTSP_PORT_DEFAULT;
    strcpy(stPrefs.hostname, SERVER_RTSP_IP);
    printf("\n");
    printf("hostname is: %s\n", stPrefs.hostname);
    printf("rtsp listening port is: %d\n", stPrefs.port);
    printf("Input rtsp://%s/live to play this\n", stPrefs.hostname, stPrefs.port);
    printf("\n");
}

void RTSP_initserver(RTSP_buffer *rtsp, int fd)
{
    rtsp->fd = fd;
    rtsp->session_list = (RTSP_session *) calloc(1, sizeof(RTSP_session));
    rtsp->session_list->session_id = -1;
}

int RTP_get_port_pair(port_pair *pair)
{
    int i;
    for (i=0; i<MAX_CONNECTION; ++i)
    {
        if (s_uPortPool[i]!=0)
        {
            pair->RTP=(s_uPortPool[i]-s_u32StartPort)*2+s_u32StartPort;
            pair->RTCP=pair->RTP+1;
            s_uPortPool[i]=0;
            return ERR_NOERROR;
        }
    }
    return ERR_GENERIC;
}

void AddClient(RTSP_buffer **ppRtspList, int fd)
{
    RTSP_buffer *pRtsp=NULL,*pRtspNew=NULL;
    if (*ppRtspList==NULL) {
        if (!(*ppRtspList=(RTSP_buffer*)calloc(1,sizeof(RTSP_buffer)))) {
            fprintf(stderr,"alloc memory error %s,%i\n", __FILE__, __LINE__);
            return;
        }
        pRtsp = *ppRtspList;
    } else {
        for (pRtsp=*ppRtspList; pRtsp!=NULL; pRtsp=pRtsp->next) {
            pRtspNew=pRtsp;
        }
        if (pRtspNew!=NULL) {
            if (!(pRtspNew->next=(RTSP_buffer *)calloc(1,sizeof(RTSP_buffer)))) {
                fprintf(stderr, "error calloc %s,%i\n", __FILE__, __LINE__);
                return;
            }
            pRtsp=pRtspNew->next;
            pRtsp->next=NULL;
        }
    }
    if (g_s32Maxfd < fd) {
         g_s32Maxfd = fd;
    }
    RTSP_initserver(pRtsp,fd);
    fprintf(stderr,"Incoming RTSP connection accepted on socket: %d\n",pRtsp->fd);

}

/*
 * return -1 on ERROR
 * return RTSP_not_full (0) if a full RTSP message is NOT present in the in_buffer yet.
 * return RTSP_method_rcvd (1) if a full RTSP message is present in the in_buffer and is
 *                     ready to be handled.
 * return RTSP_interlvd_rcvd (2) if a complete RTP/RTCP interleaved packet is present.
 * terminate on really ugly cases.
 */
int RTSP_full_msg_rcvd(RTSP_buffer *rtsp, int *hdr_len, int *body_len)
{
    int eomh;    /* end of message header found */
    int mb;       /* message body exists */
    int tc;         /* terminator count */
    int ws;        /* white space */
    unsigned int ml;              /* total message length including any message body */
    int bl;                           /* message body length */
    char c;                         /* character */
    int control;
    char *p;
    if (rtsp->in_buffer[0] == '$')  {
        uint16_t *intlvd_len = (uint16_t *)&rtsp->in_buffer[2];
        if ((bl = ntohs(*intlvd_len)) <= rtsp->in_size) {
            fprintf(stderr,"Interleaved RTP or RTCP packet arrived (len: %hu).\n", bl);
            if (hdr_len)
                *hdr_len = 4;
            if (body_len)
                *body_len = bl;
            return RTSP_interlvd_rcvd;
        } else {
            fprintf(stderr,"Non-complete Interleaved RTP or RTCP packet arrived.\n");
            return RTSP_not_full;
        }
    }
    eomh = mb = ml = bl = 0;
    while (ml <= rtsp->in_size) {
        control = strcspn(&(rtsp->in_buffer[ml]), "\r\n");
        if(control > 0)
            ml += control;
        else
            return ERR_GENERIC;
        /* haven't received the entire message yet. */
        if (ml > rtsp->in_size)
            return RTSP_not_full;
        tc = ws = 0;
        while (!eomh && ((ml + tc + ws) < rtsp->in_size)) {
            c = rtsp->in_buffer[ml + tc + ws];
            if (c == '\r' || c == '\n')
                tc++;
            else if ((tc < 3) && ((c == ' ') || (c == '\t'))) {
                ws++;
            } else {
                 break;
            }
        }
        /* must be the end of the message header */
        if ((tc > 2) || ((tc == 2) && (rtsp->in_buffer[ml] == rtsp->in_buffer[ml + 1])))
            eomh = 1;
        ml += tc + ws;
        if (eomh) {
            ml += bl;
            if (ml <= rtsp->in_size)
            break;  /* all done finding the end of the message. */
        }
        if (ml >= rtsp->in_size)
            return RTSP_not_full;
        if (!mb) {
            /* content length token not yet encountered. */
            if (!strncmp(&(rtsp->in_buffer[ml]), HDR_CONTENTLENGTH, strlen(HDR_CONTENTLENGTH))) {
                mb = 1;
                ml += strlen(HDR_CONTENTLENGTH);
                while (ml < rtsp->in_size) {
                    c = rtsp->in_buffer[ml];
                    if ((c == ':') || (c == ' '))
                        ml++;
                    else
                        break;
                }
                if (sscanf(&(rtsp->in_buffer[ml]), "%d", &bl) != 1) {
                    fprintf(stderr,"RTSP_full_msg_rcvd(): Invalid ContentLength encountered in message.\n");
                    return ERR_GENERIC;
                }
            }
        }
    }
    if (hdr_len)
        *hdr_len = ml - bl;
    if (body_len) {
    /*
     * go through any trailing nulls.  Some servers send null terminated strings
     * following the body part of the message.  It is probably not strictly
     * legal when the null byte is not included in the Content-Length count.
     * However, it is tolerated here.
     */
        for (tc = rtsp->in_size - ml, p = &(rtsp->in_buffer[ml]); tc && (*p == '\0'); p++, bl++, tc--);
            *body_len = bl;
    }
    return RTSP_method_rcvd;
}

int RTSP_valid_response_msg(unsigned short *status, RTSP_buffer * rtsp)
{
    char ver[32], trash[15];
    unsigned int stat;
    unsigned int seq;
    int pcnt; /* parameter count */
    /* assuming "stat" may not be zero (probably faulty) */
    stat = 0;
    pcnt = sscanf(rtsp->in_buffer, " %31s %u %s %s %u\n%*255s ", ver, &stat, trash, trash, &seq);
    /*
    /* C->S CMD rtsp://IP:port/suffix RTSP/1.0\r\n |head
     * CSeq: 1 \r\n |
     * Content_Length:**  |body
     * S->C RTSP/1.0 200 OK\r\n
     * CSeq: 1\r\n
     * Date:....
     */
    if (strncmp(ver, "RTSP/", 5))
        return 0;
    if (pcnt < 3 || stat == 0)
        return 0;
    if (rtsp->rtsp_cseq != seq + 1) {
        fprintf(stderr,"Invalid sequence number returned in response.\n");
        return ERR_GENERIC;
    }
    *status = stat;
    return 1;
}
int RTSP_validate_method(RTSP_buffer * pRtsp)
{
    char method[32], hdr[16];
    char object[256];
    char ver[32];
    unsigned int seq;
    int pcnt;   /* parameter count */
    int mid = ERR_GENERIC;
    char *p;
    char trash[255];
    *method = *object = '\0';
    seq = 0;
     printf("");
     if ( (pcnt = sscanf(pRtsp->in_buffer, " %31s %255s %31s\n%15s", method, object, ver, hdr)) != 4){
          printf("========\n%s\n==========\n",pRtsp->in_buffer);
          printf("%s ",method); 
          printf("%s ",object);
          printf("%s ",ver);
          printf("hdr:%s\n",hdr);
             return ERR_GENERIC;
     }
     if ((p = strstr(pRtsp->in_buffer, "CSeq")) == NULL) {
          return ERR_GENERIC;
     } else {
          if(sscanf(p,"%254s %d",trash,&seq)!=2){
               return ERR_GENERIC;
          }
     }
    if (strcmp(method, RTSP_METHOD_DESCRIBE) == 0) {
        mid = RTSP_ID_DESCRIBE;
    }
    if (strcmp(method, RTSP_METHOD_ANNOUNCE) == 0) {
        mid = RTSP_ID_ANNOUNCE;
    }
    if (strcmp(method, RTSP_METHOD_GET_PARAMETERS) == 0) {
        mid = RTSP_ID_GET_PARAMETERS;
    }
    if (strcmp(method, RTSP_METHOD_OPTIONS) == 0) {
        mid = RTSP_ID_OPTIONS;
    }
    if (strcmp(method, RTSP_METHOD_PAUSE) == 0) {
        mid = RTSP_ID_PAUSE;
    }
    if (strcmp(method, RTSP_METHOD_PLAY) == 0) {
        mid = RTSP_ID_PLAY;
    }
    if (strcmp(method, RTSP_METHOD_RECORD) == 0) {
        mid = RTSP_ID_RECORD;
    }
    if (strcmp(method, RTSP_METHOD_REDIRECT) == 0) {
        mid = RTSP_ID_REDIRECT;
    }
    if (strcmp(method, RTSP_METHOD_SETUP) == 0) {
        mid = RTSP_ID_SETUP;
    }
    if (strcmp(method, RTSP_METHOD_SET_PARAMETER) == 0) {
        mid = RTSP_ID_SET_PARAMETER;
    }
    if (strcmp(method, RTSP_METHOD_TEARDOWN) == 0) {
        mid = RTSP_ID_TEARDOWN;
    }
    pRtsp->rtsp_cseq = seq;
    return mid;
}

int ParseUrl(const char *pUrl, char *pServer, unsigned short *port, char *pFileName, size_t FileNameLen)
{
    int s32NoValUrl;
    char *pFull = (char *)malloc(strlen(pUrl) + 1);
    strcpy(pFull, pUrl);
    if (strncmp(pFull, "rtsp://", 7) == 0) {
        char *pSuffix;
        if((pSuffix = strchr(&pFull[7], '/')) != NULL) {
            char *pPort;
            char pSubPort[128];
            pPort=strchr(&pFull[7], ':');
            if (pPort != NULL) {
                strncpy(pServer,&pFull[7],pPort-pFull-7);
                printf("server:%s\n",pServer);
                strncpy(pSubPort, pPort+1, pSuffix-pPort-1);
                pSubPort[pSuffix-pPort-1] = '\0';
                *port = (unsigned short) atol(pSubPort);
                printf("port:%d\n",port);
            } else {
                *port = SERVER_RTSP_PORT_DEFAULT;
            }
            pSuffix++;
            while(*pSuffix == ' '||*pSuffix == '\t') {
                pSuffix++;
            }
            strcpy(pFileName, pSuffix);
            s32NoValUrl = 0;
        } else {
            *port = SERVER_RTSP_PORT_DEFAULT;
            *pFileName = '\0';
            s32NoValUrl = 1;
        }
    } else {
        *pFileName = '\0';
        s32NoValUrl = 1;
    }
    free(pFull);
    return s32NoValUrl;
}

char *GetSdpId(char *buffer)
{
    time_t t;
    buffer[0]='\0';
    t = time(NULL);
    sprintf(buffer,"%.0f",(float)t+2208988800U);
    return buffer;
}

void GetSdpDescr(RTSP_buffer * pRtsp, char *pDescr, char *s8Str)
{
    char pSdpId[128];
    char rtp_port[5];
    struct sockaddr Addr;
    sock_ntop_host(&Addr, sizeof(struct sockaddr), s8Str, 128);
    GetSdpId(pSdpId);
    strcpy(pDescr, "v=0\r\n");
    strcat(pDescr, "o=-");
    strcat(pDescr, pSdpId);
    strcat(pDescr," ");
    strcat(pDescr, pSdpId);
    strcat(pDescr," IN IP4 ");
    strcat(pDescr, s8Str);
    strcat(pDescr, "\r\n");
    strcat(pDescr, "s=Unnamed\r\n");
    strcat(pDescr, "i=N/A\r\n");
    strcat(pDescr, "c=");
    strcat(pDescr, "IN ");          /* Network type: Internet. */
    strcat(pDescr, "IP4 ");          /* Address type: IP4. */
    strcat(pDescr, inet_ntoa(((struct sockaddr_in *)(&pRtsp->stClientAddr))->sin_addr));
    strcat(pDescr, "\r\n");
    strcat(pDescr, "t=0 0\r\n");     
    strcat(pDescr, "a=recvonly\r\n");
    /**** media specific ****/
    strcat(pDescr,"m=");
    strcat(pDescr,"video ");
    sprintf(rtp_port,"%d",s_u32StartPort);
    strcat(pDescr, rtp_port);
    strcat(pDescr," RTP/AVP "); /* Use UDP */
    strcat(pDescr,"96\r\n");
    // strcat(pDescr, "\r\n");
    strcat(pDescr,"b=RR:0\r\n");
    /**** Dynamically defined payload ****/
    strcat(pDescr,"a=rtpmap:96");
    strcat(pDescr," ");     
    strcat(pDescr,"H264/90000");
    strcat(pDescr, "\r\n");
    strcat(pDescr,"a=fmtp:96 packetization-mode=1;");
    strcat(pDescr,"profile-level-id=");
    strcat(pDescr,psp.base64profileid);
    strcat(pDescr,";sprop-parameter-sets=");
    strcat(pDescr,psp.base64sps);
    strcat(pDescr,",");
    strcat(pDescr,psp.base64pps);
    strcat(pDescr,";");
    strcat(pDescr, "\r\n");
    strcat(pDescr,"a=control:trackID=0");
    strcat(pDescr, "\r\n");
}

void add_time_stamp(char *b, int crlf)
{
    struct tm *t;
    time_t now;
    now = time(NULL);
    t = gmtime(&now);
    strftime(b + strlen(b), 38, "Date: %a, %d %b %Y %H:%M:%S GMT"RTSP_EL, t);
    if (crlf) {
        strcat(b, "\r\n");     /* add a message header terminator (CRLF) */
    }
}

int SendDescribeReply(RTSP_buffer * rtsp, char *object, char *descr, char *s8Str)
{
    char *pMsgBuf;
    int s32MbLen;
    s32MbLen = 2048;
    pMsgBuf = (char *)malloc(s32MbLen);
    if (!pMsgBuf) {
        fprintf(stderr,"send_describe_reply(): unable to allocate memory\n");
        send_reply(500, 0, rtsp); /* internal server error */
        if (pMsgBuf)  {
            free(pMsgBuf);
        }
        return ERR_ALLOC;
    }
    sprintf(pMsgBuf, "%s %d %s"RTSP_EL"CSeq: %d"RTSP_EL"Server: %s/%s"RTSP_EL, RTSP_VER, 200, get_stat(200), rtsp->rtsp_cseq, PACKAGE, VERSION);
    add_time_stamp(pMsgBuf, 0);
    strcat(pMsgBuf, "Content-Type: application/sdp"RTSP_EL);
    sprintf(pMsgBuf + strlen(pMsgBuf), "Content-Base: rtsp://%s/%s/"RTSP_EL, s8Str, object);
    sprintf(pMsgBuf + strlen(pMsgBuf), "Content-Length: %d"RTSP_EL, strlen(descr));
    strcat(pMsgBuf, RTSP_EL);
    strcat(pMsgBuf, descr);
    bwrite(pMsgBuf, (unsigned short) strlen(pMsgBuf), rtsp);
    free(pMsgBuf);
    return ERR_NOERROR;
}

int RTSP_describe(RTSP_buffer * pRtsp)
{
    char object[255], trash[255];
    char *p;
    unsigned short port;
    char s8Url[255];
    char s8Descr[MAX_DESCR_LENGTH];
    char server[128];
    char s8Str[128];
    if (!sscanf(pRtsp->in_buffer, " %*s %254s ", s8Url)) {
        fprintf(stderr, "Error %s,%i\n", __FILE__, __LINE__);
        send_reply(400, 0, pRtsp); /* bad request */
        printf("get URL error");
        return ERR_NOERROR;
    }
    switch (ParseUrl(s8Url, server, &port, object, sizeof(object))) {
        case 1:
            fprintf(stderr, "Error %s,%i\n", __FILE__, __LINE__);
            send_reply(400, 0, pRtsp);
            printf("url request error");
            return ERR_NOERROR;
            break;
        case -1:
            fprintf(stderr,"url error while parsing !\n");
            send_reply(500, 0, pRtsp);
            printf("inner error");
            return ERR_NOERROR;
            break;
        default:
            break;
    }
    if ((p = strstr(pRtsp->in_buffer, HDR_CSEQ)) == NULL) {
        fprintf(stderr, "Error %s,%i\n", __FILE__, __LINE__);
        send_reply(400, 0, pRtsp);  /* Bad Request */
        printf("get serial num error");
        return ERR_NOERROR;
    } else {
        if (sscanf(p, "%254s %d", trash, &(pRtsp->rtsp_cseq)) != 2) {
            fprintf(stderr, "Error %s,%i\n", __FILE__, __LINE__);
            send_reply(400, 0, pRtsp);
            printf("get serial num 2 error");
            return ERR_NOERROR;
        }
    }
    GetSdpDescr(pRtsp, s8Descr, s8Str);
    SendDescribeReply(pRtsp, object, s8Descr, s8Str);
    return ERR_NOERROR;
}

int send_options_reply(RTSP_buffer * pRtsp, long cseq)
{
    char r[1024];
    sprintf(r, "%s %d %s"RTSP_EL"CSeq: %ld"RTSP_EL, RTSP_VER, 200, get_stat(200), cseq);
    strcat(r, "Public: OPTIONS,DESCRIBE,SETUP,PLAY,PAUSE,TEARDOWN"RTSP_EL);
    strcat(r, RTSP_EL);
    bwrite(r, (unsigned short) strlen(r), pRtsp);
    return ERR_NOERROR;
}

int RTSP_options(RTSP_buffer * pRtsp)
{
    char *p;
    char trash[255];
    unsigned int cseq;
    char method[255], url[255], ver[255];

    if ((p = strstr(pRtsp->in_buffer, HDR_CSEQ)) == NULL) {
        fprintf(stderr, "Error %s,%i\n", __FILE__, __LINE__);
        send_reply(400, 0, pRtsp); /* Bad Request */
        printf("serial num error");
        return ERR_NOERROR;
    } else {
        if (sscanf(p, "%254s %d", trash, &(pRtsp->rtsp_cseq)) != 2) {
            fprintf(stderr, "Error %s,%i\n", __FILE__, __LINE__);
            send_reply(400, 0, pRtsp);/* Bad Request */
            printf("serial num 2 error");
            return ERR_NOERROR;
        }
    }
    cseq = pRtsp->rtsp_cseq;
    sscanf(pRtsp->in_buffer, " %31s %255s %31s ", method, url, ver);
    fprintf(stderr,"%s %s %s \n",method,url,ver);
    send_options_reply(pRtsp, cseq);
    return ERR_NOERROR;
}

int send_setup_reply(RTSP_buffer *pRtsp, RTSP_session *pSession, RTP_session *pRtpSes)
{
    char s8Str[1024];
    sprintf(s8Str, "%s %d %s"RTSP_EL"CSeq: %ld"RTSP_EL"Server: %s/%s"RTSP_EL, RTSP_VER,\
              200, get_stat(200), (long int)pRtsp->rtsp_cseq, PACKAGE, VERSION);
    add_time_stamp(s8Str, 0);
    sprintf(s8Str + strlen(s8Str), "Session: %d"RTSP_EL"Transport: ", (pSession->session_id));
    switch (pRtpSes->transport.type) {
        case RTP_rtp_avp:
            if (pRtpSes->transport.u.udp.is_multicast) {
                // sprintf(s8Str + strlen(s8Str), "RTP/AVP;multicast;ttl=%d;destination=%s;port=", (int)DEFAULT_TTL, descr->multicast);
            } else {
                sprintf(s8Str + strlen(s8Str), "RTP/AVP;unicast;client_port=%d-%d;destination=192.168.245.65;source=%s;server_port=", \
                      pRtpSes->transport.u.udp.cli_ports.RTP, pRtpSes->transport.u.udp.cli_ports.RTCP,"192.168.245.96");
            }
            sprintf(s8Str + strlen(s8Str), "%d-%d"RTSP_EL, pRtpSes->transport.u.udp.ser_ports.RTP, pRtpSes->transport.u.udp.ser_ports.RTCP);
            break;
        case RTP_rtp_avp_tcp:
            sprintf(s8Str + strlen(s8Str), "RTP/AVP/TCP;interleaved=%d-%d"RTSP_EL, \
                    pRtpSes->transport.u.tcp.interleaved.RTP, pRtpSes->transport.u.tcp.interleaved.RTCP);
            break;
        default:
            break;
    }
    strcat(s8Str, RTSP_EL);
    bwrite(s8Str, (unsigned short) strlen(s8Str), pRtsp);
    return ERR_NOERROR;
}

int RTSP_setup(RTSP_buffer * pRtsp)
{
    char s8TranStr[128], *s8Str;
    char *pStr;
    RTP_transport Transport;
    int s32SessionID=0;
    RTP_session *rtp_s, *rtp_s_prec;
    RTSP_session *rtsp_s;
    if ((s8Str = strstr(pRtsp->in_buffer, HDR_TRANSPORT)) == NULL) {
        fprintf(stderr, "Error %s,%i\n", __FILE__, __LINE__);
        send_reply(406, 0, pRtsp);     // Not Acceptable
        printf("not acceptable");
        return ERR_NOERROR;
    }
    if (sscanf(s8Str, "%*10s %255s", s8TranStr) != 1) {
        fprintf(stderr,"SETUP request malformed: Transport string is empty\n");
        send_reply(400, 0, pRtsp);       // Bad Request
        printf("check transport 400 bad request");
        return ERR_NOERROR;
    }
    fprintf(stderr,"*** transport: %s ***\n", s8TranStr);
    if (!pRtsp->session_list ) {
        pRtsp->session_list = (RTSP_session *) calloc(1, sizeof(RTSP_session));
    }
    rtsp_s = pRtsp->session_list;
    if (pRtsp->session_list->rtp_session == NULL) {
        pRtsp->session_list->rtp_session = (RTP_session *) calloc(1, sizeof(RTP_session));
        rtp_s = pRtsp->session_list->rtp_session;
    } else {
        for (rtp_s = rtsp_s->rtp_session; rtp_s != NULL; rtp_s = rtp_s->next) {
            rtp_s_prec = rtp_s;
        }
        rtp_s_prec->next = (RTP_session *) calloc(1, sizeof(RTP_session));
        rtp_s = rtp_s_prec->next;
    }
    rtp_s->pause = 1;
    rtp_s->hndRtp = NULL;
    Transport.type = RTP_no_transport;
    if((pStr = strstr(s8TranStr, RTSP_RTP_AVP))) {
        //Transport: RTP/AVP
        pStr += strlen(RTSP_RTP_AVP);
        if ( !*pStr || (*pStr == ';') || (*pStr == ' ')) {
            if (strstr(s8TranStr, "unicast")) {
                if((pStr = strstr(s8TranStr, "client_port"))) {
                    pStr = strstr(s8TranStr, "=");
                    sscanf(pStr + 1, "%d", &(Transport.u.udp.cli_ports.RTP));
                    pStr = strstr(s8TranStr, "-");
                    sscanf(pStr + 1, "%d", &(Transport.u.udp.cli_ports.RTCP));
                }
                if (RTP_get_port_pair(&Transport.u.udp.ser_ports) != ERR_NOERROR) {
                    fprintf(stderr, "Error %s,%d\n", __FILE__, __LINE__);
                    send_reply(500, 0, pRtsp);/* Internal server error */
                    return ERR_GENERIC;
                }
                rtp_s->hndRtp = (struct _tagStRtpHandle*)RtpCreate((unsigned int)(((struct sockaddr_in *)(&pRtsp->stClientAddr))->sin_addr.s_addr), Transport.u.udp.cli_ports.RTP, _h264nalu);
                printf("<><><><>Creat RTP<><><><>\n");
                Transport.u.udp.is_multicast = 0;
            } else {
                printf("multicast not codeing\n");
            }
            Transport.type = RTP_rtp_avp;
        } else if (!strncmp(s8TranStr, "/TCP", 4)) {
            if ((pStr = strstr(s8TranStr, "interleaved"))) {
                pStr = strstr(s8TranStr, "=");
                sscanf(pStr + 1, "%d", &(Transport.u.tcp.interleaved.RTP));
                if ((pStr = strstr(pStr, "-"))) {
                    sscanf(pStr + 1, "%d", &(Transport.u.tcp.interleaved.RTCP));
                } else {
                    Transport.u.tcp.interleaved.RTCP = Transport.u.tcp.interleaved.RTP + 1; } 
            } else {

            }
            Transport.rtp_fd = pRtsp->fd;
        }
    }
    printf("pstr=%s\n",pStr);
    if (Transport.type == RTP_no_transport) {
        fprintf(stderr,"AAAAAAAAAAA Unsupported Transport,%s,%d\n", __FILE__, __LINE__);
        send_reply(461, 0, pRtsp);// Bad Request
        return ERR_NOERROR;
    }
    memcpy(&rtp_s->transport, &Transport, sizeof(Transport));
    if ((pStr = strstr(pRtsp->in_buffer, HDR_SESSION)) != NULL) {
        if (sscanf(pStr, "%*s %d", &s32SessionID) != 1) {
            fprintf(stderr, "Error %s,%i\n", __FILE__, __LINE__);
            send_reply(454, 0, pRtsp); // Session Not Found
            return ERR_NOERROR;
        }
    } else {
        struct timeval stNowTmp;
        gettimeofday(&stNowTmp, 0);
        srand((stNowTmp.tv_sec * 1000) + (stNowTmp.tv_usec / 1000));
        s32SessionID = 1 + (int) (10.0 * rand() / (100000 + 1.0));
        if (s32SessionID == 0) {
            s32SessionID++;
        }
    }
    pRtsp->session_list->session_id = s32SessionID;
    pRtsp->session_list->rtp_session->sched_id = schedule_add(rtp_s);
    send_setup_reply(pRtsp, rtsp_s, rtp_s);
    return ERR_NOERROR;
}

int send_play_reply(RTSP_buffer * pRtsp, RTSP_session * pRtspSessn)
{
     char s8Str[1024];
     char s8Temp[30];
     sprintf(s8Str, "%s %d %s"RTSP_EL"CSeq: %d"RTSP_EL"Server: %s/%s"RTSP_EL, RTSP_VER, 200,\
               get_stat(200), pRtsp->rtsp_cseq, PACKAGE, VERSION);
     add_time_stamp(s8Str, 0);
     sprintf(s8Temp, "Session: %d"RTSP_EL, pRtspSessn->session_id);
     strcat(s8Str, s8Temp);
     strcat(s8Str, RTSP_EL);
     bwrite(s8Str, (unsigned short) strlen(s8Str), pRtsp);
     return ERR_NOERROR;
}

int RTSP_play(RTSP_buffer * pRtsp)
{
    char *pStr;
    char pTrash[128];
    long int s32SessionId;
    RTSP_session *pRtspSesn;
    RTP_session *pRtpSesn;

    if ((pStr = strstr(pRtsp->in_buffer, HDR_CSEQ)) == NULL) {
          send_reply(400, 0, pRtsp);   /* Bad Request */
          printf("get CSeq!!400");
          return ERR_NOERROR;
    } else {
        if (sscanf(pStr, "%254s %d", pTrash, &(pRtsp->rtsp_cseq)) != 2) {
            send_reply(400, 0, pRtsp);    /* Bad Request */
            printf("get CSeq!! 2 400");
            return ERR_NOERROR;
        }
    }
    if ((pStr = strstr(pRtsp->in_buffer, HDR_SESSION)) != NULL) {
        if (sscanf(pStr, "%254s %ld", pTrash, &s32SessionId) != 2) {
            send_reply(454, 0, pRtsp);// Session Not Found
            printf("Session Not Found");
            return ERR_NOERROR;
        }
    } else {
        send_reply(400, 0, pRtsp);// bad request
        printf("Session Not Found bad request");
        return ERR_NOERROR;
    }
    pRtspSesn = pRtsp->session_list;
    if (pRtspSesn != NULL) {
        if (pRtspSesn->session_id == s32SessionId) {
            for (pRtpSesn = pRtspSesn->rtp_session; pRtpSesn != NULL; pRtpSesn = pRtpSesn->next) {
                if (!pRtpSesn->started) {
                    printf("\t+++++++++++++++++++++\n");
                    printf("\tstart to play %d now!\n", pRtpSesn->sched_id);
                    printf("\t+++++++++++++++++++++\n");
                    if (schedule_start(pRtpSesn->sched_id, NULL) == ERR_ALLOC) {
                        return ERR_ALLOC;
                    }
                } else {
                    if (!pRtpSesn->pause) {
                        printf("PLAY: already playing\n");
                    }
                }
            }
        } else {
            send_reply(454, 0, pRtsp); // Session not found
            return ERR_NOERROR;
        }
    } else {
        send_reply(415, 0, pRtsp); // Internal server error
        return ERR_GENERIC;
    }
     send_play_reply(pRtsp, pRtspSesn);
     return ERR_NOERROR;
}

int send_teardown_reply(RTSP_buffer * pRtsp, long SessionId, long cseq)
{
    char s8Str[1024];
    char s8Temp[30];
    sprintf(s8Str, "%s %d %s"RTSP_EL"CSeq: %ld"RTSP_EL"Server: %s/%s"RTSP_EL, RTSP_VER,\
              200, get_stat(200), cseq, PACKAGE, VERSION);
    add_time_stamp(s8Str, 0);
    sprintf(s8Temp, "Session: %ld"RTSP_EL, SessionId);
    strcat(s8Str, s8Temp);
    strcat(s8Str, RTSP_EL);
    bwrite(s8Str, (unsigned short) strlen(s8Str), pRtsp);
    return ERR_NOERROR;
}

int RTSP_teardown(RTSP_buffer * pRtsp)
{
    char *pStr;
    char pTrash[128];
    long int s32SessionId;
    RTSP_session *pRtspSesn;
    RTP_session *pRtpSesn;

    if ((pStr = strstr(pRtsp->in_buffer, HDR_CSEQ)) == NULL) {
        send_reply(400, 0, pRtsp); // Bad Request
        printf("get CSeq error");
        return ERR_NOERROR;
    } else {
        if (sscanf(pStr, "%254s %d", pTrash, &(pRtsp->rtsp_cseq)) != 2) {
            send_reply(400, 0, pRtsp);    // Bad Request
            printf("get CSeq 2 error");
            return ERR_NOERROR;
        }
    }
    if ((pStr = strstr(pRtsp->in_buffer, HDR_SESSION)) != NULL) {
        if (sscanf(pStr, "%254s %ld", pTrash, &s32SessionId) != 2) {
            send_reply(454, 0, pRtsp);     // Session Not Found
            return ERR_NOERROR;
        }
    } else {
        s32SessionId = -1;
    }
    pRtspSesn = pRtsp->session_list;
    if (pRtspSesn == NULL) {
        send_reply(415, 0, pRtsp);  // Internal server error
        return ERR_GENERIC;
    }
    if (pRtspSesn->session_id != s32SessionId) {
        send_reply(454, 0, pRtsp);     // Session not found
        return ERR_NOERROR;
    }
    send_teardown_reply(pRtsp, s32SessionId, pRtsp->rtsp_cseq);
    RTP_session *pRtpSesnTemp;
    pRtpSesn = pRtspSesn->rtp_session;
    while (pRtpSesn != NULL) {
        pRtpSesnTemp = pRtpSesn;
        pRtspSesn->rtp_session = pRtpSesn->next;
        pRtpSesn = pRtpSesn->next;
        RtpDelete((unsigned int)pRtpSesnTemp->hndRtp);
        schedule_remove(pRtpSesnTemp->sched_id);
        g_s32DoPlay--;
    }
    if (g_s32DoPlay == 0) {
        printf("no user online now resetfifo\n");
        ringreset;
        RTP_port_pool_init(RTP_DEFAULT_PORT);
    }
    if (pRtspSesn->rtp_session == NULL) {
        free(pRtsp->session_list);
        pRtsp->session_list = NULL;
    }
    return ERR_NOERROR;
}

void RTSP_state_machine(RTSP_buffer * pRtspBuf, int method)
{

#ifdef RTSP_DEBUG
     trace_point();
#endif
    char *s;
    RTSP_session *pRtspSess;
    long int session_id;
    char trash[255];
    char szDebug[255];
    if ((s = strstr(pRtspBuf->in_buffer, HDR_SESSION)) != NULL) {
        if (sscanf(s, "%254s %ld", trash, &session_id) != 2) {
            fprintf(stderr,"Invalid Session number %s,%i\n", __FILE__, __LINE__);
            send_reply(454, 0, pRtspBuf);
            return;
        }
    }
    pRtspSess = pRtspBuf->session_list;
    if (pRtspSess == NULL) {
        return;
    }
#ifdef RTSP_DEBUG
    sprintf(szDebug,"state_machine:current state is  ");
    strcat(szDebug,((pRtspSess->cur_state==0)?"init state":((pRtspSess->cur_state==1)?"ready state":"play state")));
    printf("%s\n", szDebug);
#endif
    switch (pRtspSess->cur_state) {
        case INIT_STATE: {
        fprintf(stderr,"current method code is:%d\n",method);
            switch (method) {
                case RTSP_ID_DESCRIBE:
                    RTSP_describe(pRtspBuf);
                    break;
                case RTSP_ID_SETUP:
                    if (RTSP_setup(pRtspBuf) == ERR_NOERROR) {
                         pRtspSess->cur_state = READY_STATE;
                        fprintf(stderr,"TRANSFER TO READY STATE!\n");
                    }
                    break;
                case RTSP_ID_TEARDOWN:
                    RTSP_teardown(pRtspBuf);
                    break;
                case RTSP_ID_OPTIONS:
                    if (RTSP_options(pRtspBuf) == ERR_NOERROR) {
                         pRtspSess->cur_state = INIT_STATE;
                    }
                    break;
                case RTSP_ID_PLAY:
                case RTSP_ID_PAUSE:
                    send_reply(455, 0, pRtspBuf);
                    break;
                default:
                    send_reply(501, 0, pRtspBuf);
                    break;
            }
            break;
        }
        case READY_STATE: {
            fprintf(stderr,"current method code is:%d\n",method);
            switch (method) {
                case RTSP_ID_PLAY:
                    if (RTSP_play(pRtspBuf) == ERR_NOERROR) {
                        fprintf(stderr,"\tStart Playing!\n");
                        // pRtspSess->cur_state = PLAY_STATE;
                    }
                    break;
                case RTSP_ID_SETUP:
                    if (RTSP_setup(pRtspBuf) == ERR_NOERROR) {
                        pRtspSess->cur_state = READY_STATE;
                    }
                    break;
                case RTSP_ID_TEARDOWN:
                    RTSP_teardown(pRtspBuf);
                    break;
                case RTSP_ID_OPTIONS:
                    if (RTSP_options(pRtspBuf) == ERR_NOERROR) {
                        pRtspSess->cur_state = INIT_STATE;
                    }
                    break;

                case RTSP_ID_PAUSE: // method not valid this state.
                    send_reply(455, 0, pRtspBuf);
                    break;
                case RTSP_ID_DESCRIBE:
                    RTSP_describe(pRtspBuf);
                    break;
                default:
                    send_reply(501, 0, pRtspBuf);
                    break;
            }
            break;
        }
        case PLAY_STATE: {
            switch (method) {
                case RTSP_ID_PLAY:
                    fprintf(stderr,"UNSUPPORTED: Play while playing.\n");
                    send_reply(551, 0, pRtspBuf);   // Option not supported
                    break;
                case RTSP_ID_TEARDOWN:
                    RTSP_teardown(pRtspBuf);
                    break;
                case RTSP_ID_OPTIONS:
                    break;
                case RTSP_ID_DESCRIBE:
                    RTSP_describe(pRtspBuf);
                    break;
                case RTSP_ID_SETUP:
                    break;
            }
            break;
        } /* PLAY state */
        default: {
            fprintf(stderr,"%s State error: unknown state=%d, method code=%d\n", __FUNCTION__, pRtspSess->cur_state, method);
        }
        break;
    } /* end of current state switch */
    printf("leaving rtsp_state_machine!\n");
}

void RTSP_remove_msg(int len, RTSP_buffer * rtsp)
{
    rtsp->in_size -= len;
    if (rtsp->in_size && len) {
        memmove(rtsp->in_buffer, &(rtsp->in_buffer[len]), RTSP_BUFFERSIZE - len);
        memset(&(rtsp->in_buffer[RTSP_BUFFERSIZE - len]), 0, len);
    }
}

void RTSP_discard_msg(RTSP_buffer * rtsp)
{
    int hlen, blen;

#ifdef RTSP_DEBUG
//     trace_point();
#endif

    //�ҳ����������׸���Ϣ�ĳ��ȣ�Ȼ��ɾ��
    if (RTSP_full_msg_rcvd(rtsp, &hlen, &blen) > 0)
        RTSP_remove_msg(hlen + blen, rtsp);
}

int RTSP_handler(RTSP_buffer *pRtspBuf)
{
    int s32Meth;
    while(pRtspBuf->in_size) {
        s32Meth = RTSP_validate_method(pRtspBuf);
        if (s32Meth < 0) {
            fprintf(stderr,"Bad Request %s,%d\n", __FILE__, __LINE__);
            printf("bad request, requestion not exit %d",s32Meth);
            send_reply(400, NULL, pRtspBuf);
        } else {
            RTSP_state_machine(pRtspBuf, s32Meth);
            printf("exit Rtsp_state_machine\r\n");
        }
        RTSP_discard_msg(pRtspBuf);
        printf("4\r\n");
    }
    return ERR_NOERROR;
}

int RtspServer(RTSP_buffer *rtsp)
{
    fd_set rset,wset;
    struct timeval t;
    int size;
    static char buffer[RTSP_BUFFERSIZE+1]; /* +1 to control the final '\0'*/
    int n;
    int res;
    struct sockaddr ClientAddr;
    if (rtsp == NULL) {
        return ERR_NOERROR;
    }

    FD_ZERO(&rset);
    FD_ZERO(&wset);
    t.tv_sec=0;
    t.tv_usec=100000;

    FD_SET(rtsp->fd,&rset);
    if (select(g_s32Maxfd+1,&rset,0,0,&t) < 0) {
          fprintf(stderr,"select error %s %d\n", __FILE__, __LINE__);
          send_reply(500, NULL, rtsp);
          return ERR_GENERIC; //errore interno al server
     }
     if (FD_ISSET(rtsp->fd,&rset)) {
          memset(buffer,0,sizeof(buffer));
          size=sizeof(buffer)-1;
          n = tcp_read(rtsp->fd, buffer, size, &ClientAddr);
          if (n==0) {
               return ERR_CONNECTION_CLOSE;
          }
          if (n<0) {
               fprintf(stderr,"read() error %s %d\n", __FILE__, __LINE__);
               send_reply(500, NULL, rtsp);
               return ERR_GENERIC;
          }
          if (rtsp->in_size+n>RTSP_BUFFERSIZE) {
               fprintf(stderr,"RTSP buffer overflow (input RTSP message is most likely invalid).\n");
               send_reply(500, NULL, rtsp);
               return ERR_GENERIC;
          }
          fprintf(stderr,"INPUT_BUFFER was:%s\n", buffer);
          memcpy(&(rtsp->in_buffer[rtsp->in_size]),buffer,n);
          rtsp->in_size+=n;
          memset(buffer, 0, n);
          memcpy(&rtsp->stClientAddr, &ClientAddr, sizeof(ClientAddr));
          if ((res=RTSP_handler(rtsp))==ERR_GENERIC) {
               fprintf(stderr,"Invalid input message.\n");
               return ERR_NOERROR;
          }
     }
     if (rtsp->out_size>0) {
          n= tcp_write(rtsp->fd,rtsp->out_buffer,rtsp->out_size);
          printf("5\r\n");
          if (n<0) {
               fprintf(stderr,"tcp_write error %s %i\n", __FILE__, __LINE__);
               send_reply(500, NULL, rtsp);
               return ERR_GENERIC; //errore interno al server
          }
          memset(rtsp->out_buffer, 0, rtsp->out_size);
          rtsp->out_size = 0;
     }
    return ERR_NOERROR;
}

void ScheduleConnections(RTSP_buffer **rtsp_list, int *conn_count)
{
    int res;
    RTSP_buffer *pRtsp=*rtsp_list,*pRtspN=NULL;
    RTP_session *r=NULL, *t=NULL;
    while (pRtsp!=NULL) {
        if ((res = RtspServer(pRtsp))!=ERR_NOERROR) {
            if (res==ERR_CONNECTION_CLOSE || res==ERR_GENERIC) {
                if (res==ERR_CONNECTION_CLOSE)
                    fprintf(stderr,"fd:%d,RTSP connection closed by client.\n",pRtsp->fd);
                else
                    fprintf(stderr,"fd:%d,RTSP connection closed by server.\n",pRtsp->fd);
                if (pRtsp->session_list!=NULL) {
                    r=pRtsp->session_list->rtp_session;
                    while (r!=NULL) {
                        t = r->next;
                        RtpDelete((unsigned int)(r->hndRtp));
                        schedule_remove(r->sched_id);
                        r=t;
                    }
                    free(pRtsp->session_list);
                    pRtsp->session_list=NULL;
                    g_s32DoPlay--;
                    if (g_s32DoPlay == 0) {
                        printf("user abort! no user online now resetfifo\n");
                        ringreset;
                        RTP_port_pool_init(RTP_DEFAULT_PORT);
                    }
                    fprintf(stderr,"WARNING! fd:%d RTSP connection truncated before ending operations.\n",pRtsp->fd);
                }
                // wait for
                close(pRtsp->fd);
                --*conn_count;
                num_conn--;
                if (pRtsp==*rtsp_list) {
                    printf("first error,pRtsp is null\n");
                    *rtsp_list=pRtsp->next;
                    free(pRtsp);
                    pRtsp=*rtsp_list;
                } else {
                      printf("dell current fd:%d\n",pRtsp->fd);
                      pRtspN->next=pRtsp->next;
                      free(pRtsp);
                      pRtsp=pRtspN->next;
                      printf("current next fd:%d\n",pRtsp->fd);
                }
                if (pRtsp==NULL && *conn_count<0) {
                    fprintf(stderr,"to stop cchedule_do thread\n");
                    stop_schedule=1;
                }
            } else {
                  printf("current fd:%d\n",pRtsp->fd);
                  pRtsp = pRtsp->next;
            }
        } else {
            pRtspN = pRtsp;
            pRtsp = pRtsp->next;
        }
    }
}

void RTP_port_pool_init(int port)
{
    int i;
    s_u32StartPort = port;
    for (i=0; i<MAX_CONNECTION; ++i) {
        s_uPortPool[i] = i+s_u32StartPort;
    }
}

void EventLoop(int s32MainFd)
{
    static int s32ConCnt = 0;
    int s32Fd = -1;
    static RTSP_buffer *pRtspList=NULL;
    RTSP_buffer *p=NULL;
    unsigned int u32FdFound;

    if (s32ConCnt!=-1) {
        s32Fd= tcp_accept(s32MainFd);
    }

    if (s32Fd >= 0) {
        for (u32FdFound=0,p=pRtspList; p!=NULL; p=p->next) {
            if (p->fd == s32Fd) {
                u32FdFound=1;
                break;
            }
        }
        if (!u32FdFound) {
            if (s32ConCnt<MAX_CONNECTION) {
                ++s32ConCnt;
                AddClient(&pRtspList,s32Fd);
            } else {
                fprintf(stderr, "exceed the MAX client, ignore this connecting\n");
                return;
            }
            num_conn++;
            fprintf(stderr, "%s Connection reached: %d\n", __FUNCTION__, num_conn);
        }
    }
    ScheduleConnections(&pRtspList,&s32ConCnt);
}

void IntHandl(int i)
{
    stop_schedule = 1;
    g_s32Quit = 1;
}

char * base64_encode(const unsigned char * bindata, char * base64, int binlength)
{
    int i, j;
    unsigned char current;
    char * base64char = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (i = 0, j = 0 ; i < binlength ; i += 3) {
        current = (bindata[i] >> 2) ;
        current &= (unsigned char)0x3F;
        base64[j++] = base64char[(int)current];
        current = ((unsigned char)(bindata[i] << 4)) & ((unsigned char)0x30);
        if (i + 1 >= binlength) {
            base64[j++] = base64char[(int)current];
            base64[j++] = '=';
            base64[j++] = '=';
            break;
        }
        current |= ((unsigned char)(bindata[i+1] >> 4)) & ((unsigned char) 0x0F);
        base64[j++] = base64char[(int)current];

        current = ((unsigned char)(bindata[i+1] << 2)) & ((unsigned char)0x3C) ;
        if (i + 2 >= binlength) {
            base64[j++] = base64char[(int)current];
            base64[j++] = '=';
            break;
        }
        current |= ((unsigned char)(bindata[i+2] >> 6)) & ((unsigned char) 0x03);
        base64[j++] = base64char[(int)current];

        current = ((unsigned char)bindata[i+2]) & ((unsigned char)0x3F) ;
        base64[j++] = base64char[(int)current];
    }
    base64[j] = '\0';
    return base64;
}

void base64_encode2(char *in, const int in_len, char *out, int out_len)
{
    static const char *codes ="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int base64_len = 4 * ((in_len+2)/3);
    char *p = out;
    int times = in_len / 3;
    int i;
    for (i=0; i<times; ++i) {
        *p++ = codes[(in[0] >> 2) & 0x3f];
        *p++ = codes[((in[0] << 4) & 0x30) + ((in[1] >> 4) & 0xf)];
        *p++ = codes[((in[1] << 2) & 0x3c) + ((in[2] >> 6) & 0x3)];
        *p++ = codes[in[2] & 0x3f];
        in += 3;
    }
    if (times * 3 + 1 == in_len) {
        *p++ = codes[(in[0] >> 2) & 0x3f];
        *p++ = codes[((in[0] << 4) & 0x30) + ((in[1] >> 4) & 0xf)];
        *p++ = '=';
        *p++ = '=';
    }
    if (times * 3 + 2 == in_len) {
        *p++ = codes[(in[0] >> 2) & 0x3f];
        *p++ = codes[((in[0] << 4) & 0x30) + ((in[1] >> 4) & 0xf)];
        *p++ = codes[((in[1] << 2) & 0x3c)];
        *p++ = '=';
    }
    *p = 0;
}

void UpdateSps(unsigned char *data,int len)
{
    int i;
    if(len>21)
        return ;
    sprintf(psp.base64profileid,"%x%x%x",data[1],data[2],data[3]);//sps[0] 0x67
#if 0
    ///a=fmtp:96 packetization-mode=1;profile-level-id=4b9096;sprop-parameter-sets=8kuQlme8xyAX,+sIKBQ==;
    base64_encode((unsigned char *)data, psp.base64sps, len);///sps
#else
    //a=fmtp:96 packetization-mode=1;profile-level-id=ee69bc;sprop-parameter-sets=xu5pvGeDzEef,qFT3dm==;
    base64_encode2(data, len, psp.base64sps, 512);
#endif
}

void UpdatePps(unsigned char *data,int len)
{
    int i;
    if (len>21)
        return ;
#if 0
    base64_encode((unsigned char *)data, psp.base64pps, len);
#else
    char pic1_paramBase64[512] = {0};
    base64_encode2(data, len, psp.base64pps, 512);
#endif
}


