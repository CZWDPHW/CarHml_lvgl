/*
 * speed_forester1.h - Style1 车速表（对应 Qt SpeedDial/ForesterSpeed1.qml）
 *
 * Qt 侧为 620x620 的 Item：底图 + 指针 Rotation + 14 个速度刻度 Label。
 * 刻度 Label 会按当前车速把对应数字放大到 1.2 倍，其余保持原字号。
 * 公英制由 SettingsMgr.mileType 决定：刻度文本、指针角度、单位标签都会变。
 */
#ifndef CARMW_UI_SPEED_FORESTER1_H
#define CARMW_UI_SPEED_FORESTER1_H

#include <stdbool.h>

#include "ui/widgets/ui_dial.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 刻度数字数量：0、20、40 ... 280 共 14 个 */
#define SPEED_F1_LABEL_COUNT 14

typedef struct {
    ui_dial_t dial;                             /* 表盘及指针动画目标 */
    lv_obj_t *unit;                             /* 单位标签：km/h / mph */
    lv_obj_t *labels[SPEED_F1_LABEL_COUNT];     /* 各刻度文本，做高亮用 */
    int32_t highlight;                          /* 当前放大的刻度序号，-1 表示无 */
    bool shown_mile;                            /* 上次刻度/单位是否英里制 */
    bool shown_valid;                           /* 是否已有过有效状态 */
} speed_forester1_t;

/*
 * 创建车速表，x/y 为表盘在 parent 内的左上角坐标。
 * 指针图片由组件内部整图加载，跨样式切换复用。
 */
bool speed_forester1_create(speed_forester1_t *speed, lv_obj_t *parent,
                            int32_t x, int32_t y);

/*
 * 按车速刷新指针角度、刻度高亮与单位标签。
 * speed_kph 为公里/小时；mile 为 true 时按 Qt 的英里换算显示刻度与角度。
 */
void speed_forester1_apply(speed_forester1_t *speed, uint32_t speed_kph,
                           bool mile);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_SPEED_FORESTER1_H */
