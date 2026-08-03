#include <linux/input.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include<stdio.h>
#include"touch.h"
int get_dir()
{
    //1.打开触摸屏的设备文件 
    int tc_fd = open("/dev/input/event0",O_RDWR);
    //加出错判断
     if(tc_fd==-1)
    {
        printf("open tc fail");
        return -1;
    }
    //定义一个输入事件结构体变量 来每次获取输入设备的信息
    struct input_event ev;

    int x=-1,y=-1;
    int x1=0,y=0;
                
    //一直获取输入设备的信息
    while(1)
    {
        //把输入设备事件读取过来
        read(tc_fd,&ev,sizeof(ev));
                    
        //对事件进行解析 获取x和y的坐标值
                    
        //获取x的坐标值
        if(ev.type==EV_ABS && ev.code==ABS_X)
        {
            if(x==-1)
            {
                x = ev.value;
            }
            x1 = ev.value;
        }
        //获取y的坐标值
        if(ev.type==EV_ABS && ev.code==ABS_Y)
        {
            if(y==-1)
            {
                y = ev.value;
            }
            y1 = ev.value;
        }
       //找到一个退出的条件
        //手指按下获取坐标 手指离开 退出获取坐标
        if(ev.type==EV_KEY && ev.code == BTN_TOUCH && ev.value==0)
        {
            break;
        }
    }
    close(tc_fd);
    if(abs(x1-x)>abs(y1-y))
    {
        if(x1>x)
        {
            return RIGHT;
        }
        else
        {
            return LEFT;
        }
    }
    else
    {
        if(y1>y)
        {
            return DOWN;
        }
        else
        {
            return UP;
        }

    }
}