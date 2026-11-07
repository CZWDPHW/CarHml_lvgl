/*
 * fuel_forester1.h - Style1 油量表（对应 Qt FuelTank/ForesterFuelTank1.qml）
 *
 * Qt 侧为 250x268 的 Item：底图 + 刻度裁剪图 + 油枪图标 + F/E 标记。
 * 油量不高于 25% 时刻度图换成红色版本，这是 Qt 的 source 三元表达式。
 */
#ifndef CARMW_UI_FUEL_FORESTER1_H
#define CARMW_UI_FUEL_FORESTER1_H

#include <stdbool.h>

#include "lvgl.h"

#include "ui/widgets/ui_tick.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 不高于该百分比时换成红色刻度图（Qt: >25 白，否则红） */
#define FUEL_F1_LOW_PERCENT 25

typedef struct {
    lv_obj_t *root;      /* 表盘容器，尺寸 250x368 */
    ui_tick_t tick;      /* 刻度裁剪条 */
    bool low;            /* 当前是否处于低油量配色，避免重复换图 */
} fuel_forester1_t;

/* 创建油量表，x/y 为容器在 parent 内的左上角坐标 */
bool fuel_forester1_create(fuel_forester1_t *fuel, lv_obj_t *parent,
                           int32_t x, int32_t y);

/* 按油量百分比（0~100）刷新液面与低油量配色 */
void fuel_forester1_apply(fuel_forester1_t *fuel, uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_FUEL_FORESTER1_H */