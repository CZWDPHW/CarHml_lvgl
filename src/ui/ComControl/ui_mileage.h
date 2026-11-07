/*
 * ui_mileage.h - 总里程 / 小计里程（所有样式通用）
 *
 * 对应 Qt ComControl/ComTotalMileage.qml 与 ComSubTotalMileage.qml。
 * 换算规则：
 *   总里程  odometer_raw 单位为 km，英里保留 1 位小数
 *   小计里程 Trip 原始值单位为 0.1 km，公里除 10，英里除 16.0934
 */
#ifndef CARMW_UI_COM_MILEAGE_H
#define CARMW_UI_COM_MILEAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* "1677721.5 KM" 一类文本足够 */
#define UI_MILEAGE_TEXT_MAX 24

typedef enum {
    UI_MILEAGE_ODO = 0,   /* 总里程，前缀固定 "ODO:" */
    UI_MILEAGE_TRIP,      /* 小计里程，前缀随 trip_type 变化 */
} ui_mileage_kind_t;

typedef struct {
    lv_obj_t *root;            /* 行容器 */
    lv_obj_t *prefix;          /* "ODO:" / "Trip A:" / "Trip B:" */
    lv_obj_t *value;           /* 数值文本 */
    ui_mileage_kind_t kind;    /* 总里程还是小计里程 */
    uint32_t shown_raw;        /* 上次显示的原始值 */
    uint8_t shown_key;         /* 上次的档位键：ODO 恒为 0，Trip 为 trip_type */
    bool shown_mile;           /* 上次的单位 */
} ui_mileage_t;

/* 创建里程行，x/y 为行容器左上角坐标，gap 由内部固定为 4 */
bool ui_mileage_create(ui_mileage_t *mileage, lv_obj_t *parent, int32_t x,
                       int32_t y, ui_mileage_kind_t kind,
                       const lv_font_t *font);

/*
 * 按车状态刷新里程；
 * mile 对应 SettingsMgr.mileType，true 时为英里。
 */
void ui_mileage_apply(ui_mileage_t *mileage, const car_mileage_t *src,
                      bool mile);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_COM_MILEAGE_H */