#ifndef _RTSP_SERVICE_H
#define _RTSP_SERVICE_H
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <math.h>
#include <stdio.h>

#define trace_point()  do {printf("rtsp_tracepoint: %s,%s,%d\n",__FILE__,__FUNCTION__,__LINE__); } while(0)  //10728

/*error codes define,yanf*/
#define ERR_NOERROR          0
#define ERR_GENERIC             -1
#define ERR_NOT_FOUND       -2
#define ERR_PARSE           -3
#define ERR_ALLOC               -4
#define ERR_INPUT_PARAM         -5
#define ERR_NOT_SD          -6
#define ERR_UNSUPPORTED_PT      -7
#define ERR_EOF             -8
#define ERR_FATAL                   -9
#define ERR_CONNECTION_CLOSE        -10

#define HDR_CONTENTLENGTH "Content-Length"
#define HDR_ACCEPT "Accept"
#define HDR_ALLOW "Allow"
#define HDR_BLOCKSIZE "Blocksize"
#define HDR_CONTENTTYPE "Content-Type"
#define HDR_DATE "Date"
#define HDR_REQUIRE "Require"
#define HDR_TRANSPORTREQUIRE "Transport-Require"
#define HDR_SEQUENCENO "SequenceNo"
#define HDR_CSEQ "CSeq"
#define HDR_STREAM "Stream"
#define HDR_SESSION "Session"
#define HDR_TRANSPORT "Transport"
#define HDR_RANGE "Range"
#define HDR_USER_AGENT "User-Agent"

#define RTSP_METHOD_MAXLEN 15
#define RTSP_METHOD_DESCRIBE "DESCRIBE"
#define RTSP_METHOD_ANNOUNCE "ANNOUNCE"
#define RTSP_METHOD_GET_PARAMETERS "GET_PARAMETERS"
#define RTSP_METHOD_OPTIONS "OPTIONS"
#define RTSP_METHOD_PAUSE "PAUSE"
#define RTSP_METHOD_PLAY "PLAY"
#define RTSP_METHOD_RECORD "RECORD"
#define RTSP_METHOD_REDIRECT "REDIRECT"
#define RTSP_METHOD_SETUP "SETUP"
#define RTSP_METHOD_SET_PARAMETER "SET_PARAMETER"
#define RTSP_METHOD_TEARDOWN "TEARDOWN"

#define RTSP_ID_DESCRIBE 0
#define RTSP_ID_ANNOUNCE 1
#define RTSP_ID_GET_PARAMETERS 2
#define RTSP_ID_OPTIONS 3
#define RTSP_ID_PAUSE 4
#define RTSP_ID_PLAY 5
#define RTSP_ID_RECORD 6
#define RTSP_ID_REDIRECT 7
#define RTSP_ID_SETUP 8
#define RTSP_ID_SET_PARAMETER 9
#define RTSP_ID_TEARDOWN 10

#define RTSP_not_full 0
#define RTSP_method_rcvd 1
#define RTSP_interlvd_rcvd 2

#define RTSP_BUFFERSIZE 4096
#define MAX_DESCR_LENGTH 4096

/* Stati della macchina a stati del server rtsp*/
#define INIT_STATE      0
#define READY_STATE     1
#define PLAY_STATE      2

#define RTSP_VER "RTSP/1.0"

#define RTSP_EL "\r\n"

#define PACKAGE "sunshine"
#define VERSION "1.11"

//extern struct _tagStRtpHandle *HndRtp;


typedef struct
{
    int RTP;
    int RTCP;
} port_pair;

typedef enum{
    RTP_no_transport=0,
    RTP_rtp_avp,
    RTP_rtp_avp_tcp
} rtp_type;

typedef struct _RTP_transport
{
    rtp_type type;
    int rtp_fd;
    union{
    struct {
        port_pair cli_ports;
        port_pair ser_ports;
        unsigned char is_multicast;
    } udp;
    struct {
        port_pair interleaved;
    } tcp;
    // other trasports here
    } u;
} RTP_transport;

typedef struct _RTP_session {
    struct _tagStRtpHandle *hndRtp;
    RTP_transport transport;
    unsigned char pause;
    unsigned char started;
    int sched_id;
    struct _RTP_session *next;
} RTP_session;

typedef struct _RTSP_session {
    int cur_state;
    int session_id;
    RTP_session *rtp_session;
    struct _RTSP_session *next;
} RTSP_session;

typedef struct _RTSP_buffer {
    int fd;
    unsigned int port;
    struct sockaddr stClientAddr;
    char in_buffer[RTSP_BUFFERSIZE];
    unsigned int in_size;
    char out_buffer[RTSP_BUFFERSIZE+MAX_DESCR_LENGTH];
    int out_size;

    unsigned int rtsp_cseq;
    char descr[MAX_DESCR_LENGTH];
    RTSP_session *session_list;
    struct _RTSP_buffer *next;
} RTSP_buffer;

char *sock_ntop_host(const struct sockaddr *sa, socklen_t salen, char *str, size_t len);
int tcp_accept(int fd);
int tcp_connect(unsigned short port, char *addr);
int tcp_listen(unsigned short port);
int tcp_read(int fd, void *buffer, int nbytes, struct sockaddr *Addr);
int tcp_send(int fd, void *dataBuf, unsigned int dataSize);
int tcp_write(int fd, char *buffer, int nbytes);

#define MAX_PROCESS    1 /*number of fork*/
#define MAX_CONNECTION    10 /*rtsp connection*/

typedef struct _play_args
{
    struct tm playback_time;
    short playback_time_valid;
    float start_time;
    short start_time_valid;
    float end_time;
} stPlayArgs;

typedef unsigned int (*RTP_play_action)(unsigned int u32Rtp, char *pData, int s32DataSize, unsigned int u32TimeStamp);

typedef struct _schedule_list
{
    int valid;
    int BeginFrame;
    RTP_session *rtp_session;
    RTP_play_action play_action;
} stScheList;

int ScheduleInit();
void *schedule_do(void *nothing);
int schedule_add(RTP_session *rtp_session /*,RTSP_session *rtsp_session*/);
int schedule_start(int id,stPlayArgs *args);
void schedule_stop(int id);
int schedule_remove(int id);
int schedule_resume(int id, stPlayArgs *args);

typedef enum
{
    /*sender report,for transmission and reception statics from participants that are active senders*/
    SR=200,
    /*receiver report,for reception statistics from participants that are not active senders
     * and in combination with SR for    active senders reporting on more than 31 sources
     */
    RR=201,
    SDES=202, /* Source description items, including CNAME,NAME,EMAIL,etc */
    BYE=203, /* Indicates end of participation */
    APP=204 /* Application-specific functions */
} rtcp_pkt_type;

/* Define default RTSP listening port */
#define SERVER_RTSP_PORT_DEFAULT 554
#define SERVER_RTSP_IP "192.168.200.200"

typedef struct _StServPrefs {
    char hostname[256];
    char serv_root[256];
    char log[256];
    unsigned int port;
    unsigned int max_session;
} StServPrefs;

int send_reply(int err, char *addon, RTSP_buffer * rtsp);
int bwrite(char *buffer, unsigned short len, RTSP_buffer * rtsp);
const char *get_stat(int err);

#endif