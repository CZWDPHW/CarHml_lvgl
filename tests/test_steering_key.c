/*
 * test_steering_key.c - 方向盘按键 0x20 到界面的整条链路回归
 *
 * 真帧路径：内存端口 -> parser -> registry -> decode -> 链路按键队列
 *           -> car_ui_key_dispatch -> ui_app_handle_key
 * 行为对齐 Qt 的 Handler.js handleSteeringKey / main.qml：
 *   pressed: 0 释放 / 1 短按 / 2 长按
 *   短按 13 返回首页、19 上一张、20 下一张、21 左、22 右、23 确定、31 切样式
 *   长按 19/20 映射为左/右推摇杆
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "lvgl.h"

#include "carmw/car_link.h"
#include "carmw/car_port_memory.h"
#include "carmw/car_protocol.h"
#include "carmw/car_state.h"
#include "ui/car_ui_key.h"
#include "ui/ui_app.h"
#include "ui/ui_node.h"
#include "ui/ui_style.h"
#include "ui/ui_fonts.h"
#include "ui/settings/ui_settings_page.h"

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if(!(cond)) {                                                        \
            ++g_failures;                                                    \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
        }                                                                    \
    } while(0)

static int g_checks;
static int g_failures;

static car_port_memory_t s_mem;
static car_port_t s_port;
static car_link_t s_link;

/* 设置存储：整条链路测试用内存值（板端为 main.c 里的持久设置） */
static car_settings_t s_settings;

static void settings_changed(void *user, uint32_t mask)
{
    (void)user;
    (void)mask;
    ui_app_settings_refresh();
}

/* 推一帧 0x20 并跑一遍解析/解码/入队 */
static void push_key(uint8_t code, uint8_t pressed)
{
    uint8_t payload[2];
    uint8_t frame[32];
    size_t len;

    payload[0] = code;
    payload[1] = pressed;
    car_port_memory_advance(&s_mem, 20U);
    len = car_frame_encode(0x20U, (uint8_t)CAR_OP_STATUS, payload, 2U,
                           frame, sizeof(frame));
    CHECK(len > 0U);
    car_port_memory_push(&s_mem, frame, len);
    car_link_pump(&s_link, 0);
}

static lv_obj_t *find_child_by_rect(lv_obj_t *parent, int32_t x, int32_t y,
                                    int32_t w, int32_t h)
{
    for(int32_t i = 0; i >= 0; ++i) {
        lv_obj_t *child = lv_obj_get_child(parent, i);

        if(child == NULL) {
            break;
        }
        if(lv_obj_get_x(child) == x && lv_obj_get_y(child) == y &&
           lv_obj_get_width(child) == w && lv_obj_get_height(child) == h) {
            return child;
        }
    }

    return NULL;
}

/* 当前可见中心卡片的序号（隐藏的卡片尺寸为 0，不算） */
static int visible_card_index(lv_obj_t *stack)
{
    for(int32_t i = 0; i >= 0; ++i) {
        lv_obj_t *card = lv_obj_get_child(stack, i);

        if(card == NULL) {
            break;
        }
        if(!lv_obj_has_flag(card, LV_OBJ_FLAG_HIDDEN) &&
           lv_obj_get_width(card) > 0 && lv_obj_get_height(card) > 0) {
            return (int)i;
        }
    }
    return -1;
}

static lv_obj_t *s_page;
static lv_obj_t *s_stack;

/* 与 Qt settingStackView 一致的下钻顺序：先选分类，再选中子项 */
#define ROOT_MY_CAR_INDEX 2U
#define ROOT_INFO_INDEX 1U
#define ROOT_ITINERARY_INDEX 0U
#define MYCAR_STYLE_INDEX 0U
#define MYCAR_UNIT_INDEX 1U
#define MYCAR_CARD_INDEX 2U
#define MYCAR_ROUTINE_INDEX 3U
#define MYCAR_ECO_INDEX 4U
#define MYCAR_ECO_HISTORY_INDEX 5U

static ui_dispatch_ctx_t s_ctx;

/* 根页按右键直到选中下标 index（Qt：虚焦点为空时第一次点击落到第 0 项） */
static void root_move_to(uint8_t index)
{
    for(uint8_t i = 0U; i <= index; ++i) {
        push_key(22U, 1U);
        (void)car_ui_key_dispatch(&s_link, &s_ctx);
    }
}

/* 首次锁定后焦点在第一个可选项；按菜单 ID 找车辆入口，避免写死按一次下键。 */
static void enter_itinerary_vehicle(void)
{
    root_move_to(ROOT_ITINERARY_INDEX);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    const ui_settings_menu_t *menu = ui_settings_menu_get(UI_SETTINGS_MENU_ITINERARY);
    uint8_t target = ui_settings_menu_index(UI_SETTINGS_MENU_ITINERARY, UI_SETTINGS_IT_VEHICLE);
    CHECK(target != UI_SETTINGS_MENU_INDEX_NONE);
    if(target == UI_SETTINGS_MENU_INDEX_NONE) return;
    bool first = true;
    for(uint8_t row = 0; row <= target; ++row) {
        if(menu->items[row] == UI_SETTINGS_IT_TRIP) continue;
        if(first) { first = false; continue; }
        push_key(20U, 1U);
        CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    }
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
}

/* 根页选中“我的座驾”并进入（Qt 第一次 OK 只是进入分类，焦点落在第一项） */
static void enter_mycar(void)
{
    root_move_to(ROOT_MY_CAR_INDEX);
    push_key(23U, 1U);
    (void)car_ui_key_dispatch(&s_link, &s_ctx);
}

/* 在我的座驾里从当前项下移到指定项 */
static void mycar_move_to(uint8_t from, uint8_t to)
{
    while(from < to) {
        push_key(20U, 1U);
        (void)car_ui_key_dispatch(&s_link, &s_ctx);
        ++from;
    }
    while(from > to) {
        push_key(19U, 1U);
        (void)car_ui_key_dispatch(&s_link, &s_ctx);
        --from;
    }
}

static void setup_forester(void)
{
    car_state_t st;
    car_state_view_t view;

    car_state_init(&st);
    memset(&view, 0, sizeof(view));
    view.has_data = true;
    view.state = st;

    ui_app_deinit();
    CHECK(ui_app_init());
    car_settings_reset(&s_settings);
    car_settings_set_notify(&s_settings, settings_changed, NULL);
    ui_app_settings_set_settings(&s_settings);
    CHECK(ui_app_set_style("forester1"));
    ui_app_apply(&view);

    s_page = ui_app_current_root();
    s_stack = NULL;
    if(s_page != NULL) {
        lv_obj_update_layout(lv_screen_active());
        s_stack = find_child_by_rect(s_page, 553, 140, 814, 509);
    }

    memset(&s_mem, 0, sizeof(s_mem));
    car_port_memory_attach(&s_port, &s_mem);
    CHECK(car_link_init(&s_link, &s_port));
    car_link_pump(&s_link, 0); /* 完成握手 */
}

static void teardown_forester(void)
{
    car_link_deinit(&s_link);
    ui_app_deinit();
}

/* 键值原样透传：不再按低 5 位截断（与 Qt data[0] & 0xFF 一致） */
static void test_code_not_masked(void)
{
    car_key_event_t evt;

    setup_forester();
    /* 0x25 的低 5 位是 5，若被截断就变成"下一曲" */
    push_key(0x25U, 1U);
    CHECK(car_link_take_keys(&s_link, &evt, 1U) == 1U);
    CHECK(evt.code == 0x25U);
    CHECK(evt.pressed == 1U);
    teardown_forester();
}

/* 短按 19/20/21/22 在非 CarPlay、非设置页时翻中间卡片（首尾循环） */
static void test_short_key_cycle(void)
{
    ui_dispatch_ctx_t ctx = {0};

    setup_forester();
    CHECK(s_stack != NULL);
    if(s_stack == NULL) {
        teardown_forester();
        return;
    }
    CHECK(lv_obj_get_child_count(s_stack) == 10U);
    CHECK(visible_card_index(s_stack) == 0);

    /* 默认 mask 0x3FD：4X4-I（栈下标 2）隐藏，下移直接到行程（栈下标 1） */
    push_key(20U, 1U); /* 下 */
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(visible_card_index(s_stack) == 1);

    push_key(22U, 1U); /* 右 */
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(visible_card_index(s_stack) == 3);

    push_key(19U, 1U); /* 上 */
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(visible_card_index(s_stack) == 1);

    push_key(21U, 1U); /* 左 */
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(visible_card_index(s_stack) == 0);

    push_key(21U, 1U); /* 首尾循环 */
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(visible_card_index(s_stack) == 9);

    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(visible_card_index(s_stack) == 0);

    teardown_forester();
}

