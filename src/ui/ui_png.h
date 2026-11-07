/*
 * ui_png.h - 最小 PNG 写出（PC 截图用）
 */
#ifndef CARMW_UI_PNG_H
#define CARMW_UI_PNG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 把 ARGB8888 像素写成 PNG（8bit RGBA）。
 * path 为本地文件路径；成功返回 true。
 */
bool ui_png_write(const char *path, const uint32_t *argb, int32_t w, int32_t h);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_PNG_H */
