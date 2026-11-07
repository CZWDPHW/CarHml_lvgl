/*
 * ui_small_door.h - 顶部小车门状态图（所有样式通用）
 *
 * 对应 Qt CentralDisplay/PageSmallDoor.qml 在 Style1 顶部 Row 中的用法：
 * 车门未关（故障码 22）时显示 64×64 背景 + 左右前门叠加图，其余门图隐藏。
 * 素材路径、尺寸由样式层决定，控件内部不出现样式名。
 */
#ifndef CARMW_UI_COM_SMALL_DOOR_H
#define CARMW_UI_COM_SMALL_DOOR_H

#include <stdbool.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root; /* 64×64 根容器，整组显示/隐藏 */
} ui_small_door_t;

/*
 * 创建小车门图。三个路径必须是已解析的完整路径（样式层用 UI_IMG() 传入），
 * 因为 lv_image_set_src 不会复制路径字符串。
 */
bool ui_small_door_create(ui_small_door_t *door, lv_obj_t *parent,
                          int32_t x, int32_t y,
                          const char *bg, const char *left_front,
                          const char *right_front);

/* 显示/隐藏整组图片；重复调用无副作用 */
void ui_small_door_show(ui_small_door_t *door, bool show);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_COM_SMALL_DOOR_H */