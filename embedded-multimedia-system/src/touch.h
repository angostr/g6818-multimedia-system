#ifndef __TOUCH_H__
#define __TOUCH_H__

/* 滑动方向 */
#define UP    1
#define DOWN  2
#define LEFT  3
#define RIGHT 4
#define FLAG  5    /* 中心按钮点击 */

/* 触摸手势检测（传入已打开的触摸设备 fd） */
int  get_swipe_direction(int tc_fd);

/* 按钮区域判断 */
int  isbutton(int x, int y, int button_x, int button_y, int r);

/* 电子相册（触摸翻页浏览） */
void photo_album(char *pathname[]);

#endif
