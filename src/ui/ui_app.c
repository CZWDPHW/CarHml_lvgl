/*
 * ui_app.c - 页面舞台与样式切换
 *
 * 舞台是屏幕上的全屏透明容器，样式将自己的页面创建在舞台下。
 * 切换样式时先销毁旧组件，再创建新组件，并立即补一次当前状态。
 * 设置菜单是舞台上独立于样式的覆盖层，样式重建不丢层级状态。
 */
#include "ui/ui_app.h"

#include <string.h>

#include "carmw/car_settings.h"
#include "ui/settings/ui_settings_page.h"
#include "ui/ui_common.h"
#include "ui/ui_session.h"

/* 设置页无按键自动关闭（Qt settingTimerOut 8000ms） */
#define SETTINGS_TIMEOUT_MS 8000U

/* 车速低于该值才允许打开设置（Qt speed < 5） */
#define SETTINGS_SPEED_LIMIT 5U

static lv_obj_t *s_stage;
static ui_node_t *s_node;
static const ui_style_desc_t *s_desc;
static car_state_view_t s_last_view;
static bool s_has_view;

/* 设置菜单：覆盖层对象 + 跨样式保存在这里的会话状态 */
static ui_settings_page_t s_settings_page;
static bool s_settings_open;
static uint32_t s_settings_tick;
static car_settings_t *s_settings_store;
static ui_app_pending_cb_t s_pending_cb;
/* 互联会话标志：无 ZLink SDK 时由演示“已连接”或 6.6 回调置位 */
static bool s_carplay_active;

static bool settings_speed_ok(void)
{
    return s_has_view && s_last_view.has_data &&
           (s_last_view.state.speed_rpm.speed_kph < SETTINGS_SPEED_LIMIT);
}

/* Qt：设置可见 = settingsPageOpen && speed < 5 */
static void settings_sync(void)
{
    bool visible = s_settings_open && settings_speed_ok();

    if(s_settings_page.root == NULL || s_settings_page.active == visible) {
        return;
    }

    ui_settings_page_set_active(&s_settings_page, visible);
    ui_settings_page_apply(&s_settings_page);
}

/* 设置是否还有改动没落盘：显示“改动待保存”提示（ACC 关闭时统一写入） */
static void settings_pending_sync(void)
{
    bool pending = (s_pending_cb != NULL) && s_pending_cb();

    ui_settings_page_set_pending(&s_settings_page, pending);
}

static void settings_tick_restart(void)
{
    s_settings_tick = lv_tick_get();
}

/*
 * 创建全屏舞台容器，并给屏幕铺黑底；
 * 同时创建设置覆盖层（位于舞台最上层，默认隐藏）。
 */
