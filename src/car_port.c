#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "carmw/car_port.h"

#include <time.h>

uint64_t car_now_ms(void)
{
#if defined(CLOCK_MONOTONIC)
    struct timespec mono;

    if(clock_gettime(CLOCK_MONOTONIC, &mono) == 0) {
        return (uint64_t)mono.tv_sec * 1000U +
               (uint64_t)(mono.tv_nsec / 1000000L);
    }
#endif

    struct timespec wall;

    if(timespec_get(&wall, TIME_UTC) == TIME_UTC) {
        return (uint64_t)wall.tv_sec * 1000U +
               (uint64_t)(wall.tv_nsec / 1000000L);
    }

    return 0;
}
/**
 * @brief 获取当前时间戳，单位毫秒
 * @param port 串口端口
 * @return uint64_t 当前时间戳，单位毫秒
 */
uint64_t car_port_now(const car_port_t *port)
{
    if(port != NULL && port->now_ms != NULL && port->ctx != NULL) {
        return port->now_ms(port->ctx);
    }

    return car_now_ms();
}
