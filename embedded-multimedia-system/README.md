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
embedded-multimedia-system/
├── README.md                 # 项目说明
├── Makefile                  # 交叉编译脚本
├── .gitignore                # Git 忽略规则
├── src/                      # 主项目源码
│   ├── main.c                # 入口（FEATURE_MODE 宏 / make 变量切换功能模块）
│   ├── lcd.c / lcd.h         # LCD Framebuffer 驱动 + Alpha 混合
│   ├── touch.c / touch.h     # 触摸屏驱动 + 手势方向检测
│   ├── bmp.c / bmp.h         # BMP 图片解码与显示
│   ├── mplayer.c / mplayer.h # 视频播放器（MPlayer 从模式封装）
│   ├── uart.c / uart.h       # UART 串口 + GY39 传感器 + LED/蜂鸣器
│   ├── icon.h                # 播放器 UI 图标字模（32×32）
│   └── num.h                 # 数字字模（16×24）
├── demos/                    # 学习阶段练习代码（独立程序，仅供参考）
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

> 也可在编译时通过 `make FEATURE_MODE=1` 传入，Makefile 会以 `-DFEATURE_MODE=1` 注入（无需改代码）。


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

## 优化记录

### v1.2（结构 / 健壮性 / 效率）

- **清理死代码**：移除从未被引用的 `tphlhum.h`（中文字模）与功能重复且 `main()` 被注释导致「启动即退出」的 `player/` 目录。
- **命名规范**：`lcd__init__()` → `lcd_init()`（去除双下划线保留字风格）；删除从未被读写的死全局 `paused`。
- **错误处理**：`lcd_init` 增加 `mmap`/`malloc` 失败检查并返回错误码（`main` 据此退出）；`bmp_display` 校验 `read`/`malloc` 返回值与 BMP 深度/尺寸；`uart` 串口读取校验返回字节数后再解析帧；`mplayer` 增加 `pipe`/`epoll_create1`/`open` 失败处理。
- **效率**：`LED_ctrl` 由每次 `open/write/close` 改为 **fd 缓存**复用；`lcd_draw_circle` 由全屏 800×480 遍历收敛到圆的外接矩形。
- **手势判定修复**：原 `mplayer` 在滑动时调用 `get_swipe_direction()` 会重新 `open` 触摸设备并**阻塞读取下一次手势**才判定方向（方向滞后一拍）；现改为在同一手势内依据起点/终点坐标即时判定，并让 `get_swipe_direction(int fd)` 复用调用方已打开的 fd，消除重复 `open` 导致的事件分流。
- **构建修复**：`make FEATURE_MODE=N` 此前因 Makefile 未注入 `-DFEATURE_MODE` 而失效，现已生效；移除对 `player/` 的构建依赖。

### 待优化（后续可继续）

1. **硬编码资源路径**：视频列表（`mplayer.c` 的 `videopath`）与相册图片路径（`main.c` 的 `run_photo_album`）写死在代码中，建议改为命令行参数或配置文件读取。
2. **编译期功能切换**：`FEATURE_MODE` 仍为编译宏，建议改为运行时菜单 / 参数选择，单个二进制支持全部功能。
3. **`lcd_draw_circle` 算法**：可进一步用中点画圆（Bresenham）替代逐像素距离判断。
4. **`num.h` 字模尺寸**：声明为 `numinfo[][32*64/8]`，但实际数据为 16×24；建议统一为 `numinfo[][16*24/8]` 并由 `lcd_draw_num` 的 `w/h` 参数驱动，避免误导。
5. **传感器轮询**：`main.c` 中 `sleep(1)` 轮询传感器，高频场景可改为线程 + 条件变量或 inotify/select。
6. **手势宏命名**：`UP/DOWN/LEFT/RIGHT/FLAG` 为通用名，建议加前缀（如 `GESTURE_*`）避免与其他头文件冲突。
7. **演示资源缺失**：`1e.mp4` 等视频与 `12.bmp` 等图片未纳入仓库，建议补充 `assets/` 样例或 `.gitignore` 说明。

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
