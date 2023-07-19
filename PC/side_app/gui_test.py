import tkinter as tk
from PIL import ImageTk, Image, ImageDraw
import stitching
import cv2
import os
import numpy as np

class GUI:
    def __init__(self, root):
        self.root = root
        self.root.title("图片展示程序")
        var = tk.StringVar()        
        # 创建左边的控制栏
        control_frame = tk.Frame(root)
        control_frame.pack(side=tk.LEFT, padx=10, pady=10)
        
        self.txt_frame = tk.Frame(root)
        self.txtlabel = tk.Label(self.txt_frame,textvariable=var,justify='left')
        self.txtlabel.pack(side='top')
        
        self.button1 = tk.Button(control_frame, text="解码检测数据", command=self.decode)
        self.button1.pack(pady=5)
        
        self.button1 = tk.Button(control_frame, text="检测结果展示", command=self.display_photos_1_2_3)
        self.button1.pack(pady=5)
        
        self.button2 = tk.Button(control_frame, text="全景监控生成", command=self.generate_panorama)
        self.button2.pack(pady=5)

        self.button2 = tk.Button(control_frame, text="积水图例生成", command=self.vertical_view)
        self.button2.pack(pady=5)
                
        # 创建右边的展示区
        self.display_frame = tk.Frame(root)
        self.display_frame.pack(side=tk.RIGHT, padx=10, pady=10)
        
        self.image_label = tk.Label(self.display_frame)
        self.image_label.pack()
        
        self.display_label = tk.Label(self.display_frame)
        self.display_label.pack()
        
        # self.text_box = tk.Text(control_frame, height=10, width=30)
        # self.text_box.pack(pady=5)
        
        bg_image = Image.open("./bg.png")  # 替换为背景图片的文件路径
        bg_image = bg_image.resize((700, 400))  # 调整图片尺寸
        bg_photo = ImageTk.PhotoImage(bg_image)
        self.display_label.configure(image=bg_photo)
        self.display_label.image = bg_photo
    
    def state_display(self,string):
        var = tk.StringVar()
        self.txtlabel = tk.Label(self.display_frame,textvariable=var,justify='center')
        self.txtlabel.pack(side='right')
        var.set(string)
        self.txt_frame.pack(padx=0, pady=0)
    
    def decode_file(self):
        second_chars = []
        third_chars = []
        axis = []
        a=[]
        filename="tcp.txt"
        # 打开文件并读取前三行数据
        with open(filename, 'r') as file:
            lines = file.readlines()[:6]
        # 遍历每行数据
        for line in lines:
            line = line.strip()  # 去除换行符和空白字符
            words = [char for char in line]
            if len(words) == 3:
                # second_char.append(words[1])  # 将第二个字符添加到第二个数组中
                second_char = words[1][0]
                if second_char == "1":
                    second_chars.append("left perspective")
                elif second_char == "2":
                    second_chars.append("center perspective")
                elif second_char == "3":
                    second_chars.append("right perspective")
            
                third_char = words[2][0]
                if third_char == "0":
                    third_chars.append("classify:noponding")
                elif third_char == "1":
                    third_chars.append("classify:ponding")
                else:
                    third_chars.append(third_char)
            else:
                axis.append(line[2:])
        return second_chars, third_chars, axis
        
    def decode(self):
        self.clear_display()
        self.state_display("解码成功！")
        second_chars,third_chars, axis=self.decode_file()
        for i in range(len(second_chars)):
            a=(second_chars[i],'\n',third_chars[i],",\ndetect obj:\n",axis[i])
            a=''.join(a)
            self.state_display(a)
            
    def display_photos_1_2_3(self):
        self.clear_display()  # 清空展示区
        str1=("相机去畸变完成！\n")
        str2=("...\n检测目标框定完成! \n")

        #图片加框
        boxes = []
        temp=[]
        # a,b,detect_result = self.decode_file()
        second_chars,third_chars, detect_result=self.decode_file() 
        print('here',detect_result)
        for box_str in detect_result:
            box = eval(box_str)
            boxes.append(box)
        image_paths = ["left_pre.jpg", "center_pre.jpg", "right_pre.jpg"]
        for i in range(len(image_paths)):
            image_path = image_paths[i]
            image = Image.open(image_path).convert('RGBA')
            transp = Image.new('RGBA',image.size,(0,0,0,0))
            draw = ImageDraw.Draw(transp,'RGBA')
            x1, y1 = boxes[i][0]
            x2, y2 = boxes[i][3]
            color = (34, 114, 210, 128)
            draw.rectangle([x1, y1, x2, y2], outline="purple", width=3, fill=color)
            image.paste(Image.alpha_composite(image,transp))
            image.convert('RGB')
            filename_str=['left_boxed','center_boxed','right_boxed']
            # result_path = f"result_{i+1}.png"
            result_path = f"{filename_str[i]}.png"
            image.save(result_path)
            temp.append(f"Saved result image at {filename_str[i]}.png")
            print(temp[i])
        str3='\n'.join(temp)
        str=''.join([str1,str2,str3])
        self.state_display(str)
        # 在展示区显示照片1
        image1 = Image.open("left_boxed.png")  # 替换为照片1的文件路径
        print('here!',image1.size)
        image1 = image1.resize((250, 150))  # 调整照片大小
        photo1 = ImageTk.PhotoImage(image1)
        label1 = tk.Label(self.display_frame, image=photo1)
        label1.image = photo1
        label1.pack(side=tk.RIGHT)
        
        # 在展示区显示照片2
        image2 = Image.open("center_boxed.png")  # 替换为照片2的文件路径
        image2 = image2.resize((250, 150))  # 调整照片大小
        photo2 = ImageTk.PhotoImage(image2)
        label2 = tk.Label(self.display_frame, image=photo2)
        label2.image = photo2
        label2.pack(side=tk.LEFT)
        
        # 在展示区显示照片3
        image3 = Image.open("right_boxed.png")  # 替换为照片3的文件路径
        image3 = image3.resize((250, 150))  # 调整照片大小
        photo3 = ImageTk.PhotoImage(image3)
        label3 = tk.Label(self.display_frame, image=photo3)
        label3.image = photo3
        label3.pack(side=tk.LEFT)
        
    def generate_panorama(self):
        self.clear_display()  # 清空展示区
        # 图片路径列表
        imgs=["left_pre.jpg","center_pre.jpg"]
        stitcher = stitching.Stitcher()
        panorama = stitcher.stitch(imgs)
        cv2.imwrite("cat.png", panorama)

        imgs=["center_pre.jpg","right_pre.jpg"]
        stitcher = stitching.Stitcher()
        panorama = stitcher.stitch(imgs)
        cv2.imwrite("cat2.png", panorama)

        if os.path.exists("cat.png") and os.path.exists("cat2.png"):
            imgs=["cat.png","cat2.png"]
            stitcher = stitching.Stitcher()
            panorama = stitcher.stitch(imgs)
            cv2.imwrite("panorama.png", panorama)
        else:
            print("panorama generate fail")
        image2 = Image.open("panorama.png")  # 替换为照片2的文件路径
        image2 = image2.resize((1078, 320))  # 调整照片大小
        photo2 = ImageTk.PhotoImage(image2)
        label2 = tk.Label(self.display_frame, image=photo2)
        label2.image = photo2
        label2.pack(side=tk.LEFT)
        self.state_display('panorama生成成功!')
        os.remove('cat.png')
        os.remove('cat2.png')
    
    def vertical_view(self):
        self.clear_display()  # 清空展示区
        str1=("转换俯视坐标系\n......\n积水图例生成成功! ")
        img=cv2.imread('left_boxed.png')
        width,height=500,800
        pts1 = np.float32([[819, 411], [1145, 404], [397, 543], [1065, 573]])
        pts2=np.float32([[0,0],[width,0],[0,height],[width,height]])
        matrix=cv2.getPerspectiveTransform(pts1,pts2)
        img2=cv2.warpPerspective(img,matrix,(width,height))
        cv2.imwrite('1.jpg',img2)

        img=cv2.imread('center_boxed.png')
        pts1 = np.float32([[361, 402], [606, 406], [260, 583], [752, 588]])
        matrix=cv2.getPerspectiveTransform(pts1,pts2)
        img2=cv2.warpPerspective(img,matrix,(width,height))
        cv2.imwrite('2.jpg',img2)

        img=cv2.imread('right_boxed.png')
        pts1 = np.float32([[3, 264], [335, 269], [5, 389], [679, 333]])
        matrix=cv2.getPerspectiveTransform(pts1,pts2)
        img2=cv2.warpPerspective(img,matrix,(width,height))
        cv2.imwrite('3.jpg',img2)
        # print('nihao',type(img2))
        self.image_paths = ["1.jpg", "2.jpg", "3.jpg"]
        images = [Image.open(image_path) for image_path in self.image_paths[:3]]

        # images=['left_boxed.png','center_boxed.png','right_boxed.png']
        # 缩小图片到0.2倍
        resized_images = [image.resize((int(image.width * 0.2), int(image.height * 0.2)), Image.ANTIALIAS) for image in images]
        # 计算拼接后图片的大小
        width = sum([image.width for image in resized_images])
        height = max([image.height for image in resized_images])
        # 创建新图片对象
        concatenated_image = Image.new("RGB", (width, height))
        
        # 拼接图片
        x_offset = 0
        for image in resized_images:
            concatenated_image.paste(image, (x_offset, 0))
            x_offset += image.width
        concatenated_image = concatenated_image.resize((750, 400))  # 调整照片大小
        # print(type(concatenated_image))
        concatenated_image.save('积水图例.png')
        photo2 = ImageTk.PhotoImage(concatenated_image)
        label2 = tk.Label(self.display_frame, image=photo2)
        label2.image = photo2
        label2.pack(side=tk.LEFT)
        for i in range(len(self.image_paths)):
            os.remove(self.image_paths[i])
        self.state_display('积水图例生成成功！')

    
    def clear_display(self):
        for widget in self.display_frame.winfo_children():
            widget.destroy()
        
if __name__ == "__main__":
    root = tk.Tk()
    gui = GUI(root)
    root.mainloop()
