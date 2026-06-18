#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include<stdio.h>
#include <stdlib.h>
int *plcd=NULL;
void lcd_draw_point(int x,int y,int color)
{
    if(x >= 0 && x < 800 && y >= 0 && y < 480)
    {
        *(plcd + 800*y + x) = color;
    }
}
int main()
{
    int lcd_fd = open("/dev/fb0",O_RDWR);
    if(lcd_fd == -1)
    {
        printf("open lcd fail\n");
        return -1;
    }
    plcd=mmap(NULL,800*480*4,PROT_READ|PROT_WRITE, MAP_SHARED,lcd_fd,0);

     for(int i=0;i<480;i++)
    {
        for(int j=0;j<800;j++)
        {
           
            lcd_draw_point(j,i,0xff0000);
        }
    }
        
    int bmp_fd = open("./12.bmp",O_RDWR);
    if(bmp_fd == -1)
    {
        printf("open bmp fail\n");
        return -1;
    }
    int width;
    lseek(bmp_fd,0x12,SEEK_SET);
    read(bmp_fd,&width,4); 
    
    int height;
    lseek(bmp_fd,0x16,SEEK_SET);
    read(bmp_fd,&height,4);

    short depth;
    lseek(bmp_fd,0x1c,SEEK_SET);
    read(bmp_fd,&depth,2);

    int line_valid_bytes;//每一行的有效字节数
    int line_bytes;//每一行的实际字节数 = 有效字节数 + 赖子数
    int total_bytes;//整个总字节数
    int laizi = 0;//每一行需要填充的字节数
            
            //求每一行的有效字节数
    line_valid_bytes = abs(width)*(depth/8);
            
            //判断有效字节数是否为4的整数倍
    if(line_valid_bytes % 4)
    {
        laizi = 4 - line_valid_bytes%4;
    }
            // 每一行的实际字节数
    line_bytes = line_valid_bytes + laizi;
            
            //像素数组的总字节数
    total_bytes = line_bytes * abs(height);
            
    unsigned char *piexls = malloc(total_bytes);
            
            //把数据读取过来
    lseek(bmp_fd,0x36,SEEK_SET);
    read(bmp_fd,piexls,total_bytes);

    unsigned char a,r,g,b;//依次来获取每个颜色分量值 以字节为单位来获取
    int color;//每次像素点的颜色分量值
    int i = 0;//变量像素数组
    int x,y;
    for(y=0;y<abs(height);y++)     
    {
        for(x = 0;x < abs(width);x++)
        {
                        //依次保存每个像素点的颜色分量值
            b = piexls[i++];
            g = piexls[i++];
            r = piexls[i++];
            if(depth == 32)
            {
                a = piexls[i++];
            }
            else
            {
                a = 0;
            }
                        //合成一个像素的颜色值
            color = a<<24|r<<16|g<<8|b;
                        
                        //调用画点函数,确定在哪个位置进行上色
            lcd_draw_point(width>0? x:abs(width)-1-x,height>0? 
abs(height)-1-y:y,color);
        }
        i += laizi;//跳过每一行最后面的赖子数
    }
    free(piexls);
    munmap(plcd,800*480*4);
    close(bmp_fd);
    close(lcd_fd);
    return 0;
}