
/*
 * rotate_forester1.c - Style1 转速表实现
 *
 * 坐标与角度公式取自 Qt RotationDial/ForesterRotate1.qml：
 *   指针角度 = 180° - rpm × 0.024
 *   刻度高亮：rpm 落在 (n×1000-200, n×1000+200] 时把该刻度放大
 *
 * LVGL 旋转角度单位为 0.1° 且正值顺时针，
 * 180° 写成 1800，公式为 1800 - rpm × 6 / 25（0.024 = 6/25，整数运算避免浮点）。
 */
#include "ui/RotationDial/rotate_forester1.h"

#include <stddef.h>

#include "ui/ui_assets.h"
#include "ui/ui_theme.h"
#include "ui/widgets/ui_dial.h"

#define ROTATE_F1_SIZE 620

/* 指针图按 alpha 包围盒裁剪：源图 620x620，(278,71)-(342,338) -> 64x267。
 * 表盘中心 (310,310)，因此裁剪图摆放 (278,71)、轴心 (32,239)。
 * 裁剪由 tools/png2bin.py 的 CROP_TO_ALPHA_BBOX 执行，两端常量必须同步。 */
#define NEEDLE_X 278
#define NEEDLE_Y 71
#define NEEDLE_PIVOT_X 32
#define NEEDLE_PIVOT_Y 239

/* 1~8 刻度文本位置，取自 Qt 中各 Label 的 x/y */
static const ui_dial_label_t s_labels[ROTATE_F1_LABEL_COUNT] = {
    {396, 480, "1"},
    {448, 420, "2"},
    {492, 340, "3"},
    {500, 254, "4"},
    {470, 175, "5"},
    {414, 106, "6"},
    {340, 76, "7"},
    {240, 76, "8"},
};

/* 指针需整图驻留 RAM 才能旋转绘制，跨样式切换只加载一次 */
static lv_image_dsc_t s_needle;

/* 判断第 index 档是否高亮：对应 rpm 区间为 (index×1000-200, index×1000+200] */
static bool is_hit(size_t index, uint32_t engine_rpm)
{
    int32_t rpm = (int32_t)engine_rpm;
    int32_t base = (int32_t)(index + 1U) * 1000;

    return rpm > base - 200 && rpm <= base + 200;
}

bool rotate_forester1_create(rotate_forester1_t *rotate, lv_obj_t *parent,
                             int32_t x, int32_t y)
{
    if(rotate == NULL) {
        return false;
    }

    rotate->highlight = -1;

    if(!ui_assets_load_image(&s_needle, "SubaruForester/rotate_point1.bin")) {
        return false;
    }

    ui_dial_create(&rotate->dial, parent, x, y, ROTATE_F1_SIZE,
                   "SubaruForester/rotate1_bg.bin", &s_needle,
                   NEEDLE_X, NEEDLE_Y, NEEDLE_PIVOT_X, NEEDLE_PIVOT_Y, 1800);

    ui_dial_add_text(&rotate->dial, "x1000/min", 270, 350, UI_FONT_SM);

    /* "0" 位置固定，Qt 里恒为 48 号字（rpm 极低时会变 48，正常也是最大档） */
    rotate->zero = ui_dial_add_text(&rotate->dial, "0", 290, 500, UI_FONT_3XL);

    ui_dial_add_labels(&rotate->dial, s_labels, ROTATE_F1_LABEL_COUNT, UI_FONT_2XL,
                       rotate->labels, ROTATE_F1_LABEL_COUNT);

    return true;
}

void rotate_forester1_apply(rotate_forester1_t *rotate, uint32_t engine_rpm)
{
    int32_t target = -1;

    if(rotate == NULL || rotate->dial.needle == NULL) {
        return;
    }

    /* 指针角度：180° - rpm × 0.024，换成 LVGL 的 0.1° 单位 */
    ui_dial_set_angle(&rotate->dial, 1800 - (int32_t)engine_rpm * 6 / 25);

    /* 找出当前转速命中的刻度 */
    for(size_t i = 0; i < ROTATE_F1_LABEL_COUNT; i++) {
        if(is_hit(i, engine_rpm)) {
            target = (int32_t)i;
        }
    }

    /* 刻度没有变化就到此为止，避免每帧重设样式 */
    if(target == rotate->highlight) {
        return;
    }

    rotate->highlight = target;

    for(size_t i = 0; i < ROTATE_F1_LABEL_COUNT; i++) {
        const lv_font_t *font = ((int32_t)i == target) ? UI_FONT_3XL : UI_FONT_2XL;

        lv_obj_set_style_text_font(rotate->labels[i], font, 0);
    }
}