/* 释放(0) 不翻卡；长按 20 等同右；23 打开设置；13 回首页；31 只切样式 */
static void test_press_state_filter(void)
{
    ui_dispatch_ctx_t ctx = {0};

    setup_forester();
    CHECK(s_stack != NULL);
    if(s_stack == NULL) {
        teardown_forester();
        return;
    }

    push_key(20U, 0U);
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(visible_card_index(s_stack) == 0);

    push_key(20U, 2U); /* 长按 20 等同右，见下个用例 */
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(visible_card_index(s_stack) == 1);

    push_key(23U, 1U); /* 确定：车速 < 5，打开设置 */
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(ui_app_settings_is_open());
    CHECK(visible_card_index(s_stack) == 1);

    push_key(13U, 1U); /* 设置根页：返回即关闭设置 */
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(!ui_app_settings_is_open());
    CHECK(!ctx.nav_home);
    CHECK(visible_card_index(s_stack) == 1);

    push_key(13U, 1U); /* 设置关闭后：返回首页 */
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(ctx.nav_home);
    CHECK(!ctx.style_switch);
    CHECK(visible_card_index(s_stack) == 1);

    teardown_forester();
}

/* 设置页打开时的按键路由：车速门限、13 逐级返回、31 屏蔽 */
static void test_settings_navigation(void)
{
    ui_dispatch_ctx_t ctx = {0};

    setup_forester();
    if(s_stack == NULL) {
        teardown_forester();
        return;
    }

    /* 车速 60 时 23 不打开设置 */
    {
        car_state_t st;
        car_state_view_t view;

        car_state_init(&st);
        st.speed_rpm.speed_kph = 60U;
        st.speed_rpm.valid = true;
        memset(&view, 0, sizeof(view));
        view.has_data = true;
        view.state = st;
        ui_app_apply(&view);
    }
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(!ui_app_settings_is_open());

    /* 车速回到 0：23 打开，8 秒计时在每帧按键时重置 */
    {
        car_state_t st;
        car_state_view_t view;

        car_state_init(&st);
        memset(&view, 0, sizeof(view));
        view.has_data = true;
        view.state = st;
        ui_app_apply(&view);
    }
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(ui_app_settings_is_open());
    CHECK(ui_app_settings_visible());

    /* 19/20 交给设置页：不再翻中心卡片 */
    CHECK(visible_card_index(s_stack) == 0);
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(visible_card_index(s_stack) == 0);

    /* 13 在根页：关闭设置 */
    push_key(13U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(!ui_app_settings_is_open());

    /* 重新打开后 31 被屏蔽，不切样式 */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    push_key(31U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(!ctx.style_switch);
    CHECK(strcmp(ui_app_current_style()->id, "forester1") == 0);

    teardown_forester();
}

/* 23 进入设置后，8 秒无按键在下一帧快照里自动关闭（Qt settingTimerOut） */
static void test_settings_timeout(void)
{
    ui_dispatch_ctx_t ctx = {0};
    car_state_t st;
    car_state_view_t view;

    setup_forester();
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(ui_app_settings_is_open());

    lv_tick_inc(9000);
    car_state_init(&st);
    memset(&view, 0, sizeof(view));
    view.has_data = true;
    view.state = st;
    ui_app_apply(&view);
    CHECK(!ui_app_settings_is_open());

    teardown_forester();
}

/* 设置写入：计量单位与中间卡片落到 car_settings */
static void test_settings_write(void)
{
    setup_forester();
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(ui_app_settings_is_open());

    /* 我的座驾：进入后焦点落在第一项（仪表风格） */
    enter_mycar();

    /* 下移到“计量单位”，OK 进入 */
    mycar_move_to(MYCAR_STYLE_INDEX, MYCAR_UNIT_INDEX);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    /* Qt：子页首次 OK 只是锁定列表，选中第一项（里程） */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);

    /* 第一项是里程：OK 在 KM / MILE 间切换 */
    CHECK(s_settings.values.unit_mileage == CAR_UNIT_METRIC);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.unit_mileage == CAR_UNIT_IMPERIAL);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.unit_mileage == CAR_UNIT_METRIC);

    /* 下移到温度：OK 在 C / F 间切换 */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.unit_temperature == CAR_UNIT_IMPERIAL);

    /* 下移到“确定”，OK 回到我的座驾 */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.unit_mileage == CAR_UNIT_METRIC);
    CHECK(s_settings.values.unit_temperature == CAR_UNIT_IMPERIAL);

    /* 我的座驾 -> 中间卡片设置，进入后焦点在第一项 NAV */
    mycar_move_to(MYCAR_UNIT_INDEX, MYCAR_CARD_INDEX);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.center_card_mask == CAR_SETTINGS_CARD_DEFAULT);

    /* OK 取消 NAV，再下移勾选 4X4-I：按位翻转 */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.center_card_mask == UINT32_C(0x3FC));
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.center_card_mask == UINT32_C(0x3FE));

    /* 设置值有变化 -> dirty，等 ACC 关闭统一落盘 */
    CHECK(car_settings_dirty(&s_settings));

    teardown_forester();
}

/* 信息选项 → 行车信息：3 个开关切换落到 car_settings，仪表立即跟随 */
static void test_settings_driving_info(void)
{
    setup_forester();
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(ui_app_settings_is_open());

    /* 根页右移到“信息选项”并进入，焦点落在第一项 DRIVING INFO */
    root_move_to(ROOT_INFO_INDEX);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    /* 首次 OK 只是锁定列表，选中第一项 */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    /* 再 OK 进入行车信息子页，焦点落在第一项 */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);

    /* 车外温度：缺省关，OK 打开 */
    CHECK(s_settings.values.out_temperature == 0U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.out_temperature == 1U);

    /* 下移档位：缺省开，OK 关闭，仪表档位立即隐藏 */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.gear_on == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.gear_on == 0U);

    /* 下移平均油耗：缺省关，OK 打开 */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.avg_fuel_on == 0U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.avg_fuel_on == 1U);

    /* 下移到“返回”，OK 回到信息选项列表，仍在设置内 */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(ui_app_settings_is_open());

    teardown_forester();
}

/* 常规设置：亮度 OK 步进 10% 循环，超速报警开关落盘，仪表 ECO 指示跟随设置 */
static void test_settings_general_eco(void)
{
    setup_forester();
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(ui_app_settings_is_open());

    /* 我的座驾 → GENERAL（第 4 项） */
    enter_mycar();
    mycar_move_to(MYCAR_STYLE_INDEX, MYCAR_ROUTINE_INDEX);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    /* Qt：子页首次 OK 只是锁定列表，选中第一项（白天亮度） */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);

    /* 白天亮度：缺省 100，OK 回到 0 */
    CHECK(s_settings.values.daylight == 100U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.daylight == 0U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.daylight == 10U);

    /* 黑夜亮度：1 次 OK 缺省 100 → 0 */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.nightlight == 0U);

    /* 超速报警：缺省关，OK 打开 */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.speed_limit_on == 0U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.speed_limit_on == 1U);

    /* 返回到我的座驾，再进 ECO：ECO 指示缺省开，OK 关闭后仪表图标隐藏 */
    push_key(20U, 1U); /* 超速预警 */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U); /* 返回 */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(ui_app_settings_is_open());

    mycar_move_to(MYCAR_ROUTINE_INDEX, MYCAR_ECO_INDEX);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    /* 子页首次 OK 只锁定列表，焦点落在“显示”（油门引导） */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);

    /* 显示：缺省 油门引导 → OK 切到油耗 → 再切回 */
    CHECK(s_settings.values.eco_or_fuel == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.eco_or_fuel == 0U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.eco_or_fuel == 1U);

    /* ECO 指示：下一行，缺省开 → 关 */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.eco_indicator == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.eco_indicator == 0U);

    teardown_forester();
}

/* 只读详情：真实方向盘帧的任意短按/长按只返回一级，释放不退出。 */
static void test_eco_history_page(void)
{
    const uint8_t keys[] = {13U, 19U, 20U, 21U, 22U, 23U, 26U, 31U, 0x25U};

    for(unsigned version_page = 0; version_page < 2; ++version_page) {
        setup_forester();
        car_state_view_t view = {0};
        car_state_init(&view.state);
        view.has_data = true;
        view.state.trip.valid = true;
        view.state.trip.avg_fuel_x10 = 123U;
        ui_app_apply(&view);
        ui_app_settings_open();
        if(version_page) {
            root_move_to(UI_SETTINGS_ROOT_SYSTEM);
            push_key(23U, 1U);
            (void)car_ui_key_dispatch(&s_link, &s_ctx);
            for(unsigned i = 0; i <= UI_SETTINGS_SYSTEM_VERSION; ++i) {
                push_key(20U, 1U);
                (void)car_ui_key_dispatch(&s_link, &s_ctx);
            }
        }
        else {
            enter_mycar();
            mycar_move_to(MYCAR_STYLE_INDEX, MYCAR_ECO_HISTORY_INDEX);
        }
        lv_obj_t *body = lv_obj_get_child(ui_app_settings_root(), 2);
        int parent_index = visible_card_index(body);
        CHECK(parent_index >= 0);
        push_key(23U, 1U);
        (void)car_ui_key_dispatch(&s_link, &s_ctx);
        int detail_index = visible_card_index(body);
        CHECK(detail_index >= 0 && detail_index != parent_index);
        const ui_style_desc_t *style = ui_app_current_style();
        car_settings_t before = s_settings;
        CHECK(!car_settings_dirty(&s_settings));

        for(uint8_t pressed = 1U; pressed <= 2U; ++pressed) {
            for(size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
                push_key(keys[i], 0U);
                CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
                CHECK(visible_card_index(body) == detail_index);
                push_key(keys[i], pressed);
                CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
                CHECK(ui_app_settings_is_open());
                CHECK(visible_card_index(body) == parent_index);
                CHECK(lv_label_get_text(lv_obj_get_child(ui_app_settings_root(), 0))[0] == '\0');
                CHECK(ui_app_current_style() == style);
                CHECK(visible_card_index(s_stack) == 0);
                CHECK(!s_ctx.nav_home && !s_ctx.style_switch);
                CHECK(memcmp(&s_settings.values, &before.values, sizeof(before.values)) == 0);
                CHECK(!car_settings_dirty(&s_settings));
                /* 再按确认回到同一详情，验证焦点未被退出按键继续移动。 */
                push_key(23U, 1U);
                CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
                CHECK(visible_card_index(body) == detail_index);
            }
        }
        teardown_forester();
    }
}

