/**
 * BMP 图片解码与显示 — G6818
 *
 * 支持 24-bit / 32-bit 色深，自动处理行对齐（4 字节边界）
 */

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "lcd.h"
#include "bmp.h"

/*
 * 显示单张 BMP 图片
 *   x0, y0: 左上角 LCD 坐标
 *   path:   BMP 文件路径
 */
void disbmp(int x0, int y0, char *path)
{
    int bmp_fd = open(path, O_RDONLY);
    if (bmp_fd == -1) {
        printf("open bmp fail: %s\n", path);
        return;
    }

    /* 读取 BMP 头信息 */
    int width, height;
    lseek(bmp_fd, 0x12, SEEK_SET);
    read(bmp_fd, &width,  4);
    lseek(bmp_fd, 0x16, SEEK_SET);
    read(bmp_fd, &height, 4);

    short depth;
    lseek(bmp_fd, 0x1c, SEEK_SET);
    read(bmp_fd, &depth,  2);

    /* 计算行字节数（含 4 字节对齐填充） */
    int line_valid_bytes = abs(width) * (depth / 8);
    int laizi = 0;
    if (line_valid_bytes % 4)
        laizi = 4 - line_valid_bytes % 4;
    int line_bytes  = line_valid_bytes + laizi;
    int total_bytes = line_bytes * abs(height);

    /* 读取像素数据 */
    unsigned char *pixels = malloc(total_bytes);
    lseek(bmp_fd, 0x36, SEEK_SET);
    read(bmp_fd, pixels, total_bytes);
    close(bmp_fd);

    /* 逐像素绘制 */
    unsigned char a, r, g, b;
    int color;
    int i = 0;
    int x, y;

    for (y = 0; y < abs(height); y++) {
        for (x = 0; x < abs(width); x++) {
            b = pixels[i++];
            g = pixels[i++];
            r = pixels[i++];
            a = (depth == 32) ? pixels[i++] : 0;

            color = (a << 24) | (r << 16) | (g << 8) | b;

            /* BMP 行序从底到顶，width>0 时需翻转 Y */
            int draw_x = (width  > 0) ? x + x0 : abs(width)  - 1 - x + x0;
            int draw_y = (height > 0) ? abs(height) - 1 - y + y0 : y + y0;
            lcd_draw_point(draw_x, draw_y, color);
        }
        i += laizi;  /* 跳过行末填充字节 */
    }

    free(pixels);
}

/*
 * 幻灯片模式：依次全屏显示多张 BMP
 *   path[]: 图片路径数组
 *   num:    图片数量
 *   color:  切换间隙的填充色
 */
void dis_all_bmp(int x, int y, char *path[], int num, int color)
{
    for (int i = 0; i < num; i++) {
        disbmp(0, 0, path[i]);
        sleep(2);
        lcd_clear(color);
    }
}
