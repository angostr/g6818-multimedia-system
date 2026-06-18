#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include<stdio.h>
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
    int x1=0;
    int y1=0;
    plcd=mmap(NULL,800*480*4,PROT_READ|PROT_WRITE, MAP_SHARED,lcd_fd,0);
    for(int i=0;i<480;i++)
    {
        for(int j=0;j<800;j++)
        {
            if(j<400&&j>=200&&i<200&&i>=100)
            {
                lcd_draw_point(j,i,0xff0000);
            }
            else
            {
                x1=j-600;
                y1=i-320;
                if(x1*x1+y1*y1<=400)
                {
                    lcd_draw_point(j,i,0x0000ff);
                }
                else
                {
                    lcd_draw_point(j,i,0x000000);
                }
            }
        }
    }
    munmap(plcd,800*480*4);
    close(lcd_fd);
    return 0;
}