/* 长按 19/20 在 Qt 中映射为左/右，同样翻卡 */
static void test_long_press_maps_left_right(void)
{
    ui_dispatch_ctx_t ctx = {0};

    setup_forester();
    CHECK(s_stack != NULL);
    if(s_stack == NULL) {
        teardown_forester();
        return;
    }
    CHECK(visible_card_index(s_stack) == 0);

    push_key(20U, 2U);
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(visible_card_index(s_stack) == 1);

    push_key(19U, 2U);
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(visible_card_index(s_stack) == 0);

    teardown_forester();
}

/* 按住持续上报 pressed=1 时与 Qt 一致：每次上报都翻 */
static void test_held_repeat(void)
{
    ui_dispatch_ctx_t ctx = {0};

    setup_forester();
    CHECK(s_stack != NULL);
    if(s_stack == NULL) {
        teardown_forester();
        return;
    }

    for(uint8_t i = 0U; i < 3U; ++i) {
        push_key(20U, 1U);
        CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    }
    CHECK(visible_card_index(s_stack) == 4);

    teardown_forester();
}

/* 31 只在当前车型合法样式内切换，不会跳到其它车型的占位样式 */
static void test_style_switch_key(void)
{
    ui_dispatch_ctx_t ctx = {0};

    setup_forester();
    CHECK(s_stack != NULL);
    if(s_stack == NULL) {
        teardown_forester();
        return;
    }

    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(visible_card_index(s_stack) == 1);

    push_key(31U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 1U);
    CHECK(ctx.style_switch);
    CHECK(!ctx.nav_home);
    /* Forester 目前只迁移了 Style1：general1 属于 PublicUi，不得被选到 */
    CHECK(strcmp(ui_app_current_style()->id, "forester1") == 0);

    teardown_forester();
}

/* 风格列表只含当前车型的可用样式（Qt PageMyCarStyle） */
static void test_style_options_per_vehicle(void)
{
    setup_forester();

    CHECK(ui_app_style_option_count() == 1U);
    CHECK(ui_app_style_option_at(0U) != NULL);
    CHECK(strcmp(ui_app_style_option_at(0U)->id, "forester1") == 0);
    CHECK(ui_style_vehicle_count("subaru_forester") == 1U);
    CHECK(ui_style_vehicle_count("public_ui") == 1U);
    CHECK(ui_style_default() != NULL);

    /* 同一车型没有第二个样式：31 不改变当前样式 */
    CHECK(!ui_app_style_switch_next());
    CHECK(strcmp(ui_app_current_style()->id, "forester1") == 0);

    teardown_forester();
}

/* 统一入口：切换后保存 dial_style，并按 Qt sendSwitchUI 重新向 MCU 取数 */
static uint8_t s_cmd_ids[16];
static size_t s_cmd_count;
static car_cmd_t s_last_cmd;

static void capture_cmd(const car_cmd_t *cmd)
{
    if(s_cmd_count < sizeof(s_cmd_ids)) {
        s_cmd_ids[s_cmd_count] = cmd->cmd_id;
        ++s_cmd_count;
    }
    s_last_cmd = *cmd;
}

/* 注入车辆设置回读值（等价 0x16 Status 更新），设置页据此显示并取反/取选项 */
static void apply_vehicle_settings(const car_vehicle_setting_t *vs)
{
    car_state_t st;
    car_state_view_t view;

    car_state_init(&st);
    st.vehicle_setting = *vs;
    memset(&view, 0, sizeof(view));
    view.has_data = true;
    view.state = st;
    ui_app_apply(&view);
}

static void test_style_switch_persists_and_refreshes(void)
{
    static const uint8_t expected[] = {
        0x02U, 0x33U, 0x0AU, 0x30U, 0x31U, 0x29U, 0x16U,
    };

    setup_forester();
    ui_app_settings_set_cmd_cb(capture_cmd);
    s_cmd_count = 0U;

    /* 已是当前样式：不重载、不重复取数 */
    CHECK(ui_app_switch_style("forester1"));
    CHECK(s_cmd_count == 0U);

    /* 用占位样式验证真实切换路径（完成后回到 Forester） */
    CHECK(ui_app_switch_style("general1"));
    CHECK(strcmp(ui_app_current_style()->id, "general1") == 0);
    CHECK(strcmp(s_settings.values.dial_style, "general1") == 0);
    CHECK(s_cmd_count == sizeof(expected) / sizeof(expected[0]));
    for(size_t i = 0U; i < s_cmd_count && i < sizeof(expected); ++i) {
        CHECK(s_cmd_ids[i] == expected[i]);
    }

    CHECK(ui_app_switch_style("forester1"));
    CHECK(strcmp(ui_app_current_style()->id, "forester1") == 0);
    CHECK(strcmp(s_settings.values.dial_style, "forester1") == 0);

    ui_app_settings_set_cmd_cb(NULL);
    teardown_forester();
}

/*
 * 行程资讯 → 车辆设置 → 灯光设置：OK 只下发 0x16/0x86，
 * 显示等 MCU 回读，不在本地翻转。
 */
static void test_settings_vehicle_light(void)
{
    setup_forester();
    ui_app_settings_set_cmd_cb(capture_cmd);
    s_cmd_count = 0U;

    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(ui_app_settings_is_open());

    /* 行程资讯 → 车辆设置，入口位置由菜单决定。 */
    enter_itinerary_vehicle();
    CHECK(ui_app_settings_is_open());

    /* 首次 OK 选中 LIGHT SETTINGS，再 OK 进入灯光设置 */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);

    /* 首次 OK 选中 WELCOME LIGHT，再 OK 按回读值取反下发 */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 0U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 1U);
    CHECK(s_last_cmd.cmd_id == 0x16U);
    CHECK(s_last_cmd.op == CAR_OP_SET_GET);
    CHECK(s_last_cmd.len == 3U);
    CHECK(s_last_cmd.payload[0] == 0x86U);
    CHECK(s_last_cmd.payload[1] == 0x0AU); /* index 10 迎宾灯 */
    CHECK(s_last_cmd.payload[2] == 0x00U); /* 回读 ON -> 下发 OFF */

    /* 返回车辆设置，下移到 DOORS，进入车门设置 */
    push_key(20U, 1U); /* 下移到 AUTO INTERIOR */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U); /* 下移到 HEADLIGHT SENSITIVITY */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U); /* 下移到 LIGHT OFF DELAY */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U); /* 下移到 BACK */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U); /* 返回车辆设置，焦点在 LIGHT SETTINGS */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U); /* 下移到 TURN LIGHT */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U); /* 下移到 DOORS */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U); /* 进入车门设置 */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U); /* 选中 HORN FEEDBACK */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U); /* 喇叭响应取反下发 */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 2U);
    CHECK(s_last_cmd.cmd_id == 0x16U);
    CHECK(s_last_cmd.payload[0] == 0x86U);
    CHECK(s_last_cmd.payload[1] == 0x0FU); /* index 15 */
    CHECK(s_last_cmd.payload[2] == 0x00U);

    /* 下移到自动解锁，OK 循环到下一选项（缺省 0 -> 1） */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 3U);
    CHECK(s_last_cmd.payload[1] == (uint8_t)((1U << 6) | 16U));

    ui_app_settings_set_cmd_cb(NULL);
    teardown_forester();
}

/* 车辆设置 → 转向灯 / 雨刷器：OK 只下发 0x16，索引与回读取反一致 */
static void test_settings_vehicle_turn_wiper(void)
{
    setup_forester();
    ui_app_settings_set_cmd_cb(capture_cmd);
    s_cmd_count = 0U;

    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    enter_itinerary_vehicle();

    /* 选中 LIGHT SETTINGS 后下移到 TURN LIGHT，进入转向灯 */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);

    /* 选中 LANE CHANGE FLASH，再 OK 下发 index 12（回读 ON -> OFF） */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 1U);
    CHECK(s_last_cmd.cmd_id == 0x16U);
    CHECK(s_last_cmd.payload[0] == 0x86U);
    CHECK(s_last_cmd.payload[1] == (uint8_t)((0U << 6) | 12U));
    CHECK(s_last_cmd.payload[2] == 0x00U);

    /* 返回车辆设置，下移到 WIPER，进入雨刷器 */
    push_key(20U, 1U); /* 下移到 BACK */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U); /* 返回车辆设置，焦点在 TURN LIGHT */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U); /* DOORS */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U); /* WIPER */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U); /* 进入雨刷器 */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);

    /* 三项依次取反下发 index 20 / 21 / 22 */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 2U);
    CHECK(s_last_cmd.payload[0] == 0x86U);
    CHECK(s_last_cmd.payload[1] == (uint8_t)((0U << 6) | 20U));

    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 3U);
    CHECK(s_last_cmd.payload[1] == (uint8_t)((0U << 6) | 21U));

    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 4U);
    CHECK(s_last_cmd.payload[1] == (uint8_t)((0U << 6) | 22U));

    ui_app_settings_set_cmd_cb(NULL);
    teardown_forester();
}

