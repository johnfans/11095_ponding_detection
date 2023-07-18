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

#include <stdio.h>
#include "lwip/sockets.h"
#include "net_demo.h"

static char g_request[] = "command rogger";
static char s_response[]= "command execute";
static char f_response[]= "command failed";
static char t_response[128]="";


int TcpClientTest(const char* host, unsigned short port, char g_response[])
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // TCP socket
    int reclen;

    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;  // AF_INET表示IPv4协议
    serverAddr.sin_port = htons(port);  // 端口号，从主机字节序转为网络字节序
    if (inet_pton(AF_INET, host, &serverAddr.sin_addr) <= 0) {  // 将主机IP地址从“点分十进制”字符串 转化为 标准格式（32位整数）
        printf("inet_pton failed!\r\n");
        lwip_close(sockfd);
    }

    // 尝试和目标主机建立连接，连接成功会返回0 ，失败返回 -1
    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        printf("connect failed!\r\n");
        lwip_close(sockfd);
    }
    printf("connect to server %s success!\r\n", host);

    while (1)
    {
    // 建立连接成功之后，这个TCP socket描述符 —— sockfd 就具有了 “连接状态”，发送、接收 对端都是 connect 参数指定的目标主机和端口
    ssize_t retval = recv(sockfd, &t_response, sizeof(t_response), 0);
    if (retval <= 0) {
        printf("send g_response from server failed or done, %ld!\r\n", retval);
        continue;
    }
    t_response[retval] = '\0';
    printf("recv g_response{%s} %ld from server done!\r\n", t_response, retval);
    reclen=(int)retval;
    
    retval = send(sockfd, g_request, sizeof(g_request), 0);
    if (retval < 0) {
        printf("send g_request failed!\r\n");
        continue;
    }
    printf("send g_request{%s} %ld to server done!\r\n", g_request, retval);
    break;


    
    
    sleep(1);
    }
    lwip_close(sockfd);
    return reclen;
    
}

int NetDemoTest(unsigned short port, const char* host, char g_response[])
{   
    int reclen;
    (void) host;
    printf("TcpClientTest start\r\n");
    printf("I will connect to %s\r\n", host);
    reclen=TcpClientTest(host, port, g_response);
    return reclen;
}

int recall(const char* host, unsigned short port, int flag)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // TCP socket
    int reclen;

    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;  // AF_INET表示IPv4协议
    serverAddr.sin_port = htons(port);  // 端口号，从主机字节序转为网络字节序
    if (inet_pton(AF_INET, host, &serverAddr.sin_addr) <= 0) {  // 将主机IP地址从“点分十进制”字符串 转化为 标准格式（32位整数）
        printf("inet_pton failed!\r\n");
        lwip_close(sockfd);
    }

    // 尝试和目标主机建立连接，连接成功会返回0 ，失败返回 -1
    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        printf("connect failed!\r\n");
        lwip_close(sockfd);
    }
    printf("connect to server %s success!\r\n", host);
    
    if (flag==1)
    {
        ssize_t retval = send(sockfd, s_response, sizeof(s_response), 0);
        if (retval < 0) {
            printf("send s_response failed!\r\n");
            lwip_close(sockfd);return 0;
            }
        printf("send g_request{%s} %ld to server done!\r\n", s_response, retval);
        return 1;
        
    }
    if (flag==0)
    {
        ssize_t retval = send(sockfd, f_response, sizeof(f_response), 0);
        if (retval < 0) {
            printf("send f_response failed!\r\n");
            lwip_close(sockfd);return 0;
            }
        printf("send g_request{%s} %ld to server done!\r\n", f_response, retval);
        return 1;
        
    }
}
