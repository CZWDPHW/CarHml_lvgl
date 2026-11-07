/*
 * rotate_forester1.h - Style1 转速表（对应 Qt RotationDial/ForesterRotate1.qml）
 *
 * Qt 侧为 620x620 的 Item：底图 + 指针 Rotation + 8 个转速刻度 Label。
 * 与车速表的差异：
 *   - 指针角度随转速递减（180° 起）
 *   - 高亮判据是左开右闭区间，(800, 1200]、(1800, 2200] ……
 *   - "0" 刻度常驻大字号，其余刻度仅在命中时放大
 */
#ifndef CARMW_UI_ROTATE_FORESTER1_H
#define CARMW_UI_ROTATE_FORESTER1_H

#include <stdbool.h>

#include "ui/widgets/ui_dial.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 刻度数字数量：1~8，加上单独的 "0" 共 9 个文本 */
#define ROTATE_F1_LABEL_COUNT 8

typedef struct {
    ui_dial_t dial;                            /* 表盘及指针动画目标 */
    lv_obj_t *zero;                            /* "0" 刻度，始终大字号 */
    lv_obj_t *labels[ROTATE_F1_LABEL_COUNT];   /* 1~8 刻度文本 */
    int32_t highlight;                         /* 当前放大的刻度序号，-1 表示无 */
} rotate_forester1_t;

/*
 * 创建转速表，x/y 为表盘在 parent 内的左上角坐标。
 * 指针图片由组件内部整图加载，跨样式切换复用。
 */
bool rotate_forester1_create(rotate_forester1_t *rotate, lv_obj_t *parent,
                             int32_t x, int32_t y);

/* 按发动机转速（转/分）刷新指针角度与刻度高亮 */
void rotate_forester1_apply(rotate_forester1_t *rotate, uint32_t engine_rpm);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_ROTATE_FORESTER1_H */
