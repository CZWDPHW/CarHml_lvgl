#ifndef CARMW_CAR_MD5_H
#define CARMW_CAR_MD5_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef struct {
    uint32_t state[4];
    uint64_t bytes;
    uint8_t block[64];
} car_md5_t;
/* Qt packages use MD5 for accidental corruption detection. */
void car_md5_init(car_md5_t *ctx);
void car_md5_update(car_md5_t *ctx, const void *data, size_t size);
void car_md5_final(car_md5_t *ctx, char hex[33]);
bool car_md5_file(const char *path, char hex[33]);
bool car_md5_read_sidecar(const char *path, char hex[33]);
#endif
