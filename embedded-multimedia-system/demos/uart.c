#include <sys/types.h>          /* See NOTES */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>


//串口初始化的函数 参数是串口设备的文件描述符
void init_tty(int fd)
{    
    //声明设置串口的结构体
    struct termios termios_new;
    //先清空该结构体
    bzero( &termios_new, sizeof(termios_new));
    //	cfmakeraw()设置终端属性，就是设置termios结构中的各个参数。
    cfmakeraw(&termios_new);
    //设置波特率
    //termios_new.c_cflag=(B9600);
    cfsetispeed(&termios_new, B9600);
    cfsetospeed(&termios_new, B9600);
    //CLOCAL和CREAD分别用于本地连接和接受使能，因此，首先要通过位掩码的方式激活这两个选项。    
    termios_new.c_cflag |= CLOCAL | CREAD;
    //通过掩码设置数据位为8位
    termios_new.c_cflag &= ~CSIZE;
    termios_new.c_cflag |= CS8; 
    //设置无奇偶校验
    termios_new.c_cflag &= ~PARENB;
    //一位停止位
    termios_new.c_cflag &= ~CSTOPB;
    tcflush(fd,TCIFLUSH);
    // 可设置接收字符和等待时间，无特殊要求可以将其设置为0
    termios_new.c_cc[VTIME] = 10;
    termios_new.c_cc[VMIN] = 1;
    // 用于清空输入/输出缓冲区
        tcflush (fd, TCIFLUSH);
    //完成配置后，可以使用以下函数激活串口设置
    if(tcsetattr(fd,TCSANOW,&termios_new) )
        printf("Setting the serial1 failed!\n");
}

int tem = 0;
int humidity=0;
int pressure=0;
int height=0;
/*
    函数:获取光照强度
        get_lux
        获取GY39返回的光照强度
        通过开发板输出 
*/
void get_tem_humidity_pressure()
{
    int tem = 0;
    int humidity=0;
    int pressure=0;
    int height=0;
    //打开GY39连接的那个串口的文件描述符
    int con_fd = open("/dev/ttySAC1",O_RDWR);
    if(con_fd == -1)
    {
        printf("open gy39 fail\n");
        return ;
    }

    //进行串口初始化
    init_tty(con_fd);


    //GY39模块的光强配置指令
    char send_buf2[3] = {0xa5,0x82,0x27};

    //发送指令给模块 
    int ret = write(con_fd,send_buf2,3);
    if(ret != 3)
    {
        sleep(1);
    }
  
    //获取模块回复的数据的数组
    unsigned char recvbuf[15] = {0};
    ret = read(con_fd,recvbuf,15);
    // if(ret != 15)
    // {
    //     printf("read data error\n");
    // }

    //数据的解析
    if(recvbuf[2] == 0x45 && recvbuf[1] == 0x5a && recvbuf[0] == 0x5a)
    {
        // lux = recvbuf[4]<<24|recvbuf[5]<<16|recvbuf[6]<<8|recvbuf[7];
        tem=recvbuf[4]<<8|recvbuf[5];
        tem=tem/100;
        printf("tem ==== %d\n",tem);

        pressure=recvbuf[6]<<24|recvbuf[7]<<16|recvbuf[8]<<8|recvbuf[9];
        pressure=pressure/100;
        printf("pressure ==== %d pa\n",pressure);

        humidity=recvbuf[10]<<8|recvbuf[11];
        humidity=humidity/100;
        printf("humidity ==== %d%%\n",humidity);

        height=recvbuf[12]<<8|recvbuf[13];
        printf("height ==== %d\n",height);

    }
    
}


// int main()
// {
//     while(1)
//     {
//         get_tem_humidity_pressure();
//         sleep(2);
//     }
//     return 0;
// // }