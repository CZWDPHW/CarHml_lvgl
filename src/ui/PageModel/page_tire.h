/*
 * page_tire.h - 中心卡片：胎压监测（对应 TirePressure/PageMainTirePressure.qml）
 *
 * Forester 使用的分支：标题 + 432×320 区域内左右两列、前后两行的四个胎压文字块。
 * 胎压单位跟随 0x39：0 = kPa，1 = bar（÷102），2 = PSI（÷7.03）。
 */
#ifndef CARMW_UI_PAGE_TIRE_H
#define CARMW_UI_PAGE_TIRE_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;
    lv_obj_t *value[4];        /* FL FR RL RR，24 号字体 */
    lv_obj_t *unit[4];         /* 单位，18 号字体 */
        lv_obj_t *img_car;         /* 车体底图 396×386 */
    lv_obj_t *img_warn[4];     /* 左前/右前/左后/右后 异常高亮，状态 > 1 时显示 */
    uint8_t last_state[4];     /* 上次的高亮开关，用于跳过刷新 */
    uint16_t last_kpa[4];      /* 上次显示的原始 kPa，用于跳过刷新 */
    uint16_t last_unit;        /* 上次的单位，0xFF 表示未初始化 */
} tire_card_t;

/* 在中心卡片容器内创建胎压卡片（586×509，居中于 814×509） */
bool tire_card_create(tire_card_t *card, lv_obj_t *parent);

/* 刷新四个胎压数值与单位（数据来自 0x0A / 0x39） */
void tire_card_apply(tire_card_t *card, const car_state_t *state);

void tire_card_destroy(tire_card_t *card);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_PAGE_TIRE_H */