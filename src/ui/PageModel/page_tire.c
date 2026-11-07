/*
 * page_tire.c - 中心卡片：胎压监测（Subaru 样式，对应 SubaruTirePressure.qml）
 *
 * Qt 坐标链：
 *   Loader 586×509 居中于容器 → 卡片在容器内 (114,0)
 *   标题 "胎压信息" 36 号、宽 486、topMargin 25、水平居中
 *   胎压区 432×320：水平居中，y 固定 58（不垂直居中）→ 卡片内 (77,58)
 *   车体底图 396×386 水平居中于胎压区 → 卡片内 (95,58)
 *   四张异常高亮图与底图同尺寸同位置，胎压状态 > 1（偏低/偏高）时叠加显示
 *   四个文字块 (52,77)/(328,77)/(52,233)/(328,233)：
 *     左列 Row 固定 -18 → 文字起点 34；右列 leftMagrin = -32 → 文字起点 296
 *
 * 温度行 Qt 有（22 号，"%1℃"，温度为 0 时隐藏），carmw 暂无胎温数据，故不建。
 */
#include "ui/PageModel/page_tire.h"

#include <stdio.h>

#include "ui/ui_assets.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"

#define CARD_X   114
#define CARD_Y   0
#define CARD_W   586
#define CARD_H   509

#define TITLE_TOP   25
#define AREA_W      432
#define AREA_H      320
#define AREA_X      ((CARD_W - AREA_W) / 2)        /* 77 */
#define AREA_Y      58                            /* Qt: 水平居中，但不垂直居中 */

#define IMG_W       396
#define IMG_H       386
#define IMG_X       (AREA_X + (AREA_W - IMG_W) / 2)   /* 95 */
#define IMG_Y       AREA_Y

#define TIRE_NO_VALUE  0x0FFFU

/* bin 图：尺寸在 bin 头里，这里只设位置 */
static lv_obj_t *tire_image(lv_obj_t *parent, const char *path,
                            int32_t x, int32_t y)
{
    lv_obj_t *img = lv_image_create(parent);

    lv_image_set_src(img, path);
    lv_obj_set_pos(img, x, y);
    return img;
}