/* 车辆设置 → 保养设置：数字编辑确认下发 0x36，剩余/重置为 2 字节 */
static void test_settings_vehicle_service(void)
{
    setup_forester();
    ui_app_settings_set_cmd_cb(capture_cmd);
    s_cmd_count = 0U;

    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    enter_itinerary_vehicle();

    /* LIGHT -> TURN -> DOORS -> WIPER -> SERVICE */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    for(uint8_t i = 0U; i < 4U; ++i) {
        push_key(20U, 1U);
        CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    }
    push_key(23U, 1U); /* 进入保养主列表 */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);

    /* 选中 OIL CHANGE，进入子页 */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);

    /* 选中 SET SERVICE KM，进入数字编辑 */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);

    /* 6 位 -> CONFIRM，默认 5000 原样下发 */
    for(uint8_t i = 0U; i < 6U; ++i) {
        push_key(20U, 1U);
        CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    }
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 1U);
    CHECK(s_last_cmd.cmd_id == 0x36U);
    CHECK(s_last_cmd.op == CAR_OP_SET_GET);
    CHECK(s_last_cmd.len == 4U);
    CHECK(s_last_cmd.payload[0] == 0U);    /* 机油 */
    CHECK(s_last_cmd.payload[1] == 0x00U); /* 5000 = 0x001388 */
    CHECK(s_last_cmd.payload[2] == 0x13U);
    CHECK(s_last_cmd.payload[3] == 0x88U);

    /* 剩余公里数查询：{item, 0} */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 2U);
    CHECK(s_last_cmd.cmd_id == 0x36U);
    CHECK(s_last_cmd.len == 2U);
    CHECK(s_last_cmd.payload[0] == 0U);
    CHECK(s_last_cmd.payload[1] == 0x00U);

    /* 重置：{item, 1} */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 3U);
    CHECK(s_last_cmd.len == 2U);
    CHECK(s_last_cmd.payload[0] == 0U);
    CHECK(s_last_cmd.payload[1] == 0x01U);

    ui_app_settings_set_cmd_cb(NULL);
    teardown_forester();
}

/*
 * 工厂门控项：点灯灵敏度（扩展 control 2）、灯光关闭延时（扩展 control 1）、
 * 外部车门 13 / 单车门解锁 14 / 智能钥匙 24 / 选择解锁 25 / 自动上锁 28、
 * 远程启动发动机 23。选项值与 Qt PageListSelect 一致。
 */
static void test_settings_vehicle_gated(void)
{
    car_vehicle_setting_t vs;

    setup_forester();
    ui_app_settings_set_cmd_cb(capture_cmd);
    s_cmd_count = 0U;

    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    enter_itinerary_vehicle();

    /* 回读值：灵敏度 FAST(2)、延时 90s(5)、远程启动 ON，其余默认 OFF */
    memset(&vs, 0, sizeof(vs));
    vs.headlight_sensitivity = 2U;
    vs.headlight_off_delay = 5U;
    vs.remote_engine_start = true;
    apply_vehicle_settings(&vs);

    /* 灯光设置：WELCOME -> AUTO INTERIOR -> SENSITIVITY */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U); /* WELCOME */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U); /* AUTO INTERIOR */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U); /* SENSITIVITY */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    /* 进入多选，焦点在 FAST(2)，下移到 VERY FAST(3) 后确认 */
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 1U);
    CHECK(s_last_cmd.cmd_id == 0x16U);
    CHECK(s_last_cmd.len == 3U);
    CHECK(s_last_cmd.payload[0] == 0x86U);
    CHECK(s_last_cmd.payload[1] == 0x00U); /* 扩展帧：控制字放第 3 字节 */
    CHECK(s_last_cmd.payload[2] == (uint8_t)((3U << 4) | 2U));

    /* 下移到灯光关闭延时，焦点在 90s(5)，改选 120s(6) 后确认 */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 2U);
    CHECK(s_last_cmd.payload[1] == 0x00U);
    CHECK(s_last_cmd.payload[2] == (uint8_t)((6U << 4) | 1U));

    /* 返回车辆设置，LIGHT -> TURN -> DOORS */
    push_key(20U, 1U); /* DELAY -> BACK */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U); /* 返回车辆设置 */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U); /* TURN LIGHT */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U); /* DOORS */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U); /* 进入车门设置 */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U); /* 选中 HORN FEEDBACK */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);

    /* 自动解锁：缺省 0 -> 1 */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 3U);
    CHECK(s_last_cmd.payload[1] == (uint8_t)((1U << 6) | 16U));

    /* 外部车门 index 13：回读 OFF -> ON */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 4U);
    CHECK(s_last_cmd.payload[1] == (uint8_t)((1U << 6) | 13U));

    /* 单车门解锁 index 14 */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 5U);
    CHECK(s_last_cmd.payload[1] == (uint8_t)((1U << 6) | 14U));

    /* 智能钥匙 index 24 */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 6U);
    CHECK(s_last_cmd.payload[1] == (uint8_t)((1U << 6) | 24U));

    /* 选择解锁 index 25 */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 7U);
    CHECK(s_last_cmd.payload[1] == (uint8_t)((1U << 6) | 25U));

    /* 车门自动上锁：多选页 3 项，缺省 SPEED(0) -> P SHIFT(1)，走 index 28 */
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 8U);
    CHECK(s_last_cmd.payload[1] == (uint8_t)((1U << 6) | 28U));

    /* 返回车辆设置，DOORS -> WIPER -> SERVICE -> REMOTE START，回读 ON -> OFF */
    push_key(20U, 1U); /* AUTO DOOR LOCK -> BACK */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    push_key(23U, 1U); /* 返回车辆设置 */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    for(uint8_t i = 0U; i < 3U; ++i) {
        push_key(20U, 1U);
        CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    }
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_cmd_count == 9U);
    CHECK(s_last_cmd.payload[1] == (uint8_t)((0U << 6) | 23U));

    ui_app_settings_set_cmd_cb(NULL);
    teardown_forester();
}

/* 一次取出多条时按顺序逐条分发（连续两次翻卡） */
static void test_batch_order(void)
{
    ui_dispatch_ctx_t ctx = {0};

    setup_forester();
    CHECK(s_stack != NULL);
    if(s_stack == NULL) {
        teardown_forester();
        return;
    }

    push_key(20U, 1U);
    push_key(20U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &ctx) == 2U);
    CHECK(visible_card_index(s_stack) == 3);

    teardown_forester();
}

/* 队列空时不产生事件 */
static void test_empty_queue(void)
{
    ui_dispatch_ctx_t ctx = {0};

    setup_forester();
    for(uint8_t i = 0U; i < 3U; ++i) {
        CHECK(car_ui_key_dispatch(&s_link, &ctx) == 0U);
        CHECK(!ctx.style_switch);
        CHECK(!ctx.nav_home);
    }
    teardown_forester();
}

/* 设置改动立即生效：关掉当前卡落到第一张可见卡，全部关掉后中心区域为空且翻键无效 */
static void test_card_mask_live(void)
{
    ui_dispatch_ctx_t ctx = {0};

    setup_forester();
    CHECK(s_stack != NULL);
    if(s_stack == NULL) {
        teardown_forester();
        return;
    }

    /* 翻到胎压（注册序号 3） */
    push_key(20U, 1U); /* 0 -> 1 行程 */
    (void)car_ui_key_dispatch(&s_link, &ctx);
    push_key(22U, 1U); /* 1 -> 3 胎压（跳过关闭的 4x4-i） */
    (void)car_ui_key_dispatch(&s_link, &ctx);
    CHECK(visible_card_index(s_stack) == 3);

    /* 取消勾选胎压：当前卡不可见，落到第一张可见卡（导航，序号 0） */
    CHECK(car_settings_set_center_card_mask(&s_settings,
                                            CAR_SETTINGS_CARD_DEFAULT & ~(UINT32_C(1) << 2)));
    CHECK(visible_card_index(s_stack) == 0);

    /* 全部关闭：中心区域没有任何可见卡，翻键也不会再显示卡片 */
    CHECK(car_settings_set_center_card_mask(&s_settings, 0U));
    CHECK(visible_card_index(s_stack) == -1);
    push_key(20U, 1U);
    (void)car_ui_key_dispatch(&s_link, &ctx);
    push_key(22U, 1U);
    (void)car_ui_key_dispatch(&s_link, &ctx);
    CHECK(visible_card_index(s_stack) == -1);

    /* 重新打开导航：第一张可见卡立即回来 */
    CHECK(car_settings_set_center_card_mask(&s_settings, CAR_SETTINGS_CARD_DEFAULT));
    CHECK(visible_card_index(s_stack) == 0);

    teardown_forester();
}

