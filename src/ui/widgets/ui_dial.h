/*
 * ui_dial.h - 表盘控件（底图 + 旋转指针 + 刻度数字）
 *
 * 对应 Qt 的 SpeedDial / RotationDial：
 *   - 底图为普通 bin 图片，按资源路径流式绘制
 *   - 指针需旋转，图片源必须是整图驻留 RAM 的 lv_image_dsc_t
 *   - 刻度数字由样式层传入坐标表，控件负责整体创建
 *
 * 控件只管图形，角度与数值的换算公式属于样式（各样式表盘量程不同）。
 */
#ifndef CARMW_UI_DIAL_H
#define CARMW_UI_DIAL_H

#include <stdbool.h>
#include <stddef.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 一个刻度文本：以表盘容器左上角为原点的坐标 + 文本内容 */
typedef struct {
    int16_t x;
    int16_t y;
    const char *text;
} ui_dial_label_t;

typedef struct {
    lv_obj_t *root;     /* 表盘容器，尺寸为 size×size */
    lv_obj_t *needle;   /* 指针图片，轴心在表盘中心 */
    int32_t size;       /* 表盘边长，需与底图一致 */
    int32_t target_angle; /* 有符号目标角度，不随图片角度取模 */
} ui_dial_t;

/*
 * 创建表盘：
 *   parent  父对象
 *   dial    调用方提供的控件结构
 *   x/y     表盘在 parent 内的左上角坐标
 *   size    表盘边长（正方形）
 *   bg      底图资源路径
 *   needle  指针图像描述符，需由 ui_assets_load_image 预先整图加载
 *   needle_x/needle_y  指针图相对表盘左上角的摆放坐标（裁剪过的指针图非 0,0）
 *   pivot_x/pivot_y    旋转轴心相对指针图左上角的坐标
 *   angle   初始角度，单位 0.1°，正值顺时针，直接显示不插值
 */
void ui_dial_create(ui_dial_t *dial, lv_obj_t *parent, int32_t x, int32_t y,
                    int32_t size, const char *bg, const lv_image_dsc_t *needle,
                    int32_t needle_x, int32_t needle_y,
                    int32_t pivot_x, int32_t pivot_y, int32_t angle);

/* 从当前显示角度线性过渡 100ms；相同目标不重启，删除对象树自动清理动画。 */
void ui_dial_set_angle(ui_dial_t *dial, int32_t angle);

/*
 * 批量添加刻度文本，白色，坐标以表盘左上角为原点。
 * out 非空时按顺序回填各文本控件句柄，供样式层做刻度高亮；out_count 为其容量。
 */
void ui_dial_add_labels(ui_dial_t *dial, const ui_dial_label_t *items,
                        size_t count, const lv_font_t *font,
                        lv_obj_t **out, size_t out_count);

/* 在表盘内添加一段静态文本，用于单位标识（km/h、x1000/min 等） */
lv_obj_t *ui_dial_add_text(ui_dial_t *dial, const char *text, int32_t x,
                           int32_t y, const lv_font_t *font);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_DIAL_H */
