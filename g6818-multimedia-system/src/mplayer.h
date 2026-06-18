#ifndef __MPLAYER_H__
#define __MPLAYER_H__

extern int paused;           // 0=播放  1=暂停
extern int playback_ended;   // 1=视频已播完

// 视频列表及索引
extern const char* videopath[];   // 视频路径数组
extern int video_count;           // 视频总数
extern int current_video_idx;     // 当前播放索引

// 启动 / 停止 MPlayer 进程
int  start_mplayer(const char* filename);
void quit_mplayer();

// 播放控制
void play_pause();
void seek_forward(int seconds);
void seek_backward(int seconds);
void set_volume(int volume);

// 视频切换
void switch_to_prev_video(void);
void switch_to_next_video(void);

// 主界面入口
void video_player(void);

#endif
