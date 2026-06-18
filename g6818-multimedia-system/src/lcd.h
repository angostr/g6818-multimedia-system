#ifndef __LCD_H__
#define __LCD_H__

/* 初始化 / 关闭 */
void lcd__init__(void);
void lcd_close(void);

/* 基本绘图 */
void lcd_clear(int color);
void lcd_draw_point(int x, int y, int color);
void lcd_draw_rect(int x, int y, int w, int h, int color);
void lcd_draw_circle(int x, int y, int r, int color);

/* 点阵字模 */
void lcd_draw_word(int x, int y, int w, int h, unsigned char data[], int color);
void lcd_draw_num(int x, int y, int w, int h, int color, int s);

/* Alpha 混合 */
void lcd_draw_point_alpha(int x, int y, int color);
void lcd_fill_rect(int x, int y, int width, int height, int color);

#endif
