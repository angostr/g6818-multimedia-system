#ifndef __TOUCH_H__
#define __TOUCH_H__

/* 滑动方向 / 手势类型 */
#define UP    1
#define DOWN  2
#define LEFT  3
#define RIGHT 4
#define FLAG  5    /* 中心按钮点击（相册：进入幻灯片） */
#define GESTURE_TAP 6  /* 通用点击（位移很小，非中心） */

/* 返回主菜单请求标志：任一模式内触发后，主循环将重新显示菜单。
 * 在 main.c 中定义，本文件仅声明。 */
extern int g_request_menu;

/* 触摸手势检测（传入已打开的触摸设备 fd）
 * 返回 UP/DOWN/LEFT/RIGHT/FLAG/TAP/-1。
 * 当返回 TAP 时，*tap_x / *tap_y 输出映射后的 LCD 坐标（800×480）。 */
int  get_swipe_direction(int tc_fd, int *tap_x, int *tap_y);

/* 按钮区域判断 */
int  isbutton(int x, int y, int button_x, int button_y, int r);

/* 电子相册（触摸翻页浏览） */
void photo_album(char *pathname[]);

#endif
