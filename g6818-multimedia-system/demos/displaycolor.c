#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
int main()
{
    int color[800 * 480] = {0};
    for (int i = 0; i < 800 * 480; i++)
    {
        color[i] = 0xff2345;
    }
    int lcd_fd = open("/dev/fb0", O_RDWR);
    if (lcd_fd == -1)
    {
        printf("open lcd fail\n");
        return -1;
    }
    write(lcd_fd, color, 800 * 480 * 4);

    close(lcd_fd);
}