static void test_boot_animation_selection(void)
{
    ui_settings_page_t page;
    car_settings_t settings;
    ui_key_event_t key = {0};
    car_settings_reset(&settings);
    ui_settings_page_set_settings(&settings);
    CHECK(ui_settings_page_create(&page, lv_screen_active()));
    ui_settings_page_set_active(&page, true);
    page.st.view = UI_SETTINGS_VIEW_SYSTEM;
    page.st.system_index = 5U;
    key.pressed = 1U;
    key.code = UI_KEY_OK;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.view == UI_SETTINGS_VIEW_SELECT);
    ui_settings_page_next_or_back(&page);
    CHECK(page.st.view == UI_SETTINGS_VIEW_SELECT);
    CHECK(page.st.select_index == 1U);
    ui_settings_page_next_or_back(&page);
    CHECK(page.st.select_index == 2U);
    ui_settings_page_next_or_back(&page);
    CHECK(page.st.select_index == 0U);
    key.code = UI_KEY_DOWN;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(settings.values.animation_style == 0U);
    CHECK(ui_settings_page_back(&page));
    CHECK(page.st.view == UI_SETTINGS_VIEW_SYSTEM);
    CHECK(!car_settings_dirty(&settings));
    key.code = UI_KEY_OK;
    CHECK(ui_settings_page_key(&page, &key));
    key.code = UI_KEY_DOWN;
    CHECK(ui_settings_page_key(&page, &key));
    key.code = UI_KEY_OK;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(settings.values.animation_style == 1U);
    CHECK(page.st.view == UI_SETTINGS_VIEW_SYSTEM);
    ui_settings_page_destroy(&page);
    ui_settings_page_set_settings(NULL);
}

static void test_date_wheel(void)
{
    ui_settings_page_t page;
    car_settings_t settings;
    ui_key_event_t key = {.pressed = 1, .code = UI_KEY_OK};
    car_clock_t clock = {.valid = true, .year = 2026, .month = 2,
                         .day = 28, .hour = 10, .minute = 11};
    car_units_t units = {.valid = true, .mile = true, .hour24 = true,
                         .fahrenheit = true, .fuel_unit = 2, .pressure_unit = 1};
    car_cmd_t expected;

    car_settings_reset(&settings);
    ui_settings_page_set_settings(&settings);
    ui_settings_page_set_cmd_cb(capture_cmd);
    CHECK(ui_settings_page_create(&page, lv_screen_active()));
    ui_settings_page_set_active(&page, true);

    /* 非仪表时间源：只有时间源/返回两行，OK 在时间源行下发 0x05 */
    ui_settings_page_set_date(&page, &clock, &units, 1);
    page.st.view = UI_SETTINGS_VIEW_SYSTEM;
    page.st.system_index = 1;
    s_cmd_count = 0;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.view == UI_SETTINGS_VIEW_DATE);
    CHECK(page.st.date_item == 0U);
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(s_cmd_count == 1U);
    CHECK(s_last_cmd.cmd_id == 5 && s_last_cmd.payload[0] == 6 && s_last_cmd.payload[1] == 2);
    CHECK(page.time_source == 1);
    /* 两行循环：0 → 1 → 0（右/上都是 next） */
    key.code = UI_KEY_UP;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_item == 1U);
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_item == 0U);
    key.code = UI_KEY_LEFT;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_item == 1U);
    /* 返回行 OK：回系统设置并停在日期时间行 */
    key.code = UI_KEY_OK;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.view == UI_SETTINGS_VIEW_SYSTEM && page.st.system_index == 1U);

    /* 仪表时间源：进入滚轮页，光标停在时间源行（Qt sourceIndex=2）。
     * 时间源 2 未变化 → 不重进滚轮，草稿保留上一次的值。 */
    ui_settings_page_set_date(&page, &clock, &units, 2);
    page.st.view = UI_SETTINGS_VIEW_SYSTEM;
    page.st.system_index = 1;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.view == UI_SETTINGS_VIEW_DATE);
    CHECK(!lv_obj_has_flag(page.date_wheel_list, LV_OBJ_FLAG_HIDDEN));
    CHECK(page.st.date_item == 2U && page.st.date_focus == 0xFFU);
    CHECK(page.st.date_year_base == 2021U);
    CHECK(page.st.date_values[1] == 1U && page.st.date_values[2] == 27U);
    CHECK(strcmp(lv_label_get_text(page.title), "TIME SETTINGS") == 0U);
    /* 时间源行 OK：下发 0x05 {6,1}（仪表 → 取反） */
    s_cmd_count = 0;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(s_cmd_count == 1U);
    CHECK(s_last_cmd.cmd_id == 5 && s_last_cmd.op == CAR_OP_SET_GET &&
          s_last_cmd.len == 2U && s_last_cmd.payload[0] == 6 && s_last_cmd.payload[1] == 1);
    /* 行移动：2→3（返回）→0（滚轮），末端回到 0 */
    key.code = UI_KEY_UP;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_item == 3U);
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_item == 0U);
    key.code = UI_KEY_OK;

    /* 时间源切回仪表：重进滚轮，草稿取回读值 2026-02-28 10:11 */
    ui_settings_page_set_date(&page, &clock, &units, 1);
    CHECK(page.time_source == 1U);
    CHECK(page.st.date_focus == 0xFFU);
    CHECK(ui_settings_page_key(&page, &key)); /* OK 进入非仪表时间源页 */
    CHECK(page.st.date_item == 0U);           /* 首项 */
    CHECK(ui_settings_page_key(&page, &key)); /* 首项：下发时间源 */
    key.code = UI_KEY_LEFT;                   /* 两行：next → 返回行 */
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_item == 1U); /* 非仪表只有时间源/返回两行 */
    key.code = UI_KEY_OK;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.view == UI_SETTINGS_VIEW_SYSTEM && page.st.system_index == 1U);
    /* 时间源改回仪表：重进滚轮 */
    ui_settings_page_set_date(&page, &clock, &units, 2);
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.view == UI_SETTINGS_VIEW_DATE);
    CHECK(page.st.date_item == 2U && page.st.date_focus == 0xFFU);
    /* 回到滚轮行（Qt rowCount=4 取模） */
    key.code = UI_KEY_UP;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_item == 3U);
    key.code = UI_KEY_UP;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_item == 0U && page.st.date_focus == 0xFFU);
    CHECK(page.st.date_year_base == 2021U);
    CHECK(page.st.date_values[0] == 5U && page.st.date_values[1] == 1U &&
          page.st.date_values[2] == 27U && page.st.date_values[3] == 10U &&
          page.st.date_values[4] == 11U);
    /* 滚轮行：OK 进入第 0 列，再 OK 到第 1 列 */
    key.code = UI_KEY_OK;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_focus == 0U && page.st.date_item == 0U);
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_focus == 1U);
    key.code = UI_KEY_UP;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_values[1] == 2U);
    ui_settings_page_next_or_back(&page);
    CHECK(page.st.date_values[1] == 3U);
    key.code = UI_KEY_DOWN;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_values[1] == 2U);
    /* 月列取模：12 → 1，1 → 12 */
    page.st.date_values[1] = 12U; /* 真实 13 月，超出取模范围 */
    key.code = UI_KEY_UP;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_values[1] == 1U);
    key.code = UI_KEY_DOWN;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_values[1] == 0U);
    /* 12 月（索引 11）→ 1 月（索引 0），日被截到 31 日以内 */
    page.st.date_values[1] = 11U;
    page.st.date_values[2] = 30U;
    key.code = UI_KEY_UP;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_values[1] == 0U);
    CHECK(page.st.date_values[2] == 30U);
    /* 二月（索引 1，2026 非闰年 28 天，日索引 0~27）加一后截到 27 */
    page.st.date_focus = 2U;
    page.st.date_values[1] = 1U;
    page.st.date_values[2] = 27U;
    key.code = UI_KEY_UP;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_values[2] == 27U);
    page.st.date_values[2] = 26U;
    key.code = UI_KEY_UP;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_values[2] == 27U);
    /* 年列：2021 起 31 项（2021~2051），索引 30 到顶、0 到底都不循环 */
    page.st.date_focus = 0U;
    page.st.date_values[0] = 30U;
    key.code = UI_KEY_UP;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_values[0] == 30U);
    /* 中间值减一正常 */
    page.st.date_values[0] = 5U;
    key.code = UI_KEY_DOWN;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_values[0] == 4U);
    page.st.date_values[0] = 0U;
    key.code = UI_KEY_DOWN; /* 年列到底不循环 */
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_values[0] == 0U);
    /* OK 从分钟列前进到确认列；确认列 UP 反向回到分钟列 */
    page.st.date_item = 0U;
    page.st.date_focus = 4U;
    page.st.date_child_back = false;
    key.code = UI_KEY_OK;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_focus == 5U);
    CHECK(page.st.date_child_back);
    key.code = UI_KEY_UP;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.date_focus == 4U);

    /* 确认下发 0x32：2026-03-01 10:11 */
    page.st.date_values[0] = 5U;
    page.st.date_values[1] = 2U;
    page.st.date_values[2] = 0U;
    page.st.date_values[3] = 10U;
    page.st.date_values[4] = 11U;
    page.st.date_focus = 5U;
    page.st.date_item = 0U;
    key.code = UI_KEY_OK;
    s_cmd_count = 0;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(s_cmd_count == 1U);
    /* hour 的 bit7 标记 24 小时制（car_cmd_set_time 固定置位） */
    CHECK(s_last_cmd.cmd_id == 0x32 && s_last_cmd.payload[0] == 26 &&
          s_last_cmd.payload[1] == 3 && s_last_cmd.payload[2] == 1 &&
          s_last_cmd.payload[3] == (10 | 0x80) && s_last_cmd.payload[4] == 11);
    CHECK(page.st.date_focus == 0xFFU);
    /* 非法日期（2/29 非闰年）不下发 */
    page.st.date_values[1] = 1U;
    page.st.date_values[2] = 28U;
    page.st.date_focus = 5U;
    page.st.date_item = 0U;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(s_cmd_count == 1U);

    /* 时间制式行：24h → 12h */
    page.st.date_focus = 0xFFU;
    page.st.date_item = 1U;
    key.code = UI_KEY_OK;
    CHECK(ui_settings_page_key(&page, &key));
    s_cmd_count = 0;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(car_cmd_set_units(&expected, true, false, true, 2, 1));
    CHECK(s_cmd_count == 1U && s_last_cmd.cmd_id == expected.cmd_id);
    CHECK(memcmp(s_last_cmd.payload, expected.payload, expected.len) == 0);
    units.hour24 = false;
    ui_settings_page_set_date(&page, &clock, &units, 2);
    CHECK(!page.date_units.hour24);

    /* 回读不覆盖编辑中的草稿；退出后重新进入取回读值 */
    clock = (car_clock_t){.valid = true, .year = 2024, .month = 2,
                          .day = 29, .hour = 10, .minute = 11};
    ui_settings_page_set_date(&page, &clock, &units, 2);
    CHECK(page.st.date_values[1] == 1U && page.st.date_values[2] == 28U);
    CHECK(page.st.date_year_base == 2021U);
    page.st.date_item = 3U;
    key.code = UI_KEY_OK;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.view == UI_SETTINGS_VIEW_SYSTEM && page.st.system_index == 1U);
    clock = (car_clock_t){.valid = true, .year = 2024, .month = 2,
                          .day = 29, .hour = 10, .minute = 11};
    ui_settings_page_set_date(&page, &clock, &units, 2);
    key.code = UI_KEY_OK;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.view == UI_SETTINGS_VIEW_DATE);
    CHECK(page.st.date_values[0] == 5U && page.st.date_values[1] == 1U &&
          page.st.date_values[2] == 28U);

    /* 系统设置列表的日期时间摘要 */
    CHECK(ui_settings_page_back(&page));
    CHECK(page.st.view == UI_SETTINGS_VIEW_SYSTEM);
    CHECK(strcmp(lv_label_get_text(lv_obj_get_child(page.system_rows[1], 1)),
                 "2024.2.29 10:11") == 0);
    CHECK(strcmp(lv_label_get_text(lv_obj_get_child(page.system_rows[1], 0)),
                 "DATE & TIME") == 0);
    clock = (car_clock_t){.valid = true, .year = 2026, .month = 3,
                          .day = 4, .hour = 9, .minute = 5};
    ui_settings_page_set_date(&page, &clock, &units, 2);
    ui_settings_page_apply(&page);
    ui_settings_page_destroy(&page);
    ui_settings_page_set_settings(NULL);
    ui_settings_page_set_cmd_cb(NULL);
}

