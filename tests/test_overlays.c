#include "test_util.h"

#include "carmw/car_link.h"
#include "carmw/car_port_memory.h"
#include "carmw/car_protocol.h"
#include "car_decode_internal.h"
#include "ui/ComControl/ui_lamp_group.h"
#include "ui/ComControl/ui_lamp_row.h"
#include "ui/ComControl/ui_range.h"
#include "ui/FuelTank/fuel_forester1.h"
#include "ui/PageModel/page_short_term.h"
#include "ui/SpeedDial/speed_forester1.h"
#include "ui/overlays/ov_cruise.h"
#include "ui/overlays/ov_tips.h"
#include "ui/overlays/ov_tire.h"
#include "ui/ui_app.h"
#include "ui/settings/ui_settings_page.h"
#include "ui/ui_session.h"
#include "ui/ui_theme.h"

int g_checks;
int g_failures;

/* 单对象用例：事件计时与显隐分开驱动，等价于页面里的 ui_session + ov_tire_apply */
static void tire_apply_step(ov_tire_t *ov, ov_tire_event_t *ev,
                            const car_state_t *st, bool suppressed)
{
    ov_tire_event_apply(ev, st);
    ov_tire_apply(ov, st, suppressed, ev);
}

static void test_tire(lv_obj_t *parent)
{
    const uint8_t abnormal[10] = {0x20, 220, 0x10, 220, 0x10, 220, 0x10, 220, 0, 0};
    const uint8_t normal[10] = {0x10, 220, 0x10, 220, 0x10, 220, 0x10, 220, 0, 0};
    car_state_t st;
    ov_tire_t ov = {0};
    ov_tire_event_t ev;

    car_state_init(&st);
    ov_tire_event_reset(&ev);
    CHECK(ov_tire_create(&ov, parent));
    tire_apply_step(&ov, &ev, &st, false);
    CHECK(!ov_tire_is_active(&ov));

    CHECK(car_decode_tires(abnormal, sizeof(abnormal), &st));
    tire_apply_step(&ov, &ev, &st, false);
    CHECK(ov_tire_is_active(&ov));
    for(unsigned i = 0; i < 9; ++i) {
        lv_tick_inc(1000);
        tire_apply_step(&ov, &ev, &st, false);
        CHECK(ov_tire_is_active(&ov));
    }
    lv_tick_inc(1000);
    tire_apply_step(&ov, &ev, &st, false);
    CHECK(!ov_tire_is_active(&ov));
    CHECK(lv_obj_has_flag(ov.root, LV_OBJ_FLAG_HIDDEN));
    tire_apply_step(&ov, &ev, &st, false);
    CHECK(!ov_tire_is_active(&ov)); /* 旧快照不能重开 */

    CHECK(car_decode_tires(abnormal, sizeof(abnormal), &st));
    tire_apply_step(&ov, &ev, &st, false);
    CHECK(ov_tire_is_active(&ov));
    lv_tick_inc(9000);
    CHECK(car_decode_tires(abnormal, sizeof(abnormal), &st));
    tire_apply_step(&ov, &ev, &st, false);
    lv_tick_inc(1000);
    tire_apply_step(&ov, &ev, &st, false);
    CHECK(ov_tire_is_active(&ov)); /* 相同内容的新帧续期 */
    lv_tick_inc(9000);
    tire_apply_step(&ov, &ev, &st, false);
    CHECK(!ov_tire_is_active(&ov));

    CHECK(car_decode_tires(abnormal, sizeof(abnormal), &st));
    tire_apply_step(&ov, &ev, &st, true);
    CHECK(!ov_tire_is_active(&ov));
    lv_tick_inc(5000);
    tire_apply_step(&ov, &ev, &st, false);
    CHECK(ov_tire_is_active(&ov)); /* 有效期内解除遮挡恢复 */
    tire_apply_step(&ov, &ev, &st, true);
    lv_tick_inc(5000);
    tire_apply_step(&ov, &ev, &st, true);
    tire_apply_step(&ov, &ev, &st, false);
    CHECK(!ov_tire_is_active(&ov)); /* 遮挡不暂停或重置计时 */

    CHECK(car_decode_tires(abnormal, sizeof(abnormal), &st));
    tire_apply_step(&ov, &ev, &st, false);
    CHECK(ov_tire_is_active(&ov));
    CHECK(car_decode_tires(normal, sizeof(normal), &st));
    tire_apply_step(&ov, &ev, &st, false);
    CHECK(!ov_tire_is_active(&ov));

    /* 无符号 tick 回绕后仍准确到期。 */
    lv_tick_inc(UINT32_MAX - lv_tick_get() - 4999U);
    CHECK(car_decode_tires(abnormal, sizeof(abnormal), &st));
    tire_apply_step(&ov, &ev, &st, false);
    lv_tick_inc(9999);
    tire_apply_step(&ov, &ev, &st, false);
    CHECK(ov_tire_is_active(&ov));
    lv_tick_inc(1);
    tire_apply_step(&ov, &ev, &st, false);
    CHECK(!ov_tire_is_active(&ov));
    ov_tire_destroy(&ov);
}

/* 把当前车状态作为一份完整快照交给 ui_app（与会话状态更新路径一致） */
static void page_push_view(const car_state_t *st)
{
    car_state_view_t view = {0};

    view.state = *st;
    view.has_data = true;
    ui_app_apply(&view);
}

/*
 * C1：胎压计时跨样式切换。
 * 事件进度由 ui_session 持有，页面销毁重建不会重新获得完整 10 秒；
 * 样式切走期间仍消费快照，返回时按最新事件的剩余有效期显示。
 */
static void test_tire_style_switch(void)
{
    const uint8_t abnormal[10] = {0x20, 220, 0x10, 220, 0x10, 220, 0x10, 220, 0, 0};
    const uint8_t normal[10] = {0x10, 220, 0x10, 220, 0x10, 220, 0x10, 220, 0, 0};
    car_state_t st;
    const ov_tire_event_t *ev;

    car_state_init(&st);
    CHECK(ui_app_init());
    CHECK(ui_app_set_style("forester1"));
    ev = ui_session_tire_event();

    /* 场景一：t=0 异常 → 4s 切走 → 6s 切回，仍按 t=10s 关闭 */
    ui_session_reset();
    CHECK(car_decode_tires(abnormal, sizeof(abnormal), &st));
    page_push_view(&st);
    CHECK(ov_tire_event_is_active(ev));
    lv_tick_inc(4000);
    CHECK(ui_app_set_style("general1"));
    lv_tick_inc(2000);
    CHECK(ui_app_set_style("forester1"));
    CHECK(ov_tire_event_is_active(ev));      /* 6s 时仍有效，只剩 4s */
    lv_tick_inc(3999);
    page_push_view(&st);
    CHECK(ov_tire_event_is_active(ev));      /* 9.999s 未到期 */
    lv_tick_inc(1);
    page_push_view(&st);
    CHECK(!ov_tire_event_is_active(ev));     /* 10s 到期，未因重建续期 */

    /* 场景二：已到期后切走再切回，无新帧不弹出 */
    ui_session_reset();
    CHECK(car_decode_tires(abnormal, sizeof(abnormal), &st));
    page_push_view(&st);
    CHECK(ov_tire_event_is_active(ev));
    lv_tick_inc(10000);
    page_push_view(&st);
    CHECK(!ov_tire_event_is_active(ev));
    CHECK(ui_app_set_style("general1"));
    CHECK(ui_app_set_style("forester1"));
    CHECK(!ov_tire_event_is_active(ev));

    /* 场景三：切走期间收到新的相同异常帧，只按最新事件剩余有效期显示 */
    ui_session_reset();
    CHECK(car_decode_tires(abnormal, sizeof(abnormal), &st));
    page_push_view(&st);
    lv_tick_inc(4000);
    CHECK(ui_app_set_style("general1"));
    lv_tick_inc(1000);
    CHECK(car_decode_tires(abnormal, sizeof(abnormal), &st));
    page_push_view(&st);                     /* 样式切走期间仍消费快照 */
    CHECK(ov_tire_event_is_active(ev));
    lv_tick_inc(1000);
    CHECK(ui_app_set_style("forester1"));
    lv_tick_inc(8999);
    page_push_view(&st);
    CHECK(ov_tire_event_is_active(ev));      /* t=14.999，按 5s 事件计 */
    lv_tick_inc(1);
    page_push_view(&st);
    CHECK(!ov_tire_event_is_active(ev));     /* t=15 到期，而非返回到期 */

    /* 场景四：切走期间收到正常帧，返回不显示 */
    ui_session_reset();
    CHECK(car_decode_tires(abnormal, sizeof(abnormal), &st));
    page_push_view(&st);
    lv_tick_inc(4000);
    CHECK(ui_app_set_style("general1"));
    lv_tick_inc(1000);
    CHECK(car_decode_tires(normal, sizeof(normal), &st));
    page_push_view(&st);
    CHECK(!ov_tire_event_is_active(ev));
    lv_tick_inc(1000);
    CHECK(ui_app_set_style("forester1"));
    CHECK(!ov_tire_event_is_active(ev));
}

