/*
 * ov_door.c - 覆盖层：车门状态
 *
 * 坐标：页面 (620,100)，七层图各 720×550（已按 Qt 的 DoorClip 预渲染）。
 * 显示条件：0x0B 门状态帧 valid 且左前/右前/左后/右后/后备箱/引擎盖任一打开。
 * 与 Qt 一致，非右舵：b0 左前、b1 右前、b2 左后、b3 右后、b4 后备箱、b5 引擎盖。
 */
#include "ui/overlays/ov_door.h"

#include "ui/ui_common.h"
#include "ui/ui_theme.h"

#define DOOR_X   620
#define DOOR_Y   100
#define DOOR_W   720
#define DOOR_H   550

/* 位图标志，用于跳过无变化的刷新 */
#define ST_LF  0x01U
#define ST_RF  0x02U
#define ST_LR  0x04U
#define ST_RR  0x08U
#define ST_TRK 0x10U
#define ST_HOOD 0x20U
#define ST_SHOW 0x80U

static lv_obj_t *door_image(lv_obj_t *parent, const char *path)
{
    lv_obj_t *img = lv_image_create(parent);

    lv_image_set_src(img, path);
    lv_obj_set_pos(img, 0, 0);

    return img;
}

bool ov_door_create(ov_door_t *ov, lv_obj_t *parent)
{
    if(ov == NULL) {
        return false;
    }

    ov->root = ui_blank_container(parent);
    lv_obj_set_size(ov->root, DOOR_W, DOOR_H);
    lv_obj_set_pos(ov->root, DOOR_X, DOOR_Y);

    /* 图层顺序与 Qt 声明顺序一致：底图 → 前盖 → 左前 → 左后 → 右前 → 右后 → 后备箱 */
    ov->bg = door_image(ov->root, UI_IMG("Door/Style2/ov_bg.png"));
    ov->hood = door_image(ov->root, UI_IMG("Door/Style2/ov_front.png"));
    ov->left_front = door_image(ov->root, UI_IMG("Door/Style2/ov_left_front.png"));
    ov->left_rear = door_image(ov->root, UI_IMG("Door/Style2/ov_left_after.png"));
    ov->right_front = door_image(ov->root, UI_IMG("Door/Style2/ov_right_front.png"));
    ov->right_rear = door_image(ov->root, UI_IMG("Door/Style2/ov_right_after.png"));
    ov->trunk = door_image(ov->root, UI_IMG("Door/Style2/ov_after.png"));

    lv_obj_add_flag(ov->hood, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ov->left_front, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ov->left_rear, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ov->right_front, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ov->right_rear, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ov->trunk, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_flag(ov->root, LV_OBJ_FLAG_HIDDEN);

    ov->active = 0U;
    ov->last_state = 0xFFU;

    return true;
}

static void set_visible(lv_obj_t *obj, uint8_t on)
{
    if(on != 0U) {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

void ov_door_apply(ov_door_t *ov, const car_state_t *state, bool suppressed)
{
    uint8_t bits = 0U;
    uint8_t show;

    if(ov == NULL || state == NULL) {
        return;
    }

    if(state->doors.valid) {
        if(state->doors.left_front) {
            bits |= ST_LF;
        }
        if(state->doors.right_front) {
            bits |= ST_RF;
        }
        if(state->doors.left_rear) {
            bits |= ST_LR;
        }
        if(state->doors.right_rear) {
            bits |= ST_RR;
        }
        if(state->doors.trunk) {
            bits |= ST_TRK;
        }
        if(state->doors.hood) {
            bits |= ST_HOOD;
        }
    }

    show = (bits != 0U && !suppressed) ? 1U : 0U;
    if(show != 0U) {
        bits |= ST_SHOW;
    }

    if(bits == ov->last_state) {
        return;
    }

    ov->last_state = bits;
    ov->active = show;

    set_visible(ov->root, show);
    set_visible(ov->hood, (bits & ST_HOOD) ? 1U : 0U);
    set_visible(ov->left_front, (bits & ST_LF) ? 1U : 0U);
    set_visible(ov->left_rear, (bits & ST_LR) ? 1U : 0U);
    set_visible(ov->right_front, (bits & ST_RF) ? 1U : 0U);
    set_visible(ov->right_rear, (bits & ST_RR) ? 1U : 0U);
    set_visible(ov->trunk, (bits & ST_TRK) ? 1U : 0U);
}

bool ov_door_is_active(const ov_door_t *ov)
{
    return (ov != NULL && ov->active != 0U);
}

void ov_door_destroy(ov_door_t *ov)
{
    if(ov != NULL && ov->root != NULL) {
        lv_obj_delete(ov->root);
        ov->root = NULL;
        ov->active = 0U;
        ov->last_state = 0xFFU;
    }
}
