/*
 * ui_clock.h - 时钟（所有样式通用）
 *
 * 对应 Qt ComControl/PageSystemTimer.qml：显示“时:分”，冒号每秒闪烁。
 * 数据来自 car_clock_t，hour24 决定 12/24 小时制；未收到时间时显示 --.--。
 */
#ifndef CARMW_UI_COM_CLOCK_H
#define CARMW_UI_COM_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;        /* 行容器 */
    lv_obj_t *hour;        /* "12" */
    lv_obj_t *colon;       /* ":"（固定宽度，闪烁时只改透明度） */
    lv_obj_t *minute;      /* "34" */
    lv_timer_t *blink;     /* 冒号闪烁定时器，随根对象销毁一起释放 */
    uint8_t shown_hour;    /* 上次显示的时 */
    uint8_t shown_minute;  /* 上次显示的分 */
    bool shown_hour24;     /* 上次使用的时制 */
    bool shown_valid;      /* 是否已有过有效值 */
} ui_clock_t;

/* 创建时钟，x/y 为行容器左上角坐标 */
bool ui_clock_create(ui_clock_t *clock, lv_obj_t *parent, int32_t x, int32_t y,
                     const lv_font_t *font);

/* 应用车状态快照中的时间；值未变时跳过 */
void ui_clock_apply(ui_clock_t *clock, const car_clock_t *state, bool hour24);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_COM_CLOCK_H */
