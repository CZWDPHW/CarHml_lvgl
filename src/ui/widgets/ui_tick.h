/*
 * ui_tick.h - 竖向刻度条控件（油量/水温共用）
 *
 * 对应 Qt 里 fuel1_white_tick / wather1_tick 的用法：
 * 一个固定尺寸的裁剪窗内放一张负偏移的大图，靠整体上下移动裁剪窗
 * 来实现“液面”升降。图片与裁剪窗同步位移，画面上看起来只有窗口内的部分在动。
 *
 * 坐标约定：裁剪窗初始纵坐标为 y0；液面最低（0%）时窗顶为 y_bottom，
 * 最高（100%）时为 y_top。本控件只做线性插值，业务判据留在样式层。
 */
#ifndef CARMW_UI_TICK_H
#define CARMW_UI_TICK_H

#include <stdbool.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *clip;     /* 裁剪窗，位置随百分比变化 */
    lv_obj_t *img;      /* 窗内的刻度大图，与裁剪窗反向偏移以保持图不动 */
    int32_t x;          /* 裁剪窗横坐标，固定不变 */
    int32_t y_top;      /* 100% 时裁剪窗的纵坐标 */
    int32_t y_bottom;   /* 0% 时裁剪窗的纵坐标 */
    int32_t percent;    /* 当前百分比，-1 表示尚未设置 */
} ui_tick_t;

/*
 * 创建刻度条：
 *   parent     父对象
 *   tick       调用方提供的控件结构
 *   src        刻度图片资源路径（相对路径，不含 bin16/bin32 前缀）
 *   x          裁剪窗横坐标
 *   y_bottom   0% 时裁剪窗纵坐标
 *   y_top      100% 时裁剪窗纵坐标
 *   w/h        裁剪窗尺寸
 */
bool ui_tick_create(ui_tick_t *tick, lv_obj_t *parent, const char *src,
                    int32_t x, int32_t y_bottom, int32_t y_top,
                    int32_t w, int32_t h);

/* 设置百分比 0~100，超出范围会被截断；与当前值相同时跳过 */
void ui_tick_set_percent(ui_tick_t *tick, int32_t percent);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_TICK_H */