/*
 * car_ui_key.c - 方向盘按键（0x20）到界面动作的映射
 *
 * 见 car_ui_key.h 的语义说明。按键解码与队列都在 car_link，本文件只做
 * “键值 -> 界面动作”的翻译，保证与 Qt 端 handleSteeringKey 一致。
 *
 * 设置页打开且可见时（Qt settingsPageOpen && speed < 5）：
 *   23 交给设置页，19/20/21/22/26 交给设置页，13 返回上级，
 *   31 被屏蔽（Qt 设置页打开时不切样式）。
 * 尚未收到速度帧时 23 也允许打开，避免“按键被静默吞掉”。
 */
#include "ui/car_ui_key.h"

#include <stdio.h>

#include "ui/ui_app.h"
#include "ui/ui_node.h"

/* Qt 键值（Handler.js keyNames 下标） */
#define KEY_BACK_HOME 13U
#define KEY_UP 19U
#define KEY_DOWN 20U
#define KEY_LEFT 21U
#define KEY_RIGHT 22U
#define KEY_OK 23U
#define KEY_NEXT_OR_BACK 26U
#define KEY_STYLE_SWITCH 31U

/* Qt pressed 取值 */
#define KEY_RELEASED 0U
#define KEY_SHORT 1U
#define KEY_LONG 2U

/* Qt main.qml：车速低于 5 才允许打开设置 */
#define SETTINGS_SPEED_LIMIT 5U

static car_key_event_t s_events[CAR_LINK_KEY_DEPTH];

/* 设置页是否可见（Qt：settingsPageOpen && speed < 5） */
static bool settings_visible(void)
{
    return ui_app_settings_is_open() && ui_app_settings_visible();
}

/*
 * 单条按键映射。
 * 返回 true 表示要转发给界面翻卡，并通过 ui_code 给出语义码。
 */
static bool map_key(const car_key_event_t *evt, ui_dispatch_ctx_t *ctx,
                    uint8_t *ui_code)
{
    bool in_settings;

    if(evt->pressed == KEY_RELEASED) {
        return false;
    }

    in_settings = settings_visible();

    /*
     * 长按 19/20 映射为左/右推摇杆（翻卡），其余长按无动作；
     * 设置页打开时不接受长按，避免误触翻卡。
     */
    if(evt->pressed == KEY_LONG) {
        if(in_settings) {
            return false;
        }
        if(evt->code == KEY_UP) {
            *ui_code = UI_KEY_PREV;
            return true;
        }
        if(evt->code == KEY_DOWN) {
            *ui_code = UI_KEY_NEXT;
            return true;
        }
        return false;
    }

    if(evt->pressed != KEY_SHORT) {
        return false;
    }

    switch(evt->code) {
    case KEY_BACK_HOME:
        if(in_settings) {
            (void)ui_app_settings_back();
            return false;
        }
        ctx->nav_home = true;
        return false;
    case KEY_UP:
        *ui_code = UI_KEY_UP;
        return true;
    case KEY_DOWN:
        *ui_code = UI_KEY_DOWN;
        return true;
    case KEY_LEFT:
        *ui_code = UI_KEY_LEFT;
        return true;
    case KEY_RIGHT:
        *ui_code = UI_KEY_RIGHT;
        return true;
    case KEY_NEXT_OR_BACK:
        /* Qt：设置页可见时 setNextOrBackIndex，否则无动作 */
        if(in_settings) {
            ui_app_settings_next_or_back();
        }
        return false;
    case KEY_STYLE_SWITCH:
        if(in_settings) {
            return false;
        }
        /*
         * 逐条立即执行（Qt GlobalData.steeringKeySwitch），不折叠到批次结束，
         * 保证同批 31 与方向键的相对顺序不变。
         */
        (void)ui_app_style_switch_next();
        ctx->style_switch = true;
        return false;
    case KEY_OK:
        /* Qt：设置不可见且车速达标时打开；已打开则交给设置页（并重置计时） */
        if(ui_app_settings_is_open()) {
            *ui_code = UI_KEY_OK;
            return true;
        }
        /* 未收到速度帧（has_data=false）时也允许打开，否则按键会被静默吞掉 */
        if(ui_app_settings_speed_ok() || !ui_app_has_data()) {
            ui_app_settings_open();
        }
        else {
            (void)printf("[key] OK ignored: speed>=%u\n",
                         (unsigned)SETTINGS_SPEED_LIMIT);
        }
        return false;
    default:
        return false;
    }
}

static size_t dispatch_events(size_t count, ui_dispatch_ctx_t *ctx)
{
    size_t i;
    size_t forwarded = 0U;

    for(i = 0U; i < count; ++i) {
        uint8_t ui_code = 0U;

        if(!map_key(&s_events[i], ctx, &ui_code)) {
            continue;
        }

        {
            ui_key_event_t ui_evt;

            ui_evt.code = ui_code;
            ui_evt.pressed = 1U;
            ui_app_handle_key(&ui_evt);
            ++forwarded;
        }
    }

    return forwarded;
}

size_t car_ui_key_dispatch(car_link_t *link, ui_dispatch_ctx_t *ctx)
{
    size_t count;

    if(link == NULL || ctx == NULL) {
        return 0U;
    }

    count = car_link_take_keys(link, s_events, CAR_LINK_KEY_DEPTH);
    if(count == 0U) {
        return 0U;
    }

    (void)dispatch_events(count, ctx);
    return count;
}

size_t car_ui_key_pump(car_link_t *link)
{
    size_t count;
    size_t forwarded;
    ui_dispatch_ctx_t ctx;

    if(link == NULL) {
        return 0U;
    }

    ctx.nav_home = false;
    ctx.style_switch = false;

    count = car_link_take_keys(link, s_events, CAR_LINK_KEY_DEPTH);
    if(count == 0U) {
        return 0U;
    }

    forwarded = dispatch_events(count, &ctx);

    /* 31 已在 map_key 中逐条执行，这里不再按批次补切 */
    return forwarded;
}
