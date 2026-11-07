/*
 * ui_range.h - 剩余续航（所有样式通用）
 *
 * 数据来自 car_fuel_t.remaining_range_km。
 * Qt 原文前缀为“续航里程:”，中文字库就绪前先用英文占位。
 */
#ifndef CARMW_UI_COM_RANGE_H
#define CARMW_UI_COM_RANGE_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;        /* 行容器 */
    lv_obj_t *prefix;      /* "RANGE:" */
    lv_obj_t *value;       /* "250 KM" / "155.3 MI" / "- - KM" */
    uint16_t shown_km;     /* 上次显示的公里数 */
    bool shown_mile;       /* 上次是否英里制 */
    bool shown_valid;      /* 是否已有过有效值 */
} ui_range_t;

/* 创建续航行，x/y 为行容器左上角坐标 */
bool ui_range_create(ui_range_t *range, lv_obj_t *parent, int32_t x, int32_t y,
                     const lv_font_t *font);

/*
 * 按剩余续航公里数刷新；与上次相同时跳过。
 * remaining_km == 0xFFFF 为协议占位值，显示 "- -" 且不做单位换算（对齐 Qt）。
 */
void ui_range_apply(ui_range_t *range, uint16_t remaining_km, bool mile);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_COM_RANGE_H */
