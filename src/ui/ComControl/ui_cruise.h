/*
 * ui_cruise.h - 定速巡航指示（所有样式通用）
 *
 * 对应 Qt ComControl/IconConstantSpeed.qml：
 *   Row spacing 2：52×52 图标 + 24 号数值 + 14 号单位，文字贴行底。
 *   图标默认 fixed_speed_green；故障码 31/95 绿、99 ACC 绿、239 ACC 白、
 *   240 定速白、241 ACC ready。
 *   图标在“有故障码”或“设定速度 > 0”时显示；数值和单位只在设定速度 > 0 时显示。
 */
#ifndef CARMW_UI_COM_CRUISE_H
#define CARMW_UI_COM_CRUISE_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"
#include "ui/widgets/ui_lamp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Qt LightTemplate 的默认图标尺寸 */
#define UI_CRUISE_ICON_SIZE 52

/* 图标槽位，与 IconConstantSpeed 的故障码一一对应 */
typedef enum {
    UI_CRUISE_ICON_GREEN = 0,  /* 31 / 95 定速巡航绿 */
    UI_CRUISE_ICON_ACC_GREEN,  /* 99 ACC 绿 */
    UI_CRUISE_ICON_ACC_WHITE,  /* 239 ACC 白 */
    UI_CRUISE_ICON_WHITE,      /* 240 定速巡航白 */
    UI_CRUISE_ICON_ACC_READY,  /* 241 ACC ready */
    UI_CRUISE_ICON_COUNT
} ui_cruise_icon_t;

typedef struct {
    lv_obj_t *root;      /* 行容器，高 52 */
    lv_obj_t *value;     /* 设定速度数值 */
    lv_obj_t *unit;      /* km/h / mile/h */
    ui_lamp_t lamp;      /* 巡航图标 */
    lv_image_dsc_t dsc[UI_CRUISE_ICON_COUNT]; /* 按需加载，整图驻留 RAM */
    uint8_t icon;        /* 当前图标槽位 */
    uint16_t shown_speed; /* 上次显示的数值（已换算单位） */
    bool shown_mile;      /* 上次使用的单位制 */
    bool shown_text;      /* 上次文字是否可见 */
    bool shown_valid;
} ui_cruise_t;

/*
 * 创建定速巡航指示，x/y 为 52 高行容器的左上角。
 * value_font 用 24 号，unit_font 用 14 号，与 Qt 一致。
 */
bool ui_cruise_create(ui_cruise_t *cruise, lv_obj_t *parent, int32_t x, int32_t y,
                      const lv_font_t *value_font, const lv_font_t *unit_font);

/* 按车状态或故障码刷新；无变化时跳过文字重设 */
void ui_cruise_apply(ui_cruise_t *cruise, const car_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_COM_CRUISE_H */