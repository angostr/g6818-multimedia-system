/**
 * G6818 嵌入式多媒体系统 — 主入口
 *
 * 启动后显示触摸主菜单，点击对应磁贴进入功能：
 *   0 — 视频播放器（触摸控制）
 *   1 — 电子相册（触摸滑动浏览 / 缩略图）
 *   2 — 传感器数值显示（温 / 湿 / 气压 / 海拔 / 光强）
 *   3 — 传感器联动 LED + 蜂鸣器告警
 * 任一功能内点击左上角「返回」即可回到主菜单（单二进制支持全部功能）。
 */

#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include "lcd.h"
#include "bmp.h"
#include "touch.h"
#include "uart.h"
#include "mplayer.h"

/* 返回主菜单请求标志（定义于此，touch.h 中 extern 声明） */
int g_request_menu = 0;

/* 传感器全局变量（定义在 uart.c） */
extern int lux;
extern int tem;
extern int humidity;
extern int pressure;
extern int height;

/* ── 主菜单 ────────────────────────────────────── */
#define MENU_N    4
static const int tile_x[MENU_N] = { 20, 420,  20, 420 };
static const int tile_y[MENU_N] = { 40,  40, 260, 260 };
#define TILE_W 360
#define TILE_H 200

/* 在 (tx,ty) 处命中的磁贴索引，未命中返回 -1 */
static int menu_tile_at(int tx, int ty)
{
    for (int i = 0; i < MENU_N; i++) {
        if (tx >= tile_x[i] && tx <= tile_x[i] + TILE_W &&
            ty >= tile_y[i] && ty <= tile_y[i] + TILE_H)
            return i;
    }
    return -1;
}

static void draw_menu(int sel)
{
    lcd_clear(0xff0000);
    for (int i = 0; i < MENU_N; i++) {
        int color = (i == sel) ? 0x0000FF : 0x008080;
        lcd_draw_rect(tile_x[i], tile_y[i], TILE_W, TILE_H, color);
        /* 磁贴编号（16×24 点阵，置于左上角） */
        lcd_draw_num(tile_x[i] + 12, tile_y[i] + 12, 16, 24, 0xFFFFFF, i + 1);
    }
    lcd_flush();
}

/* 显示主菜单并等待选择，返回功能索引 0~3 */
static int run_main_menu(void)
{
    int tc_fd = open("/dev/input/event0", O_RDWR);
    if (tc_fd == -1) {
        perror("open touch device fail");
        return 0;
    }

    int sel = 0;
    while (1) {
        draw_menu(sel);
        int tx = 0, ty = 0;
        int g = get_swipe_direction(tc_fd, &tx, &ty);
        if (g == GESTURE_TAP) {
            int idx = menu_tile_at(tx, ty);
            if (idx >= 0) { close(tc_fd); return idx; }
        } else if (g == UP) {
            sel = (sel + MENU_N - 1) % MENU_N;
        } else if (g == DOWN) {
            sel = (sel + 1) % MENU_N;
        }
        /* LEFT / RIGHT / FLAG 在菜单中忽略 */
    }

    close(tc_fd);
    return 0;
}

/* ── 电子相册 ──────────────────────────────────── */
static void run_photo_album(void)
{
    char *pathname[] = {"./12.bmp", "./lb.bmp", "./13.bmp", "./14.bmp"};
    photo_album(pathname);
}

/* ── 传感器模式通用：等待约 1 秒 ───────────────
 * 用 poll 阻塞至多 1 秒：超时则继续刷新；期间若在左上角点击则返回 1（请求回菜单）。
 * 这样既保持原 1 秒刷新节奏，又能在不重新编译的情况下退出传感器模式。 */
static int sensor_wait_exit(int tc_fd)
{
    struct pollfd pfd = { tc_fd, POLLIN, 0 };
    int rc = poll(&pfd, 1, 1000);
    if (rc <= 0) return 0;                 /* 超时 / 无事件 */
    int tx = 0, ty = 0;
    int g = get_swipe_direction(tc_fd, &tx, &ty);
    if (g == GESTURE_TAP && tx < 60 && ty < 40)
        return 1;
    return 0;
}

/* ── 传感器数据展示 ──────────────────────────── */
static void run_sensor_display(void)
{
    int tc_fd = open("/dev/input/event0", O_RDWR);
    if (tc_fd == -1) { perror("open touch fail"); return; }

    while (1) {
        get_tem_humidity_pressure();
        get_lux();

        lcd_clear(0xff0000);
        lcd_draw_rect(0, 0, 60, 40, 0x00AA00);   /* 左上角“返回”按钮 */
        lcd_draw_num(0,   0, 16, 24, 0x000000, tem);
        lcd_draw_num(0,  24, 16, 24, 0x000000, humidity);
        lcd_draw_num(0,  48, 16, 24, 0x000000, pressure);
        lcd_draw_num(0,  72, 16, 24, 0x000000, height);
        lcd_draw_num(0,  96, 16, 24, 0x000000, lux);
        lcd_flush();

        if (sensor_wait_exit(tc_fd)) { close(tc_fd); return; }
    }
}

/* ── 传感器 + LED / 蜂鸣器联动 ────────────────── */
static void run_sensor_with_alert(void)
{
    int tc_fd = open("/dev/input/event0", O_RDWR);
    if (tc_fd == -1) { perror("open touch fail"); return; }

    while (1) {
        get_tem_humidity_pressure();
        get_lux();
        lcd_clear(0xff0000);
        lcd_draw_rect(0, 0, 60, 40, 0x00AA00);   /* 左上角“返回”按钮 */

        LED_ctrl(100, 0);   /* 先关全部 LED */
        LED_ctrl(8,   1);   /* 默认点亮 D8 */

        if (lux > 50) {
            printf("lux more\n");
            LED_ctrl(7, 1);
            beep_ctrl(1);
        } else {
            LED_ctrl(7, 0);
            beep_ctrl(0);
        }

        lcd_draw_num(100,   0, 32, 64, 0x000000, tem);
        lcd_draw_num(100,  64, 32, 64, 0x000000, pressure);
        lcd_draw_num(100, 128, 32, 64, 0x000000, humidity);
        lcd_draw_num(100, 192, 32, 64, 0x000000, lux);
        lcd_flush();

        if (sensor_wait_exit(tc_fd)) { close(tc_fd); return; }
    }
}

/* ── main ──────────────────────────────────────── */
int main(void)
{
    if (lcd_init() != 0) {
        fprintf(stderr, "LCD init failed, exit\n");
        return 1;
    }

    /* 单二进制、运行时菜单：循环显示菜单，选择后进入对应功能，
     * 功能内触发 g_request_menu 即返回菜单，无需重新编译。 */
    while (1) {
        g_request_menu = 0;
        int choice = run_main_menu();
        switch (choice) {
        case 0:  video_player();         break;
        case 1:  run_photo_album();      break;
        case 2:  run_sensor_display();   break;
        case 3:  run_sensor_with_alert();break;
        default: break;
        }
    }

    lcd_close();
    return 0;
}
