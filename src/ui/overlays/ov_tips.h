/*
 * ov_tips.h - 覆盖层：提示条与通用提示面板
 *
 * 对应 Qt 的三个 TipsWindow：
 *   FuelLowTipsWindow   燃油低横幅 230×60 @ (845,30)，fuel.icon_state == 2 时显示
 *   DoorOpenTipsWindow  P 挡提示条，车门覆盖层激活且档位非 P 时显示
 *   GeneralTipsWindow   260×280 @ (834,253)，由 0x03 弹窗帧或 0x1C 警告事件驱动
 *
 * 通用提示面板的播放语义对齐 Qt GeneralTipsWindow：
 *   两条入口合并进一条严格 FIFO 队列，不去重；
 *   每条默认显示 2s（Qt showBriefly 未传 durationS，协议时长被忽略）；
 *   0x1C 警告无论协议里的 showTime_s 是多少都按 2s 播放。
 *
 * 已知后置项（尚未实现，不是"永不显示"）：
 *   - 0x40/0x41 warnInfo 在本工程未解码，payload 未核实，未接入。
 *
 * 已实现：
 *   - Qt 的 id 32 受 SettingsMgr.show4h 门控（对应 state->show_4h），
 *     0x1C 事件与 0x03 弹窗两条入口都会丢弃被门控的 id。
 *   - Qt `visible: !settingsPageOpen && !ismedia`：通过 ov_tips_set_obstructed()
 *     只隐藏通用提示面板，燃油低横幅与 P 挡提示条不受影响。
 */
#ifndef CARMW_UI_OV_TIPS_H
#define CARMW_UI_OV_TIPS_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 播放队列容量。Qt 的 msgQueue 无上界，这里取有界值，满时挤掉最旧一条。 */
#define OV_TIPS_QUEUE_MAX 4U

typedef struct {
    uint16_t id;
    uint32_t duration_ms;
} ov_tips_item_t;

typedef struct {
    lv_obj_t *root;          /* 全页透明容器，坐标直接用页面坐标 */
    lv_obj_t *fuel_banner;   /* 230×60 @ (845,30) */
    lv_obj_t *gear_banner;   /* 宽随文字，y=30 */
    lv_obj_t *general;       /* 260×280 @ (834,253) */
    lv_obj_t *gen_title_icon;/* 通用提示标题图标 */
    lv_obj_t *gen_title;     /* 通用提示标题文本 */
    lv_obj_t *gen_icon;      /* 通用提示中部图标 */
    lv_obj_t *gen_text;      /* 通用提示正文 */
    lv_obj_t *gen_aux;       /* 正文隐藏时的红色 "FAULT"（Qt 的 textshow=false） */

    /* 0x03 弹窗的新事件判定，沿用既有约定 */
    uint32_t popup_sig;      /* 已处理的弹窗内容签名 */
    uint32_t popup_frame_tick; /* 最近一次收到 0x03 帧的时间 */

    /* 0x1C 事件消费进度：只处理 seq > warn_seen_seq 的事件 */
    bool warn_seen_init;
    uint32_t warn_seen_seq;

    /* 播放队列与当前条目 */
    ov_tips_item_t queue[OV_TIPS_QUEUE_MAX];
    uint8_t queue_head;
    uint8_t queue_count;
    uint32_t queue_dropped;  /* 队列满时挤掉的最旧事件数，仅诊断 */
    bool active;             /* 通用提示面板是否正在播放 */
    bool obstructed;         /* 设置页/媒体页遮挡：只隐藏通用提示面板 */
    uint32_t shown_tick;     /* 当前条目起点 */
    uint32_t shown_duration; /* 当前条目时长 ms */
} ov_tips_t;

bool ov_tips_create(ov_tips_t *ov, lv_obj_t *parent);
void ov_tips_apply(ov_tips_t *ov, const car_state_t *state, bool door_active);

/*
 * 遮挡通用提示面板：Qt 的 Item 可见性为 `!settingsPageOpen && !ismedia`。
 * 遮挡期间播放队列与计时照常推进，解除遮挡后继续显示当前条目。
 */
void ov_tips_set_obstructed(ov_tips_t *ov, bool obstructed);

void ov_tips_destroy(ov_tips_t *ov);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_OV_TIPS_H */
