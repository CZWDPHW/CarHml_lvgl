/*
 * ui_assets.h - UI PNG 资源路径访问
 */
#ifndef CARMW_UI_ASSETS_H
#define CARMW_UI_ASSETS_H

#include <stdbool.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PNG 资源根目录，含结尾斜杠 */
const char *ui_assets_root(void);

/*
 * 取资源相对路径（如 "SubaruForester/speed_bg1.png"）对应的真实路径。
 * 返回指向内部静态缓冲区的指针，下一次调用会被覆盖，不要长期保存。
 */
const char *ui_assets_path(const char *rel);
bool ui_assets_load_image(lv_image_dsc_t *dsc, const char *rel);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_ASSETS_H */