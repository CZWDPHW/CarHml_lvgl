/*
 * ui_common.h - UI 公共基础：设计尺寸、资源路径、页面根容器
 */
#ifndef CARMW_UI_COMMON_H
#define CARMW_UI_COMMON_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Qt 主窗口设计尺寸 */
#define UI_DESIGN_W 1920
#define UI_DESIGN_H 720

/* Qt 的 Page 统一 scale: 0.975，LVGL 用 250/256 ≈ 0.9766 近似 */
#define UI_PAGE_SCALE 250

/* PNG 源图由 LVGL 内置解码器按需读取 */
#define UI_ASSET_ROOT "A:/src/"
#define UI_IMG(path) UI_ASSET_ROOT path

/*
 * 创建铺满屏幕、按 Qt 设计坐标摆放的页面容器。
 * 之后的控件都直接使用 Qt 里的原始 x/y/width/height。
 */
lv_obj_t *ui_page_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_COMMON_H */