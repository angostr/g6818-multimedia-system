/**
 * 触摸屏驱动 — G6818 (input 子系统)
 *
 * 设备节点: /dev/input/event0
 * 触摸坐标: 1000×600 (原始) → 800×480 (LCD)
 */

#include <linux/input.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "lcd.h"
#include "bmp.h"
#include "touch.h"

/* 触摸设备原始分辨率为 1000×600，需映射到 LCD 的 800×480 */
#define TOUCH_RAW_W  1000
#define TOUCH_RAW_H  600

/* ── 按钮区域检测 ──────────────────────────────── */

int isbutton(int x, int y, int button_x, int button_y, int r)
{
    x = x * LCD_WIDTH  / TOUCH_RAW_W;
    y = y * LCD_HEIGHT / TOUCH_RAW_H;
    x = x - button_x;
    y = y - button_y;
    return (x * x + y * y) < (r * r);
}

/* ── 触摸滑动方向检测 ────────────────────────────
 * 从已打开的触摸设备 fd 读取一次完整手势，返回方向：
 *   UP / DOWN / LEFT / RIGHT / FLAG（中心点击）/ -1（错误）
 * 由调用方负责打开 / 关闭 fd，避免重复 open 同一设备造成事件分流。
 */
int get_swipe_direction(int tc_fd)
{
    if (tc_fd < 0) {
        printf("invalid touch fd\n");
        return -1;
    }

    struct input_event ev;
    int x = -1, y = -1;
    int x1 = 0, y1 = 0;

    while (1) {
        ssize_t n = read(tc_fd, &ev, sizeof(ev));
        if (n != (ssize_t)sizeof(ev)) {
            if (n < 0) { perror("read touch failed"); return -1; }
            continue;               /* 短读，重试 */
        }

        if (ev.type == EV_ABS && ev.code == ABS_X) {
            if (x == -1) x = ev.value;
            x1 = ev.value;
        }
        if (ev.type == EV_ABS && ev.code == ABS_Y) {
            if (y == -1) y = ev.value;
            y1 = ev.value;
        }
        /* 手指抬起 → 退出 */
        if (ev.type == EV_KEY && ev.code == BTN_TOUCH && ev.value == 0)
            break;
    }

    /* 判断中心按钮点击 */
    if (isbutton(x, y, 400, 460, 50))
        return FLAG;

    /* 判断滑动方向 */
    if (abs(x1 - x) > abs(y1 - y))
        return (x1 > x) ? RIGHT : LEFT;
    else
        return (y1 > y) ? DOWN  : UP;
}

/* ── 电子相册 ──────────────────────────────────── */

void photo_album(char *pathname[])
{
    int tc_fd = open("/dev/input/event0", O_RDWR);
    if (tc_fd == -1) {
        printf("open touch device fail\n");
        return;
    }

    int result = 0;
    int sum    = 0;
    int num    = 4;

    while (1) {
        lcd_draw_circle(400, 240, 50, 0x000000);
        result = get_swipe_direction(tc_fd);
        lcd_clear(0xff0000);

        if (result == UP || result == LEFT) {
            sum--;
            if (sum == -1) sum = num - 1;
            bmp_display(400, 0, pathname[sum]);
        }
        if (result == DOWN || result == RIGHT) {
            sum++;
            if (sum == num) sum = 0;
            bmp_display(400, 0, pathname[sum]);
        }
        if (result == FLAG) {
            bmp_slideshow(400, 0, pathname, num, 0xff0000);
        }
    }

    close(tc_fd);
}
