#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include "mplayer.h"
#include "touch.h"
#include "lcd.h"
#include "icon.h"

/* ── 全局状态 ──────────────────────────────────── */
int paused            = 0;
int playback_ended    = 0;

const char* videopath[] = {"1e.mp4", "1f.mp4", "1g.mp4", "1h.mp4"};
int video_count        = sizeof(videopath) / sizeof(videopath[0]);
int current_video_idx  = 0;

static int   mplayer_pid   = -1;
static FILE* mplayer_stdin = NULL;

/* ── MPlayer 进程管理 ──────────────────────────── */

int start_mplayer(const char *filename)
{
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe create fail");
        return -1;
    }

    mplayer_pid = fork();
    if (mplayer_pid == -1) {
        perror("fork fail");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (mplayer_pid == 0) {          /* 子进程 — MPlayer */
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        execlp("mplayer", "mplayer",
               "-slave", "-quiet",
               "-zoom",
               "-keepaspect",
               "-vf", "scale=800:480",
               "-fs", "-noborder",
               filename, NULL);
        perror("execlp fail");
        exit(EXIT_FAILURE);
    } else {                         /* 父进程 */
        close(pipefd[0]);
        mplayer_stdin = fdopen(pipefd[1], "w");
        if (!mplayer_stdin) {
            perror("fdopen fail");
            close(pipefd[1]);
            return -1;
        }
        setbuf(mplayer_stdin, NULL);
        return 0;
    }
}

void quit_mplayer(void)
{
    if (mplayer_pid > 0) {
        printf("stop mplayer, PID=%d\n", mplayer_pid);
        kill(mplayer_pid, SIGTERM);
        waitpid(mplayer_pid, NULL, 0);
        mplayer_pid = -1;
    }
}

/* ── 命令发送（管道 → MPlayer slave）──────────── */

static void send_command(const char* cmd)
{
    if (mplayer_stdin) {
        fprintf(mplayer_stdin, "%s\n", cmd);
    }
}

/* ── 播放控制 ──────────────────────────────────── */

void play_pause(void)
{
    send_command("pause");
}

void seek_forward(int seconds)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "seek %d 0", seconds);
    send_command(cmd);
}

void seek_backward(int seconds)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "seek -%d 0", seconds);
    send_command(cmd);
}

void set_volume(int volume)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "volume %d 1", volume);
    send_command(cmd);
}

/* ── 视频切换 ──────────────────────────────────── */

void switch_to_prev_video(void)
{
    quit_mplayer();
    current_video_idx--;
    if (current_video_idx < 0)
        current_video_idx = video_count - 1;

    if (start_mplayer(videopath[current_video_idx]) != 0)
        fprintf(stderr, "switch video fail\n");

    playback_ended = 0;
}

void switch_to_next_video(void)
{
    quit_mplayer();
    current_video_idx++;
    if (current_video_idx >= video_count)
        current_video_idx = 0;

    if (start_mplayer(videopath[current_video_idx]) != 0)
        fprintf(stderr, "switch video fail\n");

    playback_ended = 0;
}

/* ── 主界面（触摸驱动视频播放器） ──────────────── */

void video_player(void)
{
    int shadow = 0;
    struct { int x, y; } touch = {0};

    if (start_mplayer(videopath[current_video_idx]) != 0) {
        fprintf(stderr, "MPlayer fail\n");
        return;
    }

    int tc_fd = open("/dev/input/event0", O_RDWR | O_NONBLOCK);
    if (tc_fd == -1) {
        printf("open touch device fail\n");
        return;
    }

    struct input_event ev;
    int start_x = -1, start_y = -1;

    while (1) {
        /* 读取触摸事件 */
        while (read(tc_fd, &ev, sizeof(ev)) == sizeof(ev)) {
            if (ev.type == EV_ABS && ev.code == ABS_X) {
                start_x = (start_x == -1) ? ev.value : start_x;
                touch.x = ev.value * 800 / 1000;
            }
            if (ev.type == EV_ABS && ev.code == ABS_Y) {
                start_y = (start_y == -1) ? ev.value : start_y;
                touch.y = ev.value * 480 / 600;
            }

            /* 手指抬起 → 判断点击 / 滑动 */
            if (ev.type == EV_KEY && ev.code == BTN_TOUCH && ev.value == 0) {
                int dx = abs(touch.x * 1000 / 800 - start_x);
                int dy = abs(touch.y * 600  / 480 - start_y);

                if (dx < 10 && dy < 10) {          /* 点击 */
                    if (shadow) {
                        if (touch.y < 440) {       /* 非控制栏区域 */
                            shadow = 0;
                            if (touch.x < 36 && touch.y < 50) {
                                quit_mplayer();
                                close(tc_fd);
                                lcd_close();
                                return;
                            }
                        } else if (touch.x >= 0   && touch.x < 50)  seek_forward(10);
                        else if (touch.x >= 730 && touch.x < 800) seek_backward(10);
                        else if (touch.x >= 370 && touch.x < 430) {
                            if (!playback_ended) play_pause();
                            else {
                                playback_ended = 0;
                                quit_mplayer();
                                start_mplayer(videopath[current_video_idx]);
                            }
                        }
                    } else {
                        shadow = 1;                /* 显示控制栏 */
                    }
                } else {                           /* 滑动 */
                    int dir = get_dir_cao();
                    if (dir == LEFT)        switch_to_next_video();
                    else if (dir == RIGHT)  switch_to_prev_video();
                    else if (dir == UP)     set_volume(5);
                    else if (dir == DOWN)   set_volume(-5);
                }
                start_x = start_y = -1;
            }
        }

        /* 检测视频是否播放结束 */
        if (waitpid(mplayer_pid, NULL, WNOHANG) == mplayer_pid) {
            playback_ended = 1;
            mplayer_pid = -1;
        }

        /* 绘制半透明控制栏 */
        if (shadow) {
            lcd_fill_rect(0, 440, 800, 40, 0x80808080);
            lcd_draw_word(0,   440, 32, 32, Icon[2], 0xFFFFFF);  /* 快进 */
            lcd_draw_word(384, 440, 32, 32, Icon[0], 0xFFFFFF);  /* 暂停/播放 */
            lcd_draw_word(750, 440, 32, 32, Icon[3], 0xFFFFFF);  /* 快退 */
        }

        usleep(10000);
    }
}
