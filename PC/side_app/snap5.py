import cv2
import time

t_buffer=[0]
def rtsp_monitor():
    # RTSP流媒体地址
    global t_buffer
    t_buffer_n=0
    rtsp_url = "stream_chn1.mp4"
    

    # 创建视频捕获对象
    cap = cv2.VideoCapture(rtsp_url)
    
    
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
        while not time.time()>=t+c*(1/22):
            cv2.waitKey(1)

        # 按下'q'键截图
            
            try:
                cv2.getWindowProperty("RTSP Stream",0)

            
            except:
                print("[info]rtsp监视器已关闭")
                cap.release()
                cv2.destroyAllWindows()
                return
        
        cv2.imwrite("frame"+str(t_buffer[0])+".jpg", frame)
            
        t_buffer_n=t_buffer[0]
        
        

if __name__ == "__main__":
    rtsp_monitor()
