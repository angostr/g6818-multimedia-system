/**
 * BMP 图片解码与显示 — G6818
 *
 * 支持 24-bit / 32-bit 色深，自动处理行对齐（4 字节边界）。
 * 解析基于 BITMAPINFOHEADER（40 字节头，像素数据偏移 0x36）。
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
void bmp_display(int x0, int y0, char *path)
{
    int bmp_fd = open(path, O_RDONLY);
    if (bmp_fd == -1) {
        fprintf(stderr, "open bmp fail: %s\n", path);
        return;
    }

    /* 读取 BMP 头信息 */
    int width = 0, height = 0;
    short depth = 0;

    if (lseek(bmp_fd, 0x12, SEEK_SET) == (off_t)-1 ||
        read(bmp_fd, &width,  4) != 4 ||
        lseek(bmp_fd, 0x16, SEEK_SET) == (off_t)-1 ||
        read(bmp_fd, &height, 4) != 4 ||
        lseek(bmp_fd, 0x1c, SEEK_SET) == (off_t)-1 ||
        read(bmp_fd, &depth,  2) != 2) {
        fprintf(stderr, "read bmp header fail: %s\n", path);
        close(bmp_fd);
        return;
    }

    int aw = abs(width);
    int ah = abs(height);
    if (aw == 0 || ah == 0 || (depth != 24 && depth != 32)) {
        fprintf(stderr, "unsupported bmp %s: %dx%d depth=%d\n", path, aw, ah, depth);
        close(bmp_fd);
        return;
    }

    /* 计算行字节数（含 4 字节对齐填充） */
    int line_valid_bytes = aw * (depth / 8);
    int row_padding = (line_valid_bytes % 4) ? (4 - line_valid_bytes % 4) : 0;
    int line_bytes  = line_valid_bytes + row_padding;
    long total_bytes = (long)line_bytes * ah;

    unsigned char *pixels = malloc(total_bytes);
    if (!pixels) {
        fprintf(stderr, "malloc fail for bmp: %s\n", path);
        close(bmp_fd);
        return;
    }

    if (lseek(bmp_fd, 0x36, SEEK_SET) == (off_t)-1 ||
        read(bmp_fd, pixels, total_bytes) != total_bytes) {
        fprintf(stderr, "read bmp pixel fail: %s\n", path);
        free(pixels);
        close(bmp_fd);
        return;
    }
    close(bmp_fd);

    /* 逐像素绘制 */
    unsigned char a, r, g, b;
    int color;
    int i = 0;
    int x, y;

    for (y = 0; y < ah; y++) {
        for (x = 0; x < aw; x++) {
            b = pixels[i++];
            g = pixels[i++];
            r = pixels[i++];
            a = (depth == 32) ? pixels[i++] : 0;

            color = (a << 24) | (r << 16) | (g << 8) | b;

            /* BMP 行序从底到顶，width>0 时需翻转 Y */
            int draw_x = (width  > 0) ? x + x0 : aw - 1 - x + x0;
            int draw_y = (height > 0) ? ah - 1 - y + y0 : y + y0;
            lcd_draw_point(draw_x, draw_y, color);
        }
        i += row_padding;  /* 跳过行末填充字节 */
    }

    lcd_flush();        /* 将后备缓冲区刷到 framebuffer，使图片可见 */
    free(pixels);
}

/*
 * 幻灯片模式：依次全屏显示多张 BMP
 *   path[]: 图片路径数组
 *   num:    图片数量
 *   color:  切换间隙的填充色
 */
void bmp_slideshow(int x, int y, char *path[], int num, int color)
{
    for (int i = 0; i < num; i++) {
        bmp_display(0, 0, path[i]);   /* 内部已 lcd_flush */
        sleep(2);
        lcd_clear(color);
        lcd_flush();                  /* 清屏生效 */
    }
}
