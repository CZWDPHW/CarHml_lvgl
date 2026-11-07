/*
 * ui_session.h - 跨样式存活的界面会话状态
 *
 * 样式切换会销毁并重建整页对象。依赖"事件到达时刻/已消费进度"的状态
 * （如胎压异常 10s 计时）不能放在页面对象里，否则重建即重新开始计时。
 * 这里集中保存这类会话状态：由 ui_app 在每次快照分发时更新，
 * 页面只读取结果决定显隐。
 */
#ifndef CARMW_UI_SESSION_H
#define CARMW_UI_SESSION_H

#include "carmw/car_state.h"
#include "ui/overlays/ov_tire.h"

#ifdef __cplusplus
extern "C" {
#endif

/* UI 初始化/反初始化时清空，模拟一次新的进程会话 */
void ui_session_reset(void);

/* 消费一份车状态快照：与当前是否有样式无关，样式切走期间也持续计时 */
void ui_session_apply(const car_state_t *state);

/* 胎压异常事件状态，只读；页面据此决定覆盖层显隐 */
const ov_tire_event_t *ui_session_tire_event(void);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_SESSION_H */
