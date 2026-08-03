#ifndef __UART_H__
#define __UART_H__

int  init_tty(int fd);

void get_lux(void);
void get_tem_humidity_pressure(void);

void LED_ctrl(int LED_ID, int LED_state);
void beep_ctrl(int beep_state);

#endif
