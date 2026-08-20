# GE6818 嵌入式多媒体系统 — Makefile
# 默认 ARM 交叉编译，可通过 make CC=gcc 在 x86 上做语法检查

CC          ?= arm-linux-gnueabihf-gcc
CFLAGS      ?= -Wall -O2
LDFLAGS     ?= -lpthread

SRC_DIR  = src
TARGET   = multimedia_system

SRCS     = $(wildcard $(SRC_DIR)/*.c)
OBJS     = $(SRCS:.c=.o)

.PHONY: all clean check help

all: $(TARGET)
	@echo "[OK] Build done → $(TARGET)"

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# 语法检查（不链接，仅检查编译是否通过）
check:
	$(CC) $(CFLAGS) -fsyntax-only $(SRCS)

clean:
	rm -f $(TARGET) $(SRC_DIR)/*.o

help:
	@echo "Targets:"
	@echo "  all    — 编译主项目（默认）"
	@echo "  check  — 仅语法检查，不生成二进制"
	@echo "  clean  — 清理编译产物"
	@echo ""
	@echo "Variables:"
	@echo "  CC            — 编译器 (default: arm-linux-gnueabihf-gcc)"
	@echo "  CFLAGS        — 编译选项 (default: -Wall -O2)"
	@echo ""
	@echo "Examples:"
	@echo "  make                      # ARM 交叉编译"
	@echo "  make CC=gcc check         # 本地语法检查"
