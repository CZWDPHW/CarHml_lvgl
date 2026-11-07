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