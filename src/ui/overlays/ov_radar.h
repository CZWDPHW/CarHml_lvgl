/*
 * ov_radar.h - 覆盖层：倒车/泊车雷达（对应 CentralDisplay/PageRadar.qml）
 *
 * Qt 侧默认坐标：290×430 @ (815,160)。
 * 显示条件：0x22 display_on 置位；D 挡（D/M1~M6）显示前雷达，R 挡显示后雷达。
 * 每个传感器取 color 0/1/2 → 绿/黄/红，value 为 0x3F 时隐藏该弧。
 */
#ifndef CARMW_UI_OV_RADAR_H
#define CARMW_UI_OV_RADAR_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;
    lv_obj_t *wave_front;
    lv_obj_t *wave_rear;
    lv_obj_t *arc_front[4][3];   /* [传感器][绿/黄/红] */
    lv_obj_t *arc_rear[4][3];
    uint32_t last_sig;           /* 显示状态签名，用于跳过无变化的刷新 */
    uint8_t active;
} ov_radar_t;

bool ov_radar_create(ov_radar_t *ov, lv_obj_t *parent);
void ov_radar_apply(ov_radar_t *ov, const car_state_t *state, bool suppressed);
bool ov_radar_is_active(const ov_radar_t *ov);
void ov_radar_destroy(ov_radar_t *ov);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_OV_RADAR_H */