bool ui_app_init(void)
{
    lv_obj_t *scr;

    if(s_stage != NULL) {
        return true;
    }

    scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    s_stage = lv_obj_create(scr);
    lv_obj_remove_style_all(s_stage);
    lv_obj_remove_flag(s_stage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_stage, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(s_stage, 0, 0);
    lv_obj_set_style_bg_color(s_stage, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_stage, LV_OPA_COVER, 0);

    ui_session_reset();
    ui_settings_state_init(&s_settings_page.st);
    if(!ui_settings_page_create(&s_settings_page, s_stage)) {
        lv_obj_delete(s_stage);
        s_stage = NULL;
        return false;
    }

    return true;
}

/* 先卸载当前样式组件，再销毁设置覆盖层与舞台 */
void ui_app_deinit(void)
{
    if(s_node != NULL) {
        if(s_node->vt->on_hide != NULL) {
            s_node->vt->on_hide(s_node);
        }
        if(s_node->vt->destroy != NULL) {
            s_node->vt->destroy(s_node);
        }
        lv_free(s_node);
        s_node = NULL;
    }

    ui_settings_page_destroy(&s_settings_page);
    s_settings_open = false;
    /* 样式重建后由 main.c 重新注入设置；期间不能拿旧指针刷新 */
    s_settings_store = NULL;
    s_carplay_active = false;

    if(s_stage != NULL) {
        lv_obj_delete(s_stage);
        s_stage = NULL;
    }

    s_desc = NULL;
    s_has_view = false;
    ui_session_reset();
}

/*
 * 切换样式：卸载旧节点 → 按描述分配并清零私有结构 → create。
 * 任一环节失败时回滚到“无样式”状态并返回 false。
 * 设置覆盖层在舞台上层，不受影响；装好后重新提到最前。
 */
bool ui_app_set_style(const char *id)
{
    const ui_style_desc_t *desc = ui_style_find(id);

    if(desc == NULL || s_stage == NULL) {
        return false;
    }

    if(s_desc == desc && s_node != NULL) {
        return true;
    }

    if(s_node != NULL) {
        if(s_node->vt->on_hide != NULL) {
            s_node->vt->on_hide(s_node);
        }
        if(s_node->vt->destroy != NULL) {
            s_node->vt->destroy(s_node);
        }
        lv_free(s_node);
        s_node = NULL;
    }

    s_desc = desc;
    s_node = lv_malloc(desc->node->size);

    if(s_node == NULL) {
        s_desc = NULL;
        return false;
    }

    memset(s_node, 0, desc->node->size);
    s_node->vt = desc->node;

    if(!desc->node->create(s_node, s_stage)) {
        lv_free(s_node);
        s_node = NULL;
        s_desc = NULL;
        return false;
    }

    if(s_has_view && s_node->vt->apply != NULL) {
        s_node->vt->apply(s_node, &s_last_view);
    }

    if(s_settings_page.root != NULL) {
        lv_obj_move_foreground(s_settings_page.root);
    }

    return true;
}

/* 在注册表中找到当前样式的序号，切到下一个（末尾回到开头） */
bool ui_app_next_style(void)
{
    size_t count = ui_style_count();
    size_t index = 0;

    if(count == 0) {
        return false;
    }

    if(s_desc != NULL) {
        for(size_t i = 0; i < count; i++) {
            if(ui_style_at(i) == s_desc) {
                index = i;
                break;
            }
        }
    }

    return ui_app_set_style(ui_style_at((index + 1) % count)->id);
}

/* 返回当前样式描述，供 PC 演示按钮等外部查询 */
const ui_style_desc_t *ui_app_current_style(void)
{
    return s_desc;
}

lv_obj_t *ui_app_current_root(void)
{
    return (s_node != NULL) ? s_node->root : NULL;
}

/* 设置页覆盖层的根对象（未创建时为 NULL，供测试/截图遍历） */
lv_obj_t *ui_app_settings_root(void)
{
    return s_settings_page.root;
}

/* 本地设置的单位优先于 MCU 帧里的单位位（Qt GlobalData / SettingsMgr 以设置为准） */
static void settings_override_units(void)
{
    if(s_settings_store == NULL) {
        return;
    }
    /* 温度单位 Qt 只看 SettingsMgr.tempType，不依赖单位帧 */
    s_last_view.state.units.fahrenheit =
        s_settings_store->values.unit_temperature == (uint8_t)CAR_UNIT_IMPERIAL;
    if(s_last_view.state.units.valid) {
        s_last_view.state.units.mile =
            s_settings_store->values.unit_mileage == (uint8_t)CAR_UNIT_IMPERIAL;
    }
    /* 行车信息显示开关同样以设置为准（Qt 直接绑定 SettingsMgr） */
    s_last_view.state.outside_temp_on = s_settings_store->values.out_temperature != 0U;
    s_last_view.state.gear_on = s_settings_store->values.gear_on != 0U;
    s_last_view.state.avg_fuel_on = s_settings_store->values.avg_fuel_on != 0U;
    /* 常规设置与 ECO 指示同样直接绑定本地设置 */
    s_last_view.state.eco_indicator = s_settings_store->values.eco_indicator != 0U;
    s_last_view.state.eco_or_fuel = s_settings_store->values.eco_or_fuel != 0U;
    s_last_view.state.speed_limit_on = s_settings_store->values.speed_limit_on != 0U;
    s_last_view.state.daylight = s_settings_store->values.daylight;
    s_last_view.state.nightlight = s_settings_store->values.nightlight;
    /* 雷达与四驱可见性是本地偏好，不是 MCU 实时状态 */
    s_last_view.state.radar_on = s_settings_store->values.radar_on != 0U;
    s_last_view.state.show_4h = s_settings_store->values.show_4h != 0U;
}

/* 缓存最新快照后再转发，保证样式切换后能立即补一次状态 */
void ui_app_apply(const car_state_view_t *view)
{
    if(view == NULL) {
        return;
    }

    s_last_view = *view;
    s_has_view = true;
    settings_override_units();
    /* 互联会话是 UI 会话状态，链接层不产生，按 ui_app_set_carplay_active 注入 */
    s_last_view.state.carplay_active = s_carplay_active;

    /* ECO 历史页的“当前”白柱跟随实时平均油耗（Qt GlobalData.items.avgfuel） */
    ui_settings_page_set_eco_current(&s_settings_page,
                                     s_last_view.state.trip.valid
                                         ? s_last_view.state.trip.avg_fuel_x10
                                         : 0U);

    /* 驾驶辅助三项只显示 MCU 回读值，设置页不做本地翻转 */
    ui_settings_page_set_driver_assist(
        &s_settings_page,
        s_last_view.state.driver_assist.emergency_braking,
        s_last_view.state.driver_assist.lane_departure_warning,
        s_last_view.state.driver_assist.blind_spot_warning);
    ui_settings_page_set_speed_warning(
        &s_settings_page, s_last_view.state.speed_warning.valid,
        s_last_view.state.speed_warning.speed_kph);
    ui_settings_page_set_version(
        &s_settings_page, s_last_view.state.version.main,
        s_last_view.state.version.main_valid, s_last_view.state.version.chip,
        s_last_view.state.version.chip_valid, s_last_view.state.version.wired,
        s_last_view.state.version.wired_valid);

    /* 会话状态与当前样式无关：样式切走期间也按快照持续消费事件计时 */
    if(view->has_data) {
        ui_session_apply(&s_last_view.state);
    }

    if(s_node != NULL && s_node->vt->apply != NULL) {
        s_node->vt->apply(s_node, &s_last_view);
    }

    /* 车速超过门限时 Qt 直接隐藏设置页（settingsPageOpen 不清除） */
    settings_sync();
    settings_pending_sync();

    /* 8 秒无按键：Qt settingTimerOut 触发 reSettings */
    if(s_settings_open &&
       lv_tick_elaps(s_settings_tick) >= SETTINGS_TIMEOUT_MS) {
        ui_app_settings_close();
    }
}

/* 按键事件：设置页可见时优先消费，其余转发给样式 */
void ui_app_handle_key(const ui_key_event_t *evt)
{
    if(evt == NULL) {
        return;
    }

    if(s_settings_open) {
        settings_tick_restart();
    }

    if(ui_settings_page_key(&s_settings_page, evt)) {
        return;
    }
    if(s_node != NULL && s_node->vt->on_key != NULL) {
        s_node->vt->on_key(s_node, evt);
    }
}

/* 注入设置存储：缓存并交给设置覆盖层 */
void ui_app_settings_set_settings(car_settings_t *settings)
{
    s_settings_store = settings;
    ui_settings_page_set_settings(settings);

    if(settings != NULL) {
        ui_settings_page_refresh(&s_settings_page);
    }
}

/* 注入命令通道：设置页的背光/超速报警下发 */
void ui_app_settings_set_cmd_cb(ui_settings_page_cmd_cb_t cb)
{
    ui_settings_page_set_cmd_cb(cb);
}

bool ui_app_settings_is_open(void)
{
    return s_settings_open;
}

bool ui_app_settings_visible(void)
{
    return s_settings_open && settings_speed_ok();
}

bool ui_app_settings_speed_ok(void)
{
    return settings_speed_ok();
}

bool ui_app_has_data(void)
{
    return s_has_view && s_last_view.has_data;
}

void ui_app_settings_set_pending_cb(ui_app_pending_cb_t cb)
{
    s_pending_cb = cb;
}

void ui_app_settings_open(void)
{
    if(s_settings_page.root == NULL) {
        return;
    }

    s_settings_open = true;
    settings_tick_restart();
    /* Qt：打开时从根页重新开始（reSettings 后必然是根页） */
    ui_settings_state_init(&s_settings_page.st);
    /* 调用方已校验车速（Qt：speed < 5），这里不受快照门控影响 */
    ui_settings_page_set_active(&s_settings_page, true);
    ui_settings_page_apply(&s_settings_page);
    settings_pending_sync();
}

/* Qt reSettings：关闭、停表、回到根页并清空选择 */
void ui_app_settings_close(void)
{
    s_settings_open = false;
    ui_settings_state_init(&s_settings_page.st);
    ui_settings_page_set_active(&s_settings_page, false);
    ui_settings_page_apply(&s_settings_page);
}

/* 设置值变化后刷新显示：设置页 + 用缓存快照重刷样式，让单位/卡片立即生效 */
void ui_app_settings_refresh(void)
{
    if(s_settings_store == NULL) {
        return;
    }

    ui_settings_page_refresh(&s_settings_page);

    /* 只重刷样式，不再走 ui_session_apply，避免重复消费事件计时 */
    if(s_has_view && s_node != NULL && s_node->vt->apply != NULL) {
        settings_override_units();
        s_node->vt->apply(s_node, &s_last_view);
    }
}

uint32_t ui_app_center_card_mask(void)
{
    return (s_settings_store != NULL) ? s_settings_store->values.center_card_mask
                                      : CAR_SETTINGS_CARD_DEFAULT;
}

/* 13：子页返回上级；根页没有上级时关闭设置 */
bool ui_app_settings_back(void)
{
    if(!s_settings_open) {
        return false;
    }

    settings_tick_restart();
    if(ui_settings_page_back(&s_settings_page)) {
        return true;
    }

    ui_app_settings_close();
    return true;
}

/* Qt setNextOrBackIndex：根页按索引前进，子页等同上/下选择 */
void ui_app_settings_next_or_back(void)
{
    if(!s_settings_open) {
        return;
    }

    settings_tick_restart();
    ui_settings_page_next_or_back(&s_settings_page);
    ui_settings_page_apply(&s_settings_page);
}

void ui_app_demo_media(uint8_t mode, const char *title,
                       const char *line1, const char *line2)
{
    if(s_node != NULL && s_node->vt->on_demo_media != NULL) {
        s_node->vt->on_demo_media(s_node, mode, title, line1, line2);
    }
}

void ui_app_demo_connect(uint8_t state, const char *mode_text,
                         const char *bt_name)
{
    /* Qt 在 ZLink 会话启动后置 carPlayModel=true；无 SDK 时用演示“已连接”模拟 */
    s_carplay_active = (state == 2U);

    if(s_node != NULL && s_node->vt->on_demo_connect != NULL) {
        s_node->vt->on_demo_connect(s_node, state, mode_text, bt_name);
    }
}

/* 6.6 真实互联 SDK 接入后由会话回调驱动；当前板端无调用则保持 false */
void ui_app_set_carplay_active(bool active)
{
    s_carplay_active = active;
}
