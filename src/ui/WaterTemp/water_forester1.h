/*
 * water_forester1.h - Style1 水温表（对应 Qt WaterTemp/SubaruForesterWater1.qml）
 *
 * Qt 侧为 250x268 的 Item：底图 + 刻度裁剪图 + 温度计图标 + H/C 标记。
 * 水温异常（waterTempState != 0）时图标换成高温图，位置也随之偏移。
 */
#ifndef CARMW_UI_WATER_FORESTER1_H
#define CARMW_UI_WATER_FORESTER1_H

#include <stdbool.h>

#include "lvgl.h"

#include "ui/widgets/ui_tick.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;      /* 表盘容器，尺寸 250x368 */
    lv_obj_t *icon;      /* 温度计图标，水温异常时换图并移位 */
    ui_tick_t tick;      /* 刻度裁剪条 */
    bool abnormal;       /* 当前是否处于高温图标状态 */
} water_forester1_t;

/* 创建水温表，x/y 为容器在 parent 内的左上角坐标 */
bool water_forester1_create(water_forester1_t *water, lv_obj_t *parent,
                            int32_t x, int32_t y);

/* 按水温百分比（0~100）刷新液面，abnormal 为真时切换到高温图标 */
void water_forester1_apply(water_forester1_t *water, uint8_t percent,
                           bool abnormal);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_WATER_FORESTER1_H */