/**
 * UART 串口通信 + 传感器 + 外设控制 — G6818
 *
 * - GY39 传感器: 光照 (0xA5 0x81) / 温湿压 (0xA5 0x82)
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

/* ── 串口初始化 (9600-8-N-1) ─────────────────────
 * 返回 0 成功，-1 失败
 */
int init_tty(int fd)
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
    if (tcsetattr(fd, TCSANOW, &termios_new) != 0) {
        perror("tcsetattr failed");
        return -1;
    }
    return 0;
}

/* ── GY39: 光照强度读取 ────────────────────────── */

void get_lux(void)
{
    int fd = open("/dev/ttySAC1", O_RDWR);
    if (fd == -1) {
        perror("open gy39 fail");
        return;
    }
    if (init_tty(fd) != 0) {
        close(fd);
        return;
    }

    unsigned char cmd[3] = {0xa5, 0x81, 0x26};
    if (write(fd, cmd, sizeof(cmd)) != (int)sizeof(cmd)) {
        perror("write gy39 cmd fail");
        close(fd);
        return;
    }

    unsigned char buf[9] = {0};
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n != (ssize_t)sizeof(buf)) {
        fprintf(stderr, "gy39 lux: read %zd bytes (want %zu)\n", n, sizeof(buf));
        close(fd);
        return;
    }

    /* 帧头 0x5A 0x5A，功能码 0x15 */
    if (buf[0] == 0x5a && buf[1] == 0x5a && buf[2] == 0x15) {
        lux = ((buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7]) / 100;
        printf("lux ==== %d\n", lux);
    }

    close(fd);
}

/* ── GY39: 温度 / 湿度 / 气压 / 海拔读取 ──────────── */

void get_tem_humidity_pressure(void)
{
    int fd = open("/dev/ttySAC1", O_RDWR);
    if (fd == -1) {
        perror("open gy39 fail");
        return;
    }
    if (init_tty(fd) != 0) {
        close(fd);
        return;
    }

    unsigned char cmd[3] = {0xa5, 0x82, 0x27};
    if (write(fd, cmd, sizeof(cmd)) != (int)sizeof(cmd)) {
        perror("write gy39 cmd fail");
        close(fd);
        return;
    }

    unsigned char buf[15] = {0};
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n != (ssize_t)sizeof(buf)) {
        fprintf(stderr, "gy39 tem: read %zd bytes (want %zu)\n", n, sizeof(buf));
        close(fd);
        return;
    }

    /* 帧头 0x5A 0x5A，功能码 0x45 */
    if (buf[0] == 0x5a && buf[1] == 0x5a && buf[2] == 0x45) {
        tem      = ((buf[4]  << 8) | buf[5])  / 100;
        pressure = ((buf[6]  << 24) | (buf[7]  << 16)
                  | (buf[8]  <<  8) |  buf[9]) / 100;
        humidity = ((buf[10] << 8) | buf[11]) / 100;
        height   = ((buf[12] << 8) | buf[13]);

        printf("tem ==== %d\n",       tem);
        printf("pressure ==== %d pa\n", pressure);
        printf("humidity ==== %d%%\n", humidity);
        printf("height ==== %d\n",    height);
    }

    close(fd);
}

/* ── LED 控制 (sysfs，fd 缓存避免重复 open/close) ── */

#define LED_ID_MAX 100
static int g_led_fd[LED_ID_MAX + 1] = {0};   /* LED_ID 作下标，0 表示未打开 */

static int led_open(int id)
{
    if (id >= 0 && id <= LED_ID_MAX && g_led_fd[id] > 0)
        return g_led_fd[id];

    const char *path = NULL;
    switch (id) {
    case 7:   path = "/sys/kernel/gec_ctrl/led_d7";   break;
    case 8:   path = "/sys/kernel/gec_ctrl/led_d8";   break;
    case 9:   path = "/sys/kernel/gec_ctrl/led_d9";   break;
    case 10:  path = "/sys/kernel/gec_ctrl/led_d10";  break;
    case 100: path = "/sys/kernel/gec_ctrl/led_all";  break;
    default:  return -1;
    }

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        perror("open led fail");
        return -1;
    }
    if (id >= 0 && id <= LED_ID_MAX)
        g_led_fd[id] = fd;     /* 缓存，进程生命周期内复用 */
    return fd;
}

void LED_ctrl(int LED_ID, int LED_state)
{
    int fd = led_open(LED_ID);
    if (fd < 0)
        return;

    char buf[1] = { (LED_state == 0) ? '0' : '1' };
    if (write(fd, buf, 1) != 1)
        perror("write led fail");
}

/* ── 蜂鸣器控制 (sysfs) ────────────────────────── */

void beep_ctrl(int beep_state)
{
    int beep_fd = open("/sys/kernel/gec_ctrl/beep", O_RDWR);
    if (beep_fd == -1) {
        perror("open beep error!");
        return;
    }
    char buf[1] = { (beep_state == 0) ? '0' : '1' };
    if (write(beep_fd, buf, 1) != 1)
        perror("write beep fail");
    close(beep_fd);
}
