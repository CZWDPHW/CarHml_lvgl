/*
 * ov_tire.h - 覆盖层：胎压异常（对应 TirePressure/PageMainTirePressure.qml）
 *
 * Qt 侧 mainVisible 分支：586×509 位于页面 (668,150)，内部布局与中心胎压卡片一致。
 * 显示条件：任一车轮状态 > 1（偏低/偏高），新异常帧续期 10s（tireTimer）。
 *
 * 计时与"哪些帧已消费"的进度保存在 ui_session（跨样式切换存活），
 * 覆盖层对象只负责按事件状态显隐，因此样式重建不会重获完整 10 秒。
 */
#ifndef CARMW_UI_OV_TIRE_H
#define CARMW_UI_OV_TIRE_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"
#include "ui/PageModel/page_tire.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OV_TIRE_HOLD_MS 10000U

/*
 * 胎压异常事件状态（与覆盖层对象解耦）。
 * 用 LVGL tick 记录事件到达时刻，只和 UI 线程自己的 tick 相减，
 * 不与串口后台时钟比较；tires.sequence 用于区分"新帧"与"旧快照"。
 *
 * 生命周期约定：
 *   - 进程启动/UI 重新初始化时清空：首个含异常车轮的快照按新事件显示 10s。
 *   - 胎压数据中断（valid=false）：清除进度，恢复后的首个有效帧按新事件处理，
 *     避免断线重连后序号从 0 重启导致漏判。
 */
typedef struct {
    uint32_t last_sequence; /* 最近一次已消费胎压帧的序号 */
    uint32_t event_tick;    /* 最近一次异常帧到达时的 LVGL tick */
    bool seen_frame;
    bool pending;           /* 事件仍在有效期内（尚未到期且未收到正常帧） */
} ov_tire_event_t;

typedef struct {
    lv_obj_t *root;        /* 814×509 @ (554,150)，卡片在其内 (114,0) → 页面 (668,150) */
    tire_card_t card;
    uint8_t active;
} ov_tire_t;

bool ov_tire_create(ov_tire_t *ov, lv_obj_t *parent);

/* 事件状态：清空 / 消费一份快照 / 查询是否仍在有效期内 */
void ov_tire_event_reset(ov_tire_event_t *ev);
void ov_tire_event_apply(ov_tire_event_t *ev, const car_state_t *state);
bool ov_tire_event_is_active(const ov_tire_event_t *ev);

/* 按事件状态刷新显隐；被车门遮挡时仍消费事件，但不显示 */
void ov_tire_apply(ov_tire_t *ov, const car_state_t *state, bool suppressed,
                   const ov_tire_event_t *ev);

bool ov_tire_is_active(const ov_tire_t *ov);
void ov_tire_destroy(ov_tire_t *ov);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_OV_TIRE_H */
