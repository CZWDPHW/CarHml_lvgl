/*
 * ui_lamp.h - 警示灯 / 指示灯控件
 *
 * 一个灯 = 一张图标 + 一个显示状态。
 * 状态由样式层根据车状态算出来，控件只负责按状态渲染，
 * 不感知具体是哪个报警（那样会让通用控件耦合业务）。
 */
#ifndef CARMW_UI_LAMP_H
#define CARMW_UI_LAMP_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 灯的显示状态，与 Qt iconBindMap 的几种表现一一对应 */
typedef enum {
    UI_LAMP_OFF = 0,   /* 熄灭 */
    UI_LAMP_ON,        /* 常亮 */
    UI_LAMP_BLINK,     /* 闪烁 */
    UI_LAMP_SCAN,      /* 呼吸扫描：不透明度往复渐变 */
} ui_lamp_state_t;

/* 默认闪烁半周期：Qt 快闪 300ms */
#define UI_LAMP_BLINK_MS 300

typedef struct {
    lv_obj_t *img;      /* 图标对象 */
    uint8_t state;      /* 当前 ui_lamp_state_t */
    uint32_t blink_ms;  /* 闪烁周期，默认 300ms，转向灯用 1000ms */
    lv_timer_t *blink;  /* 闪烁定时器，仅闪烁期间存在 */
} ui_lamp_t;

/*
 * 在 parent 内创建灯：icon 为图片源，x/y 为左上角坐标，按图标原始尺寸显示。
 * 初始为熄灭状态。
 */
void ui_lamp_create(ui_lamp_t *lamp, lv_obj_t *parent, const void *icon,
                    int32_t x, int32_t y);

/*
 * 按目标矩形创建灯，等比缩放居中（对应 Qt LightTemplate 的 PreserveAspectFit）。
 * icon 必须是 ui_assets_load_image 载入 RAM 的描述符：
 * 流式 bin 图不支持变换，只有 RAM 图才能缩放。
 */
void ui_lamp_create_sized(ui_lamp_t *lamp, lv_obj_t *parent,
                          const lv_image_dsc_t *icon,
                          int32_t x, int32_t y, int32_t w, int32_t h);

/* 设置闪烁周期；当前正在闪烁时会立即按新周期重启 */
void ui_lamp_set_blink_ms(ui_lamp_t *lamp, uint32_t ms);

/* 设置显示状态；与当前状态相同时不做任何事，避免每帧重启动画 */
void ui_lamp_set_state(ui_lamp_t *lamp, ui_lamp_state_t state);

/* 当前状态 */
ui_lamp_state_t ui_lamp_get_state(const ui_lamp_t *lamp);

/* 切换图标源（近光灯正常/故障图等），不改变当前显示状态 */
void ui_lamp_set_icon(ui_lamp_t *lamp, const void *icon);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_LAMP_H */