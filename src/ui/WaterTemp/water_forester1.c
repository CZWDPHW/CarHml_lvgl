/*
 * water_forester1.c - Style1 水温表实现
 *
 * 坐标取自 Qt WaterTemp/SubaruForesterWater1.qml：
 *   底图 0,0；刻度 x=40，y=108+(210-210×p/100)，120x210
 *   温度计图标：正常 (144,308)，高温 (147,312)
 *   H (46,86) 红色，C (126,320) 白色
 */
#include "ui/WaterTemp/water_forester1.h"

#include "ui/ui_assets.h"
#include "ui/ui_theme.h"

#define WATER_F1_W 250
#define WATER_F1_H 368

/* 刻度条参数，与 Qt 中 sourceClipRect 一致 */
#define WATER_F1_TICK_X 40

/* 图标资源：正常温度计 / 高温告警 */
#define WATER_F1_ICON_NORMAL "SubaruForester/wather_44.png"
#define WATER_F1_ICON_HIGH   "material2/RotateSheet/water_hig.png"

/* 创建底色、H/C 标记等静态元素 */
static void water_static_create(lv_obj_t *root)
{
    lv_obj_t *img = lv_image_create(root);
    lv_obj_t *label;

    lv_image_set_src(img, ui_assets_path("SubaruForester/wather1_bg.png"));
    lv_obj_set_pos(img, 0, 0);

    /* H：高温端，红色 */
    label = lv_label_create(root);
    lv_label_set_text(label, "H");
    lv_obj_set_style_text_font(label, UI_FONT_MD, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(UI_COLOR_RED), 0);
    lv_obj_set_pos(label, 46, 86);

    /* C：低温端，白色 */
    label = lv_label_create(root);
    lv_label_set_text(label, "C");
    lv_obj_set_style_text_font(label, UI_FONT_MD, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(UI_COLOR_WHITE), 0);
    lv_obj_set_pos(label, 126, 320);
}

bool water_forester1_create(water_forester1_t *water, lv_obj_t *parent,
                            int32_t x, int32_t y)
{
    if(water == NULL) {
        return false;
    }

    water->abnormal = false;

    water->root = ui_blank_container(parent);
    lv_obj_set_size(water->root, WATER_F1_W, WATER_F1_H);
    lv_obj_set_pos(water->root, x, y);

    water_static_create(water->root);

    if(!ui_tick_create(&water->tick, water->root,
                       "SubaruForester/wather1_tick.png",
                       WATER_F1_TICK_X, 318, 108, 120, 210)) {
        return false;
    }

    water->icon = lv_image_create(water->root);
    lv_image_set_src(water->icon, ui_assets_path(WATER_F1_ICON_NORMAL));
    lv_obj_set_pos(water->icon, 144, 308);

    return true;
}

void water_forester1_apply(water_forester1_t *water, uint8_t percent,
                           bool abnormal)
{
    if(water == NULL || water->root == NULL) {
        return;
    }

    /* 只有状态翻转时才换图移位，避免每帧重复设置 */
    if(abnormal != water->abnormal) {
        water->abnormal = abnormal;

        if(abnormal) {
            lv_image_set_src(water->icon, ui_assets_path(WATER_F1_ICON_HIGH));
            lv_obj_set_pos(water->icon, 147, 312);
        }
        else {
            lv_image_set_src(water->icon, ui_assets_path(WATER_F1_ICON_NORMAL));
            lv_obj_set_pos(water->icon, 144, 308);
        }
    }

    ui_tick_set_percent(&water->tick, percent);
}