/* 在页面根的直接子对象中按坐标+尺寸定位（页面坐标即 Qt 设计坐标） */
static lv_obj_t *page_child(lv_obj_t *parent, int32_t x, int32_t y,
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

/* 同一行高度相同、位置可变的提示框（P 挡提示条宽度随文字） */
static lv_obj_t *find_child_at_y(lv_obj_t *parent, int32_t y, int32_t h,
                                 lv_obj_t *skip)
{
    for(int32_t i = 0; i >= 0; ++i) {
        lv_obj_t *child = lv_obj_get_child(parent, i);

        if(child == NULL) {
            break;
        }
        if(child != skip && lv_obj_get_y(child) == y &&
           lv_obj_get_height(child) == h) {
            return child;
        }
    }

    return NULL;
}

/* 中心卡片栈里唯一自身未隐藏的子对象 */
static lv_obj_t *first_visible_child(lv_obj_t *parent)
{
    for(int32_t i = 0; i >= 0; ++i) {
        lv_obj_t *child = lv_obj_get_child(parent, i);

        if(child == NULL) {
            break;
        }
        if(!lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) {
            return child;
        }
    }

    return NULL;
}

static void state_set_door(car_state_t *st, bool open)
{
    st->doors.valid = true;
    st->doors.left_front = open;
    st->doors.right_front = false;
    st->doors.left_rear = false;
    st->doors.right_rear = false;
    st->doors.trunk = false;
    st->doors.hood = false;
}

static void state_set_gear(car_state_t *st, char g)
{
    st->gear.valid = true;
    st->gear.text[0] = g;
    st->gear.text[1] = '\0';
    st->gear.text[2] = '\0';
    st->gear.text[3] = '\0';
}

/* 每个测试调用都代表一帧新的胎压报文（sequence 递增） */
static void state_set_tires(car_state_t *st, bool abnormal)
{
    st->tires.valid = true;
    st->tires.sequence++;
    for(uint8_t i = 0U; i < 4U; ++i) {
        st->tires.wheels[i].valid = true;
        st->tires.wheels[i].kpa = 220U;
        st->tires.wheels[i].state = abnormal ? 2U : 1U;
    }
}

static void state_set_radar(car_state_t *st, bool on)
{
    st->radar.valid = true;
    st->radar.display_on = on;
    for(uint8_t i = 0U; i < 5U; ++i) {
        st->radar.front[i].value = 0x3FU;
        st->radar.front[i].color = 0U;
        st->radar.rear[i].value = 0x3FU;
        st->radar.rear[i].color = 0U;
    }
    if(on) {
        st->radar.front[0].value = 10U;
        st->radar.front[0].color = 1U;
        st->radar.rear[0].value = 10U;
        st->radar.rear[0].color = 2U;
    }
}

/*
 * C2：覆盖层组合与中心卡片恢复（基线：车门 > 胎压 > 巡航 > 雷达）。
 * 页面级断言全部通过 ui_app 的真实快照分发路径，不直接调用覆盖层对象。
 */
static void test_system_version_settings(void)
{
    ui_settings_page_t page = {0};
    car_state_t st;
    lv_obj_t *host;
    ui_key_event_t key = {.pressed = 1U};

    car_state_init(&st);
    host = lv_obj_create(lv_screen_active());
    CHECK(ui_settings_page_create(&page, host));
    ui_settings_page_set_active(&page, true);
    page.st.root_index = UI_SETTINGS_ROOT_SYSTEM;
    key.code = UI_KEY_OK;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.view == UI_SETTINGS_VIEW_SYSTEM);
    CHECK(lv_obj_get_child(page.system_rows[0], 0) != NULL);
    CHECK(lv_obj_get_style_text_color(lv_obj_get_child(page.system_rows[0], 0), 0).red ==
          lv_color_hex(0x848484).red);

    key.code = UI_KEY_DOWN;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.system_index == UI_SETTINGS_SYSTEM_VERSION);
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.system_index == UI_SETTINGS_SYSTEM_BACK);
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.system_index == UI_SETTINGS_SYSTEM_VERSION);

    key.code = UI_KEY_OK;
    CHECK(ui_settings_page_key(&page, &key));
    CHECK(page.st.view == UI_SETTINGS_VIEW_VERSION);
    st.version.main_valid = true;
    (void)snprintf(st.version.main, sizeof(st.version.main), "MAIN-1");
    st.version.chip_valid = true;
    (void)snprintf(st.version.chip, sizeof(st.version.chip), "CHIP-2");
    st.version.wired_valid = true;
    (void)snprintf(st.version.wired, sizeof(st.version.wired), "WIRE-3");
    ui_settings_page_set_version(&page, st.version.main, true, st.version.chip, true,
                                 st.version.wired, true);
    CHECK(strcmp(lv_label_get_text(page.version_values[1]), "MAIN-1") == 0);
    CHECK(strcmp(lv_label_get_text(page.version_values[2]), "CHIP-2") == 0);
    CHECK(strcmp(lv_label_get_text(page.version_values[3]), "WIRE-3") == 0);
    CHECK(strcmp(lv_label_get_text(page.version_values[0]), "---") == 0);
    CHECK(strcmp(lv_label_get_text(page.version_updated), "---") == 0);
    CHECK(ui_settings_page_back(&page));
    CHECK(page.st.view == UI_SETTINGS_VIEW_SYSTEM);
    CHECK(ui_settings_page_back(&page));
    CHECK(page.st.view == UI_SETTINGS_VIEW_ROOT);
    ui_settings_page_destroy(&page);
    lv_obj_delete(host);
}

