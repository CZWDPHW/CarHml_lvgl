/*
 * ui_info_row.h - 图标 + 文字的信息行（所有样式通用）
 *
 * 对应 Qt CentralTemplate 内容区里的 Row{Image + Text}：
 * 图标按 PreserveAspectFit 缩放进给定矩形，文字紧随其后垂直居中。
 * 控件只负责排版与文字刷新，不含任何样式相关常量。
 */
#ifndef CARMW_UI_COM_INFO_ROW_H
#define CARMW_UI_COM_INFO_ROW_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;       /* 行容器，宽高由内容撑开 */
    lv_obj_t *icon;       /* 图标 */
    lv_obj_t *label;      /* 文字 */
} ui_info_row_t;

/*
 * 创建一行：icon 为图片源，图标按 icon_w×icon_h 等比缩放居中，
 * 图标与文字间距 gap，文字用 font。
 */
bool ui_info_row_create(ui_info_row_t *row, lv_obj_t *parent,
                        const void *icon, int32_t icon_w, int32_t icon_h,
                        int32_t gap, const lv_font_t *font);

/* 刷新文字；与上次相同时跳过 */
void ui_info_row_set_text(ui_info_row_t *row, const char *text);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_COM_INFO_ROW_H */