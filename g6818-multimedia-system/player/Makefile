# 独立视频播放器 — Makefile
CC      ?= arm-linux-gnueabihf-gcc
CFLAGS  ?= -Wall -O2
TARGET  = mplayer_player

all: $(TARGET)

$(TARGET): mplayer.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
