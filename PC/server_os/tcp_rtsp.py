import socket
import telnetlib
import threading
import time
import cv2
import keyboard
import os
import sys


state_m=0
state_c=0
angle_m=4
ask=0
state_net=0
t_buffer=[0]

TNHOST = "192.168.243.64"
TNPORT = 23

# 登录凭据
USERNAME = "root"
PASSWORD = ""

HOST = '0.0.0.0'  # 监听地址
PORT = 8888  # 监听端口
BUFFER_SIZE = 1024  # 缓冲区大小
def rtsp_monitor():
    fname=["right_pre","center_pre","left_pre"]
    # RTSP流媒体地址
    global t_buffer
    t_buffer_n=0
    rtsp_url = "rtsp://192.168.243.64:8554/live"
    

    # 创建视频捕获对象
    cap = cv2.VideoCapture(rtsp_url)
    time.sleep(1)
    
    # 检查是否成功打开流
    if not cap.isOpened():
        print("[fail]无法打开RTSP流")
        return
    

    c=0
    t=time.time()

    # 播放流
    while True:
        # 从流中读取一帧
        c=c+1
        ret, frame = cap.read()
        
        
        # 显示帧
        cv2.imshow("RTSP Stream", frame)
        while not time.time()>=t+c*(1/25):
            cv2.waitKey(1)

        # 按下'q'键截图
            
            try:
                cv2.getWindowProperty("RTSP Stream",0)

            
            except:
                print("[info]rtsp监视器已关闭")
                cap.release()
                cv2.destroyAllWindows()
                return
        if t_buffer_n!=t_buffer[0]:
            cv2.imwrite(fname[int(t_buffer[0])]+".jpg", frame)
            
        t_buffer_n=t_buffer[0]
        


def commanding(command):
    global state_m, angle_m, state_c,state_net,tn,ask
    CAMPRO="./ohos_rtsp_demo"
    if command=="state":
        print("电机："+str(state_m))
        print("相机："+str(state_c))
        print("网络："+str(state_net))
        return
    
    if command=="state_far":
        ask=1
        return

    elif command=="detect":
        print("[info]打开检测程序")
        os.system('detect.exe')
        return
    
    elif command=="monitor":
        print("[info]打开监视器")
        monitor_thread = threading.Thread(target=rtsp_monitor)
        monitor_thread.start()
        return

    elif command=="exit":
        sys.exit()
    
    commands=command.split()
    print(commands)
    
    if commands[0]=="motor":
        if len(commands)==1:
            print("[info]请参考手册输入指令")
            return
        if commands[1]=="auto":
            state_m=1
            print("[info]将电机切换至自动...")
            return
        
        elif commands[1]=="manual":
            state_m=0
            print("[info]将电机切换至手动...")
            return
        
        elif commands[1].isdigit():
            if int(commands[1])>0 and int(commands[1])<=180:
                if state_m==1:
                    print("[fail]电机未在手动模式")
                    return
                angle_m=int(int(commands[1])/20)
                print("[info]将旋转至"+str(angle_m*20)+"度")
                return
        
        else:
            print("[fail]参数错误")
            return
    
    elif commands[0]=="cam":
        if commands[1]== "on":
            tn.write(b"cd /mnt\n")
            tn.write(CAMPRO.encode('ascii')+b"\n\n")
            tn.read_until(b"\n")
            reso = tn.read_until(b"\n")
            reso = tn.read_until(b"\n")
            print("[cam]"+reso.decode('ascii'))
            print("[info]相机已启动")
            state_c=1

        elif commands[1]=="off":
            tn.write(b"\n\n")
            state_c=0
            print("[info]相机已关闭")
        
        else:
            print("[fail]参数错误")
    else:
        print("[fail]命令有误")
    return

def writeline(content, line_number):
    with open('tcp.txt', 'r') as f:
        lines = f.readlines()

    with open('tcp.txt', 'w') as f:
        for i, line in enumerate(lines):
            if i == line_number - 1:
                f.write(content + '\n')
            f.write(line)

def tcp_thread():
    while True:
        global state_c,state_m,angle_m,t_buffer,HOST,PORT,BUFFER_SIZE,ask,TNHOST,TNPORT,state_net

        # 创建一个TCP套接字
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.bind((HOST, PORT))  # 绑定地址和端口
        server_socket.listen(1)  # 开始监听连接请求

        print(f'等待客户端连接...')

        # 等待客户端连接
        client_socket, client_address = server_socket.accept()
        #进行telnet连接

        print(f'已连接 {client_address}')
        state_net=1

        while True:
            
            # 接收客户端数据
            data = client_socket.recv(BUFFER_SIZE)  
            if not data:
                break
            else:
                mess_i=data.decode()
            
            if mess_i[0]=='t':
                t_buffer=mess_i[1:]
                print("[info]收到数据")
                print("t"+t_buffer[0])
                continue
            
            if ask!=0:
                mess_o="a"+str(ask)
                client_socket.send(mess_o.encode())
            
            # 根据字段处理服务
            if mess_i[0]=='m':
                mess_o="m"+str(state_m)+str(angle_m)
                
                ret=client_socket.send(mess_o.encode())
            elif mess_i[0]=='a':
                print(mess_i[1:])
            else :
                client_socket.send("x".encode())
            
            #写入tcp.txt
            writeline("t"+t_buffer,t_buffer[0])
                
        client_socket.close()
        server_socket.close()
        state_net=0
        print("disconnect")
        
def input_thread():
    global state_net
    while True:
        if state_net==0:
            time.sleep(0.5)
            continue
        #keyboard.wait('enter')
        time.sleep(0.5)
        command=input(">>")
        if command=="":
            continue
        commanding(command)
        



# 创建 Telnet 连接
os.system('cls')
tn = telnetlib.Telnet(TNHOST, TNPORT)
tn.read_until(b"login:")
tn.write(USERNAME.encode('ascii') + b"\n")
time.sleep(1)
tn.write(b"\n")
tn.read_until(b"#")

input_thread = threading.Thread(target=input_thread)
tcp_thread = threading.Thread(target=tcp_thread)
input_thread.start()
tcp_thread.start()
