#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "carmw/car_port_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/*
 * 波特率必须映射成 speed_t 常量。
 * 直接把 115200 这样的数值塞给 cfsetispeed() 在 glibc 上是未定义行为，
 * 很可能悄悄退化成 9600——这是旧实现里的一个真实隐患。
 */
static speed_t baud_to_speed(unsigned baud)
{
    switch(baud) {
        case 4800:
            return B4800;
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
#ifdef B230400
        case 230400:
            return B230400;
#endif
#ifdef B460800
        case 460800:
            return B460800;
#endif
#ifdef B921600
        case 921600:
            return B921600;
#endif
        default:
            return B115200;
    }
}

static bool posix_configure(car_port_posix_t *self)
{
    struct termios tty;

    if(tcgetattr(self->fd, &tty) != 0) {
        return false;
    }

    /* 等价于 cfmakeraw()，但只依赖 POSIX 定义，避免 _DEFAULT_SOURCE 依赖 */
    tty.c_iflag &= (tcflag_t) ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR |
                                IGNCR | ICRNL | IXON);
    tty.c_oflag &= (tcflag_t) ~OPOST;
    tty.c_lflag &= (tcflag_t) ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_cflag &= (tcflag_t) ~(CSIZE | PARENB | CSTOPB);
    tty.c_cflag |= (tcflag_t)(CS8 | CLOCAL | CREAD);

#ifdef CRTSCTS
    tty.c_cflag &= (tcflag_t)~CRTSCTS;
#endif

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    speed_t speed = baud_to_speed(self->baud);

    if(cfsetispeed(&tty, speed) != 0 || cfsetospeed(&tty, speed) != 0) {
        return false;
    }

    if(tcsetattr(self->fd, TCSANOW, &tty) != 0) {
        return false;
    }

    (void)tcflush(self->fd, TCIFLUSH);
    return true;
}

static bool posix_open(void *ctx)
{
    car_port_posix_t *self = (car_port_posix_t *)ctx;

    if(self == NULL || self->fd >= 0) {
        return self != NULL && self->fd >= 0;
    }

    int fd = open(self->device, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if(fd < 0) {
        return false;
    }

    self->fd = fd;

    if(!posix_configure(self)) {
        (void)close(fd);
        self->fd = -1;
        return false;
    }

    return true;
}

static void posix_close(void *ctx)
{
    car_port_posix_t *self = (car_port_posix_t *)ctx;

    if(self == NULL) {
        return;
    }

    if(self->fd >= 0) {
        (void)close(self->fd);
        self->fd = -1;
    }
}

static int posix_read(void *ctx, uint8_t *buffer, size_t capacity, int timeout_ms)
{
    car_port_posix_t *self = (car_port_posix_t *)ctx;

    if(self == NULL || self->fd < 0 || buffer == NULL || capacity == 0) {
        return -1;
    }

    struct pollfd pfd;
    pfd.fd = self->fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int ready = poll(&pfd, 1, timeout_ms);

    if(ready < 0) {
        return errno == EINTR ? 0 : -1;
    }

    if(ready == 0) {
        return 0;
    }

    if((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
        return -1;
    }

    if((pfd.revents & POLLIN) == 0) {
        return 0;
    }

    ssize_t received = read(self->fd, buffer, capacity);

    if(received < 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 0;
        }

        return -1;
    }

    return (int)received;
}

static int posix_write(void *ctx, const uint8_t *buffer, size_t len)
{
    car_port_posix_t *self = (car_port_posix_t *)ctx;
    size_t offset = 0;

    if(self == NULL || self->fd < 0 || (buffer == NULL && len != 0)) {
        return -1;
    }

    while(offset < len) {
        ssize_t written = write(self->fd, buffer + offset, len - offset);

        if(written > 0) {
            offset += (size_t)written;
            continue;
        }

        if(written < 0 && errno == EINTR) {
            continue;
        }

        if(written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            struct pollfd pfd;
            pfd.fd = self->fd;
            pfd.events = POLLOUT;
            pfd.revents = 0;

            if(poll(&pfd, 1, 50) > 0) {
                continue;
            }

            return -1;
        }

        return -1;
    }

    return (int)len;
}

static uint64_t posix_now(void *ctx)
{
    (void)ctx;
    return car_now_ms();
}

bool car_port_posix_attach(car_port_t *port,
                           car_port_posix_t *storage,
                           const char *device,
                           unsigned baud)
{
    if(port == NULL || storage == NULL) {
        return false;
    }

    memset(storage, 0, sizeof(*storage));
    storage->fd = -1;
    storage->baud = baud;

    if(device != NULL) {
        size_t device_len = strlen(device);

        if(device_len >= sizeof(storage->device)) {
            return false;
        }

        memcpy(storage->device, device, device_len + 1U);
    }

    port->ctx = storage;
    port->open = posix_open;
    port->close = posix_close;
    port->read = posix_read;
    port->write = posix_write;
    port->now_ms = posix_now;

    return true;
}

void car_port_posix_detach(car_port_t *port, car_port_posix_t *storage)
{
    if(storage != NULL) {
        posix_close(storage);
    }

    if(port != NULL) {
        memset(port, 0, sizeof(*port));
    }
}
