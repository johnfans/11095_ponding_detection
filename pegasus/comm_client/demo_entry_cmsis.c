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
#include <string.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "net_demo.h"
#include "net_params.h"
#include "wifi_connecter.h"

#include "iot_gpio_ex.h"
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "iot_uart.h"
#include "hi_uart.h"
#include "iot_watchdog.h"
#include "iot_errno.h"
#include "lwip/sockets.h"

#define UART_BUFF_SIZE 2048
#define U_SLEEP_TIME   100000
#define LED_TASK_STACK_SIZE (1024 * 4)
#define LED_TASK_PRIO 25
#define LED_GPIO 2
static char response[3];
static char m_request[]= "m";
static char t_request[128];

static char a_send[]="111";
unsigned char uartReadBuff[UART_BUFF_SIZE] = {0};



void distinguish123(char a)
{
    unsigned int conut;//
    if (a=='1')//turn to 60 degree
    {
        for (conut=0;conut<50;conut++)
        {
         // set GPIO_2 output high levels to turn on LED(servo)
         IoTGpioSetOutputVal(LED_GPIO, 1);

         // delay 1166us(2/3+0.5ms,rotate to 30degree)
         hi_udelay(1166);

         // set GPIO_2 output low levels to turn off LED
         IoTGpioSetOutputVal(LED_GPIO, 0);

         // delay 18834us(20ms-1166us)
         hi_udelay(18834);
        }
        IoTGpioSetOutputVal(LED_GPIO, 0);
    }
    if (a=='2')
    {
        for (conut=0;conut<50;conut++)
        {
             // set GPIO_2 output high levels to turn on LED(servo)
             IoTGpioSetOutputVal(LED_GPIO, 1);

             // delay 1500us(1+0.5ms,rotate to 90 degree)
             hi_udelay(1500);

             // set GPIO_2 output low levels to turn off LED
             IoTGpioSetOutputVal(LED_GPIO, 0);

             // delay 18500us(20000-1500)
             hi_udelay(18500);
        }
        IoTGpioSetOutputVal(LED_GPIO, 0);

    }
    if (a=='3')
        for (conut=0;conut<50;conut++)
        {
             // set GPIO_2 output high levels to turn on LED(servo)
             IoTGpioSetOutputVal(LED_GPIO, 1);

             // delay 1833us(1.33+0.5ms,rotate to 150 degree)
             hi_udelay(1833);

             // set GPIO_2 output low levels to turn off LED
             IoTGpioSetOutputVal(LED_GPIO, 0);

             // delay 18167us(20000-1833)
             hi_udelay(18167);
        }
        IoTGpioSetOutputVal(LED_GPIO, 0); 
}

static void ToDeg(int angle)
{
    printf("%d\n",angle);
    int angle_time;
    int angle_anti_time;
    int conut;
    angle*=20;
    printf("%d\n",angle);
    
    angle_time=angle*11+500;
    
    angle_anti_time=20000-angle_time;
    

    printf("%d\n",angle_time);
    printf("%d\n",angle_anti_time);
    
        for (conut=0;conut<50;conut++)
        {
             // set GPIO_2 output high levels to turn on LED(servo)
             IoTGpioSetOutputVal(LED_GPIO, 1);

             // delay 500+angle*1000/90us(angle/90+0.5ms,rotate to 30degree)
             hi_udelay(angle_time);

             // set GPIO_2 output low levels to turn off LED
             IoTGpioSetOutputVal(LED_GPIO, 0);

             // delay 20000-angle_timeus(20ms-~us)
             hi_udelay(angle_anti_time);
        }

        // wait for 2s
        IoTGpioSetOutputVal(LED_GPIO, 0);
        sleep(3);

        
    
}