/* 一个胎压块：数值 + 单位，横向排列、单位底部对齐 */
static void block_create(tire_card_t *card, uint8_t i, lv_obj_t *area,
                         int32_t x, int32_t y)
{
    lv_obj_t *row = ui_blank_container(area);

       lv_obj_set_pos(row, x, y);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);

    card->value[i] = lv_label_create(row);
    lv_label_set_text(card->value[i], "---");
    lv_obj_set_style_text_font(card->value[i], &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(card->value[i], lv_color_white(), 0);

    card->unit[i] = lv_label_create(row);
    lv_label_set_text(card->unit[i], "kpa");
    lv_obj_set_style_text_font(card->unit[i], &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(card->unit[i], lv_color_white(), 0);
    lv_obj_set_style_pad_bottom(card->unit[i], 2, 0);   /* bottomMargin 2 */
}

bool tire_card_create(tire_card_t *card, lv_obj_t *parent)
{
    lv_obj_t *area;
    uint8_t i;

    if(card == NULL) {
        return false;
    }

    card->root = ui_blank_container(parent);
    lv_obj_add_flag(card->root, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(card->root, CARD_W, CARD_H);
    lv_obj_set_pos(card->root, CARD_X, CARD_Y);

    /* 标题：36 号、宽 486、水平居中、topMargin 25 */
    {
        lv_obj_t *title = lv_label_create(card->root);

        /* Qt 标题为「胎压信息」36 号中文；LVGL 自带字体无完整中文字形，改用英文标题 */
        lv_label_set_text(title, "TIRE PRESSURE");
        lv_obj_set_width(title, CARD_W - 100);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, TITLE_TOP);
    }

    area = ui_blank_container(card->root);
    lv_obj_set_size(area, AREA_W, AREA_H);
    lv_obj_set_pos(area, AREA_X, AREA_Y);

    /* 车体底图 + 四张异常高亮图（后者默认隐藏） */
    card->img_car = tire_image(card->root, UI_IMG("TirePressure/car.png"),
                               IMG_X, IMG_Y);

    card->img_warn[0] = tire_image(card->root,
                                   UI_IMG("TirePressure/left_front.png"),
                                   IMG_X, IMG_Y);
    card->img_warn[1] = tire_image(card->root,
                                   UI_IMG("TirePressure/right_front.png"),
                                   IMG_X, IMG_Y);
    card->img_warn[2] = tire_image(card->root,
                                   UI_IMG("TirePressure/left_after.png"),
                                   IMG_X, IMG_Y);
    card->img_warn[3] = tire_image(card->root,
                                   UI_IMG("TirePressure/right_after.png"),
                                   IMG_X, IMG_Y);

    block_create(card, 0U, area, 34, 77);      /* 左前 */
    block_create(card, 1U, area, 296, 77);     /* 右前（leftMagrin -32） */
    block_create(card, 2U, area, 34, 233);     /* 左后 */
    block_create(card, 3U, area, 296, 233);    /* 右后（leftMagrin -32） */

    for(i = 0U; i < 4U; ++i) {
        card->last_kpa[i] = 0xFFFFU;
        card->last_state[i] = 0xFFU;
        lv_obj_add_flag(card->img_warn[i], LV_OBJ_FLAG_HIDDEN);
    }
    card->last_unit = 0xFFU;

    return true;
}

/* 按单位换算并格式化；返回单位后缀 */
static const char *format_value(char *buf, size_t size, uint16_t kpa, uint8_t unit)
{
    uint32_t x10;

    if(kpa == TIRE_NO_VALUE) {
        (void)snprintf(buf, size, "---");
        return (unit == 1U) ? "bar" : (unit == 2U) ? "PSI" : "kpa";
    }

    switch(unit) {
    case 1U:   /* bar = kpa / 102 */
        x10 = ((uint32_t)kpa * 10U + 51U) / 102U;
        (void)snprintf(buf, size, "%u.%u",
                       (unsigned)(x10 / 10U), (unsigned)(x10 % 10U));
        return "bar";
    case 2U:   /* PSI = kpa / 7.03 */
        x10 = ((uint32_t)kpa * 1000U + 351U) / 703U;
        (void)snprintf(buf, size, "%u.%u",
                       (unsigned)(x10 / 10U), (unsigned)(x10 % 10U));
        return "PSI";
    default:
        (void)snprintf(buf, size, "%u", (unsigned)kpa);
        return "kpa";
    }
}

void tire_card_apply(tire_card_t *card, const car_state_t *state)
{
    uint8_t unit;
    uint8_t i;

    if(card == NULL) {
        return;
    }

    unit = state->units.valid ? state->units.pressure_unit : 0U;
    if(unit > 2U) {
        unit = 0U;
    }

    for(i = 0U; i < 4U; ++i) {
        const car_tire_t *tire = &state->tires.wheels[i];
        uint8_t warn = (tire->valid && tire->state > 1U) ? 1U : 0U;
        char buf[16];
        const char *suffix;

        /* 异常高亮：状态 2 偏低 / 3 偏高（Qt: visible: state > 1） */
        if(warn != card->last_state[i]) {
            card->last_state[i] = warn;

            if(warn != 0U) {
                lv_obj_remove_flag(card->img_warn[i], LV_OBJ_FLAG_HIDDEN);
            }
            else {
                lv_obj_add_flag(card->img_warn[i], LV_OBJ_FLAG_HIDDEN);
            }
        }

        suffix = format_value(buf, sizeof(buf),
                              tire->valid ? tire->kpa : TIRE_NO_VALUE, unit);

        if(tire->valid && tire->kpa == card->last_kpa[i] &&
           unit == card->last_unit) {
            continue;
        }

        card->last_kpa[i] = tire->valid ? tire->kpa : TIRE_NO_VALUE;
        lv_label_set_text(card->value[i], buf);
        lv_label_set_text(card->unit[i], suffix);
    }

    card->last_unit = unit;
}

void tire_card_destroy(tire_card_t *card)
{
    if(card != NULL && card->root != NULL) {
        lv_obj_delete(card->root);
        card->root = NULL;
    }
}