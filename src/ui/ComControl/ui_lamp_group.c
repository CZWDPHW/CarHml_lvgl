/*
 * ui_lamp_group.c - 绝对坐标警示灯组实现
 *
 * 灯态映射与 ui_lamp_row 一致：0 灭 / 1 常亮 / 2 快闪 / 3 慢闪。
 * 同一盏灯有多个图标时，取当前灯态最高的一档对应的图标；
 * 故障消失后保留最后一张图（与 Qt updateIconByFault 的行为一致）。
 */
#include "ui/ComControl/ui_lamp_group.h"

#include "ui/ui_assets.h"

#define LAMP_GROUP_BLINK_FAST_MS 600U
#define LAMP_GROUP_BLINK_SLOW_MS 6000U

/* 故障等级 -> ui_lamp 状态 */
static void lamp_apply_level(ui_lamp_t *lamp, uint8_t level)
{
    switch(level) {
    case (uint8_t)CAR_WARN_ON:
        ui_lamp_set_state(lamp, UI_LAMP_ON);
        break;

    case (uint8_t)CAR_WARN_BLINK_FAST:
        ui_lamp_set_blink_ms(lamp, LAMP_GROUP_BLINK_FAST_MS);
        ui_lamp_set_state(lamp, UI_LAMP_BLINK);
        break;

    case (uint8_t)CAR_WARN_BLINK_SLOW:
        ui_lamp_set_blink_ms(lamp, LAMP_GROUP_BLINK_SLOW_MS);
        ui_lamp_set_state(lamp, UI_LAMP_BLINK);
        break;

    default:
        ui_lamp_set_state(lamp, UI_LAMP_OFF);
        break;
    }
}

/*
 * 驾驶辅助三项灯：Qt 用两张互相重叠的 IconGeneral 表达白/黄，
 * 黄图仅在开关打开且对应车道辅助状态满足时显示，其余情况显示白图。
 * 返回 -1 表示设置开关关闭、整盏灯熄灭；否则返回应使用的图标下标。
 */
static int8_t assist_lamp_variant(const ui_lamp_group_item_t *cfg,
                                  const car_state_t *state, uint8_t *level)
{
    const car_lane_assist_t *lane = &state->high_func.lane;
    bool enabled = false;
    bool highlight = false;

    switch((ui_lamp_gate_t)cfg->gate) {
    case UI_LAMP_GATE_EMERGENCY_BRAKING:
        enabled = state->driver_assist.emergency_braking;
        highlight = lane->show_car > 0U;
        break;

    case UI_LAMP_GATE_BLIND_SPOT:
        enabled = state->driver_assist.blind_spot_warning;
        highlight = lane->show_area > 0U;
        break;

    case UI_LAMP_GATE_LANE_DEPARTURE:
        enabled = state->driver_assist.lane_departure_warning;
        highlight = lane->car_state > 1U;
        break;

    default:
        break;
    }

    if(!enabled) {
        return -1;
    }

    /* 未收到 0x0E 时 lane 字段都还是 0，与 Qt 相同只显示白图 */
    *level = (uint8_t)CAR_WARN_ON;

    if(highlight && cfg->variant_count > 1U) {
        return 1;
    }

    return 0;
}

/*
 * 非车道辅助的设置驱动灯：
 *   XMode  Qt IconXMode，drive_mode 6/7/8 依次显示 xmode_w/snow/deepsnow，其余熄灭；
 *   Set    Qt IconSet，cruise_state bit1（speedok）为真时点亮。
 * 返回值同 assist_lamp_variant。
 */
static int8_t setting_lamp_variant(const ui_lamp_group_item_t *cfg,
                                   const car_state_t *state, uint8_t *level)
{
    if((ui_lamp_gate_t)cfg->gate == UI_LAMP_GATE_SPEED_SET_OK) {
        if((state->cruise.cruise_state & 0x02U) == 0U) {
            return -1;
        }

        *level = (uint8_t)CAR_WARN_ON;
        return 0;
    }

    /* XMode：6 雪地前 / 7 雪地 / 8 深雪；未收到驱动模式帧时显式熄灭，避免误报 */
    switch(state->drive_mode) {
    case 6U:
    case 7U:
    case 8U:
        if((uint8_t)(state->drive_mode - 6U) < cfg->variant_count) {
            if(!state->drive_mode_valid) {
                return -1;
            }

            *level = (uint8_t)CAR_WARN_ON;
            return (int8_t)(state->drive_mode - 6U);
        }
        return -1;

    default:
        return -1;
    }
}

