/*
 * ui_gear.h - 档位显示（所有样式通用）
 *
 * 对应 Qt ComControl/PageGear.qml 的 forester 分支：
 * 单个档位字母居中显示，默认 46 号字、90x90 容器。
 * 文本直接来自 car_gear_t.text，不做档位枚举映射。
 */
#ifndef CARMW_UI_COM_GEAR_H
#define CARMW_UI_COM_GEAR_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 与 car_gear_t.text[4] 保持同宽，便于直接比对 */
#define UI_GEAR_TEXT_MAX 4

typedef struct {
    lv_obj_t *root;                    /* 档位容器，size×size */
    lv_obj_t *label;                   /* 档位文本 */
    char shown[UI_GEAR_TEXT_MAX];      /* 上次显示的内容，用于跳过重复刷新 */
} ui_gear_t;

/* 创建档位显示，x/y 为容器左上角坐标，size 为边长，font 由样式指定 */
bool ui_gear_create(ui_gear_t *gear, lv_obj_t *parent, int32_t x, int32_t y,
                    int32_t size, const lv_font_t *font);

/* 按档位文本刷新；与上次相同时不做任何操作 */
void ui_gear_apply(ui_gear_t *gear, const char *text);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_COM_GEAR_H */