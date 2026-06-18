/**
 * LCD 显示驱动 — G6818 Framebuffer 底层操作
 *
 * 分辨率: 800 × 480, 32-bit (ARGB)
 * 设备节点: /dev/fb0
 */

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "num.h"

static int   lcd_fd = -1;
static int  *plcd   = NULL;

/* ── 初始化 / 关闭 ─────────────────────────────── */

void lcd__init__(void)
{
    lcd_fd = open("/dev/fb0", O_RDWR);
    if (lcd_fd == -1) {
        printf("open lcd fail\n");
        return;
    }
    plcd = mmap(NULL, 800 * 480 * 4, PROT_READ | PROT_WRITE,
                MAP_SHARED, lcd_fd, 0);
}

void lcd_close(void)
{
    munmap(plcd, 800 * 480 * 4);
    close(lcd_fd);
}

/* ── 基本绘图 ──────────────────────────────────── */

void lcd_clear(int color)
{
    for (int i = 0; i < 800 * 480; i++)
        *(plcd + i) = color;
}

void lcd_draw_point(int x, int y, int color)
{
    if (x >= 0 && x < 800 && y >= 0 && y < 480)
        *(plcd + 800 * y + x) = color;
}

void lcd_draw_rect(int x, int y, int w, int h, int color)
{
    for (int i = y; i <= y + h; i++)
        for (int j = x; j <= x + w; j++)
            lcd_draw_point(j, i, color);
}

void lcd_draw_circle(int x, int y, int r, int color)
{
    for (int i = 0; i <= 480; i++)
        for (int j = 0; j <= 800; j++)
            if ((j - x) * (j - x) + (i - y) * (i - y) <= r * r)
                lcd_draw_point(j, i, color);
}

/* ── 点阵字模显示 ──────────────────────────────── */

void lcd_draw_word(int x, int y, int w, int h, unsigned char data[], int color)
{
    int i, j;
    for (i = 0; i < w * h / 8; i++) {
        for (j = 7; j >= 0; j--) {
            if ((data[i] >> j) & 1) {
                int x0 = x + i % (w / 8) * 8 + (7 - j);
                int y0 = y + i / (w / 8);
                lcd_draw_point(x0, y0, color);
            }
        }
    }
}

void lcd_draw_num(int x, int y, int w, int h, int color, int s)
{
    char num[10] = {0};
    sprintf(num, "%d", s);
    for (int i = 0; num[i] != 0; i++)
        lcd_draw_word(x + i * w, y, w, h, numinfo[num[i] - '0'], color);
}

/* ── Alpha 混合 ─────────────────────────────────── */

void lcd_draw_point_alpha(int x, int y, int color)
{
    if (x < 0 || x >= 800 || y < 0 || y >= 480) return;

    int alpha = (color >> 24) & 0xFF;
    if (alpha == 0)   return;
    if (alpha == 255) { *(plcd + 800 * y + x) = color; return; }

    int bg      = *(plcd + 800 * y + x);
    int bg_r    = (bg >> 16) & 0xFF;
    int bg_g    = (bg >> 8)  & 0xFF;
    int bg_b    =  bg        & 0xFF;
    int fg_r    = (color >> 16) & 0xFF;
    int fg_g    = (color >> 8)  & 0xFF;
    int fg_b    =  color        & 0xFF;

    int out_r   = (alpha * fg_r + (255 - alpha) * bg_r) / 255;
    int out_g   = (alpha * fg_g + (255 - alpha) * bg_g) / 255;
    int out_b   = (alpha * fg_b + (255 - alpha) * bg_b) / 255;

    *(plcd + 800 * y + x) = (out_r << 16) | (out_g << 8) | out_b;
}

void lcd_fill_rect(int x, int y, int width, int height, int color)
{
    for (int row = 0; row < height; row++)
        for (int col = 0; col < width; col++)
            lcd_draw_point_alpha(x + col, y + row, color);
}

/* ── 辅助函数 ──────────────────────────────────── */

/*
 * 采样指定像素行的平均颜色（可用于自适应 UI 背景色）
 * 当前未使用，保留作为工具函数。
 */
static int sample_row_avg(int row)
{
    int r = 0, g = 0, b = 0;
    int *p = plcd + 800 * row;
    for (int x = 0; x < 800; x++) {
        int px = p[x];
        r += (px >> 16) & 0xFF;
        g += (px >> 8)  & 0xFF;
        b +=  px        & 0xFF;
    }
    return (r / 800 << 16) | (g / 800 << 8) | (b / 800);
}
