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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "lwip/sockets.h"
#include "net_demo.h"

static char g_request[128] = "Hello,I am Lwip";

void TcpServerTest(unsigned short port)
{
    int backlog = 1;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // TCP socket

    struct sockaddr_in clientAddr = {0};
    socklen_t clientAddrLen = sizeof(clientAddr);
    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);  // 端口号，从主机字节序转为网络字节序
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); // 允许任意主机接入， 0.0.0.0

    ssize_t retval = bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)); // 绑定端口
    if (retval < 0) {
        printf("bind failed, %ld!\r\n", retval);
        printf("do_cleanup...\r\n");
        lwip_close(sockfd);
    }
    printf("bind to port success!\r\n");

    retval = listen(sockfd, backlog); // 开始监听
    if (retval < 0) {
        printf("listen failed!\r\n");
        printf("do_cleanup...\r\n");
        lwip_close(sockfd);
    }
    printf("listen with %d backlog success!\r\n", backlog);

    // 接受客户端连接，成功会返回一个表示连接的 socket ， clientAddr 参数将会携带客户端主机和端口信息 ；失败返回 -1
    // 此后的 收、发 都在 表示连接的 socket 上进行；之后 sockfd 依然可以继续接受其他客户端的连接，
    //  UNIX系统上经典的并发模型是“每个连接一个进程”——创建子进程处理连接，父进程继续接受其他客户端的连接
    //  鸿蒙liteos-a内核之上，可以使用UNIX的“每个连接一个进程”的并发模型
    //     liteos-m内核之上，可以使用“每个连接一个线程”的并发模型
    int connfd = accept(sockfd, (struct sockaddr *)&clientAddr, &clientAddrLen);
    if (connfd < 0) {
        printf("accept failed, %d, %d\r\n", connfd, errno);
        printf("do_cleanup...\r\n");
        lwip_close (sockfd);
    }
    printf("accept success, connfd = %d!\r\n", connfd);
    printf("client addr info: host = %s, port = %d\r\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

    // 后续 收、发 都在 表示连接的 socket 上进行；
    retval = recv(connfd, g_request, sizeof(g_request), 0);
    if (retval < 0) {
        printf("recv g_request failed, %ld!\r\n", retval);
        sleep(1);
        lwip_close(connfd);
        sleep(1); // for debug
        lwip_close(sockfd);
    }
    printf("recv g_request{%s} from client done!\r\n", g_request);

    retval = send(connfd, g_request, strlen(g_request), 0);
    if (retval <= 0) {
        printf("send response failed, %ld!\r\n", retval);
        sleep(1);
        lwip_close(connfd);
        sleep(1); // for debug
        lwip_close(sockfd);
    }
    printf("send response{%s} to client done!\r\n", g_request);
}

void NetDemoTest(unsigned short port, const char* host)
{
    (void) host;
    printf("TcpServerTest start\r\n");
    printf("I will listen on \r\n");
    TcpServerTest(port);
    printf("TcpServerTest done!\r\n");
}
