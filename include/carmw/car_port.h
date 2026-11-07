/*
 * car_port.h - 传输抽象
 *
 * 中间件核心（parser/decode/state）不依赖任何平台设施；
 * 只有这一层跟 OS 打交道，于是：
 *   - 板子上用 car_port_posix（termios + poll）
 *   - 单元测试 / PC 演示用 car_port_memory（把字节灌进来、把字节取出去）
 *   - 换 CAN、换 SPI、换蓝牙，只要实现这 5 个函数即可
 */
#ifndef CARMW_CAR_PORT_H
#define CARMW_CAR_PORT_H

#include "carmw/car_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *ctx;

    /* 打开失败返回 false，中间件会退化为"只读缓存 + 模拟数据"模式 */
    bool (*open)(void *ctx);
    void (*close)(void *ctx);

    /*
     * 返回 >0 读到的字节数；0 表示超时/暂无数据；<0 表示设备错误。
     * timeout_ms 为本次等待上限（协作模式靠它让出 CPU）。
     */
    int (*read)(void *ctx, uint8_t *buffer, size_t capacity, int timeout_ms);

    /* 返回实际写入的字节数；<0 表示错误。 */
    int (*write)(void *ctx, const uint8_t *buffer, size_t len);

    /* 单调毫秒时钟，用于握手重试与 NACK 限速。 */
    uint64_t (*now_ms)(void *ctx);
} car_port_t;

/* 默认单调时钟（POSIX 用 CLOCK_MONOTONIC，其他平台回落到 timespec_get）。 */
uint64_t car_now_ms(void);

/* port->now_ms 为空时自动回落到 car_now_ms()。 */
uint64_t car_port_now(const car_port_t *port);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_CAR_PORT_H */
