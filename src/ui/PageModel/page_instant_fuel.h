/*
 * page_instant_fuel.h - 中心卡片：瞬时油耗（对应 CentralDisplay/PageInstantFuel.qml）
 *
 * 数据来自 0x11 帧（car_instant_fuel_t）：
 *   value_x10 = raw & 0x7FFF，per_hour = raw & 0x8000，raw == 0xFFFE 表示隐藏。
 * 单位由 0x39 的 units.fuel_unit 决定：0 = L/100km，1 = km/L，2 = mpg。
 */
#ifndef CARMW_UI_PAGE_INSTANT_FUEL_H
#define CARMW_UI_PAGE_INSTANT_FUEL_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;
    lv_obj_t *icon;    /* 84×123 图标，水平居中 */
    lv_obj_t *value;   /* 数值 + 单位，36 号，水平居中 */
} instant_fuel_card_t;

bool instant_fuel_card_create(instant_fuel_card_t *card, lv_obj_t *parent);
void instant_fuel_card_apply(instant_fuel_card_t *card, const car_state_t *state);
void instant_fuel_card_destroy(instant_fuel_card_t *card);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_PAGE_INSTANT_FUEL_H */