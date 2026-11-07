/*
 * ui_turn_signal.c - 左/右转向灯实现
 *
 * 完整周期 1000ms，apply 统一计算左右灯相位；车模复用本次灯态，
 * 不启动 ui_lamp 的独立闪烁定时器。
 */
#include "ui/ComControl/ui_turn_signal.h"

#include "ui/ui_assets.h"

#define TURN_BLINK_MS 1000U

bool ui_turn_signal_create(ui_turn_signal_t *signal, lv_obj_t *parent,
                           int32_t left_x, int32_t left_y,
                           int32_t right_x, int32_t right_y,
                           int32_t w, int32_t h)
{
    if(signal == NULL) {
        return false;
    }

    signal->blinking = false;
    signal->blink_tick = 0U;

    if(!ui_assets_load_image(&signal->left_dsc, "WarnIcon/left_lamp.bin") ||
       !ui_assets_load_image(&signal->right_dsc, "WarnIcon/right_lamp.bin")) {
        return false;
    }

    ui_lamp_create_sized(&signal->left, parent, &signal->left_dsc,
                         left_x, left_y, w, h);
    ui_lamp_create_sized(&signal->right, parent, &signal->right_dsc,
                         right_x, right_y, w, h);

    return true;
}

void ui_turn_signal_apply(ui_turn_signal_t *signal, const car_turn_t *turn)
{
    uint32_t now;
    uint32_t elapsed;
    bool on = false;

    if(signal == NULL || turn == NULL || !turn->valid) {
        return;
    }

    now = lv_tick_get();
    if(turn->left || turn->right) {
        if(!signal->blinking) {
            signal->blinking = true;
            signal->blink_tick = now;
        }
        /* 保留完整周期的余量，调度延迟不累积漂移，uint32_t 减法兼容 tick 回绕。 */
        elapsed = now - signal->blink_tick;
        signal->blink_tick += (elapsed / TURN_BLINK_MS) * TURN_BLINK_MS;
        on = (elapsed % TURN_BLINK_MS) < TURN_BLINK_MS / 2U;
    }
    else {
        signal->blinking = false;
    }

    ui_lamp_set_state(&signal->left, turn->left && on ? UI_LAMP_ON : UI_LAMP_OFF);
    ui_lamp_set_state(&signal->right, turn->right && on ? UI_LAMP_ON : UI_LAMP_OFF);
}
