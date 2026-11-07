/*
 * ui_png.c - 最小 PNG 写出（仅 PC 截图用，不参与板端构建）
 *
 * 输入为 ARGB8888 像素，输出 8bit RGB 或 RGBA 非隔行 PNG。
 * 压缩用 zlib 的 compress2（LVGL PC 构建已依赖 zlib 转换器，见 CMake 的 ZLIB 查找）。
 */
#include "ui/ui_png.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static bool write_chunk(FILE *f, const char *tag, const uint8_t *data, uint32_t len)
{
    uint8_t buf[4];
    uLong crc;

    put_u32(buf, len);
    if(fwrite(buf, 1, 4, f) != 4 || fwrite(tag, 1, 4, f) != 4) {
        return false;
    }
    if(len > 0 && fwrite(data, 1, len, f) != len) {
        return false;
    }
    crc = crc32(0L, (const Bytef *)tag, 4);
    if(len > 0) {
        crc = crc32(crc, (const Bytef *)data, len);
    }
    put_u32(buf, (uint32_t)crc);
    return fwrite(buf, 1, 4, f) == 4;
}

bool ui_png_write(const char *path, const uint32_t *argb, int32_t w, int32_t h)
{
    FILE *f;
    uint8_t ihdr[13];
    uint8_t *raw;
    uint8_t *z;
    uLongf zlen;
    size_t stride = (size_t)w * 4U;
    size_t raw_len = ((size_t)w * 4U + 1U) * (size_t)h;
    bool ok = false;
    int32_t x, y;

    if(path == NULL || argb == NULL || w <= 0 || h <= 0) {
        return false;
    }

    raw = (uint8_t *)malloc(raw_len);
    if(raw == NULL) {
        return false;
    }

    /* 逐行前缀滤波类型 0（None），像素按 A R G B 顺序写出 */
    for(y = 0; y < h; ++y) {
        uint8_t *row = raw + (size_t)y * (stride + 1U);
        const uint32_t *src = argb + (size_t)y * (size_t)w;

        row[0] = 0U;
        for(x = 0; x < w; ++x) {
            uint32_t px = src[x];
            uint8_t *d = row + 1U + (size_t)x * 4U;

            d[0] = (uint8_t)((px >> 16) & 0xFFU);
            d[1] = (uint8_t)((px >> 8) & 0xFFU);
            d[2] = (uint8_t)(px & 0xFFU);
            d[3] = (uint8_t)((px >> 24) & 0xFFU);
        }
    }

    zlen = compressBound((uLong)raw_len);
    z = (uint8_t *)malloc((size_t)zlen);
    if(z == NULL) {
        free(raw);
        return false;
    }
    if(compress2(z, &zlen, raw, (uLong)raw_len, 9) != Z_OK) {
        free(z);
        free(raw);
        return false;
    }
    free(raw);

    f = fopen(path, "wb");
    if(f == NULL) {
        free(z);
        return false;
    }

    put_u32(ihdr, (uint32_t)w);
    put_u32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8U;    /* bit depth */
    ihdr[9] = 6U;    /* color type: RGBA */
    ihdr[10] = 0U;   /* compression */
    ihdr[11] = 0U;   /* filter */
    ihdr[12] = 0U;   /* interlace */

    ok = fwrite("\x89PNG\r\n\x1a\n", 1, 8, f) == 8 &&
         write_chunk(f, "IHDR", ihdr, sizeof(ihdr)) &&
         write_chunk(f, "IDAT", z, (uint32_t)zlen) &&
         write_chunk(f, "IEND", NULL, 0);

    fclose(f);
    free(z);
    return ok;
}
