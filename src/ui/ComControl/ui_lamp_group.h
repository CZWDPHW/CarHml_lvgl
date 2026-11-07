/*
 * ui_lamp_group.h - 绝对坐标警示灯组（所有样式通用）
 *
 * 对应 Qt 页面里散落在表盘周围的 LampControl 灯：
 * 每盏灯的位置由样式层给出，图标由"故障码 -> 图标"映射决定。
 * 控件只负责布局与渲染，不感知具体业务含义。
 */
#ifndef CARMW_UI_COM_LAMP_GROUP_H
#define CARMW_UI_COM_LAMP_GROUP_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"
#include "ui/widgets/ui_lamp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 单组最多灯数 / 单灯最多图标数 */
#define UI_LAMP_GROUP_MAX 24
#define UI_LAMP_GROUP_VARIANTS 4

/*
 * 同一盏灯的一种图标。
 * code2 用于两个故障码共用一张图（如自动驻车 46/91），0 表示不使用。
 */
typedef struct {
    const char *icon;
    uint8_t code;
    uint8_t code2;
} ui_lamp_group_variant_t;

/*
 * 单灯的可见性门控来源（样式层按 Qt 的 visible 表达式选择）。
 * 驾驶辅助三项的门控同时决定图标：开关关闭整盏灯熄灭；开关打开时按
 * car_high_func.lane 的实时状态决定白/黄图标（Qt 底排 jin/BSD/real_road）。
 */
typedef enum {
    UI_LAMP_GATE_NONE = 0,
    UI_LAMP_GATE_SHOW_4H,           /* 仅 state->show_4h 为真时允许点亮（Qt show4h） */
    UI_LAMP_GATE_EMERGENCY_BRAKING, /* Qt emergencyBraking，黄图条件 showcar > 0 */
    UI_LAMP_GATE_BLIND_SPOT,        /* Qt blindSpotWarning，黄图条件 showarea > 0 */
    UI_LAMP_GATE_LANE_DEPARTURE,    /* Qt laneDepartureWarning，黄图条件 carState > 1 */
} ui_lamp_gate_t;

/* 样式层提供的单灯配置：绝对坐标 + 图标变体表 */
typedef struct {
    ui_lamp_group_variant_t variants[UI_LAMP_GROUP_VARIANTS];
    uint8_t variant_count;
    uint8_t gate; /* ui_lamp_gate_t，缺省 0（不受设置门控） */
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} ui_lamp_group_item_t;

/* 单灯运行期状态 */
typedef struct {
    ui_lamp_t lamp;
    lv_image_dsc_t dsc[UI_LAMP_GROUP_VARIANTS]; /* 缩放需要整图驻留 RAM */
    int8_t active;                              /* 当前图标下标，-1 表示未选 */
} ui_lamp_group_item_rt_t;

typedef struct {
    ui_lamp_group_item_rt_t items[UI_LAMP_GROUP_MAX];
    const ui_lamp_group_item_t *cfg; /* 灯表由样式层长期持有 */
    uint8_t count;
} ui_lamp_group_t;

/*
 * 创建整组灯。group 必须由调用方长期持有（建议页面内 static），
 * 这样样式切换重建页面时已加载的图片可以复用，不会重复 malloc。
 */
bool ui_lamp_group_create(ui_lamp_group_t *group, lv_obj_t *parent,
                          const ui_lamp_group_item_t *items, uint8_t count);

/* 按故障码刷新整组灯；无变化时内部跳过重设 */
void ui_lamp_group_apply(ui_lamp_group_t *group, const car_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_COM_LAMP_GROUP_H */