/*
 * ui_small_door.c - 顶部小车门状态图实现
 *
 * 右舵只镜像这三层图；故障码 22 的左右前门同时点亮。
 */
#include "ui/ComControl/ui_small_door.h"

#include "ui/ui_theme.h"
#include "ui/ui_assets.h"

void ui_small_door_set_right_hand_drive(ui_small_door_t *door, bool right_hand)
{
    if(door == NULL || door->root == NULL || door->right_hand_drive == right_hand) return;
    bool applied = true;
    for(uint32_t i = 0; i < lv_obj_get_child_count(door->root); ++i) {
        if(!ui_assets_mirror_image(lv_obj_get_child(door->root, i), right_hand,
                                   0, 0, 64, 64)) applied = false;
    }
    if(applied) door->right_hand_drive = right_hand;
}

/* 三张图都是 64×64，叠在同一个原点上 */
static void door_image_create(lv_obj_t *parent, const char *src)
{
    lv_obj_t *img = lv_image_create(parent);

    lv_image_set_src(img, src);
    lv_obj_set_pos(img, 0, 0);
}

bool ui_small_door_create(ui_small_door_t *door, lv_obj_t *parent,
                          int32_t x, int32_t y,
                          const char *bg, const char *left_front,
                          const char *right_front)
{
    if(door == NULL || parent == NULL || bg == NULL || left_front == NULL ||
       right_front == NULL) {
        return false;
    }

    door->right_hand_drive = false;
    door->root = ui_blank_container(parent);
    lv_obj_set_size(door->root, 64, 64);
    lv_obj_set_pos(door->root, x, y);

    door_image_create(door->root, bg);
    door_image_create(door->root, left_front);
    door_image_create(door->root, right_front);

    /* Qt 初始 visible=false，由故障码 22 打开 */
    lv_obj_add_flag(door->root, LV_OBJ_FLAG_HIDDEN);

    return true;
}

void ui_small_door_show(ui_small_door_t *door, bool show)
{
    if(door == NULL || door->root == NULL) {
        return;
    }

    if(show) {
        lv_obj_remove_flag(door->root, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(door->root, LV_OBJ_FLAG_HIDDEN);
    }
}