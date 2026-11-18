#ifndef CARMW_CAR_YMODEM_H
#define CARMW_CAR_YMODEM_H
#include "carmw/car_port.h"
#include <stdio.h>
typedef struct {
    car_port_t *port; /* Exclusive owner; normal car_link must already be stopped. */
    void *user;
    bool (*cancelled)(void *user);
    void (*progress)(void *user, unsigned percent);
    unsigned retries; /* 0 uses Qt's 4000 attempts per data block. */
} car_ymodem_t;
typedef enum {
    CAR_YMODEM_OK=0, CAR_YMODEM_IO=-1, CAR_YMODEM_TIMEOUT=-2,
    CAR_YMODEM_CANCELLED=-3, CAR_YMODEM_INVALID=-4, CAR_YMODEM_MD5=-5
} car_ymodem_result_t;
uint16_t car_ymodem_crc16(const uint8_t *data, size_t size);
/* Includes EOT/ACK and empty final header; data acknowledgement is 'C', as in Qt. */
car_ymodem_result_t car_ymodem_send(car_ymodem_t *ctx, FILE *file,
    const char *basename, uint32_t size, const char expected_md5[33]);
#endif
