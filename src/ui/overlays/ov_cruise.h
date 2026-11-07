/*
 * ov_cruise.h - 覆盖层：定速巡航/车道辅助（对应 CentralDisplay/PageNavigationInfo.qml）
 *
 * Qt 侧 830×470 @ (440,122)、scale 0.8。所有图层素材（含源裁切与缩放）
 * 已用 PIL 预渲染成页面坐标精灵，运行时只定位与显隐。
 *
 * 数据来源：
 *   0x13 car_cruise_t   controlLed（cruise_state bit0）、distanceShow（acc_state 低 3 位）、
 *                        showFrontCar（acc_state bit3）
 *   0x16/0x0E car_high_func.lane：独立保存的车道、车身、盲区及前车状态。
 */
#ifndef CARMW_UI_OV_CRUISE_H
#define CARMW_UI_OV_CRUISE_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;// 根容器
    lv_obj_t *lane;//车道容器
    lv_obj_t *left_area;//左�盲区容器
    lv_obj_t *right_area;//右盲区容器
    lv_obj_t *front_car;//前车容器
    lv_obj_t *dist[4];//距离容器
    lv_obj_t *lane_left;//左车道容器
    lv_obj_t *lane_right;//右车道容器
    lv_obj_t *off_left;//左车道偏移容器
    lv_obj_t *off_right;//右车道偏移容器
    lv_obj_t *min_car;//最小车容器
    lv_obj_t *max_car;//最大车容器
    uint32_t last_sig;// 上次的信号位图，用于跳过刷新
    uint8_t active;// 当前是否显示
} ov_cruise_t;

bool ov_cruise_create(ov_cruise_t *ov, lv_obj_t *parent);
void ov_cruise_apply(ov_cruise_t *ov, const car_state_t *state, bool suppressed);
bool ov_cruise_is_active(const ov_cruise_t *ov);
void ov_cruise_destroy(ov_cruise_t *ov);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_OV_CRUISE_H */
