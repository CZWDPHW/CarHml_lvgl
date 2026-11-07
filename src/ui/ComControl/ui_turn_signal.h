/*
 * ui_turn_signal.h - 左/右转向灯（所有样式通用）
 *
 * 对应 Qt LampControl/IconTurnSignals.qml：
 *   左右各一张图，数据为 on/off，亮起时以 1s 周期闪烁；
 *   左右同时亮即为双闪。
 * 坐标与尺寸由样式层传入（Forester1 为左 (660,98)、右 (1206,98)，54×48）。
 */
#ifndef CARMW_UI_COM_TURN_SIGNAL_H
#define CARMW_UI_COM_TURN_SIGNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"
#include "ui/widgets/ui_lamp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ui_lamp_t left;
    ui_lamp_t right;
    lv_image_dsc_t left_dsc;   /* 需缩放，整图驻留 RAM */
    lv_image_dsc_t right_dsc;
    uint32_t blink_tick;      /* 当前完整周期起点，左右共用 */
    bool blinking;
} ui_turn_signal_t;

/* 创建左右转向灯，坐标为两张图的左上角，w/h 为目标尺寸 */
bool ui_turn_signal_create(ui_turn_signal_t *signal, lv_obj_t *parent,
                           int32_t left_x, int32_t left_y,
                           int32_t right_x, int32_t right_y,
                           int32_t w, int32_t h);

/* 周期调用：无效输入保持灯态；换向沿用相位，全关后重开立即亮。
 * apply 后 left/right 的 ON/OFF 即本次可见状态，供车模同步使用。 */
void ui_turn_signal_apply(ui_turn_signal_t *signal, const car_turn_t *turn);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_COM_TURN_SIGNAL_H */
