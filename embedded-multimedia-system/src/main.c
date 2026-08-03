/**
 * G6818 嵌入式多媒体系统 — 主入口
 *
 * 通过修改 FEATURE_MODE 宏切换功能：
 *   0 — 视频播放器（默认，触摸控制）
 *   1 — 电子相册（触摸滑动浏览 / 缩略图）
 *   2 — 传感器数值显示（温 / 湿 / 气压 / 海拔 / 光强）
 *   3 — 传感器联动 LED + 蜂鸣器告警
 */

#include <stdio.h>
#include <unistd.h>
#include "lcd.h"
#include "bmp.h"
#include "touch.h"
#include "uart.h"
#include "mplayer.h"

/* ── 功能选择（编译时修改此值即可切换） ────────── */
#define FEATURE_MODE  0   /* 0=播放器 1=相册 2=传感器 3=传感器+告警 */

/* 传感器全局变量（定义在 uart.c） */
extern int lux;
extern int tem;
extern int humidity;
extern int pressure;
extern int height;

/* ── 电子相册 ──────────────────────────────────── */
static void run_photo_album(void)
{
    char *pathname[] = {"./12.bmp", "./lb.bmp", "./13.bmp", "./14.bmp"};
    photo_album(pathname);
}

/* ── 传感器数据展示 ──────────────────────────── */
static void run_sensor_display(void)
{
    lcd_clear(0xff0000);
    while (1) {
        get_tem_humidity_pressure();
        get_lux();

        lcd_clear(0xff0000);
        lcd_draw_num(0,   0, 16, 24, 0x000000, tem);
        lcd_draw_num(0,  24, 16, 24, 0x000000, humidity);
        lcd_draw_num(0,  48, 16, 24, 0x000000, pressure);
        lcd_draw_num(0,  72, 16, 24, 0x000000, height);
        lcd_draw_num(0,  96, 16, 24, 0x000000, lux);
        sleep(1);
    }
}

/* ── 传感器 + LED / 蜂鸣器联动 ────────────────── */
static void run_sensor_with_alert(void)
{
    lcd_clear(0xff0000);
    while (1) {
        get_tem_humidity_pressure();
        get_lux();
        lcd_clear(0xff0000);

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
        sleep(1);
    }
}

/* ── main ──────────────────────────────────────── */
int main(void)
{
    if (lcd_init() != 0) {
        fprintf(stderr, "LCD init failed, exit\n");
        return 1;
    }

    switch (FEATURE_MODE) {
    case 0:  video_player();         break;
    case 1:  run_photo_album();      break;
    case 2:  run_sensor_display();   break;
    case 3:  run_sensor_with_alert();break;
    default: video_player();         break;
    }

    lcd_close();
    return 0;
}
