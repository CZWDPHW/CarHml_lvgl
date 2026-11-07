/*
 * ui_assets.h - UI 资源访问
 *
 * 两部分能力：
 *   1) 路径拼接：把资源相对逻辑路径转成 PC(bin32) / 板端(bin16) 的实际路径
 *   2) 图片缓存：需要旋转/缩放等变换的图片，必须整图驻留 RAM 才能绘制，
 *      这里做一次性加载 + 幂等复用，跨样式切换不会重复分配
 */
#ifndef CARMW_UI_ASSETS_H
#define CARMW_UI_ASSETS_H

#include <stdbool.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 资源根目录，含结尾斜杠：PC 为 "A:/bin32/"，板端为 "A:/bin16/" */
const char *ui_assets_root(void);

/*
 * 取资源相对路径（如 "SubaruForester/speed_bg1.bin"）对应的真实路径。
 * 返回指向内部静态缓冲区的指针，下一次调用会被覆盖，不要长期保存。
 */
const char *ui_assets_path(const char *rel);

/*
 * 把 bin 图片整读进 RAM，生成可直接变换绘制的图像描述符。
 * dsc 由调用方提供并长期持有；已加载过则直接返回 true。
 * 加载的像素缓冲不在本模块释放，与程序生命周期一致。
 */
bool ui_assets_load_image(lv_image_dsc_t *dsc, const char *rel);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_ASSETS_H */