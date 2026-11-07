/*
 * ui_lamp_row.h - 横向灯排（所有样式通用）
 *
 * 对应 Qt 页面里 `Row { ... }` 中的一组警示灯：
 * 每个灯由故障码驱动，0/1/2/3 分别映射为灭/亮/快闪/慢闪。
 * 控件只负责布局与渲染，灯表（图标、尺寸、故障码）由样式层传入。
 */
#ifndef CARMW_UI_COM_LAMP_ROW_H
#define CARMW_UI_COM_LAMP_ROW_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"
#include "ui/widgets/ui_lamp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 单排最多支持的灯数量 */
#define UI_LAMP_ROW_MAX 8

/* 样式层提供的单个灯配置 */
typedef struct {
    const char *icon;      /* 主图标路径，如 "WarnIcon/auto_lamp.png" */
    const char *icon_alt;  /* 备选图标路径（如近光灯故障图），NULL 表示无 */
    uint8_t code;          /* 主故障码 */
    uint8_t alt_code;      /* 触发备选图标的故障码，0 表示无 */
    int32_t w;             /* 目标宽度 */
    int32_t h;             /* 目标高度 */
} ui_lamp_row_item_t;

/* 单个灯的运行期状态 */
typedef struct {
    ui_lamp_t lamp;
    lv_image_dsc_t dsc;      /* 主图标，需缩放，整图驻留 RAM */
    lv_image_dsc_t dsc_alt;  /* 备选图标 */
    uint8_t code;
    uint8_t alt_code;
    bool use_alt;            /* 当前是否显示备选图标 */
} ui_lamp_row_item_rt_t;

typedef struct {
    ui_lamp_row_item_rt_t items[UI_LAMP_ROW_MAX];
    uint8_t count;
} ui_lamp_row_t;

/*
 * 创建灯排：从 (x,y) 开始按顺序无间距排布，图标在 row_h 高度内垂直居中。
 * row_h 由样式层决定（Qt 的 Row 高度等于最高子项）。
 */
bool ui_lamp_row_create(ui_lamp_row_t *row, lv_obj_t *parent, int32_t x, int32_t y,
                        int32_t row_h, const ui_lamp_row_item_t *items,
                        uint8_t count);

/* 按故障码刷新整排灯；无变化时内部会跳过重设 */
void ui_lamp_row_apply(ui_lamp_row_t *row, const car_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_COM_LAMP_ROW_H */