static void test_date_and_connect(void)
{
    ui_settings_page_t page;
    car_settings_t settings;
    ui_key_event_t key = {0};
    car_settings_reset(&settings);
    ui_settings_page_set_settings(&settings);
    ui_settings_page_set_cmd_cb(capture_cmd);
    CHECK(ui_settings_page_create(&page, lv_screen_active()));
    ui_settings_page_set_active(&page, true);
    s_cmd_count = 0;
    key.pressed = 1;
    key.code = UI_KEY_OK;
    page.st.view = UI_SETTINGS_VIEW_CONNECT;
    page.st.date_index = 0;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(settings.values.connect_enabled == 1);
    page.st.date_index = 1;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(settings.values.connect_mode == 1);
    CHECK(lv_label_get_text(page.title)[0] == '\0'); /* Qt 互联仍在根分类中，保留图标行。 */
    CHECK(strstr(lv_label_get_text(lv_obj_get_child(page.date_rows[0], 0)), "CONFIG ONLY") != NULL);
    CHECK(strcmp(lv_label_get_text(lv_obj_get_child(page.date_rows[1], 0)), "APPLE CARPLAY") == 0);
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(settings.values.connect_mode == 2);
    CHECK(strcmp(lv_label_get_text(lv_obj_get_child(page.date_rows[1], 0)), "HICAR") == 0);
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(settings.values.connect_mode == 0);
    page.st.date_index = 0;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(settings.values.connect_enabled == 0);
    page.st.date_index = 1;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(settings.values.connect_mode == 0);
    ui_settings_page_next_or_back(&page);
    CHECK(page.st.view == UI_SETTINGS_VIEW_CONNECT);
    CHECK(page.st.date_index == 2);
    ui_settings_page_next_or_back(&page);
    CHECK(page.st.date_index == 0);
    /* 互联页仅保存配置（SDK 不可用），不下发串口命令 */
    CHECK(s_cmd_count == 0U);
    CHECK(car_settings_dirty(&settings));
    ui_settings_page_destroy(&page);
    ui_settings_page_set_settings(NULL);
    ui_settings_page_set_cmd_cb(NULL);
}

static void check_label_glyphs(lv_obj_t *root);

/* 设置页中文：日期时间滚轮页各状态的整体翻译与缺字检查 */
static void test_date_language(void)
{
    ui_settings_page_t page;
    car_settings_t settings;
    ui_key_event_t key = {.pressed = 1, .code = UI_KEY_OK};
    car_clock_t clock = {.valid = true, .year = 2026, .month = 3,
                         .day = 4, .hour = 9, .minute = 5};
    car_units_t units = {.valid = true, .mile = true, .hour24 = true,
                         .fahrenheit = true, .fuel_unit = 2, .pressure_unit = 1};

    car_settings_reset(&settings);
    CHECK(car_settings_set_language(&settings, 0));
    ui_settings_page_set_settings(&settings);
    CHECK(ui_settings_page_create(&page, lv_screen_active()));
    ui_settings_page_set_active(&page, true);
    ui_settings_page_set_date(&page, &clock, &units, 2);
    page.st.view = UI_SETTINGS_VIEW_SYSTEM;
    page.st.system_index = 1;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.view == UI_SETTINGS_VIEW_DATE);
    CHECK(strcmp(lv_label_get_text(page.title), "时间设置") == 0);
    lv_obj_t *rows[] = {page.date_wheel_rows[0], page.date_wheel_rows[1],
                        page.date_wheel_rows[2]};
    const char *expected[] = {"时间制式: 24小时", "时间源: 仪表", "返回"};
    for(unsigned i = 0; i < 3; ++i) {
        CHECK(strcmp(lv_label_get_text(lv_obj_get_child(rows[i], 0)), expected[i]) == 0);
    }
    lv_label_set_text(lv_obj_get_child(page.date_wheel_confirm, 0), NULL); /* 还原显示用省略号 */
    CHECK(strcmp(lv_label_get_text(lv_obj_get_child(page.date_wheel_confirm, 0)),
                 "确认") == 0);
    /* 中文 24px 不得超出滚轮行宽 */
    for(unsigned i = 0; i < 3; ++i) {
        lv_obj_t *label = lv_obj_get_child(rows[i], 0);
        lv_point_t size;
        lv_text_get_size(&size, lv_label_get_text(label),
                         lv_obj_get_style_text_font(label, 0), 0, 0,
                         LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        CHECK(size.x <= 524 - 16);
    }
    check_label_glyphs(page.root);
    /* 非仪表时间源同样全覆盖：首行提示，第二行时间源 */
    units.hour24 = false;
    ui_settings_page_set_date(&page, &clock, &units, 1);
    CHECK(strcmp(lv_label_get_text(lv_obj_get_child(page.date_wheel_rows[0], 0)),
                 "请在音响里操作") == 0);
    CHECK(strcmp(lv_label_get_text(lv_obj_get_child(page.date_wheel_rows[1], 0)),
                 "时间源: 中控") == 0);
    ui_settings_page_set_date(&page, &clock, &units, 0);
    CHECK(strcmp(lv_label_get_text(lv_obj_get_child(page.date_wheel_rows[1], 0)),
                 "时间源: 未知") == 0);
    check_label_glyphs(page.root);
    ui_settings_page_destroy(&page);
    ui_settings_page_set_settings(NULL);
    CHECK(car_settings_set_language(&settings, 1));
}

