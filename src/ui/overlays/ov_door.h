/*
 * ov_door.h - 覆盖层：车门状态（对应 door/PageCarDoor.qml，Forester 分支）
 *
 * Qt 侧：
 *   main.qml 的 doorLoader 590×450，PageCarDoor 自身 x=620、y=100（Style1）
 *   7 个 DoorClip 各 720×550，源图 1024×1024、scale 0.6、偏移 (170,240)
 *   任一车门/引擎盖/后备箱打开时显示；同时主页面中心内容被隐藏
 *
 * 素材已在 assets/src 里预渲染成 720×550（PIL 复刻 Qt 的缩放与裁剪），
 * 运行时只按车门状态切换 7 张图的可见性。
 */
#ifndef CARMW_UI_OV_DOOR_H
#define CARMW_UI_OV_DOOR_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;
    lv_obj_t *bg;          /* 车体底图（关闭状态） */
    lv_obj_t *hood;        /* 前引擎盖 */
    lv_obj_t *trunk;       /* 后备箱 */
    lv_obj_t *left_front;
    lv_obj_t *right_front;
    lv_obj_t *left_rear;
    lv_obj_t *right_rear;
    uint8_t active;        /* 当前是否显示 */
    uint8_t last_state;    /* 上次的门状态位图，用于跳过刷新 */
} ov_door_t;

bool ov_door_create(ov_door_t *ov, lv_obj_t *parent);
/*
 * 按车门状态刷新显隐；suppressed=true 时不显示（Qt settingsPageOpen 等遮挡），
 * 但门状态位仍记录，解除遮挡后按当前状态恢复。
 */
void ov_door_apply(ov_door_t *ov, const car_state_t *state, bool suppressed);
bool ov_door_is_active(const ov_door_t *ov);
void ov_door_destroy(ov_door_t *ov);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_OV_DOOR_H */
