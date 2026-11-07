/*
 * fuel_forester1.c - Style1 油量表实现
 *
 * 坐标取自 Qt FuelTank/ForesterFuelTank1.qml：
 *   容器 (0,0) 内：底图 0,0；刻度 x=100，y=108+(210-210×p/100)，130x210
 *   油枪图标 (66,316)，F (198,86) 白色，E (108,320) 红色
 */
#include "ui/FuelTank/fuel_forester1.h"

#include "ui/ui_assets.h"
#include "ui/ui_theme.h"

#define FUEL_F1_W 250
#define FUEL_F1_H 368

/* 刻度条参数，与 Qt 中 sourceClipRect 一致 */
#define FUEL_F1_TICK_X 100

/* 创建底色、图标与 F/E 标记等静态元素 */
static void fuel_static_create(lv_obj_t *root)
{
    lv_obj_t *img = lv_image_create(root);
    lv_obj_t *label;

    lv_image_set_src(img, ui_assets_path("SubaruForester/fuel1_bg.bin"));
    lv_obj_set_pos(img, 0, 0);

    img = lv_image_create(root);
    lv_image_set_src(img, ui_assets_path("SubaruForester/fuel_44.bin"));
    lv_obj_set_pos(img, 66, 316);

    /* F：满油端，白色 */
    label = lv_label_create(root);
    lv_label_set_text(label, "F");
    lv_obj_set_style_text_font(label, UI_FONT_MD, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(UI_COLOR_WHITE), 0);
    lv_obj_set_pos(label, 198, 86);

    /* E：空油端，红色 */
    label = lv_label_create(root);
    lv_label_set_text(label, "E");
    lv_obj_set_style_text_font(label, UI_FONT_MD, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(UI_COLOR_RED), 0);
    lv_obj_set_pos(label, 108, 320);
}

bool fuel_forester1_create(fuel_forester1_t *fuel, lv_obj_t *parent,
                           int32_t x, int32_t y)
{
    if(fuel == NULL) {
        return false;
    }

    fuel->low = false;

    fuel->root = ui_blank_container(parent);
    lv_obj_set_size(fuel->root, FUEL_F1_W, FUEL_F1_H);
    lv_obj_set_pos(fuel->root, x, y);

    fuel_static_create(fuel->root);

    /* 初始用白色刻度图，0% 时液面在底部 */
    if(!ui_tick_create(&fuel->tick, fuel->root,
                       "SubaruForester/fuel1_white_tick.bin",
                       FUEL_F1_TICK_X, 318, 108, 130, 210)) {
        return false;
    }

    return true;
}

void fuel_forester1_apply(fuel_forester1_t *fuel, uint8_t percent)
{
    bool low;

    if(fuel == NULL || fuel->root == NULL) {
        return;
    }

    /* Qt 为 fuelPercentage > 25 ? 白 : 红，即 25% 本身已经是红色 */
    low = percent <= FUEL_F1_LOW_PERCENT;

    /* 跨过 25% 阈值时换图，Qt 为 fuelPercentage > 25 ? 白 : 红 */
    if(low != fuel->low) {
        fuel->low = low;
        lv_image_set_src(fuel->tick.img,
                         ui_assets_path(low ? "SubaruForester/fuel1_tick.bin"
                                            : "SubaruForester/fuel1_white_tick.bin"));
    }

    ui_tick_set_percent(&fuel->tick, percent);
}