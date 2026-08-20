#ifndef __LCD_H__
#define __LCD_H__

/* 屏幕分辨率（GE6818 LCD: 800×480, 32-bit ARGB） */
#define LCD_WIDTH   800
#define LCD_HEIGHT  480

/* 初始化 / 关闭
 * 返回 0 成功，-1 失败（设备打开 / mmap / 缓冲区分配失败） */
int  lcd_init(void);
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

/* 双缓冲 */
void lcd_flush(void);
void lcd_flush_region(int x, int y, int w, int h);
void lcd_sync_region(int x, int y, int w, int h);

#endif
