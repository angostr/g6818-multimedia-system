/**
 * LCD 显示驱动 — GE6818 Framebuffer 底层操作
 *
 * 分辨率: 800 × 480, 32-bit (ARGB)
 * 设备节点: /dev/fb0
 */

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "num.h"

static int   lcd_fd   = -1;
static int  *plcd     = NULL;   /* mmap → /dev/fb0 */
static int  *backbuf  = NULL;   /* 双缓冲后备缓冲区 */

#define FB_SIZE (LCD_WIDTH * LCD_HEIGHT * 4)

/* ── 初始化 / 关闭 ─────────────────────────────── */

int lcd_init(void)
{
    lcd_fd = open("/dev/fb0", O_RDWR);
    if (lcd_fd == -1) {
        perror("open /dev/fb0 failed");
        return -1;
    }

    plcd = mmap(NULL, FB_SIZE, PROT_READ | PROT_WRITE,
                MAP_SHARED, lcd_fd, 0);
    if (plcd == MAP_FAILED) {
        perror("mmap /dev/fb0 failed");
        close(lcd_fd);
        lcd_fd = -1;
        return -1;
    }

    backbuf = malloc(FB_SIZE);
    if (!backbuf) {
        perror("malloc backbuf failed");
        munmap(plcd, FB_SIZE);
        close(lcd_fd);
        lcd_fd = -1;
        plcd   = NULL;
        return -1;
    }
    memcpy(backbuf, plcd, FB_SIZE);
    return 0;
}

void lcd_close(void)
{
    if (backbuf) { free(backbuf); backbuf = NULL; }
    if (plcd && plcd != MAP_FAILED) munmap(plcd, FB_SIZE);
    if (lcd_fd != -1) close(lcd_fd);
    lcd_fd = -1;
    plcd   = NULL;
}

/* ── 基本绘图 ──────────────────────────────────── */

void lcd_clear(int color)
{
    if (!backbuf) return;
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++)
        backbuf[i] = color;
}

void lcd_draw_point(int x, int y, int color)
{
    if (!backbuf) return;
    if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT)
        backbuf[LCD_WIDTH * y + x] = color;
}

void lcd_draw_rect(int x, int y, int w, int h, int color)
{
    for (int i = y; i <= y + h; i++)
        for (int j = x; j <= x + w; j++)
            lcd_draw_point(j, i, color);
}

void lcd_draw_circle(int x, int y, int r, int color)
{
    /* 仅遍历圆的外接矩形，避免全屏 800×480 无效遍历（Bresenham 可进一步优化） */
    int x0 = (x - r < 0) ? 0 : x - r;
    int x1 = (x + r >= LCD_WIDTH)  ? LCD_WIDTH  - 1 : x + r;
    int y0 = (y - r < 0) ? 0 : y - r;
    int y1 = (y + r >= LCD_HEIGHT) ? LCD_HEIGHT - 1 : y + r;

    for (int i = y0; i <= y1; i++)
        for (int j = x0; j <= x1; j++)
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
    if (!backbuf) return;
    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT) return;

    int alpha = (color >> 24) & 0xFF;
    if (alpha == 0)   return;
    if (alpha == 255) { backbuf[LCD_WIDTH * y + x] = color; return; }

    int bg      = backbuf[LCD_WIDTH * y + x];
    int bg_r    = (bg >> 16) & 0xFF;
    int bg_g    = (bg >> 8)  & 0xFF;
    int bg_b    =  bg        & 0xFF;
    int fg_r    = (color >> 16) & 0xFF;
    int fg_g    = (color >> 8)  & 0xFF;
    int fg_b    =  color        & 0xFF;

    int out_r   = (alpha * fg_r + (255 - alpha) * bg_r) / 255;
    int out_g   = (alpha * fg_g + (255 - alpha) * bg_g) / 255;
    int out_b   = (alpha * fg_b + (255 - alpha) * bg_b) / 255;

    backbuf[LCD_WIDTH * y + x] = (out_r << 16) | (out_g << 8) | out_b;
}

void lcd_fill_rect(int x, int y, int width, int height, int color)
{
    for (int row = 0; row < height; row++)
        for (int col = 0; col < width; col++)
            lcd_draw_point_alpha(x + col, y + row, color);
}

/* ── 双缓冲接口 ─────────────────────────────────── */

/*
 * lcd_flush: 将后备缓冲区整体复制到显存（消除撕裂）
 */
void lcd_flush(void)
{
    if (plcd && backbuf) memcpy(plcd, backbuf, FB_SIZE);
}

/*
 * lcd_flush_region: 将指定区域从后备缓冲区复制到显存
 */
void lcd_flush_region(int x, int y, int w, int h)
{
    if (!plcd || !backbuf) return;
    for (int row = y; row < y + h && row < LCD_HEIGHT; row++)
        memcpy(plcd + LCD_WIDTH * row + x,
               backbuf + LCD_WIDTH * row + x,
               (w < LCD_WIDTH - x ? w : LCD_WIDTH - x) * 4);
}

/*
 * lcd_sync_region: 将显存中的视频帧内容同步到后备缓冲区
 * （用于视频播放模式下，先捕获 MPlayer 渲染的画面再叠加 UI）
 */
void lcd_sync_region(int x, int y, int w, int h)
{
    if (!plcd || !backbuf) return;
    for (int row = y; row < y + h && row < LCD_HEIGHT; row++)
        memcpy(backbuf + LCD_WIDTH * row + x,
               plcd + LCD_WIDTH * row + x,
               (w < LCD_WIDTH - x ? w : LCD_WIDTH - x) * 4);
}
