/*
 * ui_clock.c - 时钟实现
 *
 * Qt 参考：PageSystemTimer.qml
 *   - Row 间距 2，冒号占 14px 固定宽度
 *   - 冒号以 1s 周期闪烁（QML 里改 visible，这里改 text_opa 避免行内重新布局）
 *   - 未收到时间时显示 "--" "." "--"
 */
#include "ui/ComControl/ui_clock.h"

#include <stdio.h>

#include "ui/ui_theme.h"

#define CLOCK_COLON_WIDTH 14
#define CLOCK_BLINK_MS 1000

/* 冒号闪烁：只切换透明度，保持行内布局稳定 */
static void blink_timer_cb(lv_timer_t *timer)
{
    ui_clock_t *clock = (ui_clock_t *)lv_timer_get_user_data(timer);
    lv_opa_t opa;

    if(clock == NULL || clock->colon == NULL) {
        return;
    }

    opa = lv_obj_get_style_text_opa(clock->colon, 0);

    lv_obj_set_style_text_opa(clock->colon,
                              opa == LV_OPA_TRANSP ? LV_OPA_COVER : LV_OPA_TRANSP,
                              0);
}

/* 根对象销毁时先释放定时器，避免回调访问已删除的标签 */
static void root_delete_cb(lv_event_t *e)
{
    ui_clock_t *clock = (ui_clock_t *)lv_event_get_user_data(e);

    if(clock != NULL && clock->blink != NULL) {
        lv_timer_delete(clock->blink);
        clock->blink = NULL;
    }
}

/* 创建一个白色文本标签 */
static lv_obj_t *clock_label(ui_clock_t *clock, const char *text,
                             const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(clock->root);

    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);

    return label;
}

bool ui_clock_create(ui_clock_t *clock, lv_obj_t *parent, int32_t x, int32_t y,
                     const lv_font_t *font)
{
    if(clock == NULL) {
        return false;
    }

    clock->root = ui_blank_container(parent);
    lv_obj_set_size(clock->root, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(clock->root, x, y);
    lv_obj_set_flex_flow(clock->root, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(clock->root, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(clock->root, 2, 0);

    clock->hour = clock_label(clock, "--", font);
    clock->colon = clock_label(clock, ".", font);
    clock->minute = clock_label(clock, "--", font);

    lv_obj_set_width(clock->colon, CLOCK_COLON_WIDTH);
    lv_obj_set_style_text_align(clock->colon, LV_TEXT_ALIGN_CENTER, 0);

    clock->shown_hour = 0;
    clock->shown_minute = 0;
    clock->shown_hour24 = false;
    clock->shown_valid = false;

    clock->blink = lv_timer_create(blink_timer_cb, CLOCK_BLINK_MS, clock);
    lv_obj_add_event_cb(clock->root, root_delete_cb, LV_EVENT_DELETE, clock);

    return true;
}

void ui_clock_apply(ui_clock_t *clock, const car_clock_t *state, bool hour24)
{
    char hour_buf[4];
    char minute_buf[4];
    uint8_t hour;
    uint8_t minute;

    if(clock == NULL || clock->hour == NULL || state == NULL || !state->valid) {
        return;
    }

    hour = state->hour;
    minute = state->minute;

    if(hour > 23U || minute > 59U) {
        return;
    }

    /* 12 小时制：0 点显示 12 */
    if(!hour24) {
        hour = (uint8_t)(hour % 12U);

        if(hour == 0U) {
            hour = 12U;
        }
    }

    if(clock->shown_valid && clock->shown_hour == hour &&
       clock->shown_minute == minute && clock->shown_hour24 == hour24) {
        return;
    }

    clock->shown_hour = hour;
    clock->shown_minute = minute;
    clock->shown_hour24 = hour24;
    clock->shown_valid = true;

    (void)snprintf(hour_buf, sizeof(hour_buf), "%02u", (unsigned)hour);
    (void)snprintf(minute_buf, sizeof(minute_buf), "%02u", (unsigned)minute);

    lv_label_set_text(clock->hour, hour_buf);
    lv_label_set_text(clock->colon, ":");
    lv_label_set_text(clock->minute, minute_buf);
}
