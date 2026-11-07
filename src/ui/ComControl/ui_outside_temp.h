/*
 * ui_outside_temp.h - 车外温度（所有样式通用）
 *
 * 对应 Qt ComControl/IconTemperature.qml：
 *   - 数值来自 car_other_info_t.outside_temp_raw，换算 (raw - 0x7C) × 0.5 ℃
 *   - Qt Handler.js 定义的三个特殊值：0xFF 隐藏 / 0xFE "--.-" / 0xFC "E"
 *   - 0 ℃ 与无数据时 Qt 显示 "--"
 *   - units.fahrenheit 为 true 时换算成华氏度
 */
#ifndef CARMW_UI_COM_OUTSIDE_TEMP_H
#define CARMW_UI_COM_OUTSIDE_TEMP_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 显示形态，用于跳过无变化的刷新 */
typedef enum {
    UI_TEMP_NORMAL = 0,   /* 正常数值 */
    UI_TEMP_HIDDEN,       /* 0xFF 隐藏 */
    UI_TEMP_DASH,         /* 0xFE --.- */
    UI_TEMP_ERROR,        /* 0xFC E */
    UI_TEMP_PLACEHOLDER   /* 无数据 / 0 ℃ 显示 -- */
} ui_temp_mode_t;

typedef struct {
    lv_obj_t *root;       /* 行容器 */
    lv_obj_t *value;      /* "22.0" */
    lv_obj_t *unit;       /* "°C" / "°F" */
    int16_t shown_x10;    /* 上次显示的数值（0.1 单位） */
    uint8_t shown_mode;   /* 上次显示的形态 */
    bool shown_fahren;    /* 上次使用的温标 */
    bool shown_valid;     /* 是否已有过有效值 */
} ui_outside_temp_t;

/* 创建温度行，x/y 为行容器左上角坐标 */
bool ui_outside_temp_create(ui_outside_temp_t *temp, lv_obj_t *parent,
                            int32_t x, int32_t y, const lv_font_t *font);

/* 应用车外温度；值未变时跳过 */
void ui_outside_temp_apply(ui_outside_temp_t *temp,
                           const car_other_info_t *info, bool fahrenheit);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_COM_OUTSIDE_TEMP_H */
