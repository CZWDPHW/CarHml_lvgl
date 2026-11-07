#include "carmw/car_port_memory.h"

#include <string.h>

static bool memory_open(void *ctx)
{
    car_port_memory_t *self = (car_port_memory_t *)ctx;

    if(self == NULL) {
        return false;
    }

    self->opened = true;
    return true;
}

static void memory_close(void *ctx)
{
    car_port_memory_t *self = (car_port_memory_t *)ctx;

    if(self != NULL) {
        self->opened = false;
    }
}

static int memory_read(void *ctx, uint8_t *buffer, size_t capacity, int timeout_ms)
{
    car_port_memory_t *self = (car_port_memory_t *)ctx;

    (void)timeout_ms; /* 内存端口没有真实的等待语义 */

    if(self == NULL || buffer == NULL) {
        return -1;
    }

    if(!self->opened || self->rx_len == 0) {
        return 0;
    }

    size_t take = self->rx_len < capacity ? self->rx_len : capacity;

    memcpy(buffer, self->rx, take);
    self->rx_len -= take;

    if(self->rx_len != 0) {
        memmove(self->rx, self->rx + take, self->rx_len);
    }

    return (int)take;
}

static int memory_write(void *ctx, const uint8_t *buffer, size_t len)
{
    car_port_memory_t *self = (car_port_memory_t *)ctx;

    if(self == NULL || !self->opened || (buffer == NULL && len != 0)) {
        return -1;
    }

    size_t space = CAR_MEM_TX_CAPACITY - self->tx_len;
    size_t accepted = len < space ? len : space;

    if(accepted != 0) {
        memcpy(self->tx + self->tx_len, buffer, accepted);
        self->tx_len += accepted;
    }

    if(accepted != len) {
        self->dropped_tx += (uint32_t)(len - accepted);
        return (int)accepted; /* 模拟短写 */
    }

    return (int)accepted;
}

static uint64_t memory_now(void *ctx)
{
    car_port_memory_t *self = (car_port_memory_t *)ctx;

    return self != NULL ? self->clock_ms : 0;
}

void car_port_memory_attach(car_port_t *port, car_port_memory_t *memory)
{
    if(port == NULL || memory == NULL) {
        return;
    }

    memset(memory, 0, sizeof(*memory));

    port->ctx = memory;
    port->open = memory_open;
    port->close = memory_close;
    port->read = memory_read;
    port->write = memory_write;
    port->now_ms = memory_now;
}

void car_port_memory_push(car_port_memory_t *memory,
                          const uint8_t *data,
                          size_t len)
{
    if(memory == NULL || (data == NULL && len != 0)) {
        return;
    }

    size_t space = CAR_MEM_RX_CAPACITY - memory->rx_len;
    size_t accepted = len < space ? len : space;

    if(accepted != 0) {
        memcpy(memory->rx + memory->rx_len, data, accepted);
        memory->rx_len += accepted;
    }

    if(accepted != len) {
        memory->dropped_rx += (uint32_t)(len - accepted);
    }
}

size_t car_port_memory_take(car_port_memory_t *memory,
                            uint8_t *out,
                            size_t capacity)
{
    if(memory == NULL || out == NULL || capacity == 0 || memory->tx_len == 0) {
        return 0;
    }

    size_t take = memory->tx_len < capacity ? memory->tx_len : capacity;

    memcpy(out, memory->tx, take);
    memory->tx_len -= take;

    if(memory->tx_len != 0) {
        memmove(memory->tx, memory->tx + take, memory->tx_len);
    }

    return take;
}

void car_port_memory_advance(car_port_memory_t *memory, uint64_t delta_ms)
{
    if(memory != NULL) {
        memory->clock_ms += delta_ms;
    }
}

void car_port_memory_reset(car_port_memory_t *memory)
{
    if(memory == NULL) {
        return;
    }

    memory->rx_len = 0;
    memory->tx_len = 0;
    memory->dropped_rx = 0;
    memory->dropped_tx = 0;
}
