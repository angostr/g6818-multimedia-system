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

/* ── 按钮区域检测 ──────────────────────────────── */

int isbutton(int x, int y, int button_x, int button_y, int r)
{
    x = x * 800 / 1000;
    y = y * 480 / 600;
    x = x - button_x;
    y = y - button_y;
    return (x * x + y * y) < (r * r);
}

/* ── 触摸滑动方向检测 ──────────────────────────── */

int get_dir_cao(void)
{
    int tc_fd = open("/dev/input/event0", O_RDWR);
    if (tc_fd == -1) {
        printf("open tc fail\n");
        return -1;
    }

    struct input_event ev;
    int x = -1, y = -1;
    int x1 = 0, y1 = 0;

    while (1) {
        read(tc_fd, &ev, sizeof(ev));

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
    close(tc_fd);

    /* 判断中心按钮点击 */
    if (isbutton(x, y, 400, 460, 50))
        return FLAG;

    /* 判断滑动方向 */
    if (abs(x1 - x) > abs(y1 - y))
        return (x1 > x) ? RIGHT : LEFT;
    else
        return (y1 > y) ? DOWN  : UP;
}

/* ── 兼容别名（旧代码调用 get_dir） ────────────── */

int get_dir(void)
{
    return get_dir_cao();
}

/* ── 电子相册 ──────────────────────────────────── */

void elec_photo_album(char *pathname[])
{
    lcd__init__();
    int result = 0;
    int sum    = 0;
    int num    = 4;

    while (1) {
        lcd_draw_circle(400, 240, 50, 0x000000);
        result = get_dir_cao();
        lcd_clear(0xff0000);

        if (result == UP || result == LEFT) {
            sum--;
            if (sum == -1) sum = num - 1;
            disbmp(400, 0, pathname[sum]);
        }
        if (result == DOWN || result == RIGHT) {
            sum++;
            if (sum == num) sum = 0;
            disbmp(400, 0, pathname[sum]);
        }
        if (result == FLAG) {
            dis_all_bmp(400, 0, pathname, num, 0xff0000);
        }
    }
}
