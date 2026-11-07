/*
 * car_port_posix.h - Linux 串口实现（termios + poll，非阻塞）
 */
#ifndef CARMW_CAR_PORT_POSIX_H
#define CARMW_CAR_PORT_POSIX_H

#include "carmw/car_port.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int fd;
    unsigned baud;
    char device[64];
} car_port_posix_t;

/*
 * 绑定实现。返回后可以调用 port->open() 真正打开串口。
 * 换设备 / 换波特率重连时，直接重新 attach 一次即可。
 */
bool car_port_posix_attach(car_port_t *port,
                           car_port_posix_t *storage,
                           const char *device,
                           unsigned baud);

/* 关闭 fd 并把 port 函数表清空（可重复调用）。 */
void car_port_posix_detach(car_port_t *port, car_port_posix_t *storage);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_CAR_PORT_POSIX_H */
