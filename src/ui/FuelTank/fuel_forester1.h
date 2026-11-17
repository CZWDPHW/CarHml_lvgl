/*
 * fuel_forester1.h - Style1 油量表（对应 Qt FuelTank/ForesterFuelTank1.qml）
 *
 * Qt 侧为 250x268 的 Item：底图 + 刻度裁剪图 + 油枪图标 + F/E 标记。
 * 油量不高于 25% 时刻度图换成红色版本，这是 Qt 的 source 三元表达式。
 * 另外按通用仪表的规则叠加 64×64 加油口方向图标（左/右 × 白/黄）。
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
    lv_obj_t *port_img;  /* 加油口方向图标 64×64，叠在原 44×44 油枪图标上 */
    lv_image_dsc_t port_dsc[4]; /* 常驻描述符：0 左白 1 左黄 2 右白 3 右黄 */
    int8_t port_variant; /* 当前变体，-1 表示未初始化 */
} fuel_forester1_t;

/* 创建油量表，x/y 为容器在 parent 内的左上角坐标 */
bool fuel_forester1_create(fuel_forester1_t *fuel, lv_obj_t *parent,
                           int32_t x, int32_t y);
/* 按油量百分比（0~100）刷新液面与低油量配色 */
void fuel_forester1_apply(fuel_forester1_t *fuel, uint8_t percent);
/*
 * 刷新加油口方向图标。
 * port_left：加油口在左侧（0x02 帧 port_left，MCU 实际位置）；
 * icon_state：0x02 帧 icon_state，2 为低油量黄图，其余白图。
 * Qt 通用油量图标用 SettingsMgr.fuelFillerLeft，本工程取帧里的真实位置。
 */
void fuel_forester1_apply_port(fuel_forester1_t *fuel, bool port_left,
                               uint8_t icon_state);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_FUEL_FORESTER1_H */