static void check_label_glyphs(lv_obj_t *root)
{
    if(lv_obj_check_type(root, &lv_label_class)) {
        const unsigned char *p = (const unsigned char *)lv_label_get_text(root);
        const lv_font_t *font = lv_obj_get_style_text_font(root, 0);
        while(*p) {
            uint32_t cp = *p++;
            if(cp >= 0xe0 && cp < 0xf0) {
                cp = ((cp & 15U) << 12) | ((p[0] & 63U) << 6) | (p[1] & 63U);
                p += 2;
            }
            else if(cp >= 0xc0 && cp < 0xe0) cp = ((cp & 31U) << 6) | (*p++ & 63U);
            lv_font_glyph_dsc_t glyph;
            if(cp > 127) {
                bool found = lv_font_get_glyph_dsc(font, &glyph, cp, 0);
                CHECK(found && !glyph.is_placeholder);
                if(found && !glyph.is_placeholder && glyph.box_w && glyph.box_h) {
                    /* A descriptor can exist even when bitmap decompression is disabled. */
                    lv_draw_buf_t *buf = lv_draw_buf_create(glyph.box_w, glyph.box_h,
                                                           LV_COLOR_FORMAT_A8, 0);
                    CHECK(buf != NULL);
                    if(buf) {
                        lv_draw_buf_clear(buf, NULL);
                        const void *bitmap = lv_font_get_glyph_bitmap(&glyph, buf);
                        CHECK(bitmap != NULL);
                        bool has_ink = false;
                        for(uint32_t j = 0; bitmap && j < buf->data_size; ++j)
                            has_ink |= buf->data[j] != 0;
                        CHECK(has_ink);
                        lv_font_glyph_release_draw_data(&glyph);
                        lv_draw_buf_destroy(buf);
                    }
                }
            }
        }
    }
    for(uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        check_label_glyphs(lv_obj_get_child(root, i));
}

static void test_language_factory(void)
{
    ui_settings_page_t page;
    car_settings_t settings;
    ui_key_event_t key = {.pressed = 1, .code = UI_KEY_OK};
    car_settings_reset(&settings);
    ui_settings_page_set_settings(&settings);
    ui_settings_page_set_cmd_cb(capture_cmd);
    CHECK(ui_settings_page_create(&page, lv_screen_active()));
    ui_settings_page_set_active(&page, true);
    page.st.view = UI_SETTINGS_VIEW_SYSTEM;
    page.st.system_index = 0;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.view == UI_SETTINGS_VIEW_LANGUAGE);
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(settings.values.language == 0);
    CHECK(strcmp(lv_label_get_text(page.title), "语言") == 0);
    check_label_glyphs(page.root);
    for(unsigned i = 2; i < 19; ++i) {
        page.st.factory_index = i;
        CHECK(ui_settings_page_key(&page, &key));
        CHECK(settings.values.language == 0);
    }
    page.st.factory_index = 1;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(settings.values.language == 1);
    CHECK(strcmp(lv_label_get_text(page.title), "LANGUAGE") == 0);
    lv_label_set_text(lv_obj_get_child(page.system_rows[3], 0), NULL);
    CHECK(strcmp(lv_label_get_text(lv_obj_get_child(page.system_rows[3], 0)), "FACTORY SETTINGS") == 0);
    CHECK(ui_settings_page_back(&page));
    page.st.system_index = 3;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.view == UI_SETTINGS_VIEW_FACTORY_PASSWORD);
    page.st.factory_index = 4;
    s_cmd_count = 0;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.factory_error && !page.st.factory_unlocked && s_cmd_count == 0);
    const uint8_t password[] = {1, 6, 8, 8};
    memcpy(page.st.factory_digits, password, sizeof(password));
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.factory_unlocked && page.st.view == UI_SETTINGS_VIEW_FACTORY);
    page.st.factory_index = 0;
    CHECK(ui_settings_page_key(&page, &key));
    const uint8_t mileage[] = {1, 2, 3, 4, 5, 6};
    memcpy(page.st.factory_digits, mileage, sizeof(mileage));
    page.st.factory_index = 6;
    CHECK(ui_settings_page_key(&page, &key));
    car_cmd_t expected;
    CHECK(car_cmd_set_mileage(&expected, 123456));
    CHECK(s_cmd_count == 1 && s_last_cmd.cmd_id == expected.cmd_id);
    CHECK(memcmp(s_last_cmd.payload, expected.payload, expected.len) == 0);
    page.st.factory_index = 1;
    CHECK(ui_settings_page_key(&page, &key));
    page.st.factory_index = 1;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(settings.values.fuel_tank == 69 && s_cmd_count == 2);
    CHECK(car_cmd_set_fuel_tank(&expected, 0, 69));
    CHECK(memcmp(s_last_cmd.payload, expected.payload, expected.len) == 0);
    page.st.factory_index = 5;
    CHECK(ui_settings_page_key(&page, &key));
    for(unsigned i = 0; i < 11; ++i) {
        page.st.factory_index = i;
        CHECK(ui_settings_page_key(&page, &key));
    }
    CHECK(settings.values.factory_features == 0 && s_cmd_count == 2);
    CHECK(lv_obj_has_flag(page.vehicle_rows[5], LV_OBJ_FLAG_HIDDEN));
    CHECK(settings.values.radar_on == 1);
    CHECK(ui_settings_page_back(&page));
    page.st.factory_index = 4;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(s_cmd_count == 2); /* 无车型回读不得发送伪造编号 */
    page.factory_models.valid = true;
    page.factory_models.parsed = CAR_MODEL_MAX;
    page.factory_models.entries[CAR_MODEL_MAX - 1].id = 42;
    page.st.factory_index = CAR_MODEL_MAX - 1;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(car_cmd_set_car_model(&expected, 42));
    CHECK(s_cmd_count == 3 && memcmp(s_last_cmd.payload, expected.payload, expected.len) == 0);
    CHECK(ui_settings_page_back(&page));
    ui_settings_page_set_cmd_cb(NULL);
    page.st.factory_index = 7;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(settings.values.radar_on == 1 && s_cmd_count == 3);
    CHECK(ui_settings_page_back(&page));
    CHECK(!page.st.factory_unlocked);
    ui_settings_page_destroy(&page);
    ui_settings_page_set_settings(NULL);
}

static void test_factory_password_wheels(void)
{
    ui_settings_page_t page;
    car_settings_t settings;
    car_settings_reset(&settings);
    settings.values.language = 1U;
    ui_settings_page_set_settings(&settings);
    ui_settings_page_set_cmd_cb(capture_cmd);
    s_cmd_count = 0U;
    CHECK(ui_settings_page_create(&page, lv_screen_active()));
    ui_settings_page_set_active(&page, true);
    page.st.view = UI_SETTINGS_VIEW_SYSTEM;
    page.st.system_index = 3U;
    ui_key_event_t key = {.pressed = 1U, .code = UI_KEY_OK};
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.view == UI_SETTINGS_VIEW_FACTORY_PASSWORD);
    CHECK(!lv_obj_has_flag(page.password_list, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(page.factory_list, LV_OBJ_FLAG_HIDDEN));
    CHECK(strcmp(lv_label_get_text(page.title), "FACTORY SETTINGS") == 0);
    lv_obj_update_layout(page.root);
    CHECK(lv_obj_get_x(page.password_bg) == 64 && lv_obj_get_y(page.password_bg) == 0);
    lv_image_header_t header;
    CHECK(lv_image_decoder_get_info(lv_image_get_src(page.password_bg), &header) == LV_RESULT_OK);
    CHECK(header.w == 466 && header.h == 190);
    for(unsigned col = 0; col < 4U; ++col) {
        lv_obj_t *roller = page.password_rollers[col];
        CHECK(lv_roller_get_selected(roller) == 0U);
        CHECK(lv_roller_get_option_count(roller) == 10U);
        CHECK(lv_obj_get_width(roller) == 90 && lv_obj_get_height(roller) == 100);
        CHECK(lv_obj_get_x(roller) == 4 + 92 * (int32_t)col && lv_obj_get_y(roller) == 45);
        CHECK(lv_obj_get_style_text_font(roller, LV_PART_MAIN) == UI_TEXT_BOLD(28));
        CHECK(lv_color_eq(lv_obj_get_style_text_color(roller, LV_PART_SELECTED),
                          lv_color_hex(col == 0U ? 0xFF0000 : 0xFFFFFF)));
    }
    /* Translation must not shorten the roller's repeated internal option buffer. */
    size_t option_length = strlen(lv_roller_get_options(page.password_rollers[0]));
    for(unsigned language = 0; language < 2U; ++language) {
        settings.values.language = language;
        ui_settings_page_apply(&page);
        CHECK(strcmp(lv_label_get_text(lv_obj_get_child(page.password_back, 0)),
                     language ? "Go back" : "返回") == 0);
        CHECK(strlen(lv_roller_get_options(page.password_rollers[0])) == option_length);
        CHECK(lv_obj_get_style_text_font(page.password_rollers[0], LV_PART_MAIN) == UI_TEXT_BOLD(28));
    }
    key.code = UI_KEY_DOWN;
    key.pressed = 0U;
    CHECK(!ui_settings_page_key(&page, &key));
    CHECK(page.st.factory_digits[0] == 0U);
    key.pressed = 1U;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.factory_digits[0] == 9U && lv_roller_get_selected(page.password_rollers[0]) == 9U);
    key.code = UI_KEY_UP;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.factory_digits[0] == 0U);
    key.code = UI_KEY_LEFT;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.factory_digits[0] == 9U);
    key.code = UI_KEY_RIGHT;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.factory_digits[0] == 0U);
    key.code = UI_KEY_OK;
    for(unsigned i = 0; i < 4U; ++i) CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.factory_index == 4U);
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.factory_error && !page.st.factory_unlocked && s_cmd_count == 0U);
    CHECK(page.st.view == UI_SETTINGS_VIEW_FACTORY_PASSWORD);
    CHECK(strcmp(lv_label_get_text(lv_obj_get_child(page.password_ok, 0)), "OK") == 0);
    key.code = UI_KEY_UP;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.factory_index == 5U && page.st.factory_child_back);
    CHECK(lv_color_eq(lv_obj_get_style_text_color(lv_obj_get_child(page.password_back, 0), 0), lv_color_hex(0xFF0000)));
    key.code = UI_KEY_DOWN;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.factory_index == 4U);
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.factory_index == 3U);
    key.code = UI_KEY_OK;
    for(unsigned i = 0; i < 3U; ++i) CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.factory_index == 0U && !page.st.factory_child_back);
    page.st.factory_index = 5U;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.view == UI_SETTINGS_VIEW_SYSTEM && !page.st.factory_unlocked);
    CHECK(lv_obj_has_flag(page.password_list, LV_OBJ_FLAG_HIDDEN));
    CHECK(strcmp(lv_label_get_text(page.title), "") == 0);
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.view == UI_SETTINGS_VIEW_FACTORY_PASSWORD && page.st.factory_index == 0U);
    CHECK(!page.st.factory_error && !page.st.factory_child_back);
    /* Enter 1688 through the same keys used by the steering dispatcher. */
    const uint8_t steps[] = {1U, 6U, 2U, 2U};
    for(unsigned col = 0; col < 4U; ++col) {
        key.code = col < 2U ? UI_KEY_UP : UI_KEY_DOWN;
        for(unsigned i = 0; i < steps[col]; ++i) CHECK(ui_settings_page_key(&page, &key));
        key.code = UI_KEY_OK;
        CHECK(ui_settings_page_key(&page, &key));
    }
    CHECK(page.st.factory_index == 4U && !page.st.factory_unlocked);
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.factory_unlocked && page.st.view == UI_SETTINGS_VIEW_FACTORY);
    CHECK(lv_obj_has_flag(page.password_list, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(page.factory_list, LV_OBJ_FLAG_HIDDEN));
    CHECK(s_cmd_count == 0U);
    ui_settings_page_destroy(&page);
    ui_settings_page_set_settings(NULL);
    ui_settings_page_set_cmd_cb(NULL);
}

