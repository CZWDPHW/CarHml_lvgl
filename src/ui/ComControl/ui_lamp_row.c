/*
 * ui_lamp_row.c - 横向灯排实现
 *
 * 布局与 Qt Row 一致：无间距、按子项宽度依次排布、行高内垂直居中。
 * 闪烁周期换算：ui_lamp 的 blink_ms 是完整亮灭周期（定时器半周期切换），
 * Qt 快闪每 300ms 切一次 -> 600ms，慢闪每 3000ms 切一次 -> 6000ms。
 */
#include "ui/ComControl/ui_lamp_row.h"

#include "ui/ui_assets.h"

#define LAMP_BLINK_FAST_MS 600U
#define LAMP_BLINK_SLOW_MS 6000U

/* 故障等级 -> ui_lamp 状态 */
static void lamp_apply_level(ui_lamp_t *lamp, uint8_t level)
{
    switch(level) {
    case (uint8_t)CAR_WARN_ON:
        ui_lamp_set_state(lamp, UI_LAMP_ON);
        break;

    case (uint8_t)CAR_WARN_BLINK_FAST:
        ui_lamp_set_blink_ms(lamp, LAMP_BLINK_FAST_MS);
        ui_lamp_set_state(lamp, UI_LAMP_BLINK);
        break;

    case (uint8_t)CAR_WARN_BLINK_SLOW:
        ui_lamp_set_blink_ms(lamp, LAMP_BLINK_SLOW_MS);
        ui_lamp_set_state(lamp, UI_LAMP_BLINK);
        break;

    default:
        ui_lamp_set_state(lamp, UI_LAMP_OFF);
        break;
    }
}

/*
 * @brief 创建横向灯排
 *
 * @param row 横向灯排结构体指针
 * @param parent 父容器指针
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param row_h 行高
 * @param items 子项配置数组指针
 * @param count 子项数量
 * @return true 成功创建
 * @return false 失败
 */
bool ui_lamp_row_create(ui_lamp_row_t *row, lv_obj_t *parent, int32_t x, int32_t y,
                        int32_t row_h, const ui_lamp_row_item_t *items,
                        uint8_t count)
{
    int32_t cursor = x;

    if(row == NULL || parent == NULL || items == NULL ||
       count == 0U || count > UI_LAMP_ROW_MAX) {
        return false;
    }

    row->count = count;

    for(uint8_t i = 0; i < count; ++i) {
        const ui_lamp_row_item_t *cfg = &items[i];
        ui_lamp_row_item_rt_t *rt = &row->items[i];
        int32_t item_y;

        rt->dsc = (lv_image_dsc_t){0};
        rt->dsc_alt = (lv_image_dsc_t){0};
        rt->code = cfg->code;
        rt->alt_code = cfg->alt_code;
        rt->use_alt = false;

        /* 缩放需要整图驻留 RAM：流式 bin 图不能做变换 */
        if(!ui_assets_load_image(&rt->dsc, cfg->icon)) {
            return false;
        }

        if(cfg->icon_alt != NULL &&
           !ui_assets_load_image(&rt->dsc_alt, cfg->icon_alt)) {
            return false;
        }

        item_y = y + (row_h - cfg->h) / 2;

        ui_lamp_create_sized(&rt->lamp, parent, &rt->dsc,
                             cursor, item_y, cfg->w, cfg->h);
        cursor += cfg->w;
    }

    return true;
}

void ui_lamp_row_apply(ui_lamp_row_t *row, const car_state_t *state)
{
    if(row == NULL || state == NULL) {
        return;
    }

    for(uint8_t i = 0; i < row->count; ++i) {
        ui_lamp_row_item_rt_t *rt = &row->items[i];
        uint8_t main_level = car_state_warning_level(state, rt->code);
        uint8_t alt_level = 0;
        uint8_t level;
        bool use_alt;

        if(rt->alt_code != 0U) {
            alt_level = car_state_warning_level(state, rt->alt_code);
        }

        /* 主/备故障码都亮时取更高等级；有备选码时优先显示故障图 */
        level = (alt_level > main_level) ? alt_level : main_level;
        use_alt = (rt->alt_code != 0U) && (alt_level != 0U) &&
                  (rt->dsc_alt.data != NULL);

        if(use_alt != rt->use_alt) {
            rt->use_alt = use_alt;
            ui_lamp_set_icon(&rt->lamp, use_alt ? &rt->dsc_alt : &rt->dsc);
        }

        lamp_apply_level(&rt->lamp, level);
    }
}