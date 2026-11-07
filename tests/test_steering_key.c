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

/* ECO 历史信息：进入即显示 4 条历史柱，OK 把“当前”平均油耗推入并落盘 */
static void test_eco_history_page(void)
{
    car_settings_t snapshot;
    uint64_t version;
    uint16_t last;
    car_state_t st;
    car_state_view_t view;

    setup_forester();

    /* 绑定落盘路径（setup 只 reset，未 load）；先删残留文件避免读到上次用例 */
    (void)unlink("/tmp/carmw_eco_hist_test.cfg");
    CHECK(car_settings_load(&s_settings, "/tmp/carmw_eco_hist_test.cfg"));

    car_state_init(&st);
    st.trip.valid = true;
    st.trip.avg_fuel_x10 = 123U;
    memset(&view, 0, sizeof(view));
    view.has_data = true;
    view.state = st;
    ui_app_apply(&view);

    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(ui_app_settings_is_open());

    enter_mycar();
    mycar_move_to(MYCAR_STYLE_INDEX, MYCAR_ECO_HISTORY_INDEX);
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(ui_app_settings_is_open());

    /* 历史柱取自持久设置（初值 9.0 / 10.5 / 8.0 / 9.2） */
    CHECK(s_settings.values.eco_history_x10[0] == 90U);
    CHECK(s_settings.values.eco_history_x10[3] == 92U);

    /* 单卡片页面：上下键无副作用 */
    {
        uint16_t before0 = s_settings.values.eco_history_x10[0];

        push_key(20U, 1U);
        CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
        push_key(19U, 1U);
        CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
        CHECK(s_settings.values.eco_history_x10[0] == before0);
    }

    /* OK = 记录当前平均油耗（12.3），最旧一条被丢弃 */
    last = s_settings.values.eco_history_x10[3];
    push_key(23U, 1U);
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(s_settings.values.eco_history_x10[3] == 123U);
    CHECK(s_settings.values.eco_history_x10[2] == last);
    CHECK(s_settings.values.eco_history_x10[0] == 105U); /* 满 4 条只丢最旧一条 */
    CHECK(car_settings_dirty(&s_settings));

    version = car_settings_snapshot(&s_settings, &snapshot);
    CHECK(car_settings_save(&snapshot, version, &s_settings));
    CHECK(!car_settings_dirty(&s_settings));
    (void)unlink("/tmp/carmw_eco_hist_test.cfg");

    /* 返回键回到我的座驾列表 */
    push_key(20U, 1U); /* 返回 */
    CHECK(car_ui_key_dispatch(&s_link, &s_ctx) == 1U);
    CHECK(ui_app_settings_is_open());

    teardown_forester();
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

/* 31 只切样式：新样式内部从第 0 张卡片开始 */
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
    test_batch_order();
    test_card_mask_live();
    test_empty_queue();

    printf("steering_key: %d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
