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
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "sdk.h"
#include "securec.h"
#include "sample_nnie_main.h"

static char **s_ppChCmdArgv = NULL;

/* function : to process abnormal case */
#if (!defined(__HuaweiLite__)) || defined(__OHOS__)
static void SAMPLE_SVP_HandleSig(int s32Signo)
{
    if (s32Signo == SIGINT || s32Signo == SIGTERM) {
        switch (*s_ppChCmdArgv[1]) {
            case '0':
                SAMPLE_SVP_NNIE_Rfcn_HandleSig();
                break;
            case '1':
                SAMPLE_SVP_NNIE_Segnet_HandleSig();
                break;
            case '2':
                SAMPLE_SVP_NNIE_FasterRcnn_HandleSig();
                break;
            case '3':
                SAMPLE_SVP_NNIE_FasterRcnn_HandleSig();
                break;
            case '4':
                SAMPLE_SVP_NNIE_Cnn_HandleSig();
                break;
            case '5':
                SAMPLE_SVP_NNIE_Ssd_HandleSig();
                break;
            case '6':
                SAMPLE_SVP_NNIE_Yolov1_HandleSig();
                break;
            case '7':
                SAMPLE_SVP_NNIE_Yolov2_HandleSig();
                break;
            case '8':
                SAMPLE_SVP_NNIE_Yolov3_HandleSig();
                break;
            case '9':
                SAMPLE_SVP_NNIE_Lstm_HandleSig();
                break;
            case 'a':
                SAMPLE_SVP_NNIE_Pvanet_HandleSig();
                break;
            case 'b':
                SAMPLE_SVP_NNIE_Rfcn_HandleSig_File();
                break;
            default:
                break;
        }
    }
}
#endif

/* function : show usage */
static void SAMPLE_SVP_Usage(const char *pchPrgName)
{
    printf("Usage : %s <index> \n", pchPrgName);
    printf("index:\n");
    printf("\t 0) RFCN(VI->VPSS->NNIE->VGS->VO).\n");
    printf("\t 1) Segnet(Read File).\n");
    printf("\t 2) FasterRcnnAlexnet(Read File).\n");
    printf("\t 3) FasterRcnnDoubleRoiPooling(Read File).\n");
    printf("\t 4) Cnn(Read File).\n");
    printf("\t 5) SSD(Read File).\n");
    printf("\t 6) Yolov1(Read File).\n");
    printf("\t 7) Yolov2(Read File).\n");
    printf("\t 8) Yolov3(Read File).\n");
    printf("\t 9) LSTM(Read File).\n");
    printf("\t a) Pvanet(Read File).\n");
    printf("\t b) Rfcn(Read File).\n");
}

/* function : nnie sample */
#if defined(__HuaweiLite__) && (!defined(__OHOS__))
int app_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    int s32Ret = HI_SUCCESS;
    sdk_init();
    s_ppChCmdArgv = argv;
#if (!defined(__HuaweiLite__)) || defined(__OHOS__)
    struct sigaction sa;
    (hi_void)memset_s(&sa, sizeof(struct sigaction), 0, sizeof(struct sigaction));
    sa.sa_handler = SAMPLE_SVP_HandleSig;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
#endif
    if (argc < 2 || argc > 2) { /* only support 2 parameters */
        SAMPLE_SVP_Usage(argv[0]);
        s32Ret = HI_FAILURE;
        goto end;
    }

    if (strncmp(argv[1], "-h", 2) == 0) { /* 2: maximum number of characters to compare */
        SAMPLE_SVP_Usage(argv[0]);
        s32Ret = HI_SUCCESS;
        goto end;
    }
    switch (*argv[1]) {
        case '0':
            SAMPLE_SVP_NNIE_Rfcn();
            break;
        case '1':
            SAMPLE_SVP_NNIE_Segnet();
            break;
        case '2':
            SAMPLE_SVP_NNIE_FasterRcnn();
            break;
        case '3':
            SAMPLE_SVP_NNIE_FasterRcnn_DoubleRoiPooling();
            break;
        case '4':
            SAMPLE_SVP_NNIE_Cnn();
            break;
        case '5':
            SAMPLE_SVP_NNIE_Ssd();
            break;
        case '6':
            SAMPLE_SVP_NNIE_Yolov1();
            break;
        case '7':
            SAMPLE_SVP_NNIE_Yolov2();
            break;
        case '8':
            SAMPLE_SVP_NNIE_Yolov3();
            break;
        case '9':
            SAMPLE_SVP_NNIE_Lstm();
            break;
        case 'a':
            SAMPLE_SVP_NNIE_Pvanet();
            break;
        case 'b':
            SAMPLE_SVP_NNIE_Rfcn_File();
            break;
        default:
            SAMPLE_SVP_Usage(argv[0]);
            break;
    }

end:
    sdk_exit();
    return s32Ret;
}
