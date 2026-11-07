/*
 * ui_mileage.c - 总里程 / 小计里程实现
 */
#include "ui/ComControl/ui_mileage.h"

#include <stdio.h>

#include "ui/ui_theme.h"

/* Qt 的 Row spacing 为 4，行内子项垂直居中 */
#define MILEAGE_F1_GAP 4

/* 创建横向排布的行容器 */
static lv_obj_t *row_create(lv_obj_t *parent, int32_t x, int32_t y)
{
    lv_obj_t *row = ui_blank_container(parent);

    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(row, x, y);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, MILEAGE_F1_GAP, 0);

    return row;
}

/* 行内创建一个白色粗体标签 */
static lv_obj_t *row_label(lv_obj_t *row, const char *text,
                           const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(row);

    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);

    return label;
}

bool ui_mileage_create(ui_mileage_t *mileage, lv_obj_t *parent, int32_t x,
                       int32_t y, ui_mileage_kind_t kind,
                       const lv_font_t *font)
{
    if(mileage == NULL) {
        return false;
    }

    mileage->root = row_create(parent, x, y);
    mileage->prefix = row_label(mileage->root,
                                kind == UI_MILEAGE_ODO ? "ODO:" : "Trip:",
                                font);
    mileage->value = row_label(mileage->root, "", font);

    mileage->kind = kind;
    mileage->shown_raw = UINT32_MAX;
    mileage->shown_key = 0xFF;
    mileage->shown_mile = false;

    return true;
}

void ui_mileage_apply(ui_mileage_t *mileage, const car_mileage_t *src,
                      bool mile)
{
    char buf[UI_MILEAGE_TEXT_MAX];
    const char *prefix;
    uint32_t raw;
    uint8_t key;

    if(mileage == NULL || mileage->value == NULL || src == NULL) {
        return;
    }

    /* 先按 kind 与 trip_type 决定显示哪一段数据 */
    if(mileage->kind == UI_MILEAGE_ODO) {
        prefix = "ODO:";
        raw = src->odometer_raw;
        key = 0;
    }
    else if(src->trip_type == 1) {
        /* trip_type：0 ODO / 1 TRIP A / 2 TRIP B */
        prefix = "Trip A:";
        raw = src->trip_a_raw;
        key = src->trip_type;
    }
    else if(src->trip_type == 2) {
        prefix = "Trip B:";
        raw = src->trip_b_raw;
        key = src->trip_type;
    }
    else {
        prefix = "Trip:";
        raw = 0;
        key = src->trip_type;
    }

    /* 数值、档位、单位都没变就跳过，避免每帧重建字符串 */
    if(raw == mileage->shown_raw && key == mileage->shown_key &&
       mile == mileage->shown_mile) {
        return;
    }

    mileage->shown_raw = raw;
    mileage->shown_key = key;
    mileage->shown_mile = mile;

    if(mileage->kind == UI_MILEAGE_ODO) {
        /* Qt ComTotalMileage：公里为整数，英里保留 1 位小数 */
        if(mile) {
            (void)snprintf(buf, sizeof(buf), "%.1f MI", (double)raw / 1.60934);
        }
        else {
            (void)snprintf(buf, sizeof(buf), "%lu KM", (unsigned long)raw);
        }
    }
    else {
        /* Qt ComSubTotalMileage：原始值以 0.1 km 为单位 */
        if(mile) {
            (void)snprintf(buf, sizeof(buf), "%.1f MI", (double)raw / 16.0934);
        }
        else {
            (void)snprintf(buf, sizeof(buf), "%.1f KM", (double)raw / 10.0);
        }
    }

    lv_label_set_text(mileage->prefix, prefix);
    lv_label_set_text(mileage->value, buf);
}