static void test_overlay_composition(void)
{
    const uint8_t lane_on[7] = {0x0E, 0, 1, 0, 1, 0, 0};
    const uint8_t lane_clear[7] = {0x0E, 0, 0, 0, 0, 0, 0};
    const uint8_t drive[2] = {0x04, 2};
    const uint8_t popup[4] = {0x00, 23, 0x02, 1};
    car_state_t st;
    lv_obj_t *page;
    lv_obj_t *stack;
    lv_obj_t *door;
    lv_obj_t *tire;
    lv_obj_t *cruise;
    lv_obj_t *radar;
    lv_obj_t *tips;
    lv_obj_t *fuel_banner;
    lv_obj_t *gear_banner;
    lv_obj_t *general;
    lv_obj_t *wave_front;
    lv_obj_t *wave_rear;
    lv_obj_t *card0;
    lv_obj_t *card_prev;
    ui_key_event_t key = {UI_KEY_NEXT, 1U};

    car_state_init(&st);
    ui_app_deinit();            /* 与 C1 用例隔离：重建舞台与样式页面 */
    CHECK(ui_app_init());
    CHECK(ui_app_set_style("forester1"));
    page = ui_app_current_root();
    CHECK(page != NULL);
    lv_obj_update_layout(page);   /* 坐标在布局更新后才可读取 */

    stack = page_child(page, 553, 140, 814, 509);
    door = page_child(page, 620, 100, 720, 550);
    tire = page_child(page, 554, 150, 814, 509);
    cruise = page_child(page, 660, 200, 600, 470);
    radar = page_child(page, 815, 160, 290, 430);
    tips = page_child(page, 0, 0, 1920, 720);
    CHECK(stack != NULL && door != NULL && tire != NULL &&
          cruise != NULL && radar != NULL && tips != NULL);

    fuel_banner = page_child(tips, 845, 30, 230, 60);
    gear_banner = find_child_at_y(tips, 30, 60, fuel_banner);
    general = page_child(tips, 834, 253, 260, 280);
    CHECK(fuel_banner != NULL && gear_banner != NULL && general != NULL);

    wave_front = page_child(radar, 50, 10, 191, 90);
    wave_rear = page_child(radar, 50, 330, 191, 90);
    CHECK(wave_front != NULL && wave_rear != NULL);

    page_push_view(&st);
    CHECK(!lv_obj_has_flag(stack, LV_OBJ_FLAG_HIDDEN));

    /* 先切到非第一张卡片，作为“完全退出后恢复原卡片”的基准 */
    card0 = first_visible_child(stack);
    CHECK(card0 != NULL);
    ui_app_handle_key(&key);
    card_prev = first_visible_child(stack);
    CHECK(card_prev != NULL);
    CHECK(card_prev != card0);

    /* 单层：车门 */
    state_set_door(&st, true);
    page_push_view(&st);
    CHECK(lv_obj_has_flag(stack, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(door, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(tire, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(cruise, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(radar, LV_OBJ_FLAG_HIDDEN));

    /* Qt settingsPageOpen：车门也隐藏，关闭设置后按当前门状态恢复 */
    ui_app_settings_open();
    page_push_view(&st);
    CHECK(lv_obj_has_flag(door, LV_OBJ_FLAG_HIDDEN));
    ui_app_settings_close();
    page_push_view(&st);
    CHECK(!lv_obj_has_flag(door, LV_OBJ_FLAG_HIDDEN));

    /* 单层：胎压（车门关闭后） */
    state_set_door(&st, false);
    state_set_tires(&st, true);
    page_push_view(&st);
    CHECK(!lv_obj_has_flag(tire, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(door, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(cruise, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(radar, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(stack, LV_OBJ_FLAG_HIDDEN));

    /* 多层竞争：车门 + 胎压 + 巡航 + 雷达，只显示车门 */
    state_set_door(&st, true);
    CHECK(car_decode_high_func(lane_on, sizeof(lane_on), &st));
    state_set_radar(&st, true);
    state_set_gear(&st, 'D');
    page_push_view(&st);
    CHECK(!lv_obj_has_flag(door, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(tire, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(cruise, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(radar, LV_OBJ_FLAG_HIDDEN));

    /* 车门退出 → 胎压恢复（仍有异常事件）；再退出 → 巡航恢复 */
    state_set_door(&st, false);
    page_push_view(&st);
    CHECK(lv_obj_has_flag(door, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(tire, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(cruise, LV_OBJ_FLAG_HIDDEN));

    state_set_tires(&st, false);
    page_push_view(&st);
    CHECK(lv_obj_has_flag(tire, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(cruise, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(radar, LV_OBJ_FLAG_HIDDEN));

    /* 巡航退出 → 雷达恢复；全部退出后回到切走前的那张卡片 */
    CHECK(car_decode_high_func(lane_clear, sizeof(lane_clear), &st));
    page_push_view(&st);
    CHECK(lv_obj_has_flag(cruise, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(radar, LV_OBJ_FLAG_HIDDEN));

    /* Qt radaronoff 实为“雷达开启”：关闭后即使有帧也不显示 */
    st.radar_on = false;
    page_push_view(&st);
    CHECK(lv_obj_has_flag(radar, LV_OBJ_FLAG_HIDDEN));
    st.radar_on = true;
    page_push_view(&st);
    CHECK(!lv_obj_has_flag(radar, LV_OBJ_FLAG_HIDDEN));

    /* Qt carPlayModel：互联会话进行中隐藏雷达 */
    ui_app_set_carplay_active(true);
    page_push_view(&st);
    CHECK(lv_obj_has_flag(radar, LV_OBJ_FLAG_HIDDEN));
    ui_app_set_carplay_active(false);
    page_push_view(&st);
    CHECK(!lv_obj_has_flag(radar, LV_OBJ_FLAG_HIDDEN));

    /* Qt settingsPageOpen：雷达与中心内容隐藏，关闭后恢复 */
    ui_app_settings_open();
    page_push_view(&st);
    CHECK(lv_obj_has_flag(radar, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(stack, LV_OBJ_FLAG_HIDDEN));
    ui_app_settings_close();
    page_push_view(&st);
    CHECK(!lv_obj_has_flag(radar, LV_OBJ_FLAG_HIDDEN));

    /* 雷达方向：D 前波 / R 后波 / P 两波都不显示（Qt 按 geartext 分支） */
    CHECK(!lv_obj_has_flag(wave_front, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(wave_rear, LV_OBJ_FLAG_HIDDEN));
    state_set_gear(&st, 'R');
    page_push_view(&st);
    CHECK(lv_obj_has_flag(wave_front, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(wave_rear, LV_OBJ_FLAG_HIDDEN));
    state_set_gear(&st, 'P');
    page_push_view(&st);
    CHECK(lv_obj_has_flag(wave_front, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(wave_rear, LV_OBJ_FLAG_HIDDEN));

    state_set_radar(&st, false);
    page_push_view(&st);
    CHECK(lv_obj_has_flag(radar, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(stack, LV_OBJ_FLAG_HIDDEN));
    CHECK(first_visible_child(stack) == card_prev);

    /* 门与档位：开门 + D 显示 P 挡提示条，切 P 后立即消失 */
    state_set_door(&st, true);
    state_set_gear(&st, 'D');
    page_push_view(&st);
    CHECK(!lv_obj_has_flag(door, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(gear_banner, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(stack, LV_OBJ_FLAG_HIDDEN));

    state_set_gear(&st, 'P');
    page_push_view(&st);
    CHECK(lv_obj_has_flag(gear_banner, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(door, LV_OBJ_FLAG_HIDDEN));

    /* 提示叠加：车门显示时弹通用提示，叠加显示且不改变中心卡片状态 */
    state_set_gear(&st, 'D');
    CHECK(car_decode_popup(popup, sizeof(popup), &st));
    page_push_view(&st);
    CHECK(!lv_obj_has_flag(general, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(gear_banner, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(door, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(stack, LV_OBJ_FLAG_HIDDEN));

    /* 按真实刷新粒度推进 1s：0x03 帧间隔 <1s 时不重播，到期后隐藏 */
    for(uint8_t i = 0U; i < 10U; ++i) {
        lv_tick_inc(100);
        page_push_view(&st);
    }
    CHECK(lv_obj_has_flag(general, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(door, LV_OBJ_FLAG_HIDDEN));

    /* 之后不再收到 0x03 帧，避免后续大跨度计时被误判为新事件 */
    st.popup.valid = false;

    state_set_door(&st, false);
    page_push_view(&st);
    CHECK(lv_obj_has_flag(gear_banner, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(stack, LV_OBJ_FLAG_HIDDEN));
    CHECK(first_visible_child(stack) == card_prev);

    /* 巡航子帧交错：0x0E 建立 → 0x04 → 0x1C → 0x0E 清零；车道保持到显式清零 */
    CHECK(car_decode_high_func(lane_on, sizeof(lane_on), &st));
    page_push_view(&st);
    CHECK(!lv_obj_has_flag(cruise, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(stack, LV_OBJ_FLAG_HIDDEN));

    CHECK(car_decode_high_func(drive, sizeof(drive), &st));
    page_push_view(&st);
    CHECK(!lv_obj_has_flag(cruise, LV_OBJ_FLAG_HIDDEN));

    car_state_warn_event_push(&st, 23U);
    page_push_view(&st);
    CHECK(!lv_obj_has_flag(cruise, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(general, LV_OBJ_FLAG_HIDDEN));

    CHECK(car_decode_high_func(lane_clear, sizeof(lane_clear), &st));
    page_push_view(&st);
    CHECK(lv_obj_has_flag(cruise, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(general, LV_OBJ_FLAG_HIDDEN)); /* 提示独立到期 */
    CHECK(!lv_obj_has_flag(stack, LV_OBJ_FLAG_HIDDEN));

    lv_tick_inc(2000);
    page_push_view(&st);
    CHECK(lv_obj_has_flag(general, LV_OBJ_FLAG_HIDDEN));

    /* 同一快照重复 apply：显示不变，胎压事件计时不被重置 */
    state_set_tires(&st, true);
    page_push_view(&st);
    CHECK(!lv_obj_has_flag(tire, LV_OBJ_FLAG_HIDDEN));
    lv_tick_inc(5000);
    page_push_view(&st);
    page_push_view(&st);
    page_push_view(&st);
    lv_tick_inc(4999);
    page_push_view(&st);
    CHECK(!lv_obj_has_flag(tire, LV_OBJ_FLAG_HIDDEN));
    lv_tick_inc(1);
    page_push_view(&st);
    CHECK(lv_obj_has_flag(tire, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(stack, LV_OBJ_FLAG_HIDDEN));
    CHECK(first_visible_child(stack) == card_prev);
}

/* ---------------- C3：灯态、卡片、状态保留与资源生命周期 ---------------- */

/* C3-1：25/4 独立灯位 + 共用图标码 46/91 + 同档多图优先级（Qt 表序靠前者优先） */
static const ui_lamp_group_item_t s_c3_group_items[] = {
    { .variants = {{"WarnIcon/oil.bin", 25U, 0U}}, .variant_count = 1,
      .x = 0, .y = 0, .w = 64, .h = 64 },
    { .variants = {{"WarnIcon/abs.bin", 4U, 0U}}, .variant_count = 1,
      .x = 70, .y = 0, .w = 46, .h = 46 },
    { .variants = {{"WarnIcon/park_auto.bin", 46U, 91U}}, .variant_count = 1,
      .x = 130, .y = 0, .w = 46, .h = 46 },
    { .variants = {{"WarnIcon/READY_g.bin", 139U, 0U},
                   {"WarnIcon/READY_r.bin", 140U, 0U}},
      .variant_count = 2, .x = 190, .y = 0, .w = 64, .h = 64 },
};

/* C3-1：主码 15 常亮 / 备码 229 故障图；备码非零时优先生效 */
static const ui_lamp_row_item_t s_c3_row_items[] = {
    {"WarnIcon/low_beam.bin", "WarnIcon/low_beam_faulty.bin", 15U, 229U, 56, 56},
};

static void check_group_state(ui_lamp_group_t *group, uint8_t index,
                              ui_lamp_state_t state)
{
    const ui_lamp_group_item_rt_t *rt = &group->items[index];

    CHECK(ui_lamp_get_state(&rt->lamp) == state);

    if(state == UI_LAMP_ON) {
        CHECK(lv_obj_get_style_image_opa(rt->lamp.img, 0) == LV_OPA_COVER);
    }
    else if(state == UI_LAMP_OFF) {
        CHECK(lv_obj_get_style_image_opa(rt->lamp.img, 0) == LV_OPA_TRANSP);
    }
    else if(state == UI_LAMP_BLINK) {
        CHECK(rt->lamp.blink != NULL);
    }
}

static void test_lamp_states(lv_obj_t *screen)
{
    const uint8_t global_on[1] = {1U};
    const uint8_t global_off[1] = {0U};
    const uint8_t off25[2] = {0U, 25U};
    const uint8_t on4[2] = {1U, 4U};
    const uint8_t on25[2] = {1U, 25U};
    const uint8_t clear25[2] = {0U, 25U};
    const uint8_t clear4[2] = {0U, 4U};
    const uint8_t blink25[2] = {2U, 25U};
    const uint8_t on46[2] = {1U, 46U};
    const uint8_t clear46[2] = {0U, 46U};
    const uint8_t slow91[2] = {3U, 91U};
    const uint8_t clear91[2] = {0U, 91U};
    const uint8_t on139[2] = {1U, 139U};
    const uint8_t on140[2] = {1U, 140U};
    const uint8_t clear139[2] = {0U, 139U};
    const uint8_t clear140[2] = {0U, 140U};
    const uint8_t on15[2] = {1U, 15U};
    const uint8_t clear15[2] = {0U, 15U};
    const uint8_t on229[2] = {1U, 229U};
    const uint8_t clear229[2] = {0U, 229U};
    car_state_t st;
    ui_lamp_group_t group = {0};
    ui_lamp_row_t row = {0};
    lv_obj_t *holder;

    car_state_init(&st);
    holder = lv_obj_create(screen);
    CHECK(ui_lamp_group_create(&group, holder, s_c3_group_items,
                               (uint8_t)(sizeof(s_c3_group_items) /
                                         sizeof(s_c3_group_items[0]))));
    CHECK(ui_lamp_row_create(&row, holder, 300, 0, 64, s_c3_row_items, 1U));

    /* 全局 OFF / 全局 ON 覆盖全部灯位 */
    CHECK(car_decode_warnings(global_off, 1, &st));
    ui_lamp_group_apply(&group, &st);
    for(uint8_t i = 0U; i < 4U; ++i) {
        check_group_state(&group, i, UI_LAMP_OFF);
    }

    CHECK(car_decode_warnings(global_on, 1, &st));
    ui_lamp_group_apply(&group, &st);
    for(uint8_t i = 0U; i < 4U; ++i) {
        check_group_state(&group, i, UI_LAMP_ON);
    }

    /* 局部覆盖：只改列出的码，其余保持全局状态 */
    CHECK(car_decode_warnings(off25, 2, &st));
    ui_lamp_group_apply(&group, &st);
    check_group_state(&group, 0U, UI_LAMP_OFF);
    check_group_state(&group, 1U, UI_LAMP_ON);
    check_group_state(&group, 2U, UI_LAMP_ON);

    CHECK(car_decode_warnings(on4, 2, &st));
    ui_lamp_group_apply(&group, &st);
    check_group_state(&group, 0U, UI_LAMP_OFF);
    check_group_state(&group, 1U, UI_LAMP_ON);

    /* 再次全局 OFF 后局部 ON：仍只有指定灯位恢复 */
    CHECK(car_decode_warnings(global_off, 1, &st));
    ui_lamp_group_apply(&group, &st);
    for(uint8_t i = 0U; i < 4U; ++i) {
        check_group_state(&group, i, UI_LAMP_OFF);
    }

    /* 25亮 → 4亮 → 25灭：两个灯位各自独立，不互相清除 */
    CHECK(car_decode_warnings(on25, 2, &st));
    ui_lamp_group_apply(&group, &st);
    check_group_state(&group, 0U, UI_LAMP_ON);
    check_group_state(&group, 1U, UI_LAMP_OFF);

    CHECK(car_decode_warnings(on4, 2, &st));
    ui_lamp_group_apply(&group, &st);
    check_group_state(&group, 0U, UI_LAMP_ON);
    check_group_state(&group, 1U, UI_LAMP_ON);

    CHECK(car_decode_warnings(clear25, 2, &st));
    ui_lamp_group_apply(&group, &st);
    check_group_state(&group, 0U, UI_LAMP_OFF);
    check_group_state(&group, 1U, UI_LAMP_ON);

    CHECK(car_decode_warnings(clear4, 2, &st));
    ui_lamp_group_apply(&group, &st);
    check_group_state(&group, 0U, UI_LAMP_OFF);
    check_group_state(&group, 1U, UI_LAMP_OFF);

    /* 全局 ON 之上再局部覆盖为快闪 */
    CHECK(car_decode_warnings(global_on, 1, &st));
    ui_lamp_group_apply(&group, &st);
    CHECK(car_decode_warnings(blink25, 2, &st));
    ui_lamp_group_apply(&group, &st);
    check_group_state(&group, 0U, UI_LAMP_BLINK);
    check_group_state(&group, 1U, UI_LAMP_ON);

    /* 共用图标码 46/91：取较高灯态，单码清除不影响另一码 */
    CHECK(car_decode_warnings(global_off, 1, &st));
    ui_lamp_group_apply(&group, &st);
    CHECK(car_decode_warnings(on46, 2, &st));
    ui_lamp_group_apply(&group, &st);
    check_group_state(&group, 2U, UI_LAMP_ON);
    CHECK(car_decode_warnings(clear46, 2, &st));
    CHECK(car_decode_warnings(slow91, 2, &st));
    ui_lamp_group_apply(&group, &st);
    check_group_state(&group, 2U, UI_LAMP_BLINK);
    CHECK(car_decode_warnings(clear91, 2, &st));
    ui_lamp_group_apply(&group, &st);
    check_group_state(&group, 2U, UI_LAMP_OFF);

    /* 同档多图：表中靠前的图标优先；前图对应码熄灭后切到后图 */
    CHECK(car_decode_warnings(on139, 2, &st));
    ui_lamp_group_apply(&group, &st);
    CHECK(group.items[3].active == 0);
    check_group_state(&group, 3U, UI_LAMP_ON);
    CHECK(lv_image_get_src(group.items[3].lamp.img) == &group.items[3].dsc[0]);

    CHECK(car_decode_warnings(on140, 2, &st));
    ui_lamp_group_apply(&group, &st);
    CHECK(group.items[3].active == 0);
    CHECK(lv_image_get_src(group.items[3].lamp.img) == &group.items[3].dsc[0]);

    CHECK(car_decode_warnings(clear139, 2, &st));
    ui_lamp_group_apply(&group, &st);
    CHECK(group.items[3].active == 1);
    CHECK(lv_image_get_src(group.items[3].lamp.img) == &group.items[3].dsc[1]);

    CHECK(car_decode_warnings(on139, 2, &st));
    ui_lamp_group_apply(&group, &st);
    CHECK(group.items[3].active == 0);

    /* 灯排主/备码：备码非零时优先显示故障图，与灯态高低无关 */
    CHECK(car_decode_warnings(clear139, 2, &st));
    CHECK(car_decode_warnings(clear140, 2, &st));
    CHECK(car_decode_warnings(on15, 2, &st));
    ui_lamp_row_apply(&row, &st);
    CHECK(!row.items[0].use_alt);
    CHECK(lv_image_get_src(row.items[0].lamp.img) == &row.items[0].dsc);
    check_group_state(&group, 0U, UI_LAMP_OFF);

    CHECK(car_decode_warnings(on229, 2, &st));
    ui_lamp_row_apply(&row, &st);
    CHECK(row.items[0].use_alt);
    CHECK(lv_image_get_src(row.items[0].lamp.img) == &row.items[0].dsc_alt);
    CHECK(ui_lamp_get_state(&row.items[0].lamp) == UI_LAMP_ON);

    CHECK(car_decode_warnings(clear229, 2, &st));
    ui_lamp_row_apply(&row, &st);
    CHECK(!row.items[0].use_alt);
    CHECK(lv_image_get_src(row.items[0].lamp.img) == &row.items[0].dsc);
    CHECK(ui_lamp_get_state(&row.items[0].lamp) == UI_LAMP_ON);

    CHECK(car_decode_warnings(clear15, 2, &st));
    ui_lamp_row_apply(&row, &st);
    CHECK(ui_lamp_get_state(&row.items[0].lamp) == UI_LAMP_OFF);

    lv_obj_delete(holder);
}

/* Qt show4h 门控的灯位（码 33 四驱打开 / 36 四驱故障） */
static const ui_lamp_group_item_t s_4h_gate_items[] = {
    { .variants = {{"WarnIcon/four_g.bin", 33U, 0U}}, .variant_count = 1,
      .x = 0, .y = 0, .w = 64, .h = 64, .gate = UI_LAMP_GATE_SHOW_4H },
};

static void test_lamp_4h_gate(lv_obj_t *screen)
{
    const uint8_t on33[2] = {1U, 33U};
    car_state_t st;
    ui_lamp_group_t group = {0};
    lv_obj_t *holder = lv_obj_create(screen);

    car_state_init(&st);
    CHECK(ui_lamp_group_create(&group, holder, s_4h_gate_items, 1U));

    /* Qt show4h 缺省 false：即使 MCU 点亮 33，灯位也保持熄灭 */
    CHECK(!st.show_4h);
    CHECK(car_decode_warnings(on33, 2, &st));
    ui_lamp_group_apply(&group, &st);
    check_group_state(&group, 0U, UI_LAMP_OFF);

    /* 打开 show4h 后同一故障码恢复点亮 */
    st.show_4h = true;
    ui_lamp_group_apply(&group, &st);
    check_group_state(&group, 0U, UI_LAMP_ON);

    lv_obj_delete(holder);
}

/* 快闪 600ms / 慢闪 6000ms：虚拟时钟推进，重复 apply 不得重置相位 */
static const ui_lamp_group_item_t s_c3_blink_items[] = {
    { .variants = {{"WarnIcon/oil.bin", 9U, 0U}}, .variant_count = 1,
      .x = 0, .y = 0, .w = 64, .h = 64 },
    { .variants = {{"WarnIcon/abs.bin", 10U, 0U}}, .variant_count = 1,
      .x = 70, .y = 0, .w = 46, .h = 46 },
};

static void test_lamp_blink(lv_obj_t *screen)
{
    const uint8_t fast[2] = {2U, 9U};
    const uint8_t clear9[2] = {0U, 9U};
    const uint8_t slow[2] = {3U, 10U};
    car_state_t st;
    ui_lamp_group_t group = {0};
    lv_obj_t *holder;
    lv_timer_t *fast_timer;

    car_state_init(&st);
    holder = lv_obj_create(screen);
    CHECK(ui_lamp_group_create(&group, holder, s_c3_blink_items,
                               (uint8_t)(sizeof(s_c3_blink_items) /
                                         sizeof(s_c3_blink_items[0]))));

    CHECK(car_decode_warnings(fast, 2, &st));
    ui_lamp_group_apply(&group, &st);
    check_group_state(&group, 0U, UI_LAMP_BLINK);
    CHECK(!lv_obj_has_flag(group.items[0].lamp.img, LV_OBJ_FLAG_HIDDEN));
    fast_timer = group.items[0].lamp.blink;
    CHECK(fast_timer != NULL);

    /* 快闪半周期 300ms：299ms 不变，300ms 切为隐藏 */
    lv_tick_inc(299);
    (void)lv_timer_handler();
    CHECK(!lv_obj_has_flag(group.items[0].lamp.img, LV_OBJ_FLAG_HIDDEN));

    lv_tick_inc(1);
    (void)lv_timer_handler();
    CHECK(lv_obj_has_flag(group.items[0].lamp.img, LV_OBJ_FLAG_HIDDEN));

    /* 重复 apply 相同快照：定时器实例与相位都不得变化 */
    for(uint8_t i = 0U; i < 10U; ++i) {
        ui_lamp_group_apply(&group, &st);
    }
    CHECK(group.items[0].lamp.blink == fast_timer);
    CHECK(lv_obj_has_flag(group.items[0].lamp.img, LV_OBJ_FLAG_HIDDEN));

    lv_tick_inc(300);
    (void)lv_timer_handler();
    CHECK(!lv_obj_has_flag(group.items[0].lamp.img, LV_OBJ_FLAG_HIDDEN));

    /* 再走一个 600ms 完整周期：亮 → 灭 → 亮 */
    lv_tick_inc(300);
    (void)lv_timer_handler();
    CHECK(lv_obj_has_flag(group.items[0].lamp.img, LV_OBJ_FLAG_HIDDEN));
    CHECK(group.items[0].lamp.blink == fast_timer);

    lv_tick_inc(300);
    (void)lv_timer_handler();
    CHECK(!lv_obj_has_flag(group.items[0].lamp.img, LV_OBJ_FLAG_HIDDEN));

    /* 慢闪：切周期时允许重新从亮开始，之后 3000/6000ms 相位稳定 */
    CHECK(car_decode_warnings(clear9, 2, &st));
    CHECK(car_decode_warnings(slow, 2, &st));
    ui_lamp_group_apply(&group, &st);
    check_group_state(&group, 1U, UI_LAMP_BLINK);
    CHECK(group.items[1].lamp.blink != NULL);
    CHECK(!lv_obj_has_flag(group.items[1].lamp.img, LV_OBJ_FLAG_HIDDEN));

    lv_tick_inc(2999);
    (void)lv_timer_handler();
    CHECK(!lv_obj_has_flag(group.items[1].lamp.img, LV_OBJ_FLAG_HIDDEN));

    lv_tick_inc(1);
    (void)lv_timer_handler();
    CHECK(lv_obj_has_flag(group.items[1].lamp.img, LV_OBJ_FLAG_HIDDEN));

    for(uint8_t i = 0U; i < 10U; ++i) {
        ui_lamp_group_apply(&group, &st);
    }
    CHECK(lv_obj_has_flag(group.items[1].lamp.img, LV_OBJ_FLAG_HIDDEN));

    lv_tick_inc(3000);
    (void)lv_timer_handler();
    CHECK(!lv_obj_has_flag(group.items[1].lamp.img, LV_OBJ_FLAG_HIDDEN));

    lv_tick_inc(3000);
    (void)lv_timer_handler();
    CHECK(lv_obj_has_flag(group.items[1].lamp.img, LV_OBJ_FLAG_HIDDEN));

    lv_obj_delete(holder);
}

static void page_key(uint8_t code)
{
    ui_key_event_t evt = {.code = code, .pressed = 1U};

    ui_app_handle_key(&evt);
}

/*
 * C3-3：按页面注册表顺序访问 10 张中心卡片：
 *   0 导航车辆视图 / 1 行程数据 / 2 4x4-i / 3 胎压 / 4 底盘控制 /
 *   5 短期记忆 / 6 瞬时油耗 / 7 燃油经济性 / 8 多媒体 / 9 互联信息
 */
static void test_card_cycle(void)
{
    car_state_t st;
    lv_obj_t *page;
    lv_obj_t *stack;
    lv_obj_t *cards[10];
    lv_obj_t *card0_after;
    uint8_t unique = 1U;

    car_state_init(&st);
    ui_app_deinit();
    CHECK(ui_app_init());
    CHECK(ui_app_set_style("forester1"));
    page = ui_app_current_root();
    CHECK(page != NULL);
    page_push_view(&st);
    lv_obj_update_layout(page);

    stack = page_child(page, 553, 140, 814, 509);
    CHECK(stack != NULL);
    CHECK(lv_obj_get_child_count(stack) == 10);
    CHECK(page_child(stack, 87, 95, 660, 340) == first_visible_child(stack));

    /* 默认 mask 为 0x3FD（Qt），4X4-I 隐藏：顺序翻卡只会看到 9 张 */
    cards[0] = first_visible_child(stack);
    CHECK(cards[0] != NULL);
    for(uint8_t i = 1U; i < 9U; ++i) {
        page_key(UI_KEY_NEXT);
        cards[i] = first_visible_child(stack);
        CHECK(cards[i] != NULL);
    }
    for(uint8_t i = 0U; i < 9U; ++i) {
        for(uint8_t j = (uint8_t)(i + 1U); j < 9U; ++j) {
            if(cards[i] == cards[j]) {
                unique = 0U;
            }
        }
    }
    CHECK(unique);

    /* 右越界回到第 0 张，左越界回到最后一张（下标 9 的 CONNECT） */
    page_key(UI_KEY_NEXT);
    CHECK(first_visible_child(stack) == cards[0]);
    page_key(UI_KEY_PREV);
    CHECK(first_visible_child(stack) == cards[8]);

    /* 覆盖层退出后恢复同一张卡片（从最后一张顺序走 3 次到下标 2 行程） */
    for(uint8_t i = 0U; i < 3U; ++i) {
        page_key(UI_KEY_NEXT);
    }
    CHECK(first_visible_child(stack) == cards[2]);
    state_set_door(&st, true);
    page_push_view(&st);
    CHECK(lv_obj_has_flag(stack, LV_OBJ_FLAG_HIDDEN));
    state_set_door(&st, false);
    page_push_view(&st);
    CHECK(!lv_obj_has_flag(stack, LV_OBJ_FLAG_HIDDEN));
    CHECK(first_visible_child(stack) == cards[2]);

    /* 样式往返后新页面从第 0 张开始，左右切换仍循环 */
    CHECK(ui_app_set_style("general1"));
    CHECK(ui_app_set_style("forester1"));
    page = ui_app_current_root();
    CHECK(page != NULL);
    lv_obj_update_layout(page);
    stack = page_child(page, 553, 140, 814, 509);
    CHECK(stack != NULL);
    card0_after = first_visible_child(stack);
    CHECK(card0_after == page_child(stack, 87, 95, 660, 340));
    page_key(UI_KEY_PREV);
    CHECK(first_visible_child(stack) != card0_after);
    page_key(UI_KEY_NEXT);
    CHECK(first_visible_child(stack) == card0_after);
}

/* 在控件子树中按标签文本查找（覆盖标签嵌套在行/表盘内部的情况） */
static lv_obj_t *find_label_text(lv_obj_t *parent, const char *text)
{
    lv_obj_t *nested;

    for(int32_t i = 0; i >= 0; ++i) {
        lv_obj_t *child = lv_obj_get_child(parent, i);

        if(child == NULL) {
            break;
        }
        if(lv_obj_check_type(child, &lv_label_class) &&
           strcmp(lv_label_get_text(child), text) == 0) {
            return child;
        }
        nested = find_label_text(child, text);
        if(nested != NULL) {
            return nested;
        }
    }

    return NULL;
}

/* 行容器内第 2 个标签即数值（ui_mileage / ui_range 的 FLEX_ROW 结构） */
static lv_obj_t *row_value(lv_obj_t *prefix)
{
    return lv_obj_get_child(lv_obj_get_parent(prefix), 1);
}

static void check_restored_state(lv_obj_t *page)
{
    lv_obj_t *gear = page_child(page, 920, 564, 90, 90);
    lv_obj_t *dial = page_child(page, 166, 79, 620, 620);
    lv_obj_t *fuel = page_child(page, 1589, 295, 250, 368);
    lv_obj_t *odo = find_label_text(page, "ODO:");
    lv_obj_t *range = find_label_text(page, "RANGE: ");
    lv_obj_t *label140;

    CHECK(gear != NULL && dial != NULL && fuel != NULL);
    CHECK(odo != NULL && range != NULL);

    CHECK(find_label_text(gear, "D") != NULL);

    label140 = find_label_text(dial, "140");
    CHECK(label140 != NULL);
    CHECK(lv_obj_get_style_text_font(label140, 0) == UI_FONT_LG);
    CHECK(find_label_text(dial, "km/h") != NULL);

    CHECK(strcmp(lv_label_get_text(row_value(odo)), "5000 KM") == 0);
    CHECK(strcmp(lv_label_get_text(row_value(range)), "300 KM") == 0);

    /* 油量 25%：刻度窗 130×210，y = 318 + (108-318)×25/100 = 266 */
    CHECK(page_child(fuel, 100, 266, 130, 210) != NULL);
}

/*
 * C3-4：样式往返后档位/车速/油量/里程立即恢复；
 * 已到期的一次性提示不因页面重建而重放。
 */
static void test_state_restore(void)
{
    car_state_t st;
    lv_obj_t *page;
    lv_obj_t *tips;
    lv_obj_t *general;

    car_state_init(&st);
    st.speed_rpm.valid = true;
    st.speed_rpm.speed_kph = 137U;
    state_set_gear(&st, 'D');
    st.fuel.valid = true;
    st.fuel.percent = 25U;
    st.fuel.icon_state = 2U;
    st.fuel.remaining_range_km = 300U;
    st.mileage.valid = true;
    st.mileage.trip_type = 1U;
    st.mileage.odometer_raw = 5000U;
    st.mileage.trip_a_raw = 12345U;

    ui_app_deinit();
    CHECK(ui_app_init());
    CHECK(ui_app_set_style("forester1"));
    page = ui_app_current_root();
    CHECK(page != NULL);
    page_push_view(&st);
    lv_obj_update_layout(page);
    check_restored_state(page);

    /* 0x1C 一次性提示：2s 到期后关闭 */
    car_state_warn_event_push(&st, 23U);
    page_push_view(&st);
    tips = page_child(page, 0, 0, 1920, 720);
    CHECK(tips != NULL);
    general = page_child(tips, 834, 253, 260, 280);
    CHECK(general != NULL);
    CHECK(!lv_obj_has_flag(general, LV_OBJ_FLAG_HIDDEN));

    lv_tick_inc(2000);
    page_push_view(&st);
    CHECK(lv_obj_has_flag(general, LV_OBJ_FLAG_HIDDEN));

    /* 往返期间同一快照会被立即补发，重建页面不得重放已消费事件 */
    CHECK(ui_app_set_style("general1"));
    CHECK(ui_app_set_style("forester1"));
    page = ui_app_current_root();
    CHECK(page != NULL);
    lv_obj_update_layout(page);
    check_restored_state(page);
    tips = page_child(page, 0, 0, 1920, 720);
    general = page_child(tips, 834, 253, 260, 280);
    CHECK(general != NULL);
    CHECK(lv_obj_has_flag(general, LV_OBJ_FLAG_HIDDEN));
}

static uint32_t count_objects(lv_obj_t *obj)
{
    uint32_t count = 1U;
    int32_t children = lv_obj_get_child_count(obj);

    for(int32_t i = 0; i < children; ++i) {
        count += count_objects(lv_obj_get_child(obj, i));
    }

    return count;
}

static uint32_t count_timers(void)
{
    uint32_t count = 0U;
    lv_timer_t *timer;

    for(timer = lv_timer_get_next(NULL); timer != NULL;
        timer = lv_timer_get_next(timer)) {
        count++;
    }

    return count;
}

/* C3-5：预热后 100 次样式往返，对象/定时器数量应保持稳定 */
static void test_style_cycles(void)
{
    car_state_t st;
    uint32_t obj_cold;
    uint32_t obj_warm;
    uint32_t obj_100;
    uint32_t timer_cold;
    uint32_t timer_warm;
    uint32_t timer_100;

    car_state_init(&st);
    page_push_view(&st);

    obj_cold = count_objects(lv_screen_active());
    timer_cold = count_timers();

    CHECK(ui_app_set_style("general1"));
    CHECK(ui_app_set_style("forester1"));
    obj_warm = count_objects(lv_screen_active());
    timer_warm = count_timers();
    printf("C3资源趋势：冷启动对象=%u 定时器=%u；首次往返后对象=%u 定时器=%u\n",
           (unsigned)obj_cold, (unsigned)timer_cold,
           (unsigned)obj_warm, (unsigned)timer_warm);

    for(uint16_t i = 0U; i < 100U; ++i) {
        CHECK(ui_app_set_style("general1"));
        CHECK(ui_app_set_style("forester1"));
    }

    obj_100 = count_objects(lv_screen_active());
    timer_100 = count_timers();
    printf("C3资源趋势：100次往返后对象=%u 定时器=%u\n",
           (unsigned)obj_100, (unsigned)timer_100);

    CHECK(obj_100 == obj_warm);
    CHECK(timer_100 == timer_warm);
}

/* C3-6：正常关闭 → 重新初始化 → 再显示 */
static void test_app_lifecycle(void)
{
    car_state_t st;
    lv_obj_t *page;
    lv_obj_t *stack;

    car_state_init(&st);
    ui_app_deinit();
    CHECK(ui_app_current_root() == NULL);
    CHECK(ui_app_current_style() == NULL);

    CHECK(ui_app_init());
    CHECK(ui_app_set_style("forester1"));
    page = ui_app_current_root();
    CHECK(page != NULL);
    page_push_view(&st);
    lv_obj_update_layout(page);
    stack = page_child(page, 553, 140, 814, 509);
    CHECK(stack != NULL);
    CHECK(!lv_obj_has_flag(stack, LV_OBJ_FLAG_HIDDEN));

    ui_app_deinit();
    CHECK(ui_app_current_root() == NULL);

    CHECK(ui_app_init());
    CHECK(ui_app_set_style("forester1"));
    page = ui_app_current_root();
    CHECK(page != NULL);
    page_push_view(&st);
    lv_obj_update_layout(page);
    stack = page_child(page, 553, 140, 814, 509);
    CHECK(stack != NULL);
    CHECK(!lv_obj_has_flag(stack, LV_OBJ_FLAG_HIDDEN));
}

static void test_cruise(lv_obj_t *parent)
{
    /* 0x0E 子帧：[0] 子帧号 [2] laneCenter [4] carState [5] showArea [6] showCar */
    const uint8_t lane[7] = {0x0E, 0, 1, 0, 1, 3, 1};
    const uint8_t drive[2] = {0x04, 2};
    const uint8_t clear[7] = {0x0E, 0, 0, 0, 0, 0, 0};
    const uint8_t lane_c0[7] = {0x0E, 0, 1, 0, 0, 0, 0};
    const uint8_t lane_c2[7] = {0x0E, 0, 1, 0, 2, 0, 0};
    const uint8_t lane_c3[7] = {0x0E, 0, 1, 0, 3, 0, 0};
    const uint8_t area_r[7] = {0x0E, 0, 0, 0, 1, 1, 0};
    const uint8_t area_l[7] = {0x0E, 0, 0, 0, 1, 2, 0};
    const uint8_t area_lr[7] = {0x0E, 0, 0, 0, 1, 3, 0};
    const uint8_t car_only[7] = {0x0E, 0, 0, 0, 0, 0, 1};
    /* Qt Style1 分支的跟车小车 y，索引 distanceShow 0..4 */
    const int32_t min_car_y[5] = {33, 61, 39, 33, 17};
    car_state_t st;
    ov_cruise_t ov = {0};

    car_state_init(&st);
    CHECK(ov_cruise_create(&ov, parent));
    CHECK(car_decode_high_func(lane, sizeof(lane), &st));
    ov_cruise_apply(&ov, &st, false);
    CHECK(ov_cruise_is_active(&ov));
    CHECK(!lv_obj_has_flag(ov.left_area, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(ov.right_area, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(ov.front_car, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(ov.max_car, LV_OBJ_FLAG_HIDDEN));
    CHECK(car_decode_high_func(drive, sizeof(drive), &st));
    ov_cruise_apply(&ov, &st, false);
    CHECK(ov_cruise_is_active(&ov));
    CHECK(!lv_obj_has_flag(ov.left_area, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(ov.right_area, LV_OBJ_FLAG_HIDDEN));
    ov_cruise_apply(&ov, &st, true);
    CHECK(!ov_cruise_is_active(&ov));
    ov_cruise_apply(&ov, &st, false);
    CHECK(ov_cruise_is_active(&ov));
    CHECK(car_decode_high_func(clear, sizeof(clear), &st));
    ov_cruise_apply(&ov, &st, false);
    CHECK(!ov_cruise_is_active(&ov));

    /* carState=0 且无盲区/前车：只有车道底图，其余全部隐藏 */
    CHECK(car_decode_high_func(lane_c0, sizeof(lane_c0), &st));
    ov_cruise_apply(&ov, &st, false);
    CHECK(ov_cruise_is_active(&ov));
    CHECK(!lv_obj_has_flag(ov.lane, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.left_area, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.right_area, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.front_car, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.max_car, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.min_car, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.lane_left, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.lane_right, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.off_left, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.off_right, LV_OBJ_FLAG_HIDDEN));
    for(uint8_t i = 0U; i < 4U; ++i) {
        CHECK(lv_obj_has_flag(ov.dist[i], LV_OBJ_FLAG_HIDDEN));
    }

    /* showArea 1/2/3 对应右/左/两侧盲区；carState=1 恢复居中大车 */
    CHECK(car_decode_high_func(area_r, sizeof(area_r), &st));
    ov_cruise_apply(&ov, &st, false);
    CHECK(!lv_obj_has_flag(ov.right_area, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.left_area, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(ov.max_car, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.lane_left, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.lane_right, LV_OBJ_FLAG_HIDDEN));

    CHECK(car_decode_high_func(area_l, sizeof(area_l), &st));
    ov_cruise_apply(&ov, &st, false);
    CHECK(lv_obj_has_flag(ov.right_area, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(ov.left_area, LV_OBJ_FLAG_HIDDEN));

    CHECK(car_decode_high_func(area_lr, sizeof(area_lr), &st));
    ov_cruise_apply(&ov, &st, false);
    CHECK(!lv_obj_has_flag(ov.right_area, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(ov.left_area, LV_OBJ_FLAG_HIDDEN));

    /* carState=2/3：左右偏移车道 + 偏移车辆，底图/居中大车/跟车小车全部隐藏 */
    CHECK(car_decode_high_func(lane_c2, sizeof(lane_c2), &st));
    ov_cruise_apply(&ov, &st, false);
    CHECK(lv_obj_has_flag(ov.lane, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(ov.lane_left, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(ov.off_left, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.lane_right, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.off_right, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.max_car, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.min_car, LV_OBJ_FLAG_HIDDEN));

    CHECK(car_decode_high_func(lane_c3, sizeof(lane_c3), &st));
    ov_cruise_apply(&ov, &st, false);
    CHECK(lv_obj_has_flag(ov.lane, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.lane_left, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.off_left, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(ov.lane_right, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(ov.off_right, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.max_car, LV_OBJ_FLAG_HIDDEN));

    /* showCar=1 + carState=0：前车与居中大车显示；距离 0~4 逐级点亮并切换跟车小车 y */
    CHECK(car_decode_high_func(car_only, sizeof(car_only), &st));
    for(uint8_t d = 0U; d <= 4U; ++d) {
        st.cruise.valid = true;
        st.cruise.cruise_state = 1U;                     /* controlLed */
        st.cruise.acc_state = (uint8_t)(0x08U | d);      /* bit3 前车 + 跟车距离 */
        st.cruise.set_speed_kph = 100U;
        ov_cruise_apply(&ov, &st, false);
        lv_obj_update_layout(ov.root);
        CHECK(ov_cruise_is_active(&ov));
        CHECK(!lv_obj_has_flag(ov.front_car, LV_OBJ_FLAG_HIDDEN));
        CHECK(!lv_obj_has_flag(ov.max_car, LV_OBJ_FLAG_HIDDEN));
        CHECK(!lv_obj_has_flag(ov.min_car, LV_OBJ_FLAG_HIDDEN));
        CHECK(lv_obj_get_y(ov.min_car) == min_car_y[d]);
        for(uint8_t i = 0U; i < 4U; ++i) {
            if(d >= (uint8_t)(i + 1U)) {
                CHECK(!lv_obj_has_flag(ov.dist[i], LV_OBJ_FLAG_HIDDEN));
            }
            else {
                CHECK(lv_obj_has_flag(ov.dist[i], LV_OBJ_FLAG_HIDDEN));
            }
        }
    }

    /* carState=2 时距离即使为 4 也不点亮，跟车小车同样隐藏 */
    CHECK(car_decode_high_func(lane_c2, sizeof(lane_c2), &st));
    st.cruise.acc_state = 0x0CU;                         /* 前车 + 距离 4 */
    ov_cruise_apply(&ov, &st, false);
    CHECK(!lv_obj_has_flag(ov.lane_left, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.min_car, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.max_car, LV_OBJ_FLAG_HIDDEN));
    for(uint8_t i = 0U; i < 4U; ++i) {
        CHECK(lv_obj_has_flag(ov.dist[i], LV_OBJ_FLAG_HIDDEN));
    }

    /* controlLed 单独点亮即可显示；熄灭且无其他条件时整层隐藏 */
    CHECK(car_decode_high_func(clear, sizeof(clear), &st));
    st.cruise.cruise_state = 1U;
    st.cruise.acc_state = 0U;
    ov_cruise_apply(&ov, &st, false);
    CHECK(ov_cruise_is_active(&ov));
    CHECK(!lv_obj_has_flag(ov.lane, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.left_area, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.right_area, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.front_car, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.max_car, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.min_car, LV_OBJ_FLAG_HIDDEN));

    st.cruise.cruise_state = 0U;
    ov_cruise_apply(&ov, &st, false);
    CHECK(!ov_cruise_is_active(&ov));
    CHECK(lv_obj_has_flag(ov.root, LV_OBJ_FLAG_HIDDEN));

    ov_cruise_destroy(&ov);
}

/*
 * 14.8：驾驶辅助三项门控（Qt PageNavigationInfo 的 blindSpotWarning /
 * emergencyBraking / laneDepartureWarning）。状态来自 0x16 回读，缺省全开。
 */
static void test_driver_assist_gates(lv_obj_t *parent)
{
    const uint8_t area_lr[7] = {0x0E, 0, 0, 0, 1, 3, 0};
    const uint8_t lane_c2[7] = {0x0E, 0, 1, 0, 2, 0, 0};
    const uint8_t car_only[7] = {0x0E, 0, 0, 0, 0, 0, 1};
    const uint8_t lane_c0[7] = {0x0E, 0, 1, 0, 0, 0, 0};
    car_state_t st;
    ov_cruise_t ov = {0};

    car_state_init(&st);
    CHECK(ov_cruise_create(&ov, parent));
    CHECK(st.driver_assist.emergency_braking);
    CHECK(st.driver_assist.lane_departure_warning);
    CHECK(st.driver_assist.blind_spot_warning);

    /* 盲区预警关闭：两侧盲区都不显示，居中大车（carState=1）不受影响 */
    st.driver_assist.blind_spot_warning = false;
    CHECK(car_decode_high_func(area_lr, sizeof(area_lr), &st));
    ov_cruise_apply(&ov, &st, false);
    CHECK(ov_cruise_is_active(&ov));
    CHECK(lv_obj_has_flag(ov.left_area, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.right_area, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(ov.max_car, LV_OBJ_FLAG_HIDDEN));

    /* 紧急制动关闭：showCar=1 时前车隐藏，居中大车仍显示 */
    st.driver_assist.emergency_braking = false;
    CHECK(car_decode_high_func(car_only, sizeof(car_only), &st));
    ov_cruise_apply(&ov, &st, false);
    CHECK(lv_obj_has_flag(ov.front_car, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(ov.max_car, LV_OBJ_FLAG_HIDDEN));

    /* 车道偏离关闭：carState=2 时偏移轨道与车辆隐藏 */
    st.driver_assist.lane_departure_warning = false;
    CHECK(car_decode_high_func(lane_c2, sizeof(lane_c2), &st));
    ov_cruise_apply(&ov, &st, false);
    CHECK(lv_obj_has_flag(ov.lane_left, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.off_left, LV_OBJ_FLAG_HIDDEN));

    /* 三项全关：即使 controlLed 点亮，整层也不激活 */
    st.cruise.valid = true;
    st.cruise.cruise_state = 1U;
    st.cruise.acc_state = 0U;
    CHECK(car_decode_high_func(lane_c0, sizeof(lane_c0), &st));
    ov_cruise_apply(&ov, &st, false);
    CHECK(!ov_cruise_is_active(&ov));
    CHECK(lv_obj_has_flag(ov.root, LV_OBJ_FLAG_HIDDEN));

    ov_cruise_destroy(&ov);
}

/* LVGL 内部把旋转角归一化到 0~3600，这里还原成带符号的 0.1° 便于断言 */
static int32_t needle_rot(lv_obj_t *needle)
{
    int32_t rot = lv_image_get_rotation(needle);

    return rot > 1800 ? rot - 3600 : rot;
}

/*
 * 车速表公英制闭环：刻度文本、高亮序号、单位标签、指针角度都来自同一份
 * 单位状态；数值不变只切单位时必须全部刷新。
 * 预期值按 Qt ForesterSpeed1.qml 的公式独立计算，不用被测函数反推。
 */
static void test_speed_units(lv_obj_t *parent)
{
    speed_forester1_t sp = {0};
    const lv_font_t *f5;

    CHECK(speed_forester1_create(&sp, parent, 0, 0));

    /* 公制 100 km/h：96 < 100 < 106 → 刻度 5；角度 -195 + 75 = -120 */
    speed_forester1_apply(&sp, 100U, false);
    CHECK(sp.highlight == 5);
    CHECK(strcmp(lv_label_get_text(sp.labels[5]), "100") == 0);
    f5 = lv_obj_get_style_text_font(sp.labels[5], 0);
    CHECK(f5 == UI_FONT_LG);
    CHECK(lv_obj_get_style_text_font(sp.labels[0], 0) == UI_FONT_MD);
    CHECK(strcmp(lv_label_get_text(sp.unit), "km/h") == 0);
    lv_tick_inc(100);
    lv_anim_refr_now();
    CHECK(needle_rot(sp.dial.needle) == -1200);

    /* 数值不变只切英里制：scale = 100 × 0.6214 = 62.14 → 刻度 3 */
    speed_forester1_apply(&sp, 100U, true);
    CHECK(sp.highlight == 3);
    CHECK(strcmp(lv_label_get_text(sp.labels[3]), "60") == 0);
    CHECK(strcmp(lv_label_get_text(sp.labels[5]), "90") == 0);
    CHECK(strcmp(lv_label_get_text(sp.unit), "mph") == 0);
    /* -195 + 100 × 0.6214 × 0.75 = -148.4 → -1484（0.1° 单位） */
    lv_tick_inc(100);
    lv_anim_refr_now();
    CHECK(needle_rot(sp.dial.needle) == -1484);

    /* 切回公制，文本与角度都要回到公制状态 */
    speed_forester1_apply(&sp, 100U, false);
    CHECK(sp.highlight == 5);
    CHECK(strcmp(lv_label_get_text(sp.labels[5]), "100") == 0);
    CHECK(strcmp(lv_label_get_text(sp.unit), "km/h") == 0);
    lv_tick_inc(100);
    lv_anim_refr_now();
    CHECK(needle_rot(sp.dial.needle) == -1200);

    /* 高亮序号不变但单位变化：speed=3 公制/英里都命中刻度 0，
     * 缓存必须按单位刷新，第 5 个刻度的文本从 100 变成 90 */
    speed_forester1_apply(&sp, 3U, false);
    CHECK(sp.highlight == 0);
    CHECK(strcmp(lv_label_get_text(sp.labels[5]), "100") == 0);
    speed_forester1_apply(&sp, 3U, true);
    CHECK(sp.highlight == 0);
    CHECK(strcmp(lv_label_get_text(sp.labels[5]), "90") == 0);
    CHECK(strcmp(lv_label_get_text(sp.unit), "mph") == 0);

    /* 阈值边界照抄 Qt 的严格不等式，区间之间的空档不高亮 */
    speed_forester1_apply(&sp, 5U, false);
    CHECK(sp.highlight == 0);
    speed_forester1_apply(&sp, 6U, false);
    CHECK(sp.highlight == -1);
    speed_forester1_apply(&sp, 25U, false);
    CHECK(sp.highlight == 1);
    speed_forester1_apply(&sp, 26U, false);
    CHECK(sp.highlight == -1);
    /* 250 km/h 落在 Qt 的 246~276 空档；276 不满足 > 276，277 命中末档 */
    speed_forester1_apply(&sp, 250U, false);
    CHECK(sp.highlight == -1);
    speed_forester1_apply(&sp, 276U, false);
    CHECK(sp.highlight == -1);
    speed_forester1_apply(&sp, 277U, false);
    CHECK(sp.highlight == 13);
    CHECK(strcmp(lv_label_get_text(sp.labels[13]), "280") == 0);

    /* 英里制 >129 的分段：-135.3456 + (130-129) × 0.9321 = -134.4 */
    speed_forester1_apply(&sp, 130U, true);
    CHECK(sp.highlight == 4);
    CHECK(strcmp(lv_label_get_text(sp.labels[4]), "80") == 0);
    lv_tick_inc(100);
    lv_anim_refr_now();
    CHECK(needle_rot(sp.dial.needle) == -1344);

    lv_obj_delete(sp.dial.root);
}

/* 续航：0xFFFF 是 Qt 的不可用占位，任何单位下都不能换算成巨大数值 */
static void test_range_units(lv_obj_t *parent)
{
    ui_range_t rg = {0};

    CHECK(ui_range_create(&rg, parent, 0, 0, UI_FONT_MD));

    ui_range_apply(&rg, 100U, false);
    CHECK(strcmp(lv_label_get_text(rg.value), "100 KM") == 0);

    /* 数值不变只切单位，必须刷新 */
    ui_range_apply(&rg, 100U, true);
    CHECK(strcmp(lv_label_get_text(rg.value), "62.1 MI") == 0);

    ui_range_apply(&rg, 1U, true);
    CHECK(strcmp(lv_label_get_text(rg.value), "0.6 MI") == 0);

    ui_range_apply(&rg, 0U, false);
    CHECK(strcmp(lv_label_get_text(rg.value), "0 KM") == 0);

    ui_range_apply(&rg, 65535U, false);
    CHECK(strcmp(lv_label_get_text(rg.value), "- - KM") == 0);
    ui_range_apply(&rg, 65535U, true);
    CHECK(strcmp(lv_label_get_text(rg.value), "- - MI") == 0);

    lv_obj_delete(rg.root);
}

/* 油量阈值：Qt 为 fuelPercentage > 25 ? 白 : 红，25% 本身必须是红色 */
static void test_fuel_threshold(lv_obj_t *parent)
{
    fuel_forester1_t f = {0};

    CHECK(fuel_forester1_create(&f, parent, 0, 0));

    fuel_forester1_apply(&f, 26U);
    CHECK(!f.low);
    CHECK(strstr((const char *)lv_image_get_src(f.tick.img),
                 "fuel1_white_tick.bin") != NULL);

    fuel_forester1_apply(&f, 25U);
    CHECK(f.low);
    CHECK(strstr((const char *)lv_image_get_src(f.tick.img),
                 "fuel1_white_tick.bin") == NULL);

    /* 缓存：仍为红色时不换图，但液面数值继续更新 */
    fuel_forester1_apply(&f, 24U);
    CHECK(f.low);
    CHECK(f.tick.percent == 24);

    fuel_forester1_apply(&f, 26U);
    CHECK(!f.low);

    fuel_forester1_apply(&f, 25U);
    CHECK(f.low);

    lv_obj_delete(f.root);
}

/*
 * 短期油耗：单位选择与数值有效性分开。
 * 预期值按 Qt PageShortTerm.qml 的公式独立计算：
 *   L/100km = avg/10；km/L = 1000/(avg/10)；mpg = 2824.81/(avg/10)；
 *   avg <= 0 → "0"；avg > 1200 → "--.-"；特殊值不改写单位标签。
 */
static void test_short_term(lv_obj_t *parent)
{
    short_term_card_t card = {0};
    car_state_t st;

    CHECK(short_term_card_create(&card, parent));

    car_state_init(&st);
    st.trip.valid = true;
    st.trip.distance_km = 100U;
    st.trip.avg_speed_kph = 50U;
    st.trip.drive_minutes = 90U;
    st.units.valid = true;
    st.units.fuel_unit = 0U;

    /* avg=0：值固定 "0"，单位仍然按 fuelType 显示 */
    st.trip.avg_fuel_x10 = 0U;
    short_term_card_apply(&card, &st);
    CHECK(strcmp(lv_label_get_text(card.value[3]), "0") == 0);
    CHECK(strcmp(lv_label_get_text(card.fuel_unit), "L/100km") == 0);

    st.units.fuel_unit = 1U;
    short_term_card_apply(&card, &st);
    CHECK(strcmp(lv_label_get_text(card.value[3]), "0") == 0);
    CHECK(strcmp(lv_label_get_text(card.fuel_unit), "km/L") == 0);

    st.units.fuel_unit = 2U;
    short_term_card_apply(&card, &st);
    CHECK(strcmp(lv_label_get_text(card.value[3]), "0") == 0);
    CHECK(strcmp(lv_label_get_text(card.fuel_unit), "mpg") == 0);

    /* avg=1（0.1 L/100km）：倒数单位不做除零，直接按公式换算 */
    st.trip.avg_fuel_x10 = 1U;
    st.units.fuel_unit = 0U;
    short_term_card_apply(&card, &st);
    CHECK(strcmp(lv_label_get_text(card.value[3]), "0.1") == 0);

    st.units.fuel_unit = 1U;
    short_term_card_apply(&card, &st);
    CHECK(strcmp(lv_label_get_text(card.value[3]), "1000.0") == 0);

    st.units.fuel_unit = 2U;
    short_term_card_apply(&card, &st);
    CHECK(strcmp(lv_label_get_text(card.value[3]), "2824.8") == 0);

    /* avg=1200（120.0 L/100km）仍在有效区间 */
    st.trip.avg_fuel_x10 = 1200U;
    st.units.fuel_unit = 0U;
    short_term_card_apply(&card, &st);
    CHECK(strcmp(lv_label_get_text(card.value[3]), "120.0") == 0);
    CHECK(strcmp(lv_label_get_text(card.fuel_unit), "L/100km") == 0);

    st.units.fuel_unit = 1U;
    short_term_card_apply(&card, &st);
    CHECK(strcmp(lv_label_get_text(card.value[3]), "0.8") == 0);

    st.units.fuel_unit = 2U;
    short_term_card_apply(&card, &st);
    CHECK(strcmp(lv_label_get_text(card.value[3]), "2.4") == 0);

    /* avg=1201：超过阈值显示 "--.-"，单位标签保持不变 */
    st.trip.avg_fuel_x10 = 1201U;
    short_term_card_apply(&card, &st);
    CHECK(strcmp(lv_label_get_text(card.value[3]), "--.-") == 0);
    CHECK(strcmp(lv_label_get_text(card.fuel_unit), "mpg") == 0);

    st.units.fuel_unit = 1U;
    short_term_card_apply(&card, &st);
    CHECK(strcmp(lv_label_get_text(card.value[3]), "--.-") == 0);
    CHECK(strcmp(lv_label_get_text(card.fuel_unit), "km/L") == 0);

    st.units.fuel_unit = 0U;
    short_term_card_apply(&card, &st);
    CHECK(strcmp(lv_label_get_text(card.value[3]), "--.-") == 0);
    CHECK(strcmp(lv_label_get_text(card.fuel_unit), "L/100km") == 0);

    /* 单位无效：按公制默认，不能凭未初始化内容切换 */
    st.trip.avg_fuel_x10 = 1200U;
    st.units.valid = false;
    st.units.fuel_unit = 2U;
    st.units.mile = true;
    short_term_card_apply(&card, &st);
    CHECK(strcmp(lv_label_get_text(card.value[3]), "120.0") == 0);
    CHECK(strcmp(lv_label_get_text(card.fuel_unit), "L/100km") == 0);
    CHECK(strcmp(lv_label_get_text(card.value[0]), "100 km") == 0);
    CHECK(strcmp(lv_label_get_text(card.value[1]), "50 km/h") == 0);

    short_term_card_destroy(&card);
}

/*
 * 通用警告：0x1C 事件进入 ov_tips 的 FIFO 队列，
 * Qt showBriefly 未传 durationS，因此每条固定播放 2s。
 */
static void test_tips_warn(lv_obj_t *parent)
{
    car_state_t st;
    ov_tips_t ov = {0};
    uint32_t dropped_before;
    const uint8_t ev23[4] = {0x1CU, 0x00U, 23U, 1U};
    const uint8_t ev27[4] = {0x1CU, 0x00U, 27U, 3U};
    const uint8_t lane[7] = {0x0EU, 0, 1, 0, 1, 3, 1};
    const uint8_t popup23[4] = {0x00U, 23U, 0x02U, 1U};

    car_state_init(&st);
    CHECK(ov_tips_create(&ov, parent));

    /* 界面创建前已经发生的事件不重放 */
    CHECK(car_decode_high_func(ev23, 4, &st));
    ov_tips_apply(&ov, &st, false);
    CHECK(!ov.active);
    CHECK(ov.queue_count == 0U);
    CHECK(lv_obj_has_flag(ov.general, LV_OBJ_FLAG_HIDDEN));

    /* 其他 0x16 子帧插入不会吞掉警告事件 */
    CHECK(car_decode_high_func(lane, sizeof(lane), &st));
    CHECK(car_decode_high_func(ev23, 4, &st));
    ov_tips_apply(&ov, &st, false);
    CHECK(ov.active);
    CHECK(strcmp(lv_label_get_text(ov.gen_text), "Low fuel") == 0);
    CHECK(!lv_obj_has_flag(ov.gen_text, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(ov.gen_aux, LV_OBJ_FLAG_HIDDEN));

    /* 同一快照重复 apply 不能重播 */
    ov_tips_apply(&ov, &st, false);
    CHECK(ov.active);
    CHECK(ov.queue_count == 0U);

    /* 同 ID 连续两次真实事件：两条都入队 */
    CHECK(car_decode_high_func(ev23, 4, &st));
    CHECK(car_decode_high_func(ev23, 4, &st));
    ov_tips_apply(&ov, &st, false);
    CHECK(ov.queue_count == 2U); /* 当前播放中，两条在等 */

    /* 同一快照重复 apply 不重复入队 */
    ov_tips_apply(&ov, &st, false);
    CHECK(ov.queue_count == 2U);

    lv_tick_inc(1999);
    ov_tips_apply(&ov, &st, false);
    CHECK(ov.active);
    CHECK(ov.queue_count == 2U);

    lv_tick_inc(1);
    ov_tips_apply(&ov, &st, false);
    CHECK(ov.active);          /* 第一条到期，第二条开始播放 */
    CHECK(ov.queue_count == 1U);

    lv_tick_inc(2000);
    ov_tips_apply(&ov, &st, false);
    CHECK(ov.active);
    CHECK(ov.queue_count == 0U);

    lv_tick_inc(2000);
    ov_tips_apply(&ov, &st, false);
    CHECK(!ov.active);
    CHECK(lv_obj_has_flag(ov.general, LV_OBJ_FLAG_HIDDEN));

    /* id 24/25/26：正文隐藏，辅助红字 FAULT 显示 */
    car_state_warn_event_push(&st, 24U);
    car_state_warn_event_push(&st, 25U);
    ov_tips_apply(&ov, &st, false);
    CHECK(ov.active);
    CHECK(lv_obj_has_flag(ov.gen_text, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(ov.gen_aux, LV_OBJ_FLAG_HIDDEN));
    CHECK(ov.queue_count == 1U);

    lv_tick_inc(2000);
    ov_tips_apply(&ov, &st, false);
    CHECK(ov.active);
    CHECK(lv_obj_has_flag(ov.gen_text, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(ov.gen_aux, LV_OBJ_FLAG_HIDDEN));
    CHECK(ov.queue_count == 0U);

    lv_tick_inc(2000);
    ov_tips_apply(&ov, &st, false);
    CHECK(!ov.active);

    /* 播放队列容量 4：一次到达 6 条时挤掉最旧的 2 条 */
    dropped_before = ov.queue_dropped;
    car_state_warn_event_push(&st, 31U);
    car_state_warn_event_push(&st, 33U);
    car_state_warn_event_push(&st, 34U);
    car_state_warn_event_push(&st, 35U);
    car_state_warn_event_push(&st, 36U);
    car_state_warn_event_push(&st, 37U);
    ov_tips_apply(&ov, &st, false);
    CHECK(ov.queue_dropped == dropped_before + 2U);
    CHECK(strcmp(lv_label_get_text(ov.gen_text), "Lane blind spot fault") == 0);

    /* 播放剩余 3 条，再等最后一条到期 */
    lv_tick_inc(2000);
    ov_tips_apply(&ov, &st, false);
    lv_tick_inc(2000);
    ov_tips_apply(&ov, &st, false);
    lv_tick_inc(2000);
    ov_tips_apply(&ov, &st, false);
    lv_tick_inc(2000);
    ov_tips_apply(&ov, &st, false);
    CHECK(!ov.active);
    CHECK(ov.queue_count == 0U);

    /* 0x1C 与 0x03 交错：同一快照内先补 0x1C，再判定 0x03 新帧 */
    CHECK(car_decode_high_func(ev27, 4, &st));
    CHECK(car_decode_popup(popup23, 4, &st));
    ov_tips_apply(&ov, &st, false);
    CHECK(ov.active);
    CHECK(strcmp(lv_label_get_text(ov.gen_title), "PARK/START") == 0);
    CHECK(strcmp(lv_label_get_text(ov.gen_text), "Start") == 0);
    CHECK(ov.queue_count == 1U);

    /* 0x03 新帧的判定依赖最近一次帧时间，步进必须小于 1s */
    lv_tick_inc(400);
    ov_tips_apply(&ov, &st, false);
    lv_tick_inc(400);
    ov_tips_apply(&ov, &st, false);
    lv_tick_inc(400);
    ov_tips_apply(&ov, &st, false);
    lv_tick_inc(400);
    ov_tips_apply(&ov, &st, false);
    lv_tick_inc(400);
    ov_tips_apply(&ov, &st, false);
    CHECK(ov.active);          /* 0x03 条目开始播放，duration_s=1 */
    CHECK(strcmp(lv_label_get_text(ov.gen_text), "Low fuel") == 0);
    CHECK(ov.queue_count == 0U);

    lv_tick_inc(500);
    ov_tips_apply(&ov, &st, false);
    CHECK(ov.active);
    lv_tick_inc(500);
    ov_tips_apply(&ov, &st, false);
    CHECK(!ov.active);
    CHECK(lv_obj_has_flag(ov.general, LV_OBJ_FLAG_HIDDEN));

    /* id 32 受 SettingsMgr.show4h 门控：关闭时不入队，开启后正常播放 */
    st.popup.valid = false; /* 断开 0x03 持续帧，避免计时推进时补发弹窗 */
    st.show_4h = false;
    car_state_warn_event_push(&st, 32U);
    ov_tips_apply(&ov, &st, false);
    CHECK(!ov.active);
    CHECK(ov.queue_count == 0U);
    CHECK(lv_obj_has_flag(ov.general, LV_OBJ_FLAG_HIDDEN));

    st.show_4h = true;
    car_state_warn_event_push(&st, 32U);
    ov_tips_apply(&ov, &st, false);
    CHECK(ov.active);
    CHECK(!lv_obj_has_flag(ov.general, LV_OBJ_FLAG_HIDDEN));

    lv_tick_inc(2000);
    ov_tips_apply(&ov, &st, false);
    CHECK(!ov.active);

    /* 设置页/媒体页遮挡：只隐藏通用提示面板，播放计时照常推进 */
    st.show_4h = true;
    car_state_warn_event_push(&st, 32U);
    ov_tips_set_obstructed(&ov, true);
    ov_tips_apply(&ov, &st, false);
    CHECK(ov.active);
    CHECK(lv_obj_has_flag(ov.general, LV_OBJ_FLAG_HIDDEN));

    ov_tips_set_obstructed(&ov, false);
    ov_tips_apply(&ov, &st, false);
    CHECK(ov.active);
    CHECK(!lv_obj_has_flag(ov.general, LV_OBJ_FLAG_HIDDEN));

    lv_tick_inc(2000);
    ov_tips_apply(&ov, &st, false);
    CHECK(!ov.active);

    ov_tips_destroy(&ov);
}

/*
 * 端到端：编码后的 0x16/0x1C 真帧经内存端口 -> parser -> registry -> decode
 * -> 状态快照 -> ov_tips，确认警告能从真实解析链进入 UI。
 */
static void test_warn_end_to_end(lv_obj_t *parent)
{
    car_port_memory_t mem;
    car_port_t port;
    car_link_t link;
    car_state_view_t view;
    ov_tips_t ov = {0};
    uint8_t frame[64];
    const uint8_t payload[4] = {0x1CU, 0x00U, 23U, 5U};
    size_t flen;

    car_port_memory_attach(&port, &mem);
    CHECK(car_link_init(&link, &port));
    CHECK(ov_tips_create(&ov, parent));

    car_link_pump(&link, 0); /* 完成握手 */

    /* 首个快照只记录进度，不重放界面创建前的事件 */
    car_port_memory_advance(&mem, 100);
    flen = car_frame_encode(0x16, (uint8_t)CAR_OP_STATUS, payload, 4,
                            frame, sizeof(frame));
    CHECK(flen > 0U);
    car_port_memory_push(&mem, frame, flen);
    car_link_pump(&link, 0);
    car_link_view(&link, &view);
    CHECK(view.state.warn_events.count == 1U);
    ov_tips_apply(&ov, &view.state, false);
    CHECK(!ov.active);

    /* 再来一条同 ID 真帧：进入队列并立即显示 2s */
    car_port_memory_advance(&mem, 100);
    flen = car_frame_encode(0x16, (uint8_t)CAR_OP_STATUS, payload, 4,
                            frame, sizeof(frame));
    car_port_memory_push(&mem, frame, flen);
    car_link_pump(&link, 0);
    car_link_view(&link, &view);
    ov_tips_apply(&ov, &view.state, false);
    CHECK(ov.active);
    CHECK(strcmp(lv_label_get_text(ov.gen_text), "Low fuel") == 0);

    lv_tick_inc(2000);
    ov_tips_apply(&ov, &view.state, false);
    CHECK(!ov.active);
    CHECK(lv_obj_has_flag(ov.general, LV_OBJ_FLAG_HIDDEN));

    ov_tips_destroy(&ov);
    car_link_deinit(&link);
}

int main(void)
{
    lv_init();
    /* 无窗口、无硬件的对象状态测试，不执行渲染。 */
    lv_display_t *display = lv_display_create(1920, 720);
    if(display == NULL) {
        return 1;
    }
    /* 不渲染像素：去掉刷新定时器，lv_timer_handler 只驱动被测闪烁定时器 */
    lv_display_delete_refr_timer(display);
    test_tire(lv_screen_active());
    test_lamp_states(lv_screen_active());
    test_lamp_4h_gate(lv_screen_active());
    test_lamp_blink(lv_screen_active());
    test_cruise(lv_screen_active());
    test_driver_assist_gates(lv_screen_active());
    test_speed_units(lv_screen_active());
    test_range_units(lv_screen_active());
    test_fuel_threshold(lv_screen_active());
    test_short_term(lv_screen_active());
    test_tips_warn(lv_screen_active());
    test_warn_end_to_end(lv_screen_active());
    test_tire_style_switch();
    test_system_version_settings();
    test_overlay_composition();
    test_card_cycle();
    test_state_restore();
    test_style_cycles();
    test_app_lifecycle();
    ui_app_deinit();
    lv_display_delete(display);
    lv_deinit();
    printf("覆盖层回归：%d 项检查，%d 项失败\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
