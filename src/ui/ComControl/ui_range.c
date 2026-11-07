/*
 * ui_range.c - 剩余续航实现
 */
#include "ui/ComControl/ui_range.h"

#include <stdio.h>

#include "ui/ui_theme.h"

#define RANGE_F1_TEXT_MAX 16

/* 行内创建一个白色标签 */
static lv_obj_t *row_label(lv_obj_t *row, const char *text,
                           const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(row);

    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);

    return label;
}

bool ui_range_create(ui_range_t *range, lv_obj_t *parent, int32_t x, int32_t y,
                     const lv_font_t *font)
{
    if(range == NULL) {
        return false;
    }

    range->root = ui_blank_container(parent);
    lv_obj_set_size(range->root, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(range->root, x, y);
    lv_obj_set_flex_flow(range->root, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(range->root, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* 中文字库就绪后这里换成 “续航里程: ” */
    range->prefix = row_label(range->root, "RANGE: ", font);
    range->value = row_label(range->root, "", font);

    range->shown_km = 0;
    range->shown_mile = false;
    range->shown_valid = false;

    return true;
}

void ui_range_apply(ui_range_t *range, uint16_t remaining_km, bool mile)
{
    char buf[RANGE_F1_TEXT_MAX];

    if(range == NULL || range->value == NULL) {
        return;
    }

    if(range->shown_valid && range->shown_km == remaining_km &&
       range->shown_mile == mile) {
        return;
    }

    range->shown_km = remaining_km;
    range->shown_mile = mile;
    range->shown_valid = true;

    /* 0xFFFF 是 Qt 的不可用占位，英里制也不能换算成巨大数值 */
    if(remaining_km == UINT16_MAX) {
        (void)snprintf(buf, sizeof(buf), "- - %s", mile ? "MI" : "KM");
    }
    else if(mile) {
        (void)snprintf(buf, sizeof(buf), "%.1f MI",
                       (double)remaining_km / 1.60934);
    }
    else {
        (void)snprintf(buf, sizeof(buf), "%u KM", (unsigned)remaining_km);
    }

    lv_label_set_text(range->value, buf);
}