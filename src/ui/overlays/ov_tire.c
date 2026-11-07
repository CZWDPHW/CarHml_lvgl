/*
 * ov_tire.c - 覆盖层：胎压异常
 *
 * 复用中心卡片的 tire_card_t：外层容器 814×509 放在 (554,150)，
 * 卡片自身的 (114,0) 偏移正好把 586×509 落到页面 (668,150)，与 Qt mainVisible 一致。
 *
 * 显隐：新异常胎压帧触发并续期 10s；正常帧或到期后关闭。
 * 事件计时存放在 ov_tire_event_t（由 ui_session 持有），跨样式切换不丢失。
 * suppressed 由更高优先级的覆盖层（车门）传入，不暂停计时。
 */
#include "ui/overlays/ov_tire.h"

#include "ui/ui_common.h"
#include "ui/ui_theme.h"

#define OV_X 554
#define OV_Y 150
#define OV_W 814
#define OV_H 509

bool ov_tire_create(ov_tire_t *ov, lv_obj_t *parent)
{
    if(ov == NULL) {
        return false;
    }

    ov->root = ui_blank_container(parent);
    lv_obj_set_size(ov->root, OV_W, OV_H);
    lv_obj_set_pos(ov->root, OV_X, OV_Y);

    if(!tire_card_create(&ov->card, ov->root)) {
        lv_obj_delete(ov->root);
        ov->root = NULL;
        return false;
    }

    lv_obj_add_flag(ov->root, LV_OBJ_FLAG_HIDDEN);
    ov->active = 0U;

    return true;
}

void ov_tire_event_reset(ov_tire_event_t *ev)
{
    if(ev == NULL) {
        return;
    }

    ev->last_sequence = 0U;
    ev->event_tick = 0U;
    ev->seen_frame = false;
    ev->pending = false;
}

void ov_tire_event_apply(ov_tire_event_t *ev, const car_state_t *state)
{
    bool abnormal = false;

    if(ev == NULL || state == NULL) {
        return;
    }

    /* 数据源中断：清除进度，恢复后的首个有效帧按新事件处理（含序号重启）。 */
    if(!state->tires.valid) {
        ev->pending = false;
        ev->seen_frame = false;
        return;
    }

    /* 序号随成功收帧递增：相同内容的新帧续期，重复刷新旧快照不续期。 */
    if(!ev->seen_frame || state->tires.sequence != ev->last_sequence) {
        ev->last_sequence = state->tires.sequence;
        ev->seen_frame = true;
        for(uint8_t i = 0U; i < 4U; ++i) {
            if(state->tires.wheels[i].valid && state->tires.wheels[i].state > 1U) {
                abnormal = true;
            }
        }
        ev->pending = abnormal;
        if(abnormal) {
            ev->event_tick = lv_tick_get();
        }
    }

    if(ev->pending && lv_tick_elaps(ev->event_tick) >= OV_TIRE_HOLD_MS) {
        ev->pending = false;
    }
}

bool ov_tire_event_is_active(const ov_tire_event_t *ev)
{
    return (ev != NULL && ev->pending);
}

void ov_tire_apply(ov_tire_t *ov, const car_state_t *state, bool suppressed,
                   const ov_tire_event_t *ev)
{
    uint8_t active;

    if(ov == NULL || state == NULL) {
        return;
    }

    tire_card_apply(&ov->card, state);

    /* 被车门遮挡时仍消费新帧和计时；解除遮挡只恢复尚未到期的事件。 */
    active = (!suppressed && ov_tire_event_is_active(ev)) ? 1U : 0U;
    if(active != ov->active) {
        if(active != 0U) {
            lv_obj_remove_flag(ov->root, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(ov->root, LV_OBJ_FLAG_HIDDEN);
        }
        ov->active = active;
    }
}

bool ov_tire_is_active(const ov_tire_t *ov)
{
    return (ov != NULL && ov->active != 0U);
}

void ov_tire_destroy(ov_tire_t *ov)
{
    if(ov != NULL) {
        tire_card_destroy(&ov->card);
        if(ov->root != NULL) {
            lv_obj_delete(ov->root);
            ov->root = NULL;
        }
        ov->active = 0U;
    }
}
