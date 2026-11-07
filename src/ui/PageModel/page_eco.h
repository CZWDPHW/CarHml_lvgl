/*
 * page_eco.h - 中心卡片：燃油经济性（对应 CentralDisplay/EcoShow.qml）
 *
 * 数据来自 0x0F 帧（car_eco_t）：enabled 决定圆环/水平两种模式，
 * value 0~255 → 百分比。平均油耗复用 0x0E 的 trip.avg_fuel_x10。
 */
#ifndef CARMW_UI_PAGE_ECO_H
#define CARMW_UI_PAGE_ECO_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;
    lv_obj_t *circular;    /* 圆环模式容器 546×300 */
    lv_obj_t *horizontal;  /* 水平刻度模式容器 546×300 */
    lv_obj_t *arc;         /* 240° 圆弧 */
    lv_obj_t *knob;        /* 自绘末端光点 20×20 */
    lv_obj_t *score;       /* ECO SCORE 数值 */
    lv_obj_t *avg;         /* 圆环模式底部平均油耗 */
    lv_obj_t *h_avg;       /* 水平模式平均油耗数值 */
    lv_obj_t *bar;         /* 水平模式进度条填充 */
    lv_obj_t *pointer;     /* 水平模式指针 28×28 */
    uint32_t last_pct;     /* 上次百分比，用于去重 */
    uint32_t last_avg_x10; /* 上次平均油耗 x10，用于去重 */
    uint8_t circular_on;   /* 上次模式：1 圆环 / 0 水平 */
} eco_card_t;

bool eco_card_create(eco_card_t *card, lv_obj_t *parent);
void eco_card_apply(eco_card_t *card, const car_state_t *state);
void eco_card_destroy(eco_card_t *card);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_PAGE_ECO_H */