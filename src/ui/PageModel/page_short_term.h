/*
 * page_short_term.h - 中心卡片：短期记忆（对应 CentralDisplay/PageShortTerm.qml）
 *
 * 数据来自 0x0E 帧（car_trip_t）：小计里程、平均时速、行驶时长、平均油耗。
 * 油耗单位随 0x39 的 fuel_unit：0 = L/100km，1 = km/L，2 = mpg。
 */
#ifndef CARMW_UI_PAGE_SHORT_TERM_H
#define CARMW_UI_PAGE_SHORT_TERM_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;
    lv_obj_t *value[4];   /* 里程 / 时速 / 时长 / 油耗 */
    lv_obj_t *fuel_unit;  /* 油耗单位，18 号 */
} short_term_card_t;

bool short_term_card_create(short_term_card_t *card, lv_obj_t *parent);
void short_term_card_apply(short_term_card_t *card, const car_state_t *state);
void short_term_card_destroy(short_term_card_t *card);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_PAGE_SHORT_TERM_H */