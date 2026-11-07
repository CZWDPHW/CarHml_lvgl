/*
 * ui_theme.h - UI 主题常量
 *
 * 颜色、字号、通用尺寸集中在这里。
 * 样式文件不再直接写 lv_color_hex(0x...) 与字号数字，
 * 后续做明暗主题时只需改这一处。
 */
#ifndef CARMW_UI_THEME_H
#define CARMW_UI_THEME_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- 颜色 ---------- */
#define UI_COLOR_WHITE  0xFFFFFF
#define UI_COLOR_BLACK  0x000000
#define UI_COLOR_RED    0xDC143C  /* 警示/刻度热端 */
#define UI_COLOR_GRAY   0x9A9A9A  /* 次级文字 */

/* ---------- 字号 ---------- */
#define UI_FONT_SM  (&lv_font_montserrat_20)
#define UI_FONT_MD  (&lv_font_montserrat_24)
#define UI_FONT_LG  (&lv_font_montserrat_28)
#define UI_FONT_XL  (&lv_font_montserrat_36)
#define UI_FONT_2XL (&lv_font_montserrat_44)
#define UI_FONT_3XL (&lv_font_montserrat_48)

/*
 * 全屏透明容器：去掉默认样式与滚动，尺寸铺满父对象。
 * 大量控件都是“无样式容器”，抽出来避免每个创建函数重复写三行。
 */
static inline lv_obj_t *ui_blank_container(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);

    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    return obj;
}

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_THEME_H */