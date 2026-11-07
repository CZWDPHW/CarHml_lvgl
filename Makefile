# lvgl_two - 车载 MCU 串口协议解析中间件
#
#   make                 构建静态库 + 单元测试 + 演示程序
#   make test            跑全部单元测试
#   make demo            构建并运行无硬件演示
#   make clean
#
#   make CROSS=aarch64-linux-gnu- TARGET=arm   交叉编译（只构建库和演示）
#
# 只用标准 POSIX + C11，没有任何第三方依赖。

CROSS   ?=
CC      := $(CROSS)gcc
AR      := $(CROSS)ar

CFLAGS  ?= -O2
WARN    := -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes
INCLUDE := -Iinclude -Isrc
CFLAGS_ALL := -std=c11 $(CFLAGS) $(WARN) $(INCLUDE)

BUILD := build
BIN   := bin

LIB_SRC := $(wildcard src/*.c) src/port/car_port_memory.c src/port/car_port_posix.c
LIB_OBJ := $(patsubst %.c,$(BUILD)/%.o,$(LIB_SRC))
LIB     := $(BIN)/libcarmw.a

TEST_SRC := $(wildcard tests/*.c)
TEST_OBJ := $(patsubst %.c,$(BUILD)/%.o,$(TEST_SRC))

.PHONY: all lib test demo clean help

all: $(LIB) $(BIN)/carmw_tests $(BIN)/carmw_demo

lib: $(LIB)

$(LIB): $(LIB_OBJ)
	@mkdir -p $(BIN)
	$(AR) rcs $@ $^

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_ALL) -c $< -o $@

$(BIN)/carmw_tests: $(TEST_OBJ) $(LIB)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS_ALL) $^ -o $@ -lpthread

$(BIN)/carmw_demo: $(BUILD)/examples/demo.o $(LIB)
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS_ALL) $^ -o $@ -lpthread

test: $(BIN)/carmw_tests
	./$(BIN)/carmw_tests

demo: $(BIN)/carmw_demo
	./$(BIN)/carmw_demo

clean:
	rm -rf $(BUILD) $(BIN)

help:
	@echo "make            构建全部"
	@echo "make test       单元测试"
	@echo "make demo       无硬件演示"
	@echo "make clean      清理"
