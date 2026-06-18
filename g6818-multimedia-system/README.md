# 嵌入式多媒体系统 — 基于 G6818 开发板

基于 **G6818 开发板**（三星 S5P6818，ARM Cortex-A53）设计并实现的嵌入式多媒体系统，集成了 **图形用户界面（GUI）**、**电子相册** 与 **视频播放器** 三大功能模块。

## 功能特性

### 1. 图形用户界面（GUI）
- 基于 Framebuffer（`/dev/fb0`）的底层图形绘制
- 支持像素级画点、画线、画矩形、画圆
- 支持字符 / 数字 / 中文汉字点阵字模显示
- 支持 ARGB Alpha 透明混合渲染（半透明控件）
- `lcd_fill_rect` 支持带透明度的矩形填充

### 2. 电子相册
- BMP 格式图片解码与显示（支持 24/32 位色深，自动处理行对齐）
- 触摸滑动手势切换图片（上下左右四向）
- 大图浏览 / 缩略图幻灯片两种模式
- 点击中心按钮进入多图轮播模式

### 3. 视频播放器
- 基于 MPlayer 从模式（slave mode），通过管道（pipe）实现进程间通信
- 触摸点击：播放 / 暂停切换
- 左右滑动：切换上一个 / 下一个视频
- 上下滑动：调节音量
- Alpha 混合半透明控制栏（快进、暂停/播放、快退、退出）
- 视频播放结束自动检测与重播

### 4. 环境传感器集成（GY39 模块）
- 通过 UART（`/dev/ttySAC1`，9600-8-N-1）读取光照强度（指令 `0xA5 0x81`）
- 读取温度、湿度、气压、海拔数据（指令 `0xA5 0x82`）
- LED 灯控、蜂鸣器控制（sysfs 接口）

## 硬件平台

| 项目 | 参数 |
|------|------|
| 主控芯片 | Samsung S5P6818 (ARM Cortex-A53, 八核) |
| LCD 分辨率 | 800 × 480 (32-bit ARGB) |
| 触摸屏 | 电容触摸（I2C → input 子系统，`/dev/input/event0`） |
| 传感器 | GY39（光照 + 温湿度 + 气压，UART1） |
| 操作系统 | Embedded Linux |

## 目录结构

```
g6818-multimedia-system/
├── README.md                 # 项目说明
├── Makefile                  # 交叉编译脚本
├── .gitignore                # Git 忽略规则
├── src/                      # 主项目源码
│   ├── main.c                # 入口（FEATURE_MODE 宏切换功能模块）
│   ├── lcd.c / lcd.h         # LCD Framebuffer 驱动 + Alpha 混合
│   ├── touch.c / touch.h     # 触摸屏驱动 + 手势方向检测
│   ├── bmp.c / bmp.h         # BMP 图片解码与显示
│   ├── mplayer.c / mplayer.h # 视频播放器（MPlayer 从模式封装）
│   ├── uart.c / uart.h       # UART 串口 + GY39 传感器 + LED/蜂鸣器
│   ├── icon.h                # 播放器 UI 图标字模（32×32）
│   ├── num.h                 # 大号数字字模（32×64）
│   └── tphlhum.h             # 中文汉字字模（温度/湿度/气压/光强/℃/Pa/%）
├── player/                   # 独立视频播放器（键盘控制版）
│   ├── mplayer.c
│   └── Makefile
├── demos/                    # 学习阶段练习代码（8 个独立程序）
│   └── README.md
└── docs/
    └── mplayer-notes.md      # MPlayer 使用笔记
```

## 编译方法

### 交叉编译（开发板运行）

```bash
# 默认 ARM 交叉编译
make

# 或显式指定编译器
make CC=arm-linux-gnueabihf-gcc
```

### 本地语法检查（不生成二进制）

```bash
make CC=gcc check
```

### 切换功能模式

修改 `src/main.c` 中的 `FEATURE_MODE` 宏：

| 值 | 功能 |
|----|------|
| 0 | 视频播放器（默认） |
| 1 | 电子相册 |
| 2 | 传感器数值显示 |
| 3 | 传感器 + LED / 蜂鸣器联动告警 |

```c
#define FEATURE_MODE  0   /* 改为 0~3 切换功能 */
```

### 编译独立播放器

```bash
make player
```

### 清理

```bash
make clean
```

## 运行依赖

- 开发板需已移植 MPlayer 可执行文件（`execlp("mplayer", ...)` 可找到）
- 视频文件放置于程序运行目录（默认列表：`1e.mp4`、`1f.mp4`、`1g.mp4`、`1h.mp4`，见 `src/mplayer.c`）
- GY39 传感器模块连接至 UART1（`/dev/ttySAC1`）
- 需要 `/dev/fb0`、`/dev/input/event0` 设备节点

## 已知问题

1. **硬编码资源路径**：视频文件列表（`mplayer.c`）和相册图片路径（`main.c` 中 `run_photo_album`）写死在代码中
2. **拼音命名**：部分函数 / 变量使用拼音（`disbmp`、`get_dir_cao`、`laizi` 等），不影响功能但可读性不佳
3. **LED_ctrl 资源泄漏**：每次调用都 open/close，高频调用时效率较低
4. **`lcd_draw_circle` 性能**：全屏遍历 800×480 像素绘制圆，可用 Bresenham 算法优化
5. **`mplayer.h` 中 `send_command` 未导出**：已改为 `static`，仅供 `mplayer.c` 内部使用

> 注：v1.1 已修复的 Bug → `uart.c` 中 `LED_ID = 10`（赋值误写为比较）、`mplayer.h` 中 `static` 变量在头文件定义导致的多副本风险。

## 项目背景

本项目为嵌入式 Linux 课程设计 / 毕业设计项目，完整实现了从底层硬件驱动到上层应用交互的开发流程，体现了以下能力：

- 嵌入式 Linux 系统编程（文件 I/O、进程管理、管道通信）
- Framebuffer 图形界面底层开发（点 / 线 / 矩形 / 圆 / Alpha 混合）
- 输入子系统（input subsystem）触摸事件处理与手势识别
- UART 串口通信协议解析（GY39 二进制协议）
- 多进程协作（fork + pipe + MPlayer slave mode）
- 点阵字模设计与渲染

## License

本项目仅用于学习与展示目的。