bool ui_lamp_group_create(ui_lamp_group_t *group, lv_obj_t *parent,
                          const ui_lamp_group_item_t *items, uint8_t count)
{
    if(group == NULL || parent == NULL || items == NULL ||
       count == 0U || count > UI_LAMP_GROUP_MAX) {
        return false;
    }

    group->cfg = items;
    group->count = count;

    for(uint8_t i = 0; i < count; ++i) {
        const ui_lamp_group_item_t *cfg = &items[i];
        ui_lamp_group_item_rt_t *rt = &group->items[i];

        if(cfg->variant_count == 0U ||
           cfg->variant_count > UI_LAMP_GROUP_VARIANTS) {
            return false;
        }

        rt->active = -1;

        /* 灯尺寸与 64×64 原图不一致，必须整图驻留 RAM 才能缩放 */
        for(uint8_t v = 0; v < cfg->variant_count; ++v) {
            if(!ui_assets_load_image(&rt->dsc[v], cfg->variants[v].icon)) {
                return false;
            }
        }

        ui_lamp_create_sized(&rt->lamp, parent, &rt->dsc[0],
                             cfg->x, cfg->y, cfg->w, cfg->h);
    }

    return true;
}

void ui_lamp_group_apply(ui_lamp_group_t *group, const car_state_t *state)
{
    if(group == NULL || group->cfg == NULL || state == NULL) {
        return;
    }

    for(uint8_t i = 0; i < group->count; ++i) {
        const ui_lamp_group_item_t *cfg = &group->cfg[i];
        ui_lamp_group_item_rt_t *rt = &group->items[i];
        int8_t best = -1;
        uint8_t best_level = 0;

        if(cfg->gate >= (uint8_t)UI_LAMP_GATE_EMERGENCY_BRAKING &&
           cfg->gate <= (uint8_t)UI_LAMP_GATE_LANE_DEPARTURE) {
            int8_t idx = assist_lamp_variant(cfg, state, &best_level);

            if(idx < 0) {
                best_level = 0U;
            }
            else {
                best = idx;
            }

            if(best >= 0 && rt->active != best) {
                rt->active = best;
                ui_lamp_set_icon(&rt->lamp, &rt->dsc[best]);
            }

            lamp_apply_level(&rt->lamp, best_level);
            continue;
        }

        if(cfg->gate == (uint8_t)UI_LAMP_GATE_XMODE ||
           cfg->gate == (uint8_t)UI_LAMP_GATE_SPEED_SET_OK) {
            int8_t idx = setting_lamp_variant(cfg, state, &best_level);

            if(idx < 0) {
                best_level = 0U;
            }
            else {
                best = idx;
            }

            if(best >= 0 && rt->active != best) {
                rt->active = best;
                ui_lamp_set_icon(&rt->lamp, &rt->dsc[best]);
            }

            lamp_apply_level(&rt->lamp, best_level);
            continue;
        }

        for(uint8_t v = 0; v < cfg->variant_count; ++v) {
            uint8_t level = car_state_warning_level(state,
                                                     cfg->variants[v].code);
            uint8_t level2;

            if(cfg->variants[v].code2 != 0U) {
                level2 = car_state_warning_level(state,
                                                  cfg->variants[v].code2);
                if(level2 > level) {
                    level = level2;
                }
            }

            /* 取灯态最高的一档；同档保留表中靠前的图标 */
            if(level != 0U && (best < 0 || level > best_level)) {
                best = (int8_t)v;
                best_level = level;
            }
        }

        /*
         * 设置门控：与 Qt visible: SettingsMgr.xxx 等价，条件不满足时整盏灯熄灭。
         * 故障码仍在 state 里，只是不显示。
         */
        if(cfg->gate == (uint8_t)UI_LAMP_GATE_SHOW_4H && !state->show_4h) {
            best_level = 0U;
        }

        if(best >= 0 && rt->active != best) {
            rt->active = best;
            ui_lamp_set_icon(&rt->lamp, &rt->dsc[best]);
        }

        lamp_apply_level(&rt->lamp, best_level);
    }
}