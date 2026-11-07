/*
 * ui_lamp.c - 警示灯 / 指示灯控件实现
 *
 * 闪烁与扫描都用 lv_anim 挂在灯自身的不透明度上，
 * 不做定时器轮询，也不会每帧产生 LVGL 对象。
 */
#include "ui/widgets/ui_lamp.h"

#include "ui/ui_theme.h"

/* 扫描动画单程时长 */
#define LAMP_SCAN_MS 600

/*
 * 动画执行体：把动画值写进灯的不透明度。
 * 闪烁时值在 0~255 之间跳变，扫描时在 40~255 之间往复。
 */
static void lamp_opa_anim_cb(void *var, int32_t value)
{
    lv_obj_t *img = (lv_obj_t *)var;

    lv_obj_set_style_image_opa(img, (lv_opa_t)value, 0);
}

/* 停止并释放灯上可能存在的扫描动画 */
static void lamp_anim_stop(ui_lamp_t *lamp)
{
    lv_anim_delete(lamp->img, lamp_opa_anim_cb);
}

/* 闪烁定时器：按周期直接切换可见/不可见，不做透明度渐变 */
static void lamp_blink_timer_cb(lv_timer_t *timer)
{
    ui_lamp_t *lamp = (ui_lamp_t *)lv_timer_get_user_data(timer);

    if(lamp == NULL || lamp->img == NULL) {
        return;
    }

    if(lv_obj_has_flag(lamp->img, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_remove_flag(lamp->img, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(lamp->img, LV_OBJ_FLAG_HIDDEN);
    }
}

/* 启动闪烁：定时器半周期切换一次，首次调用为亮 */
static void lamp_blink_start(ui_lamp_t *lamp)
{
    if(lamp->blink != NULL) {
        lv_timer_delete(lamp->blink);
    }

    lv_obj_remove_flag(lamp->img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_image_opa(lamp->img, LV_OPA_COVER, 0);

    lamp->blink = lv_timer_create(lamp_blink_timer_cb, lamp->blink_ms / 2U, lamp);
}

/* 停止闪烁并恢复可见，避免后续状态被隐藏标志挡住 */
static void lamp_blink_stop(ui_lamp_t *lamp)
{
    if(lamp->blink != NULL) {
        lv_timer_delete(lamp->blink);
        lamp->blink = NULL;
    }

    lv_obj_remove_flag(lamp->img, LV_OBJ_FLAG_HIDDEN);
}

/* 灯对象销毁时释放定时器，防止回调访问已删除的图标 */
static void lamp_delete_cb(lv_event_t *e)
{
    ui_lamp_t *lamp = (ui_lamp_t *)lv_event_get_user_data(e);

    if(lamp != NULL && lamp->blink != NULL) {
        lv_timer_delete(lamp->blink);
        lamp->blink = NULL;
    }
}

/* 创建并完成所有公共初始化 */
static lv_obj_t *lamp_image_create(ui_lamp_t *lamp, lv_obj_t *parent,
                                   const void *icon, int32_t x, int32_t y)
{
    lv_obj_t *img = lv_image_create(parent);

    lamp->img = img;
    lamp->state = UI_LAMP_OFF;
    lamp->blink_ms = UI_LAMP_BLINK_MS;
    lamp->blink = NULL;

    lv_image_set_src(img, icon);
    lv_obj_set_pos(img, x, y);
    lv_obj_set_style_image_opa(img, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(img, lamp_delete_cb, LV_EVENT_DELETE, lamp);

    return img;
}

void ui_lamp_create(ui_lamp_t *lamp, lv_obj_t *parent, const void *icon,
                    int32_t x, int32_t y)
{
    if(lamp == NULL) {
        return;
    }

    (void)lamp_image_create(lamp, parent, icon, x, y);
}

void ui_lamp_create_sized(ui_lamp_t *lamp, lv_obj_t *parent,
                          const lv_image_dsc_t *icon,
                          int32_t x, int32_t y, int32_t w, int32_t h)
{
    if(lamp == NULL) {
        return;
    }

    (void)lamp_image_create(lamp, parent, icon, x, y);

    /* 先定尺寸再设对齐模式，CONTAIN 会按对象尺寸算等比缩放 */
    lv_obj_set_size(lamp->img, w, h);
    lv_image_set_inner_align(lamp->img, LV_IMAGE_ALIGN_CONTAIN);
}

void ui_lamp_set_blink_ms(ui_lamp_t *lamp, uint32_t ms)
{
    if(lamp == NULL || ms < 2U || lamp->blink_ms == ms) {
        return;
    }

    lamp->blink_ms = ms;

    /* 正在闪烁时按新周期立即重启 */
    if(lamp->state == (uint8_t)UI_LAMP_BLINK) {
        lamp_blink_start(lamp);
    }
}

void ui_lamp_set_state(ui_lamp_t *lamp, ui_lamp_state_t state)
{
    if(lamp == NULL || lamp->img == NULL) {
        return;
    }

    if(lamp->state == (uint8_t)state) {
        return;
    }

    lamp->state = (uint8_t)state;
    lamp_blink_stop(lamp);
    lamp_anim_stop(lamp);

    switch(state) {
    case UI_LAMP_ON:
        lv_obj_set_style_image_opa(lamp->img, LV_OPA_COVER, 0);
        break;

    case UI_LAMP_OFF:
        lv_obj_set_style_image_opa(lamp->img, LV_OPA_TRANSP, 0);
        break;

    case UI_LAMP_BLINK:
        lamp_blink_start(lamp);
        break;

    case UI_LAMP_SCAN: {
        lv_anim_t a;

        lv_anim_init(&a);
        lv_anim_set_var(&a, lamp->img);
        lv_anim_set_exec_cb(&a, lamp_opa_anim_cb);
        lv_anim_set_values(&a, 40, LV_OPA_COVER);
        lv_anim_set_duration(&a, LAMP_SCAN_MS);
        lv_anim_set_playback_duration(&a, LAMP_SCAN_MS);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
        break;
    }

    default:
        lv_obj_set_style_image_opa(lamp->img, LV_OPA_TRANSP, 0);
        break;
    }
}

void ui_lamp_set_icon(ui_lamp_t *lamp, const void *icon)
{
    if(lamp == NULL || lamp->img == NULL || icon == NULL) {
        return;
    }

    lv_image_set_src(lamp->img, icon);
}

ui_lamp_state_t ui_lamp_get_state(const ui_lamp_t *lamp)
{
    if(lamp == NULL) {
        return UI_LAMP_OFF;
    }

    return (ui_lamp_state_t)lamp->state;
}