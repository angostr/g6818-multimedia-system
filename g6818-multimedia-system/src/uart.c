/**
 * UART 串口通信 + 传感器 + 外设控制 — G6818
 *
 * - GY39 传感器: 光照 (0x81) / 温湿压 (0x82)
 * - LED 控制:    sysfs /sys/kernel/gec_ctrl/led_*
 * - 蜂鸣器控制:  sysfs /sys/kernel/gec_ctrl/beep
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

/* ── 全局变量 ──────────────────────────────────── */
int lux       = 0;
int tem       = 0;
int humidity  = 0;
int pressure  = 0;
int height    = 0;

/* ── 串口初始化 (9600-8-N-1) ───────────────────── */

void init_tty(int fd)
{
    struct termios termios_new;
    bzero(&termios_new, sizeof(termios_new));
    cfmakeraw(&termios_new);

    cfsetispeed(&termios_new, B9600);
    cfsetospeed(&termios_new, B9600);

    termios_new.c_cflag |= CLOCAL | CREAD;
    termios_new.c_cflag &= ~CSIZE;
    termios_new.c_cflag |= CS8;
    termios_new.c_cflag &= ~PARENB;
    termios_new.c_cflag &= ~CSTOPB;

    termios_new.c_cc[VTIME] = 10;
    termios_new.c_cc[VMIN]  = 1;

    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &termios_new))
        printf("Setting the serial failed!\n");
}

/* ── GY39: 光照强度读取 ────────────────────────── */

void get_lux(void)
{
    int lux_fd = open("/dev/ttySAC1", O_RDWR);
    if (lux_fd == -1) {
        printf("open gy39 fail\n");
        return;
    }

    init_tty(lux_fd);

    char send_buf[3] = {0xa5, 0x81, 0x26};
    int ret = write(lux_fd, send_buf, 3);
    if (ret != 3) sleep(1);

    unsigned char recvbuf[9] = {0};
    ret = read(lux_fd, recvbuf, 9);
    if (ret != 9)
        printf("read data error\n");

    if (recvbuf[2] == 0x15 && recvbuf[1] == 0x5a && recvbuf[0] == 0x5a) {
        lux = (recvbuf[4] << 24) | (recvbuf[5] << 16)
            | (recvbuf[6] <<  8) |  recvbuf[7];
        lux = lux / 100;
        printf("lux ==== %d\n", lux);
    }

    close(lux_fd);
}

/* ── GY39: 温度 / 湿度 / 气压 / 海拔读取 ──────────── */

void get_tem_humidity_pressure(void)
{
    int con_fd = open("/dev/ttySAC1", O_RDWR);
    if (con_fd == -1) {
        printf("open gy39 fail\n");
        return;
    }

    init_tty(con_fd);

    char send_buf[3] = {0xa5, 0x82, 0x27};
    int ret = write(con_fd, send_buf, 3);
    if (ret != 3) sleep(1);

    unsigned char recvbuf[15] = {0};
    ret = read(con_fd, recvbuf, 15);

    if (recvbuf[2] == 0x45 && recvbuf[1] == 0x5a && recvbuf[0] == 0x5a) {
        tem      = ((recvbuf[4]  << 8) | recvbuf[5])  / 100;
        pressure = ((recvbuf[6]  << 24) | (recvbuf[7]  << 16)
                  | (recvbuf[8]  <<  8) |  recvbuf[9]) / 100;
        humidity = ((recvbuf[10] << 8) | recvbuf[11]) / 100;
        height   = ((recvbuf[12] << 8) | recvbuf[13]);

        printf("tem ==== %d\n",       tem);
        printf("pressure ==== %d pa\n", pressure);
        printf("humidity ==== %d%%\n", humidity);
        printf("height ==== %d\n",    height);
    }

    close(con_fd);
}

/* ── LED 控制 (sysfs) ──────────────────────────── */

void LED_ctrl(int LED_ID, int LED_state)
{
    int led_fd = -1;

    if (LED_ID == 7)
        led_fd = open("/sys/kernel/gec_ctrl/led_d7", O_RDWR);
    else if (LED_ID == 8)
        led_fd = open("/sys/kernel/gec_ctrl/led_d8", O_RDWR);
    else if (LED_ID == 9)
        led_fd = open("/sys/kernel/gec_ctrl/led_d9", O_RDWR);
    else if (LED_ID == 10)
        led_fd = open("/sys/kernel/gec_ctrl/led_d10", O_RDWR);
    else if (LED_ID == 100)
        led_fd = open("/sys/kernel/gec_ctrl/led_all", O_RDWR);

    if (led_fd == -1) {
        printf("open led fail\n");
        return;
    }

    int state = (LED_state == 0) ? 0 : 1;
    write(led_fd, &state, 1);
    close(led_fd);
}

/* ── 蜂鸣器控制 (sysfs) ────────────────────────── */

void beep_ctrl(int beep_state)
{
    int beep_fd = open("/sys/kernel/gec_ctrl/beep", O_RDWR);
    if (beep_fd == -1) {
        printf("open beep error!\n");
        return;
    }
    write(beep_fd, &beep_state, 1);
    close(beep_fd);
}