int TcpClientTest(const char* host, unsigned short port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // TCP socket
    int reclen=0,flag;
    char i='1';
    t_request[0]='t';
    int deg=60;
    int degR=-1;

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
        return 0;
    }
    printf("connect to server %s success!\r\n", host);

    

    while (1)
    {   
        for(i='1';i<='3';i++)
        {   t_request[1]=i;t_request[2]='\0';
            

            

            ssize_t retval = send(sockfd, m_request, sizeof(m_request), 0);
            if (retval < 0) {
                printf("send m_request failed!\r\n");
                return 0;
            }
            printf("send m_request{%s} %ld to server done!\r\n", m_request, retval);
            
            while (1)
            {
                retval = recv(sockfd, response, sizeof(response), 0);
                if (retval <= 0) {
                    printf("send g_response from server failed or done, %ld!\r\n", retval);
                    return 0;
                }
                response[retval] = '\0';
                printf("recv g_response{%s} %ld from server done!\r\n", response, retval);
                reclen=(int)retval;

                if (response[0]!='a')
                {
                    break;
                }
                retval = send(sockfd, a_send, sizeof(a_send), 0);
                if (retval < 0)
                {
                    printf("send a_send failed!\r\n");
                    return 0;
                }    
            }

            if(response[0]=='m')
            {
                if(response[1]=='0')
                {   if (response[2]==degR)
                    {
                    break;
                    }
                    degR=response[2];
                    deg=(int)response[2];
                    deg-=48;
                    
                    ToDeg(deg);
                    
                    break;
                }
                if (response[1]=='1')
                {
                    distinguish123(i);
                }
                
            }

            retval=UartTask(10);
            if(retval==1)
                strcpy(t_request+2,uartReadBuff);

            retval = send(sockfd, t_request, sizeof(t_request), 0);
            if (retval < 0) {
                printf("send t_request failed!\r\n");
                break;
            }
            printf("send t_request{%s} %ld to server done!\r\n", t_request, retval);

            osDelay(300);
            


        }



    
    


    
    
    
    

    
    }

    lwip_close(sockfd);
    return reclen;
    
}

int NetDemoTest(unsigned short port, const char* host)
{   
    int reclen;
    (void) host;
    printf("TcpClientTest start\r\n");
    printf("I will connect to %s\r\n", host);
    reclen=TcpClientTest(host, port);
    return reclen;
}



void Uart1GpioInit(void)
{
    IoTGpioInit(IOT_IO_NAME_GPIO_0);
    // 设置GPIO0的管脚复用关系为UART1_TX Set the pin reuse relationship of GPIO0 to UART1_ TX
    IoSetFunc(IOT_IO_NAME_GPIO_0, IOT_IO_FUNC_GPIO_0_UART1_TXD);
    IoTGpioInit(IOT_IO_NAME_GPIO_1);
    // 设置GPIO1的管脚复用关系为UART1_RX Set the pin reuse relationship of GPIO1 to UART1_ RX
    IoSetFunc(IOT_IO_NAME_GPIO_1, IOT_IO_FUNC_GPIO_1_UART1_RXD);
}

void Uart1Config(void)
{
    uint32_t ret;
    /* 初始化UART配置，波特率 9600，数据bit为8,停止位1，奇偶校验为NONE */
    /* Initialize UART configuration, baud rate is 9600, data bit is 8, stop bit is 1, parity is NONE */
    IotUartAttribute uart_attr = {
        .baudRate = 9600,
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };
    ret = IoTUartInit(HI_UART_IDX_1, &uart_attr);
    if (ret != IOT_SUCCESS) {
        printf("Init Uart1 Falied Error No : %d\n", ret);
        return;
    }
}

int UartTask(int comm_len)
{
    unsigned int status=0;
    
    uint32_t count = 1;
    uint32_t len = 0;
    

    // 对UART1的一些初始化 Some initialization of UART1
    
    // 对UART1参数的一些配置 Some configurations of UART1 parameters
    

    while (1) {
        

        // 通过UART1 接收数据 Receive data through UART1
        IoTUartWrite(HI_UART_IDX_1, (unsigned char*)response, comm_len);
        usleep(U_SLEEP_TIME);
        len = IoTUartRead(HI_UART_IDX_1, uartReadBuff, UART_BUFF_SIZE);
        
        if (len>0)
        {
            printf("Uart Read Data is: [ %d ] %s \r\n", count, uartReadBuff);
            status=1;
        }
        
        
        usleep(U_SLEEP_TIME);
        
        break;
            
            
        
        
        
    }
    return status;
}

static void NetDemoTask(void)
{
    int netId = ConnectToHotspot();
    int flag=0;
    int timeout = 10; /* timeout 10ms */
    while (timeout--) {
        printf("I will start lwip test!\r\n");
        osDelay(100); /* 延时100ms */
    }
    while (1)
    {
        NetDemoTest(PARAM_SERVER_PORT, PARAM_SERVER_ADDR);
        sleep(1);
    }




    printf("disconnect to AP ...\r\n");
    DisconnectWithHotspot(netId);
    printf("disconnect to AP done!\r\n");
}

static void NetDemoEntry(void)
{
    osThreadAttr_t attr;
    IoTGpioInit(LED_GPIO);

    
    IoTGpioSetDir(LED_GPIO, IOT_GPIO_DIR_OUT);

    Uart1GpioInit();
    Uart1Config();
    IoTWatchDogDisable();
    attr.name = "NetDemoTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 10240; /* 堆栈大小为10240 */
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)NetDemoTask, NULL, &attr) == NULL) {
        printf("[NetDemoEntry] Falied to create NetDemoTask!\n");
    }
}

SYS_RUN(NetDemoEntry);