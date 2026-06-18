#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

int mplayer_pid = -1;
FILE* mplayer_stdin = NULL;

// 启动MPlayer进程
int start_mplayer(const char* filename) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe创建失败");
        return -1;
    }

    mplayer_pid = fork();
    if (mplayer_pid == -1) {
        perror("fork失败");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (mplayer_pid == 0) {  // 子进程 - MPlayer
        close(pipefd[1]);  // 关闭写端
        // 将标准输入重定向到管道读端
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        
        // 启动MPlayer，-slave选项使其进入从模式，接受命令输入
        execlp("./mplayer", "mplayer", "-slave", "-quiet", filename, (char*)NULL);
        perror("execlp失败，可能未安装mplayer");
        exit(EXIT_FAILURE);
    } else {  // 父进程
        close(pipefd[0]);  // 关闭读端
        mplayer_stdin = fdopen(pipefd[1], "w");
        if (!mplayer_stdin) {
            perror("fdopen失败");
            close(pipefd[1]);
            return -1;
        }
        setbuf(mplayer_stdin, NULL);  // 禁用缓冲，确保命令立即发送
        return 0;
    }
}

// 发送命令到MPlayer
void send_command(const char* cmd) {
    if (mplayer_stdin) {
        fprintf(mplayer_stdin, "%s\n", cmd);
    }
}

// 控制函数
void play_pause() {
    send_command("pause");  // 暂停/继续切换
}

void seek_forward(int seconds) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "seek %d 0", seconds);  // 相对当前位置快进
    send_command(cmd);
}

void seek_backward(int seconds) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "seek -%d 0", seconds);  // 相对当前位置后退
    send_command(cmd);
}

void set_volume(int volume) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "volume %d 1", volume);  // 设置音量(0-100)
    send_command(cmd);
}

void quit_mplayer() {
    send_command("quit");
    if (mplayer_stdin) {
        fclose(mplayer_stdin);
        mplayer_stdin = NULL;
    }
    if (mplayer_pid != -1) {
        waitpid(mplayer_pid, NULL, 0);
        mplayer_pid = -1;
    }
}

// 显示帮助信息
void print_help() {
    printf("\n控制命令:\n");
    printf("  p - 暂停/继续\n");
    printf("  f - 快进10秒\n");
    printf("  b - 后退10秒\n");
    printf("  + - 音量增加10\n");
    printf("  - - 音量减少10\n");
    printf("  q - 退出程序\n");
    printf("  h - 显示帮助\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "用法: %s <视频文件路径>\n", argv[0]);
        return 1;
    }

    printf("正在启动MPlayer播放 %s...\n", argv[1]);
    if (start_mplayer(argv[1]) != 0) {
        fprintf(stderr, "启动MPlayer失败\n");
        return 1;
    }

    print_help();
    
    // // 处理用户输入
    // char key;
    // //从键盘输入一个字符 我们需要把这里修改成触摸/语音输入
    // while ((key = getchar()) != 'q') {
    //     switch (key) {
    //         case 'p':
    //             play_pause();
    //             printf("已切换暂停状态\n");
    //             break;
    //         case 'f':
    //             seek_forward(10);  // 快进10秒
    //             printf("快进10秒\n");
    //             break;
    //         case 'b':
    //             seek_backward(10);  // 后退10秒
    //             printf("后退10秒\n");
    //             break;
    //         case '+':
    //             set_volume(10);  // 增加音量
    //             printf("音量增加\n");
    //             break;
    //         case '-':
    //             set_volume(-10);  // 减少音量
    //             printf("音量减少\n");
    //             break;
    //         case 'h':
    //             print_help();
    //             break;
    //     }
    // }

    // 退出清理
    quit_mplayer();
    printf("程序已退出\n");
    return 0;
}

