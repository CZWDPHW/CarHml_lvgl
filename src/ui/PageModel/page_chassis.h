/*
 * page_chassis.h - 中心卡片：底盘控制（对应 CentralDisplay/ChassisControl.qml）
 *
 * 数据来自 0x26 轨迹帧：转向角、方向、底盘升降、四轮驱动指示。
 * 车模素材已按 Qt 的 sourceClipRect + 0.39 缩放预裁剪成 215×296。
 */
#ifndef CARMW_UI_PAGE_CHASSIS_H
#define CARMW_UI_PAGE_CHASSIS_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CHASSIS_TRAJ_POINTS  13   /* 轨迹采样点数（含首尾） */

typedef struct {
    lv_obj_t *root;
    lv_obj_t *angle_label;
    lv_obj_t *img_bg;          /* 底盘升降背景（全蓝） */
    lv_obj_t *img_wheel[4];    /* LF / RF / LR / RR 驱动指示 */
    lv_obj_t *traj_left;       /* 左侧预测轨迹线 */
    lv_obj_t *traj_right;      /* 右侧预测轨迹线 */
    lv_point_precise_t pts_left[CHASSIS_TRAJ_POINTS];
    lv_point_precise_t pts_right[CHASSIS_TRAJ_POINTS];
    uint8_t last_lift;
    uint8_t last_wheel[4];
    uint8_t last_angle;
    uint8_t last_right;
    uint8_t last_show;
    uint8_t last_traj;
    uint8_t last_traj_r;
} chassis_card_t;

bool chassis_card_create(chassis_card_t *card, lv_obj_t *parent);
void chassis_card_apply(chassis_card_t *card, const car_state_t *state);
void chassis_card_destroy(chassis_card_t *card);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_PAGE_CHASSIS_H */