/*
 * car_port_memory.h - 内存端口
 *
 * 一路"假串口"：把字节 push 进去就是 MCU 发来的数据，
 * 从 take 里取出来就是屏发出去的字节。
 * 单元测试和 PC 演示都用它，因此整条链路（含握手、ACK、状态快照）
 * 都能在没有硬件的情况下被验证。
 */
#ifndef CARMW_CAR_PORT_MEMORY_H
#define CARMW_CAR_PORT_MEMORY_H

#include "carmw/car_port.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAR_MEM_RX_CAPACITY 4096U
#define CAR_MEM_TX_CAPACITY 1024U

typedef struct {
    uint8_t rx[CAR_MEM_RX_CAPACITY];
    size_t rx_len;
    uint8_t tx[CAR_MEM_TX_CAPACITY];
    size_t tx_len;
    uint64_t clock_ms;
    bool opened;
    uint32_t dropped_rx;
    uint32_t dropped_tx;
} car_port_memory_t;

void car_port_memory_attach(car_port_t *port, car_port_memory_t *memory);

/* 模拟 MCU -> 屏：追加接收字节（超出容量部分丢弃并计数）。 */
void car_port_memory_push(car_port_memory_t *memory,
                          const uint8_t *data,
                          size_t len);

/* 模拟 屏 -> MCU：取出已发送字节，返回实际取出长度。 */
size_t car_port_memory_take(car_port_memory_t *memory, uint8_t *out, size_t capacity);

/* 推进虚拟时钟（握手重试、超时判断都依赖它）。 */
void car_port_memory_advance(car_port_memory_t *memory, uint64_t delta_ms);

/* 清空收发缓冲与计数，保留时钟与打开状态。 */
void car_port_memory_reset(car_port_memory_t *memory);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_CAR_PORT_MEMORY_H */
