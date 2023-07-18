/*
 * Copyright (c) 2022 HiSilicon (Shanghai) Technologies CO., LIMITED.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
#include "sample_rtsp.h"
#include "rtspservice.h"
#include "rtputils.h"
#include "ringfifo.h"
#include "mpi_sys.h"
#include "sdk.h"

extern int g_s32Quit ;

/******************************************************************************
* function    : main()
* Description : main
******************************************************************************/
#ifdef __HuaweiLite__
int app_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    sdk_init();
    int s32MainFd,temp;
    struct timespec ts = {1, 0};
    pthread_t id;
    //rtsp
    ringmalloc(256*1024);
    printf("RTSP server START\n");
    PrefsInit();
    printf("listen for client connecting...\n");
    signal(SIGINT, IntHandl);
    s32MainFd = tcp_listen(SERVER_RTSP_PORT_DEFAULT);
    if (ScheduleInit() == ERR_FATAL) {
        fprintf(stderr,"Fatal: Can't start scheduler %s, %i \nServer is aborting.\n", __FILE__, __LINE__);
        return 0;
    }
    RTP_port_pool_init(RTP_DEFAULT_PORT);
    pthread_create(&id, NULL, SAMPLE_VENC_H265_H264, NULL);
    pthread_detach(id);
    while (!g_s32Quit) {
        nanosleep(&ts, NULL);
        EventLoop(s32MainFd);
    }
    sleep(2);
    ringfree();
    sdk_exit();
    SAMPLE_PRT("\nsdk exit success\n");
    printf("The Server quit!\n");
    return 0;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