static void check_visible_text(lv_obj_t *label)
{
    lv_area_t visible;
    lv_obj_update_layout(label);
    lv_obj_get_coords(label, &visible);
    CHECK(lv_label_get_text(label)[0] != '\0');
    CHECK(lv_obj_get_width(label) > 0 && lv_obj_get_height(label) > 0);
    for(lv_obj_t *p = label; p; p = lv_obj_get_parent(p)) {
        lv_area_t bounds;
        lv_obj_get_coords(p, &bounds);
        CHECK(!lv_obj_has_flag(p, LV_OBJ_FLAG_HIDDEN));
        visible.x1 = LV_MAX(visible.x1, bounds.x1);
        visible.y1 = LV_MAX(visible.y1, bounds.y1);
        visible.x2 = LV_MIN(visible.x2, bounds.x2);
        visible.y2 = LV_MIN(visible.y2, bounds.y2);
        CHECK(visible.x1 <= visible.x2 && visible.y1 <= visible.y2);
    }
    if(lv_area_get_height(&visible) != lv_obj_get_height(label))
        fprintf(stderr, "clipped text: %s (visible=%d height=%d)\n",
                lv_label_get_text(label), (int)lv_area_get_height(&visible),
                (int)lv_obj_get_height(label));
    CHECK(lv_area_get_height(&visible) == lv_obj_get_height(label));
}

static void test_settings_visible_geometry(void)
{
    ui_settings_page_t page;
    car_settings_t settings;
    car_settings_reset(&settings);
    ui_settings_page_set_settings(&settings);
    CHECK(ui_settings_page_create(&page, lv_screen_active()));
    ui_settings_page_set_active(&page, true);
    for(unsigned language = 0; language < 2; ++language) {
        settings.values.language = language;
        ui_settings_state_init(&page.st);
        for(unsigned selected = 0; selected < UI_SETTINGS_ROOT_COUNT; ++selected) {
            page.st.root_index = selected;
            ui_settings_page_apply(&page);
            lv_obj_update_layout(page.root);
            CHECK(!lv_obj_has_flag(page.root_bar, LV_OBJ_FLAG_HIDDEN));
            CHECK(lv_obj_get_y(page.root_bar) == 80);
            for(unsigned item = 0; item < UI_SETTINGS_ROOT_COUNT; ++item) {
                CHECK(lv_obj_get_x(page.root_menu[item]) == 70 + 100 * (int32_t)item);
                CHECK(lv_obj_get_y(page.root_menu[item]) == 10);
                CHECK(lv_obj_get_width(page.root_menu[item]) == 52);
                CHECK(lv_obj_has_flag(lv_obj_get_child(page.root_menu[item], 1),
                                      LV_OBJ_FLAG_HIDDEN) == (item != selected));
            }
        }
        page.st.view = UI_SETTINGS_VIEW_MY_CAR;
        ui_settings_page_apply(&page);
        check_visible_text(lv_obj_get_child(page.mycar_rows[0], 0));
        for(unsigned i = 0; i < UI_SETTINGS_MYCAR_COUNT; ++i) {
            page.st.mycar_index = i;
            ui_settings_page_apply(&page);
            check_visible_text(lv_obj_get_child(page.mycar_rows[i], 0));
        }
        for(unsigned root = 0; root < 3; ++root) {
            page.st.view = UI_SETTINGS_VIEW_ROOT;
            page.st.root_index = root;
            ui_settings_page_apply(&page);
            lv_obj_t *row = root == 0 ? page.it_rows[0] :
                            root == 1 ? page.info_rows[0] : page.mycar_rows[0];
            check_visible_text(lv_obj_get_child(row, 0));
        }
        page.st.view = UI_SETTINGS_VIEW_CONNECT;
        ui_settings_page_apply(&page);
        check_visible_text(lv_obj_get_child(page.date_rows[0], 0));
        page.st.view = UI_SETTINGS_VIEW_SYSTEM;
        ui_settings_page_apply(&page);
        CHECK(!lv_obj_has_flag(page.root_bar, LV_OBJ_FLAG_HIDDEN));
        CHECK(lv_label_get_text(page.title)[0] == '\0');
        CHECK(lv_obj_get_height(page.system_list) == 300);
        CHECK(strcmp(lv_label_get_text(lv_obj_get_child(page.system_rows[1], 0)),
                     language == 0 ? "日期时间" : "DATE & TIME") == 0);
        CHECK(strcmp(lv_label_get_text(lv_obj_get_child(page.system_rows[1], 1)), "--.--") == 0);
        for(unsigned i = 0; i < UI_SETTINGS_SYSTEM_COUNT; ++i) {
            page.st.system_index = i;
            ui_settings_page_apply(&page);
            check_visible_text(lv_obj_get_child(page.system_rows[i], 0));
        }
        page.st.view = UI_SETTINGS_VIEW_LANGUAGE;
        ui_settings_page_apply(&page);
        lv_obj_update_layout(page.root);
        CHECK(lv_obj_has_flag(page.root_bar, LV_OBJ_FLAG_HIDDEN));
        CHECK(lv_obj_get_y(page.body) == 140);
        for(unsigned i = 0; i < 20; ++i) {
            page.st.factory_index = i;
            ui_settings_page_apply(&page);
            check_visible_text(lv_obj_get_child(page.factory_rows[i], 0));
        }
    }
    ui_settings_page_destroy(&page);
    ui_settings_page_set_settings(NULL);
}

int main(void)
{
    lv_display_t *display;

    lv_init();
    display = lv_display_create(1920, 720);
    if(display == NULL) {
        return 1;
    }
    /* 无窗口、无渲染：只测对象状态 */
    lv_display_delete_refr_timer(display);

    test_code_not_masked();
    test_short_key_cycle();
    test_press_state_filter();
    test_settings_navigation();
    test_settings_timeout();
    test_settings_write();
    test_settings_driving_info();
    test_settings_general_eco();
    test_eco_history_page();
    test_long_press_maps_left_right();
    test_held_repeat();
    test_style_switch_key();
    test_style_options_per_vehicle();
    test_style_switch_persists_and_refreshes();
    test_settings_vehicle_light();
    test_settings_vehicle_turn_wiper();
    test_settings_vehicle_service();
    test_settings_vehicle_gated();
    test_batch_order();
    test_card_mask_live();
    test_empty_queue();
    test_boot_animation_selection();
    test_date_and_connect();
    test_date_wheel();
    test_date_language();
    test_language_factory();
    test_factory_password_wheels();
    test_settings_visible_geometry();

    printf("steering_key: %d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
