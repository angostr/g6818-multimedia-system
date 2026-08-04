#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include "mplayer.h"
#include "touch.h"
#include "lcd.h"
#include "icon.h"

/* ── 全局状态 ──────────────────────────────────── */
int playback_ended    = 0;

const char* videopath[] = {"1e.mp4", "1f.mp4", "1g.mp4", "1h.mp4"};
int video_count        = sizeof(videopath) / sizeof(videopath[0]);
int current_video_idx  = 0;

static int   mplayer_pid   = -1;
static FILE* mplayer_stdin = NULL;

/* ── SIGCHLD 自管道（用于 epoll 监听子进程退出） ── */
static int sig_pipe[2] = {-1, -1};

static void sigchld_handler(int sig)
{
    char c = 1;
    /* 仅做异步信号安全的写操作；写失败忽略即可 */
    if (write(sig_pipe[1], &c, 1) == -1) { /* ignore */ }
}

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
    /* 释放管道 FILE*，避免多次切换视频时 FILE* 句柄泄漏 */
    if (mplayer_stdin) {
        fclose(mplayer_stdin);
        mplayer_stdin = NULL;
    }
}

/* ── 命令发送（管道 → MPlayer slave）──────────── */

static void send_command(const char* cmd)
{
    if (mplayer_stdin) {
        fprintf(mplayer_stdin, "%s\n", cmd);
        fflush(mplayer_stdin);
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

    /* 初始化 SIGCHLD 自管道 */
    if (pipe(sig_pipe) == -1) {
        perror("pipe(sig_pipe) failed");
        return;
    }
    if (signal(SIGCHLD, sigchld_handler) == SIG_ERR) {
        perror("signal(SIGCHLD) failed");
        close(sig_pipe[0]); close(sig_pipe[1]);
        return;
    }

    if (start_mplayer(videopath[current_video_idx]) != 0) {
        fprintf(stderr, "MPlayer fail\n");
        close(sig_pipe[0]); close(sig_pipe[1]);
        return;
    }

    int tc_fd = open("/dev/input/event0", O_RDWR | O_NONBLOCK);
    if (tc_fd == -1) {
        perror("open touch device fail");
        close(sig_pipe[0]); close(sig_pipe[1]);
        quit_mplayer();
        return;
    }

    /* epoll 实例：同时监听触摸事件和子进程退出 */
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("epoll_create1 failed");
        close(tc_fd); close(sig_pipe[0]); close(sig_pipe[1]);
        quit_mplayer();
        return;
    }

    struct epoll_event ev, events[2];

    ev.events = EPOLLIN;  ev.data.fd = tc_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, tc_fd, &ev);

    ev.events = EPOLLIN;  ev.data.fd = sig_pipe[0];
    epoll_ctl(epfd, EPOLL_CTL_ADD, sig_pipe[0], &ev);

    struct input_event evt;
    int start_x = -1, start_y = -1;

    while (1) {
        /*
         * 显示控制栏时设 33ms 超时（≈30fps 刷新叠加层，
         * 确保在 MPlayer 新帧之上保持可见）；
         * 隐藏时无限阻塞，不空转 CPU。
         */
        int timeout = shadow ? 33 : -1;
        int nfds = epoll_wait(epfd, events, 2, timeout);
        if (nfds < 0) {
            if (errno == EINTR) continue;     /* 被信号打断，重试 */
            perror("epoll_wait failed");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            /* ── 处理触摸事件 ──────────────────── */
            if (fd == tc_fd) {
                while (read(tc_fd, &evt, sizeof(evt)) == sizeof(evt)) {
                    if (evt.type == EV_ABS && evt.code == ABS_X) {
                        start_x = (start_x == -1) ? evt.value : start_x;
                        touch.x = evt.value * 800 / 1000;
                    }
                    if (evt.type == EV_ABS && evt.code == ABS_Y) {
                        start_y = (start_y == -1) ? evt.value : start_y;
                        touch.y = evt.value * 480 / 600;
                    }

                    if (evt.type == EV_KEY && evt.code == BTN_TOUCH && evt.value == 0) {
                        int cur_raw_x = touch.x * 1000 / 800;
                        int cur_raw_y = touch.y * 600  / 480;
                        int dx = abs(cur_raw_x - start_x);
                        int dy = abs(cur_raw_y - start_y);

                        if (dx < 10 && dy < 10) {          /* 点击 */
                            if (shadow) {
                                if (touch.y < 440) {
                                    shadow = 0;
                                    if (touch.x < 36 && touch.y < 50) {
                                        /* 返回主菜单：仅清理本模式资源，保留 lcd 供菜单复用 */
                                        g_request_menu = 1;
                                        quit_mplayer();
                                        signal(SIGCHLD, SIG_DFL);
                                        close(tc_fd); close(epfd);
                                        close(sig_pipe[0]); close(sig_pipe[1]);
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
                                shadow = 1;
                            }
                        } else {                           /* 滑动：同一次手势内直接判定方向 */
                            if (dx > dy) {
                                /* 与电子相册统一：右滑=下一视频，左滑=上一视频 */
                                if (cur_raw_x > start_x) switch_to_next_video();
                                else                     switch_to_prev_video();
                            } else {
                                if (cur_raw_y > start_y) set_volume(-5);
                                else                     set_volume(5);
                            }
                        }
                        start_x = start_y = -1;
                    }
                }
            }
            /* ── 处理 SIGCHLD（MPlayer 退出） ───── */
            else if (fd == sig_pipe[0]) {
                char c;
                while (read(sig_pipe[0], &c, 1) > 0) { }   /* 排空管道，避免重复触发 */
                if (mplayer_pid > 0 &&
                    waitpid(mplayer_pid, NULL, WNOHANG) == mplayer_pid) {
                    playback_ended = 1;
                    mplayer_pid = -1;
                }
            }
        }

        /* ── 绘制半透明控制栏 ─────────────────── */
        if (shadow) {
            lcd_sync_region(0, 440, 800, 40);
            lcd_fill_rect(0, 440, 800, 40, 0x80808080);
            lcd_draw_word(0,   440, 32, 32, Icon[2], 0xFFFFFF);
            lcd_draw_word(384, 440, 32, 32, Icon[0], 0xFFFFFF);
            lcd_draw_word(750, 440, 32, 32, Icon[3], 0xFFFFFF);
            lcd_flush_region(0, 440, 800, 40);
        }
    }
}
