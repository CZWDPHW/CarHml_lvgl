/*
 * page_4i.h - 中心卡片：4x4-i 驱动力分配（对应 CentralDisplay/Page4I.qml）
 *
 * Qt 侧全部由 Rectangle / Canvas 绘制，没有图片素材；
 * 前轮扭矩 = 100 - 后轮扭矩，后轮扭矩 = rearValue / 2。
 */
#ifndef CARMW_UI_PAGE_4I_H
#define CARMW_UI_PAGE_4I_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;
    lv_obj_t *fill_front;
    lv_obj_t *fill_rear;
    uint8_t front_pct;   /* 上次刷新的百分比，用于跳过重复布局 */
    uint8_t rear_pct;
} page4i_card_t;

/* 在中心卡片容器内创建 4x4-i 卡片（400×450，居中于 814×509） */
bool page4i_card_create(page4i_card_t *card, lv_obj_t *parent);

/* 设置前/后轮扭矩百分比（0~100），与上次相同则跳过 */
void page4i_card_set_torque(page4i_card_t *card, uint8_t front_pct, uint8_t rear_pct);

void page4i_card_destroy(page4i_card_t *card);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_PAGE_4I_H */