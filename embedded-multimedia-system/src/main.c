/**
 * G6818 嵌入式多媒体系统 — 主入口
 *
 * 启动后显示触摸主菜单，点击对应磁贴进入功能：
 *   0 — 视频播放器（触摸控制）
 *   1 — 电子相册（触摸滑动浏览）
 * 任一功能内点击左上角「返回」即可回到主菜单（单二进制支持全部功能）。
 */

#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include "lcd.h"
#include "bmp.h"
#include "touch.h"
#include "mplayer.h"

/* 返回主菜单请求标志（定义于此，touch.h 中 extern 声明） */
int g_request_menu = 0;

/* ── 主菜单 ────────────────────────────────────── */
#define MENU_N    2
static const int tile_x[MENU_N] = { 20, 420 };
static const int tile_y[MENU_N] = { 40,  40 };
#define TILE_W 360
#define TILE_H 400

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

/* 显示主菜单并等待选择，返回功能索引 0~1 */
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
        default: break;
        }
    }

    lcd_close();
    return 0;
}
