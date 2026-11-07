/*
 * car_ui_key.h - 方向盘按键（0x20）到界面动作的映射
 *
 * 语义与 Qt 端一致（handlers/Handler.js handleSteeringKey +
 * qml/main.qml 的 GlobalData 连接）：
 *   pressed: 0 释放 / 1 短按 / 2 长按
 *   长按 19 -> 左推摇杆，长按 20 -> 右推摇杆
 *   其余只有短按有效：
 *     13 返回首页（设置页可见时返回上一级，根页则关闭设置）
 *     19 上一张中心卡片    20 下一张中心卡片
 *     21 左（上一张）        22 右（下一张）
 *     23 确定：设置不可见且车速 < 5 时打开设置（尚未收到速度帧时也允许打开），
 *        已打开则交给设置页
 *     26 nextOrBack：设置页可见时按 Qt setNextOrBackIndex 前进
 *     31 切换表盘样式（设置页可见时屏蔽）
 *   Qt 侧不去重：按住持续上报短按时每帧都执行一次。
 */
#ifndef CARMW_CAR_UI_KEY_H
#define CARMW_CAR_UI_KEY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "carmw/car_link.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 分发上下文：记录本轮按键引发的动作，供调用方执行 */
typedef struct {
    bool nav_home;     /* 收到 13：返回首页 */
    bool style_switch; /* 收到 31：切换表盘样式 */
} ui_dispatch_ctx_t;

/*
 * 取出链路按键队列并逐条映射到界面。
 * 返回本次处理的按键条数；ctx 里的动作由调用方执行。
 */
size_t car_ui_key_dispatch(car_link_t *link, ui_dispatch_ctx_t *ctx);

/*
 * 板端入口：取出按键、转发翻卡按键，并在收到 31 时切换表盘样式。
 * 返回转发给界面的按键条数。
 */
size_t car_ui_key_pump(car_link_t *link);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_CAR_UI_KEY_H */
