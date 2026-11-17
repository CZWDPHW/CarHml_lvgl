/*
 * ui_settings_page.c - 设置菜单覆盖层
 *
 * 设置状态机与显示分别对照 Qt：
 *   qml/main.qml            settingStackView 594×504，x=663，y=120；
 *                           8 秒无按键自动关闭（reSettings），由 ui_app 计时。
 *   PageMainSettings.qml    5 个分类图标固定横排，选中换图并显示红条，
 *                           selectedIndex 初值 -1，第一次 OK 只是进入分类。
 *   PageMyCarItem.qml       第一次 OK 锁定列表（isOk），之后 OK 才执行子项。
 *   PageUnitSettings.qml    里程/温度按 OK 循环切换，确定写回并返回。
 *   PageCenterCards.qml     OK 切换勾选，返回项弹出子页。
 *
 * 设置标签由 ui_settings_language 提供中英切换（中文缺字时逐字回退并收缩行宽）。
 */
/* 板端用 clock_settime 同步系统时间，需 POSIX 2008 声明 */
#if defined(CARMW_PLATFORM_BOARD) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "ui/settings/ui_settings_page.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ui/ui_app.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"
#include "ui_settings_language.h"

/* Qt ResourceLib.settings.fontSize = 26. */
#undef UI_FONT_LG
#define UI_FONT_LG UI_TEXT(26)
#include "carmw/car_log_cleanup.h"

static ui_settings_cleanup_start_cb_t s_cleanup_start;
static ui_settings_cleanup_status_cb_t s_cleanup_status;
static bool s_cleanup_failed;

void ui_settings_page_set_cleanup_cb(ui_settings_cleanup_start_cb_t start,
                                     ui_settings_cleanup_status_cb_t status)
{
    s_cleanup_start = start;
    s_cleanup_status = status;
    s_cleanup_failed = false;
}

static unsigned cleanup_status(void)
{
    if(s_cleanup_failed) return CAR_LOG_CLEANUP_FAILED;
    return s_cleanup_start && s_cleanup_status ? s_cleanup_status() : CAR_LOG_CLEANUP_UNAVAILABLE;
}

#define SETTINGS_W 594
#define SETTINGS_H 504
#define SETTINGS_X ((UI_DESIGN_W - SETTINGS_W) / 2)
#define SETTINGS_Y 120

#define ROOT_BAR_Y 80  /* Qt 图标父容器 y=80，图标局部 y=10 */
#define ROOT_BAR_H 80
#define BODY_Y 160
#define BODY_H (SETTINGS_H - BODY_Y)
#define SUBPAGE_BODY_Y 140
#define LIST_VIEW_H 300

#define LIST_X 97      /* 子列表水平居中：(594 - 400) / 2 */
#define LIST_W 400
#define ROW_H 60
#define ROW_OFFSET_X 24
#define ROW_TEXT_PAD 10

#define TITLE_X 50     /* Qt CentralTemplate：标题宽度为 parent.width - 100 */
#define TITLE_W (SETTINGS_W - 2 * TITLE_X)
#define TITLE_Y 76

#define STYLE_ROW_H 48
#define STYLE_ROW_W 200
#define STYLE_ROW_X ((SETTINGS_W - STYLE_ROW_W) / 2)
#define UNIT_ROW_H 58
#define CARD_ROW_H 55
#define CARD_LIST_W 450
#define CARD_LIST_X ((SETTINGS_W - CARD_LIST_W) / 2)
#define CARD_BOX 24

#define COLOR_YELLOW 0xFFF59A
#define COLOR_GRAY 0x848484

/* 日期时间滚轮（Qt PageDateSettings instrumentMode）：524 宽内容区 */
#define DATE_WHEEL_W 524
#define DATE_WHEEL_X ((SETTINGS_W - DATE_WHEEL_W) / 2)
#define DATE_WHEEL_H 110
#define DATE_WHEEL_CELL_H (DATE_WHEEL_H / 3)
#define DATE_WHEEL_BOX_W 466
#define DATE_WHEEL_BOX_H 110
#define DATE_WHEEL_GROUP_Y 16 /* 滚轮条在 body 内的起点 */
#define DATE_WHEEL_CONFIRM_W 76
#define DATE_WHEEL_COL_X { 0, 192, 256, 320, 406 } /* 年 184 + 8 + 60 + 4 + 60 + 12 + 74 + 8 + 74 */
#define DATE_WHEEL_COL_W { 184, 60, 60, 74, 74 }
#define DATE_WHEEL_ROW_H 58
#define DATE_REF_GRAY 0x555555 /* 滚轮上下两行：非当前值的暗色 */

#define NONE 0xFFU

typedef struct {
    const char *label;
    uint8_t value; /* 设置值序号，写回时使用 */
} settings_option_t;

/* 分类图标：[0] 普通，[1] 选中（Qt *_img0 蓝色版） */
static const char *const s_root_icons[UI_SETTINGS_ROOT_COUNT][2] = {
    {UI_IMG("settings/menu_itineray.png"), UI_IMG("settings/menu_itineray0.png")},
    {UI_IMG("settings/menu_infromation.png"), UI_IMG("settings/menu_infromation0.png")},
    {UI_IMG("settings/menu_mycar.png"), UI_IMG("settings/menu_mycar0.png")},
    {UI_IMG("settings/menu_setting.png"), UI_IMG("settings/menu_setting0.png")},
    {UI_IMG("settings/connect.png"), UI_IMG("settings/connect0.png")},
};

static const char *const s_system_labels[UI_SETTINGS_SYSTEM_COUNT] = {
    "LANGUAGE", "DATE & TIME", "VERSION INFO", "FACTORY SETTINGS",
    "CLEAR CACHE", "BOOT ANIMATION", "BACK",
};

static const char *const s_mycar_labels[UI_SETTINGS_MYCAR_COUNT] = {
    "DIAL STYLE", "UNITS", "CENTER CARDS", "GENERAL", "ECO", "ECO HISTORY",
    "BACK",
};

/* 我的座驾子项现在全部可用（Qt enable 全为 true） */
static const bool s_mycar_enabled[UI_SETTINGS_MYCAR_COUNT] = {
    true, true, true, true, true, true, true,
};

/* 单位页：里程 km/mile、温度 °C/°F（Qt 另外的油耗/压力暂无存储，未迁移） */
static const settings_option_t s_mileage_options[] = {
    {"KM", (uint8_t)CAR_UNIT_METRIC},
    {"MILE", (uint8_t)CAR_UNIT_IMPERIAL},
};

static const settings_option_t s_temp_options[] = {
    {"C", (uint8_t)CAR_UNIT_METRIC},
    {"F", (uint8_t)CAR_UNIT_IMPERIAL},
};

#define MILEAGE_COUNT (uint8_t)(sizeof(s_mileage_options) / sizeof(s_mileage_options[0]))
#define TEMP_COUNT (uint8_t)(sizeof(s_temp_options) / sizeof(s_temp_options[0]))

/* 单位页行序：0 里程 / 1 温度 / 2 确定 */
#define UNIT_ROW_MILEAGE 0U
#define UNIT_ROW_TEMP 1U
#define UNIT_ROW_CONFIRM 2U
#define UNIT_ROW_COUNT 3U

/*
 * 仪表风格：只列当前车型已迁移的可用样式（Qt PageMyCarStyle 按 currentCarId
 * 生成列表）。最多显示 8 条，Forester 当前只有 Style1。
 */
#define STYLE_MAX 8U

/* 中心卡片：下标与 s1_page_t 的注册顺序一致，值是 car_settings 的位 */
static const char *const s_card_labels[UI_SETTINGS_CARD_COUNT] = {
    "NAV", "4X4-I", "TIRE", "CHASSIS", "ITINERARY", "SHORT TERM", "FUEL",
    "ECO", "MUSIC", "CONNECT",
};

static const uint32_t s_card_bits[UI_SETTINGS_CARD_COUNT] = {
    UINT32_C(1) << 0, UINT32_C(1) << 1, UINT32_C(1) << 2, UINT32_C(1) << 3,
    UINT32_C(1) << 4, UINT32_C(1) << 5, UINT32_C(1) << 6, UINT32_C(1) << 7,
    UINT32_C(1) << 8, UINT32_C(1) << 9,
};

/* 返回行在列表中的下标：当前车型风格项数 + 1 */
static uint8_t style_back(void)
{
    size_t count = ui_app_style_option_count();

    if(count > STYLE_MAX) {
        count = STYLE_MAX;
    }
    return (uint8_t)count;
}
#define CARD_BACK UI_SETTINGS_CARD_COUNT

/* 信息选项：Qt 根分类里只有“行车信息”已迁移 */
static const char *const s_info_labels[UI_SETTINGS_INFO_COUNT] = {
    "DRIVING INFO", "BACK",
};

/* 行车信息：行序与 Qt PageSettingDrivingInfo.leftModel 一致 */
static const char *const s_drv_labels[UI_SETTINGS_DRV_COUNT] = {
    "OUTSIDE TEMP ", "GEAR ", "AVG FUEL ", "BACK",
};

/* 常规设置：行序与 Qt PageGeneralSettings.itemIndex 一致 */
static const char *const s_routine_labels[UI_SETTINGS_ROUTINE_COUNT] = {
    "DAY LIGHT ", "NIGHT LIGHT ", "SPEED LIMIT ", "SPEED WARN ", "BACK",
};

/* ECO 设置：Qt EcoSettings 的“显示”与“ECO 指示” */
static const char *const s_eco_labels[UI_SETTINGS_ECO_COUNT] = {
    "ECO DISPLAY ", "ECO INDICATOR ", "BACK",
};

/* 行程资讯子项：行程资讯未迁移，置灰 */
static const char *const s_it_labels[UI_SETTINGS_IT_COUNT] = {
    "ITINERARY", "DRIVING ASSIST", "VEHICLE", "BACK",
};
static const bool s_it_enabled[UI_SETTINGS_IT_COUNT] = {
    false, true, true, true,
};

/* 驾驶员辅助子项：驾驶辅助 / 驻车辅助 / 底盘控制 / 电动助力转向 / 返回 */
static const char *const s_assist_labels[UI_SETTINGS_ASSIST_COUNT] = {
    "DRIVING ASSIST", "PARKING ASSIST", "CHASSIS CONTROL",
    "ELECTRIC POWER STEERING", "BACK",
};
static const bool s_assist_enabled[UI_SETTINGS_ASSIST_COUNT] = {
    true, true, true, true, true,
};

/*
 * 驻车辅助（Qt ParkAssist 的 leftModel 顺序）。前方雷达/倒车雷达/显示/
 * 雷达音量/雷达探测距离在 XTrail 车型隐藏（未纳入车型分支，恒显示）；
 * 倒车侧预警按工厂开关 bit9 隐藏；驻车辅助传感器仅 Infiniti/X21 车型。
 */
static const char *const s_park_labels[UI_SETTINGS_PARK_COUNT] = {
    "MOVING OBJECT ", "FRONT RADAR ", "REAR RADAR ", "DISPLAY ",
    "REAR CROSS TRAFFIC ", "RADAR VOLUME ", "RADAR DISTANCE ",
    "PARKING SENSOR ", "BACK",
};

/* 底盘控制（Qt ChassistControl） */
static const char *const s_chassis_labels[UI_SETTINGS_CHASSIS_COUNT] = {
    "ACTIVE TRACE CONTROL ", "ACTIVE ENGINE BRAKE ", "BACK",
};

/* Qt ParkAssist：雷达音量 低/中/高，雷达探测距离 近/中/远，传感器 OFF/ON/仅前方 */
static const char *const s_radar_volume_options[3] = { "LOW", "MID", "HIGH" };
static const char *const s_radar_distance_options[3] = { "NEAR", "MID", "FAR" };
static const char *const s_parking_sensor_options[3] = { "OFF", "ON", "FRONT ONLY" };

/* 电动助力转向（Qt ElectricPowerStepping）：常规 / 运动 */
static const char *const s_eps_labels[UI_SETTINGS_EPS_COUNT] = {
    "ELECTRIC POWER STEERING ", "BACK",
};

/* 驾驶辅助三项：值来自 MCU 0x16 回读，OK 只下发命令 */
static const char *const s_da_labels[UI_SETTINGS_DA_COUNT] = {
    "EMERGENCY BRAKING ", "LANE DEPARTURE ", "BLIND SPOT ", "BACK",
};

/* 车辆设置子项（Qt CarSettings.leftModel 顺序） */
static const char *const s_veh_labels[UI_SETTINGS_VEH_COUNT] = {
    "LIGHT SETTINGS", "TURN LIGHT", "DOORS", "WIPER", "SERVICE",
    "REMOTE ENGINE START ", "BACK",
};
static const bool s_veh_enabled[UI_SETTINGS_VEH_COUNT] = {
    true, true, true, true, true, true, true,
};

/* 灯光设置（Qt LightSettings） */
static const char *const s_light_labels[UI_SETTINGS_LIGHT_COUNT] = {
    "WELCOME LIGHT ", "AUTO INTERIOR LIGHT ",
    "HEADLIGHT SENSITIVITY ", "LIGHT OFF DELAY ", "BACK",
};

/* 车门设置（Qt DoorSetting） */
static const char *const s_door_labels[UI_SETTINGS_DOOR_COUNT] = {
    "HORN FEEDBACK ", "AUTO UNLOCK ", "EXTERNAL DOORS ", "SINGLE DOOR UNLOCK ",
    "SMART KEY LOCK ", "SELECT UNLOCK ", "AUTO DOOR LOCK ", "BACK",
};

/* Qt DoorSetting.autolock 的 3 个选项 */
static const char *const s_auto_unlock_options[3] = {
    "IGN OFF", "P SHIFT", "OFF",
};

/* Qt LightSettings：点灯灵敏度 4 项、灯光关闭延时 8 项 */
static const char *const s_sensitivity_options[4] = {
    "SLOW", "STANDARD", "FAST", "VERY FAST",
};
static const char *const s_light_delay_options[8] = {
    "OFF", "30s", "45s", "60s", "90s", "120s", "150s", "180s",
};
/* Qt DoorSetting.autolockkkkk 的 3 个选项 */
static const char *const s_auto_lock_options[3] = {
    "SPEED", "P SHIFT", "OFF",
};

/* 转向灯（Qt TurnLightSet） */
static const char *const s_turn_labels[UI_SETTINGS_TURN_COUNT] = {
    "LANE CHANGE FLASH ", "BACK",
};

/* 雨刷器（Qt WiperSet） */
static const char *const s_wiper_labels[UI_SETTINGS_WIPER_COUNT] = {
    "SPEED SENSING WIPER ", "REAR WIPER LINKAGE ", "MIST MODE ", "BACK",
};

/* 保养设置主列表（Qt Service） */
static const char *const s_service_labels[UI_SETTINGS_SERVICE_COUNT] = {
    "OIL CHANGE", "TIRE CHANGE", "CUSTOM ITEM", "BACK",
};

/* 保养子页（Qt PageOilChangeSetting 第一层） */
static const char *const s_service_row_labels[UI_SETTINGS_SVC_COUNT] = {
    "SET SERVICE KM", "REMAINING KM", "RESET", "BACK",
};

/* 设置存储：由 main.c 注入（板端为持久设置，PC 为内存默认值） */
static car_settings_t *s_settings;

/* 命令通道：板端由 main.c 注入 car_link_exec */
static ui_settings_page_cmd_cb_t s_cmd_cb;

/* 当前平均油耗（×10，Qt GlobalData.items.avgfuel），由 ui_app 每次 apply 注入 */
static uint16_t s_eco_current_x10;

/*
 * 驾驶辅助三项的 MCU 回读缓存。缺省 true 与 Qt 一致；
 * 只由 ui_app 依据 car_state.driver_assist 更新，设置页自身不修改。
 */
static bool s_da_emergency = true;
static bool s_da_lane = true;
static bool s_da_blind = true;
/* 电动助力转向回读值（0x16 index 9）：0 常规 / 1 运动，Qt 缺省 0 */
static uint8_t s_eps_mode;

/* 驻车辅助 / 底盘控制回读缓存（Qt SettingsMgr 缺省：开关 true / 选项 0） */
static bool s_pc_moving_object = true;
static bool s_pc_front_radar = true;
static bool s_pc_rear_radar = true;
static bool s_pc_display = true;
static bool s_pc_rear_cross = true;
static bool s_pc_trace = true;
static bool s_pc_engine_brake = true;
static uint8_t s_pc_radar_volume;
static uint8_t s_pc_radar_distance;
static uint8_t s_pc_parking_sensor;
static bool s_speed_warn_valid;

/* 车辆设置项的 MCU 回读缓存（Qt SettingsMgr 缺省：开关 true / 选项 0） */
static bool s_vs_welcome = true;
static bool s_vs_interior = true;
static bool s_vs_horn = true;
static uint8_t s_vs_auto_unlock;
static bool s_vs_lane_change = true;
static bool s_vs_wiper_speed = true;
static bool s_vs_wiper_rear = true;
static bool s_vs_wiper_mist = true;
static bool s_vs_external_doors = true;
static bool s_vs_single_unlock = true;
static bool s_vs_smart_key = true;
static bool s_vs_select_unlock = true;
static uint8_t s_vs_auto_lock;
static uint8_t s_vs_headlight_sensitivity;
static uint8_t s_vs_headlight_delay;
static bool s_vs_remote_start = false;

/* 保养公里数缓存（Qt SettingsManager 缺省：5000 / 10000 / 10000） */
static uint32_t s_svc_set_km[3] = { 5000U, 10000U, 10000U };
static int32_t s_svc_remain_km[3] = { 5000, 10000, 10000 };

void ui_settings_state_init(ui_settings_state_t *st)
{
    if(st == NULL) {
        return;
    }

    memset(st, 0, sizeof(*st));
    st->view = UI_SETTINGS_VIEW_ROOT;
    st->root_locked = false;
    st->root_index = NONE;
    st->mycar_locked = false;
    st->mycar_index = NONE;
    st->mycar_child = NONE;
    st->style_index = 0U;
    st->unit_index = NONE;
    st->card_index = 0U;
    st->info_index = NONE;
    st->drv_index = NONE;
    st->routine_index = NONE;
    st->eco_index = NONE;
    st->it_index = NONE;
    st->assist_index = NONE;
    st->driving_assist_index = NONE;
    st->eps_index = NONE;
    st->parking_index = NONE;
    st->chassis_index = NONE;
    st->vehicle_index = NONE;
    st->light_index = NONE;
    st->door_index = NONE;
    st->turn_index = NONE;
    st->wiper_index = NONE;
    st->service_index = NONE;
    st->service_type = 0U;
    st->service_row = NONE;
    st->service_digit_index = 0U;
    st->service_editing = false;
    st->select_kind = 0U;
    st->select_index = NONE;
    st->select_saved_index = 0U;
    st->system_index = NONE;
    st->speed_warn_digit_index = 0U;
}

void ui_settings_page_set_settings(car_settings_t *settings)
{
    s_settings = settings;
}

void ui_settings_page_set_cmd_cb(ui_settings_page_cmd_cb_t cb)
{
    s_cmd_cb = cb;
}

/* ---------------- 控件创建 ---------------- */

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    ui_settings_label_set_text(label, text);

    return label;
}

/* 400 宽、行高 60 的列表行（Qt ButtonEx）：文字默认居中 */
static lv_obj_t *make_row(lv_obj_t *parent, int32_t x, int32_t y, int32_t w,
                          int32_t h, const char *text)
{
    lv_obj_t *row = ui_blank_container(parent);
    lv_obj_t *label;

    lv_obj_set_pos(row, x, y);
    lv_obj_set_size(row, w, h);

    label = make_label(row, text, UI_FONT_LG, UI_COLOR_WHITE);
    /* Qt settings rows use 26px. Keep one line inside the fixed row height. */
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(label, w);
    lv_obj_set_height(label, lv_font_get_line_height(UI_FONT_LG));
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    return row;
}

/* Qt ComSettingMenu：52×52，横排位置固定，没有卡片背景或透视位移。 */
static void root_menu_create(ui_settings_page_t *page, uint8_t index)
{
    lv_obj_t *item = ui_blank_container(page->root_bar);
    lv_obj_set_size(item, 52, 52);
    lv_obj_set_pos(item, 70 + 100 * (int32_t)index, 10);

    lv_obj_t *img = lv_image_create(item);
    lv_image_set_src(img, s_root_icons[index][0]);
    lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *bar = ui_blank_container(item);
    lv_obj_set_size(bar, 46, 4);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xDC143C), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    page->root_menu[index] = item;
}

/*
 * 列表容器：尺寸与 body 相同，禁用手势滚动，按键程序化滚动内容。
 * 行位置直接按内容顺序排，选中项用“内容偏移”滚动，绝不移动行对象，
 * 否则测试按坐标找行、以及“返回”这类子对象都会错位。
 */
static lv_obj_t *make_list(lv_obj_t *body)
{
    lv_obj_t *list = ui_blank_container(body);

    lv_obj_set_pos(list, 0, 0);
    lv_obj_set_size(list, SETTINGS_W, LIST_VIEW_H);
    lv_obj_add_flag(list, LV_OBJ_FLAG_HIDDEN);

    return list;
}

/* 柱状图配色（Qt EcoavgShow 渐变 / 白柱 / 春绿） */
#define ECO_HIST_BAR_W 28
#define ECO_HIST_CUR_W 32
#define ECO_HIST_GAP 25
#define ECO_HIST_CHART_W 500
#define ECO_HIST_CHART_H 280
#define ECO_HIST_PLOT_H UI_SETTINGS_ECO_HISTORY_BAR_H
#define ECO_HIST_PLOT_X 70
#define ECO_HIST_PLOT_Y 40
#define ECO_HIST_BASE_Y (ECO_HIST_PLOT_Y + ECO_HIST_PLOT_H) /* 柱底 */
#define ECO_HIST_BLUE_TOP 0x4A90E2
#define ECO_HIST_BLUE_BOTTOM 0x1D4E89
#define ECO_HIST_GREEN 0x00FF7F

/* 单根柱子：宽 w、底边贴 ECO_HIST_BASE_Y，高度随数值变化（向上长） */
static lv_obj_t *eco_hist_bar(lv_obj_t *parent, int32_t x, int32_t w)
{
    lv_obj_t *bar = ui_blank_container(parent);

    lv_obj_set_width(bar, w);
    lv_obj_set_height(bar, 1);
    lv_obj_set_style_radius(bar, 2, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    /* 卡片内坐标：底部对齐，y 偏移为卡片底到柱底的距离 */
    lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, x,
                 -(ECO_HIST_CHART_H - ECO_HIST_BASE_Y));

    return bar;
}

/*
 * ECO 历史信息页（Qt EcoavgShow）：
 *   500×280 半透明卡片，左上 “l/100km”，左侧 Y 轴 30/20/10/0 与参考线，
 *   下方 4 根历史蓝柱 + 分隔线 + 当前白柱 + 最佳绿柱，底部“当前/最佳”读数。
 */
static void eco_history_create(ui_settings_page_t *page)
{
    static const int32_t axis[4] = {30, 20, 10, 0};
    lv_obj_t *list = make_list(page->body);
    lv_obj_t *card = ui_blank_container(list);
    lv_obj_t *sep;
    char text[16];
    int32_t x;

    lv_obj_set_size(card, ECO_HIST_CHART_W, ECO_HIST_CHART_H);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x050A0F), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_80, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(UI_COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_border_width(card, 1, 0);

    lv_obj_set_pos(make_label(card, "l/100km", UI_FONT_SM, 0xAAAAAA), 15, 10);

    /* Y 轴刻度 + 横向参考线 */
    for(uint8_t i = 0U; i < 4U; ++i) {
        lv_obj_t *tick;
        lv_obj_t *line;

        (void)snprintf(text, sizeof(text), "%d", (int)axis[i]);
        tick = make_label(card, text, UI_FONT_SM, UI_COLOR_WHITE);
        lv_obj_set_pos(tick, ECO_HIST_PLOT_X - 30,
                       ECO_HIST_PLOT_Y + ECO_HIST_PLOT_H / 4 * (int32_t)i - 10);

        line = ui_blank_container(card);
        lv_obj_set_pos(line, ECO_HIST_PLOT_X, ECO_HIST_PLOT_Y +
                                                 ECO_HIST_PLOT_H / 4 * (int32_t)i);
        lv_obj_set_size(line, 420, 1);
        lv_obj_set_style_bg_color(line, lv_color_hex(UI_COLOR_WHITE), 0);
        lv_obj_set_style_bg_opa(line, LV_OPA_10, 0);
    }

    /* 4 根历史柱：从左到右由旧到新 */
    x = ECO_HIST_PLOT_X;
    for(uint8_t i = 0U; i < UI_SETTINGS_ECO_HISTORY_SLOTS; ++i) {
        lv_obj_t *bar = eco_hist_bar(card, x, ECO_HIST_BAR_W);

        lv_obj_set_style_bg_color(bar, lv_color_hex(ECO_HIST_BLUE_TOP), 0);
        lv_obj_set_style_bg_grad_color(bar, lv_color_hex(ECO_HIST_BLUE_BOTTOM), 0);
        lv_obj_set_style_bg_grad_dir(bar, LV_GRAD_DIR_VER, 0);
        page->eco_hist_bars[i] = bar;
        x += ECO_HIST_BAR_W + ECO_HIST_GAP;
    }

    /* 分隔线 */
    sep = ui_blank_container(card);
    lv_obj_set_pos(sep, x, ECO_HIST_BASE_Y - ECO_HIST_PLOT_H);
    lv_obj_set_size(sep, 1, ECO_HIST_PLOT_H);
    lv_obj_set_style_bg_color(sep, lv_color_hex(UI_COLOR_WHITE), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_20, 0);
    x += 1 + ECO_HIST_GAP;

    /* 当前（白柱）与最佳（绿柱） */
    page->eco_hist_cur_bar = eco_hist_bar(card, x, ECO_HIST_CUR_W);
    lv_obj_set_style_bg_color(page->eco_hist_cur_bar, lv_color_hex(UI_COLOR_WHITE), 0);
    lv_obj_set_style_bg_opa(page->eco_hist_cur_bar, LV_OPA_90, 0);
    x += ECO_HIST_CUR_W + ECO_HIST_GAP;

    page->eco_hist_best_bar = eco_hist_bar(card, x, ECO_HIST_CUR_W);
    lv_obj_set_style_bg_color(page->eco_hist_best_bar, lv_color_hex(ECO_HIST_GREEN), 0);
    lv_obj_set_style_bg_opa(page->eco_hist_best_bar, LV_OPA_90, 0);

    /* 底部读数：左“当前”，右“最佳” */
    lv_obj_set_pos(make_label(card, "CUR", UI_FONT_SM, UI_COLOR_WHITE), 70, 236);
    page->eco_hist_cur_label = make_label(card, "0.0", UI_FONT_LG, UI_COLOR_WHITE);
    lv_obj_set_pos(page->eco_hist_cur_label, 120, 228);

    {
        lv_obj_t *best_title = make_label(card, "BEST", UI_FONT_SM, ECO_HIST_GREEN);

        lv_obj_align(best_title, LV_ALIGN_BOTTOM_RIGHT, -90, -28);
    }
    page->eco_hist_best_label = make_label(card, "0.0", UI_FONT_LG, ECO_HIST_GREEN);
    lv_obj_align(page->eco_hist_best_label, LV_ALIGN_BOTTOM_RIGHT, -40, -22);

    page->eco_hist_list = list;
}

/* ---------------- 样式应用 ---------------- */

/* 行配色与 Qt ButtonEx / ItemOffline 一致 */
static void row_style(lv_obj_t *row, bool selected, bool enabled)
{
    lv_obj_t *label = lv_obj_get_child(row, 0);
    uint32_t color;

    if(label == NULL) {
        return;
    }

    if(!enabled) {
        color = COLOR_GRAY;
    }
    else if(selected) {
        color = COLOR_YELLOW;
    }
    else {
        color = UI_COLOR_WHITE;
    }

    if(lv_color_to_u32(lv_obj_get_style_text_color(label, 0)) !=
       lv_color_to_u32(lv_color_hex(color)))
        lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    bool visible = true;
    for(lv_obj_t *parent = row; parent; parent = lv_obj_get_parent(parent)) {
        if(lv_obj_has_flag(parent, LV_OBJ_FLAG_HIDDEN)) {
            visible = false;
            break;
        }
    }
    lv_label_long_mode_t mode = selected && visible ? LV_LABEL_LONG_MODE_SCROLL : LV_LABEL_LONG_MODE_DOTS;
    if(lv_label_get_long_mode(label) != mode) lv_label_set_long_mode(label, mode);
}

/*
 * Qt：selected = selectedIndex === i；进入子项（isOk）后图标仍保持选中。
 * 选中换蓝色图标、放大到 1.15 并显示红条；位置保持固定。
 */
static void root_style(ui_settings_page_t *page)
{
    uint8_t i;


    for(i = 0U; i < UI_SETTINGS_ROOT_COUNT; ++i) {
        lv_obj_t *item = page->root_menu[i];
        lv_obj_t *img = lv_obj_get_child(item, 0);
        lv_obj_t *bar = lv_obj_get_child(item, 1);
        bool selected = (page->st.root_index == i);

        const char *src = s_root_icons[i][selected ? 1 : 0];
        if(!lv_image_get_src(img) || strcmp(lv_image_get_src(img), src) != 0)
            lv_image_set_src(img, src);
        lv_image_set_scale(img, selected ? 294 : 256); /* Qt 选中后的静止缩放 1.15 */
        if(selected) {
            lv_obj_remove_flag(bar, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void mycar_style(ui_settings_page_t *page)
{
    uint8_t i;
    bool locked = page->st.mycar_locked;

    for(i = 0U; i < UI_SETTINGS_MYCAR_COUNT; ++i) {
        row_style(page->mycar_rows[i],
                  locked && page->st.mycar_index == i, s_mycar_enabled[i]);
    }
}

static const char *option_label(const settings_option_t *options, uint8_t count,
                                uint8_t value)
{
    const char *name = options[0].label;

    for(uint8_t i = 0U; i < count; ++i) {
        if(options[i].value == value) {
            name = options[i].label;
        }
    }

    return name;
}

/* 单位页：选项文字随设置值变化 */
static void unit_page_style(ui_settings_page_t *page)
{
    uint8_t i;

    if(s_settings == NULL) {
        return;
    }

    for(i = 0U; i < UNIT_ROW_COUNT; ++i) {
        lv_obj_t *row = lv_obj_get_child(page->unit_list, i);
        lv_obj_t *label = lv_obj_get_child(row, 0);
        char text[32];

        if(i == UNIT_ROW_MILEAGE) {
            (void)snprintf(text, sizeof(text), "MILEAGE: %s",
                           option_label(s_mileage_options, MILEAGE_COUNT,
                                        s_settings->values.unit_mileage));
        }
        else if(i == UNIT_ROW_TEMP) {
            (void)snprintf(text, sizeof(text), "TEMP: %s",
                           option_label(s_temp_options, TEMP_COUNT,
                                        s_settings->values.unit_temperature));
        }
        else {
            (void)snprintf(text, sizeof(text), "CONFIRM");
        }

        ui_settings_label_set_text(label, text);
        row_style(row, page->st.unit_index == i, true);
    }
}

static void style_page_style(ui_settings_page_t *page)
{
    uint8_t i;

    for(i = 0U; i <= style_back(); ++i) {
        row_style(lv_obj_get_child(page->style_list, i),
                  page->st.style_index == i, true);
    }
}

/* 中心卡片页：右侧 24×24 勾选框 + 勾号（Qt PageCenterCards） */
static void card_page_style(ui_settings_page_t *page)
{
    uint32_t mask = (s_settings != NULL) ? s_settings->values.center_card_mask
                                         : 0U;
    uint8_t i;

    for(i = 0U; i <= CARD_BACK; ++i) {
        lv_obj_t *row = lv_obj_get_child(page->card_list, i);
        lv_obj_t *box = lv_obj_get_child(row, 1);
        lv_obj_t *tick;

        row_style(row, page->st.card_index == i, true);
        if(box == NULL) {
            continue;
        }

        lv_obj_set_style_border_color(box, lv_color_hex(
            (page->st.card_index == i) ? COLOR_YELLOW : UI_COLOR_WHITE), 0);
        tick = lv_obj_get_child(box, 0);
        if(tick != NULL) {
            bool on = (i < CARD_BACK) && ((mask & s_card_bits[i]) != 0U);
            lv_obj_set_style_line_color(tick, lv_color_hex(
                page->st.card_index == i ? COLOR_YELLOW : UI_COLOR_WHITE), 0);

            if(on) {
                lv_obj_remove_flag(tick, LV_OBJ_FLAG_HIDDEN);
            }
            else {
                lv_obj_add_flag(tick, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

/*
 * 开关行：左侧文字 + 右侧 ON/OFF（Qt ComSettingItem 的 onText/offText）。
 * 行对象第 0 个子对象是主标签，第 1 个是右侧状态标签。
 */
static void switch_row_state(lv_obj_t *row, bool selected, bool on)
{
    lv_obj_t *state = lv_obj_get_child(row, 1);

    row_style(row, selected, true);
    if(state != NULL) {
        ui_settings_label_set_text(state, on ? "ON" : "OFF");
        lv_obj_set_style_text_color(
            state, lv_color_hex(on ? COLOR_YELLOW : UI_COLOR_WHITE), 0);
    }
}

static void info_page_style(ui_settings_page_t *page)
{
    for(uint8_t i = 0U; i < UI_SETTINGS_INFO_COUNT; ++i) {
        row_style(page->info_rows[i], page->st.info_index == i, true);
    }
}

static void drv_page_style(ui_settings_page_t *page)
{
    bool out_temp = (s_settings != NULL) && (s_settings->values.out_temperature != 0U);
    bool gear = (s_settings == NULL) || (s_settings->values.gear_on != 0U);
    bool avg_fuel = (s_settings != NULL) && (s_settings->values.avg_fuel_on != 0U);

    switch_row_state(page->drv_rows[UI_SETTINGS_DRV_OUT_TEMP],
                     page->st.drv_index == UI_SETTINGS_DRV_OUT_TEMP, out_temp);
    switch_row_state(page->drv_rows[UI_SETTINGS_DRV_GEAR],
                     page->st.drv_index == UI_SETTINGS_DRV_GEAR, gear);
    switch_row_state(page->drv_rows[UI_SETTINGS_DRV_AVG_FUEL],
                     page->st.drv_index == UI_SETTINGS_DRV_AVG_FUEL, avg_fuel);
    row_style(page->drv_rows[UI_SETTINGS_DRV_BACK],
              page->st.drv_index == UI_SETTINGS_DRV_BACK, true);
}

/* 行程资讯子项：只有驾驶员辅助与返回可用 */
static void it_page_style(ui_settings_page_t *page)
{
    bool locked = page->st.root_locked;

    for(uint8_t i = 0U; i < UI_SETTINGS_IT_COUNT; ++i) {
        row_style(page->it_rows[i], locked && page->st.it_index == i,
                  s_it_enabled[i]);
    }
}

/* 驾驶员辅助子项：只有驾驶辅助与返回可用 */
static void assist_page_style(ui_settings_page_t *page)
{
    for(uint8_t i = 0U; i < UI_SETTINGS_ASSIST_COUNT; ++i) {
        row_style(page->assist_rows[i], page->st.assist_index == i,
                  s_assist_enabled[i]);
    }
}

/* 驾驶辅助三项：ON/OFF 只反映 MCU 回读，不在本地翻转 */
static void speed_warn_page_style(ui_settings_page_t *page)
{
    for(uint8_t i = 0U; i < 3U; ++i) {
        char text[2] = {(char)('0' + page->st.speed_warn_digits[i]), '\0'};

        ui_settings_label_set_text(page->speed_warn_digits[i], text);
        lv_obj_set_style_text_color(page->speed_warn_digits[i],
            lv_color_hex(page->st.speed_warn_digit_index == i
                             ? COLOR_YELLOW : UI_COLOR_WHITE), 0);
    }
}

static void drv_assist_page_style(ui_settings_page_t *page)
{
    switch_row_state(page->drv_assist_rows[UI_SETTINGS_DA_EMERGENCY],
                     page->st.driving_assist_index == UI_SETTINGS_DA_EMERGENCY,
                     s_da_emergency);
    switch_row_state(page->drv_assist_rows[UI_SETTINGS_DA_LANE],
                     page->st.driving_assist_index == UI_SETTINGS_DA_LANE,
                     s_da_lane);
    switch_row_state(page->drv_assist_rows[UI_SETTINGS_DA_BLIND],
                     page->st.driving_assist_index == UI_SETTINGS_DA_BLIND,
                     s_da_blind);
    row_style(page->drv_assist_rows[UI_SETTINGS_DA_BACK],
              page->st.driving_assist_index == UI_SETTINGS_DA_BACK, true);
}

/* 电动助力转向：模式文字只反映 MCU 回读（0 常规 / 1 运动） */
static void eps_page_style(ui_settings_page_t *page)
{
    lv_obj_t *row = page->eps_rows[UI_SETTINGS_EPS_MODE];
    lv_obj_t *state = row != NULL ? lv_obj_get_child(row, 1) : NULL;

    if(state != NULL) {
        ui_settings_label_set_text(state, (s_eps_mode == 1U) ? "SPORT" : "NORMAL");
    }
    row_style(row, page->st.eps_index == UI_SETTINGS_EPS_MODE, true);
    row_style(page->eps_rows[UI_SETTINGS_EPS_BACK],
              page->st.eps_index == UI_SETTINGS_EPS_BACK, true);
}

/* 底盘控制：两项开关只反映 MCU 回读 */
static void chassis_page_style(ui_settings_page_t *page)
{
    switch_row_state(page->chassis_rows[UI_SETTINGS_CHASSIS_TRACE],
                     page->st.chassis_index == UI_SETTINGS_CHASSIS_TRACE,
                     s_pc_trace);
    switch_row_state(page->chassis_rows[UI_SETTINGS_CHASSIS_BRAKE],
                     page->st.chassis_index == UI_SETTINGS_CHASSIS_BRAKE,
                     s_pc_engine_brake);
    row_style(page->chassis_rows[UI_SETTINGS_CHASSIS_BACK],
              page->st.chassis_index == UI_SETTINGS_CHASSIS_BACK, true);
}

/* 车辆设置：只有灯光设置/车门/返回可用，未迁移项置灰 */
static void vehicle_page_style(ui_settings_page_t *page)
{
    for(uint8_t i = 0U; i < UI_SETTINGS_VEH_COUNT; ++i) {
        if(i == UI_SETTINGS_VEH_REMOTE_START) {
            switch_row_state(page->vehicle_rows[i],
                             page->st.vehicle_index == i, s_vs_remote_start);
            continue;
        }
        row_style(page->vehicle_rows[i], page->st.vehicle_index == i,
                  s_veh_enabled[i]);
    }
}

/* 多选页选项表：返回选项数量 */
static const char *const *select_options(uint8_t kind, uint8_t *count)
{
    if(kind == UI_SETTINGS_SELECT_ANIMATION) {
        static const char *const options[] = { "DEFAULT (CONFIG ONLY)", "ANIMATION 1 (CONFIG ONLY)" };
        *count = 2U;
        return options;
    }
    if(kind == UI_SETTINGS_SELECT_SENSITIVITY) {
        *count = 4U;
        return s_sensitivity_options;
    }
    if(kind == UI_SETTINGS_SELECT_DELAY) {
        *count = 8U;
        return s_light_delay_options;
    }
    *count = 3U;
    return s_auto_lock_options;
}

/* 详情行：左侧文字 + 右侧当前选项（Qt ItemDetails） */
static void detail_row_state(lv_obj_t *row, bool selected, const char *text)
{
    lv_obj_t *state = lv_obj_get_child(row, 1);

    row_style(row, selected, true);
    if(state != NULL) {
        ui_settings_label_set_text(state, text);
        lv_obj_set_style_text_color(state, lv_color_hex(UI_COLOR_WHITE), 0);
    }
}

/* 灯光设置：ON/OFF 与当前选项均只反映 MCU 0x16 回读 */
static void light_page_style(ui_settings_page_t *page)
{
    switch_row_state(page->light_rows[UI_SETTINGS_LIGHT_WELCOME],
                     page->st.light_index == UI_SETTINGS_LIGHT_WELCOME,
                     s_vs_welcome);
    switch_row_state(page->light_rows[UI_SETTINGS_LIGHT_AUTO_INTERIOR],
                     page->st.light_index == UI_SETTINGS_LIGHT_AUTO_INTERIOR,
                     s_vs_interior);
    detail_row_state(page->light_rows[UI_SETTINGS_LIGHT_SENSITIVITY],
                     page->st.light_index == UI_SETTINGS_LIGHT_SENSITIVITY,
                     s_sensitivity_options[s_vs_headlight_sensitivity % 4U]);
    detail_row_state(page->light_rows[UI_SETTINGS_LIGHT_DELAY],
                     page->st.light_index == UI_SETTINGS_LIGHT_DELAY,
                     s_light_delay_options[s_vs_headlight_delay % 8U]);
    row_style(page->light_rows[UI_SETTINGS_LIGHT_BACK],
              page->st.light_index == UI_SETTINGS_LIGHT_BACK, true);
}

/*
 * 车门设置：开关项 ON/OFF，自动解锁/自动上锁显示当前选项
 * （Qt DoorSetting + PageListSelect）。
 */
static void door_page_style(ui_settings_page_t *page)
{
    switch_row_state(page->door_rows[UI_SETTINGS_DOOR_HORN],
                     page->st.door_index == UI_SETTINGS_DOOR_HORN, s_vs_horn);
    detail_row_state(page->door_rows[UI_SETTINGS_DOOR_AUTO_UNLOCK],
                     page->st.door_index == UI_SETTINGS_DOOR_AUTO_UNLOCK,
                     s_auto_unlock_options[s_vs_auto_unlock % 3U]);
    switch_row_state(page->door_rows[UI_SETTINGS_DOOR_EXTERNAL],
                     page->st.door_index == UI_SETTINGS_DOOR_EXTERNAL,
                     s_vs_external_doors);
    switch_row_state(page->door_rows[UI_SETTINGS_DOOR_SINGLE_UNLOCK],
                     page->st.door_index == UI_SETTINGS_DOOR_SINGLE_UNLOCK,
                     s_vs_single_unlock);
    switch_row_state(page->door_rows[UI_SETTINGS_DOOR_SMART_KEY],
                     page->st.door_index == UI_SETTINGS_DOOR_SMART_KEY,
                     s_vs_smart_key);
    switch_row_state(page->door_rows[UI_SETTINGS_DOOR_SELECT_UNLOCK],
                     page->st.door_index == UI_SETTINGS_DOOR_SELECT_UNLOCK,
                     s_vs_select_unlock);
    detail_row_state(page->door_rows[UI_SETTINGS_DOOR_AUTO_LOCK],
                     page->st.door_index == UI_SETTINGS_DOOR_AUTO_LOCK,
                     s_auto_lock_options[s_vs_auto_lock % 3U]);
    row_style(page->door_rows[UI_SETTINGS_DOOR_BACK],
              page->st.door_index == UI_SETTINGS_DOOR_BACK, true);
}

/* 通用多选页：选项 + 返回，已保存项右侧标记 */
static void select_page_style(ui_settings_page_t *page)
{
    uint8_t count = 0U;
    const char *const *options = select_options(page->st.select_kind, &count);
    uint8_t total = (uint8_t)(count + 1U);

    for(uint8_t i = 0U; i < UI_SETTINGS_SELECT_MAX; ++i) {
        lv_obj_t *label;
        lv_obj_t *mark;

        if(i >= total) {
            lv_obj_add_flag(page->select_rows[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(page->select_rows[i], LV_OBJ_FLAG_HIDDEN);

        label = lv_obj_get_child(page->select_rows[i], 0);
        mark = lv_obj_get_child(page->select_rows[i], 1);
        if(label != NULL) {
            ui_settings_label_set_text(label, (i < count) ? options[i] : "BACK");
        }
        row_style(page->select_rows[i], page->st.select_index == i, true);
        if(mark != NULL) {
            ui_settings_label_set_text(mark,
                              (i == page->st.select_saved_index) ? "*" : "");
            lv_obj_set_style_text_color(mark, lv_color_hex(COLOR_YELLOW), 0);
        }
    }
}

/* 转向灯：变更车道闪灯 3 次（0x16 index 12） */
static void turn_page_style(ui_settings_page_t *page)
{
    switch_row_state(page->turn_rows[UI_SETTINGS_TURN_LANE_CHANGE],
                     page->st.turn_index == UI_SETTINGS_TURN_LANE_CHANGE,
                     s_vs_lane_change);
    row_style(page->turn_rows[UI_SETTINGS_TURN_BACK],
              page->st.turn_index == UI_SETTINGS_TURN_BACK, true);
}

/* 雨刷器：index 20 / 21 / 22 */
static void wiper_page_style(ui_settings_page_t *page)
{
    switch_row_state(page->wiper_rows[UI_SETTINGS_WIPER_SPEED_SENSE],
                     page->st.wiper_index == UI_SETTINGS_WIPER_SPEED_SENSE,
                     s_vs_wiper_speed);
    switch_row_state(page->wiper_rows[UI_SETTINGS_WIPER_REAR_LINKAGE],
                     page->st.wiper_index == UI_SETTINGS_WIPER_REAR_LINKAGE,
                     s_vs_wiper_rear);
    switch_row_state(page->wiper_rows[UI_SETTINGS_WIPER_MIST],
                     page->st.wiper_index == UI_SETTINGS_WIPER_MIST,
                     s_vs_wiper_mist);
    row_style(page->wiper_rows[UI_SETTINGS_WIPER_BACK],
              page->st.wiper_index == UI_SETTINGS_WIPER_BACK, true);
}

/* 驻车辅助：开关项 ON/OFF，音量/距离/传感器显示当前选项（只反映回读） */
static void parking_page_style(ui_settings_page_t *page)
{
    switch_row_state(page->parking_rows[UI_SETTINGS_PARK_MOVING_OBJECT],
                     page->st.parking_index == UI_SETTINGS_PARK_MOVING_OBJECT,
                     s_pc_moving_object);
    switch_row_state(page->parking_rows[UI_SETTINGS_PARK_FRONT_RADAR],
                     page->st.parking_index == UI_SETTINGS_PARK_FRONT_RADAR,
                     s_pc_front_radar);
    switch_row_state(page->parking_rows[UI_SETTINGS_PARK_REAR_RADAR],
                     page->st.parking_index == UI_SETTINGS_PARK_REAR_RADAR,
                     s_pc_rear_radar);
    switch_row_state(page->parking_rows[UI_SETTINGS_PARK_DISPLAY],
                     page->st.parking_index == UI_SETTINGS_PARK_DISPLAY,
                     s_pc_display);
    switch_row_state(page->parking_rows[UI_SETTINGS_PARK_REAR_CROSS],
                     page->st.parking_index == UI_SETTINGS_PARK_REAR_CROSS,
                     s_pc_rear_cross);
    detail_row_state(page->parking_rows[UI_SETTINGS_PARK_VOLUME],
                     page->st.parking_index == UI_SETTINGS_PARK_VOLUME,
                     s_radar_volume_options[s_pc_radar_volume % 3U]);
    detail_row_state(page->parking_rows[UI_SETTINGS_PARK_DISTANCE],
                     page->st.parking_index == UI_SETTINGS_PARK_DISTANCE,
                     s_radar_distance_options[s_pc_radar_distance % 3U]);
    detail_row_state(page->parking_rows[UI_SETTINGS_PARK_SENSOR],
                     page->st.parking_index == UI_SETTINGS_PARK_SENSOR,
                     s_parking_sensor_options[s_pc_parking_sensor % 3U]);
    row_style(page->parking_rows[UI_SETTINGS_PARK_BACK],
              page->st.parking_index == UI_SETTINGS_PARK_BACK, true);
}

/* 保养主列表：机油 / 轮胎 / 自定义 / 返回 */
static void service_page_style(ui_settings_page_t *page)
{
    for(uint8_t i = 0U; i < UI_SETTINGS_SERVICE_COUNT; ++i) {
        row_style(page->service_rows[i], page->st.service_index == i, true);
    }
}

/* 保养子页：设置公里数 / 剩余公里数 / 重置 / 返回 */
static void service_item_page_style(ui_settings_page_t *page)
{
    uint8_t type = (uint8_t)(page->st.service_type % 3U);
    char text[16];
    lv_obj_t *state;

    row_style(page->service_item_rows[UI_SETTINGS_SVC_SET_KM],
              page->st.service_row == UI_SETTINGS_SVC_SET_KM, true);
    state = lv_obj_get_child(page->service_item_rows[UI_SETTINGS_SVC_SET_KM], 1);
    if(state != NULL) {
        (void)snprintf(text, sizeof(text), "%lukm",
                       (unsigned long)s_svc_set_km[type]);
        ui_settings_label_set_text(state, text);
        lv_obj_set_style_text_color(state, lv_color_hex(UI_COLOR_WHITE), 0);
    }

    row_style(page->service_item_rows[UI_SETTINGS_SVC_REMAIN_KM],
              page->st.service_row == UI_SETTINGS_SVC_REMAIN_KM, true);
    state = lv_obj_get_child(page->service_item_rows[UI_SETTINGS_SVC_REMAIN_KM], 1);
    if(state != NULL) {
        (void)snprintf(text, sizeof(text), "%ldkm",
                       (long)s_svc_remain_km[type]);
        ui_settings_label_set_text(state, text);
        lv_obj_set_style_text_color(state, lv_color_hex(UI_COLOR_WHITE), 0);
    }

    row_style(page->service_item_rows[UI_SETTINGS_SVC_RESET],
              page->st.service_row == UI_SETTINGS_SVC_RESET, true);
    row_style(page->service_item_rows[UI_SETTINGS_SVC_BACK],
              page->st.service_row == UI_SETTINGS_SVC_BACK, true);
}

/* 数字编辑：6 位选中位高亮，确认/返回行 */
static void service_edit_page_style(ui_settings_page_t *page)
{
    for(uint8_t i = 0U; i < UI_SETTINGS_SVC_DIGITS; ++i) {
        char text[2];

        text[0] = (char)('0' + (page->st.service_digits[i] % 10U));
        text[1] = '\0';
        ui_settings_label_set_text(page->service_edit_digits[i], text);
        lv_obj_set_style_text_color(
            page->service_edit_digits[i],
            lv_color_hex((page->st.service_digit_index == i)
                             ? COLOR_YELLOW : UI_COLOR_WHITE), 0);
    }
    row_style(page->service_edit_rows[0],
              page->st.service_digit_index == UI_SETTINGS_SVC_EDIT_CONFIRM, true);
    row_style(page->service_edit_rows[1],
              page->st.service_digit_index == UI_SETTINGS_SVC_EDIT_BACK, true);
}

/*
 * 常规设置：
 *   亮度行第 1 个子对象显示百分比；
 *   超速报警行用 ON/OFF；
 *   超速预警行未开启报警时置灰（Qt 只有开关打开才能进子页）。
 */
static void routine_page_style(ui_settings_page_t *page)
{
    uint8_t day = (s_settings != NULL) ? s_settings->values.daylight : 100U;
    uint8_t night = (s_settings != NULL) ? s_settings->values.nightlight : 100U;
    bool limit = (s_settings != NULL) && (s_settings->values.speed_limit_on != 0U);
    uint8_t i;

    for(i = 0U; i < UI_SETTINGS_ROUTINE_COUNT; ++i) {
        lv_obj_t *row = page->routine_rows[i];
        lv_obj_t *state = lv_obj_get_child(row, 1);
        bool selected = (page->st.routine_index == i);

        if(i == UI_SETTINGS_ROUTINE_DAYLIGHT ||
           i == UI_SETTINGS_ROUTINE_NIGHTLIGHT) {
            char text[8];

            (void)snprintf(text, sizeof(text), "%u%%",
                           (unsigned)((i == UI_SETTINGS_ROUTINE_DAYLIGHT)
                                          ? day : night));
            row_style(row, selected, true);
            if(state != NULL) {
                ui_settings_label_set_text(state, text);
                lv_obj_set_style_text_color(state, lv_color_hex(UI_COLOR_WHITE), 0);
            }
        }
        else if(i == UI_SETTINGS_ROUTINE_SPEED_LIMIT) {
            switch_row_state(row, selected, limit);
        }
        else if(i == UI_SETTINGS_ROUTINE_SPEED_WARN) {
            char text[16];

            row_style(row, selected && limit, limit);
            if(state != NULL) {
                if(s_speed_warn_valid) {
                    (void)snprintf(text, sizeof(text), "%u km/h",
                                   (unsigned)page->speed_warn_kph);
                }
                else {
                    (void)snprintf(text, sizeof(text), "-- km/h");
                }
                ui_settings_label_set_text(state, text);
                lv_obj_set_style_text_color(
                    state, lv_color_hex(limit ? UI_COLOR_WHITE : COLOR_GRAY), 0);
            }
        }
        else {
            row_style(row, selected, true);
        }
    }
}

static void eco_page_style(ui_settings_page_t *page)
{
    bool on = (s_settings != NULL) && (s_settings->values.eco_indicator != 0U);
    bool throttle = (s_settings == NULL) || (s_settings->values.eco_or_fuel != 0U);
    lv_obj_t *state = lv_obj_get_child(page->eco_rows[UI_SETTINGS_ECO_DISPLAY], 1);

    /* Qt desc: THROTTLE（油门引导） / FUEL（油耗） */
    row_style(page->eco_rows[UI_SETTINGS_ECO_DISPLAY],
              page->st.eco_index == UI_SETTINGS_ECO_DISPLAY, true);
    if(state != NULL) {
        ui_settings_label_set_text(state, throttle ? "THROTTLE" : "FUEL");
        lv_obj_set_style_text_color(state, lv_color_hex(UI_COLOR_WHITE), 0);
    }

    switch_row_state(page->eco_rows[UI_SETTINGS_ECO_INDICATOR],
                     page->st.eco_index == UI_SETTINGS_ECO_INDICATOR, on);
    row_style(page->eco_rows[UI_SETTINGS_ECO_BACK],
              page->st.eco_index == UI_SETTINGS_ECO_BACK, true);
}

/*
 * ECO 历史（Qt EcoavgShow / CentralTemplate）：
 *   body 高 370，卡片 500×280 居中；Y 轴量程 30.0，每 10 一条参考线；
 *   4 根历史柱（蓝）+ 当前柱（白）+ 最佳柱（绿），柱高按量程比例。
 */
static void eco_history_bar_h(lv_obj_t *bar, uint16_t value_x10)
{
    uint32_t h;

    if(value_x10 > CAR_SETTINGS_ECO_HISTORY_SCALE_X10) {
        value_x10 = (uint16_t)CAR_SETTINGS_ECO_HISTORY_SCALE_X10;
    }
    h = (uint32_t)value_x10 * (uint32_t)UI_SETTINGS_ECO_HISTORY_BAR_H /
        CAR_SETTINGS_ECO_HISTORY_SCALE_X10;

    /* 0 值时保留 1px，Qt 里 val=0 的柱子会完全不可见 */
    if(h == 0U) {
        h = 1U;
    }
    lv_obj_set_height(bar, (int32_t)h);
}

static void eco_history_refresh(ui_settings_page_t *page)
{
    char text[32];
    uint16_t best = 0U;

    if(page->eco_hist_list == NULL) {
        return;
    }

    for(uint8_t i = 0U; i < UI_SETTINGS_ECO_HISTORY_SLOTS; ++i) {
        uint16_t value = (s_settings != NULL)
                             ? s_settings->values.eco_history_x10[i] : 0U;

        eco_history_bar_h(page->eco_hist_bars[i], value);
    }

    /*
     * 当前值：Qt currentAvg = GlobalData.items.avgfuel / 10.0（×10 直用）。
     * 最佳值：Qt bestAvg 缺省 30.0，resetAllData 后重新取最小；
     * 这里按“历史 4 条 + 当前值”非零项取最小，与 Qt 的更新时机一致。
     */
    if(s_settings != NULL) {
        best = CAR_SETTINGS_ECO_HISTORY_SCALE_X10;
        for(uint8_t i = 0U; i < UI_SETTINGS_ECO_HISTORY_SLOTS; ++i) {
            uint16_t value = s_settings->values.eco_history_x10[i];

            if(value != 0U && value < best) {
                best = value;
            }
        }
    }
    if(s_eco_current_x10 != 0U && s_eco_current_x10 < best) {
        best = s_eco_current_x10;
    }

    eco_history_bar_h(page->eco_hist_cur_bar, s_eco_current_x10);
    eco_history_bar_h(page->eco_hist_best_bar, best);

    (void)snprintf(text, sizeof(text), "%u.%u",
                   (unsigned)(s_eco_current_x10 / 10U),
                   (unsigned)(s_eco_current_x10 % 10U));
    ui_settings_label_set_text(page->eco_hist_cur_label, text);
    (void)snprintf(text, sizeof(text), "%u.%u",
                   (unsigned)(best / 10U), (unsigned)(best % 10U));
    ui_settings_label_set_text(page->eco_hist_best_label, text);
}

/*
 * Qt PageMainSettings：图标行常驻，下方 StackLayout 随 selectedIndex 切换；
 * 我的座驾子项就在根页内（isOk 只是锁定列表），子页才压栈盖住图标行。
 */
static void version_refresh(ui_settings_page_t *page);
static void system_style(ui_settings_page_t *page);
static void connect_read_name(ui_settings_page_t *page);

static void list_center(lv_obj_t *list, uint8_t index, int32_t row_h);

/* ---------------- 日期时间（Qt PageDateSettings） ---------------- */

static const uint8_t s_date_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/* 各列取值个数：年 31 / 月 12 / 日 31 / 时 24 / 分 60 */
static uint16_t date_column_count(uint8_t col)
{
    static const uint16_t count[] = {31, 12, 31, 24, 60};
    return col < 5U ? count[col] : 0U;
}

static unsigned date_leap(unsigned year)
{
    return year % 4U == 0U && (year % 100U != 0U || year % 400U == 0U);
}

/* Qt initializeDateControls：firstYear = max(2000, currentYear - 5)，31 年 */
static void date_wheel_reset(ui_settings_page_t *page, uint16_t year)
{
    if(year < 2000U) {
        year = 2000U;
    }
    page->st.date_year_base = (uint16_t)(year > 2005U ? year - 5U : 2000U);
}

/*
 * Tumbler positionViewAtIndex 对齐：日历日超过当月天数时 Qt 不会自动截断，
 * 由“确认”时的日期校验兜底；这里额外把日落回当月上限，避免下发必然被拒。
 */
static void date_day_clamp(ui_settings_page_t *page)
{
    /* 月/日为 0 基索引，转成真实月份/日期 */
    uint16_t month = (uint16_t)(page->st.date_values[1] + 1U);
    uint16_t year = (uint16_t)(page->st.date_year_base + page->st.date_values[0]);
    unsigned limit = s_date_days[month - 1U] +
                     (month == 2U && date_leap(year) ? 1U : 0U);

    if(page->st.date_values[2] >= limit) {
        page->st.date_values[2] = (uint16_t)(limit - 1U);
    }
}

/* Qt okTumblerIndex / backTumblerIndex：年列到边界停住，其余列取模 */
static void date_wheel_step_value(ui_settings_page_t *page, uint8_t col, int8_t dir)
{
    uint16_t count = date_column_count(col);
    int32_t value;

    if(col >= 5U || count == 0U) {
        return;
    }
    /* 年列内部按索引 0~30 保存，越界时停住不动 */
    value = (int32_t)page->st.date_values[col] + dir;
    if(col == 0U) {
        if(value < 0 || value >= (int32_t)count) {
            return;
        }
        page->st.date_values[0] = (uint16_t)value;
        date_day_clamp(page);
        return;
    }
    page->st.date_values[col] = (uint16_t)(((value % count) + count) % count);
    if(col == 1U || col == 2U) {
        date_day_clamp(page);
    }
}

/* 月/日/时/分两位补零，年直接显示（Qt formatText） */
static void date_wheel_cell_text(char *text, size_t size, const ui_settings_page_t *page,
                                 uint8_t col, uint16_t value)
{
    if(col == 0U) {
        (void)snprintf(text, size, "%u", (unsigned)(page->st.date_year_base + value));
    }
    else {
        (void)snprintf(text, size, "%02u", (unsigned)value);
    }
}

/* Qt NumberAnimation 的 exec：整体缩放滚轮条（含背景与 5 列） */
static void date_wheel_scale_cb(void *var, int32_t value)
{
    lv_obj_set_style_transform_scale((lv_obj_t *)var, value, 0);
}

/*
 * Qt PageDateSettings：dateTimeItem 的 NumberAnimation
 * running: instrumentMode && itemIndex === 0，400ms easeInOutQuad，scale 0.8→1。
 * 每次进入滚轮行都从 0.8 重新播放。
 */
static void date_wheel_animate(ui_settings_page_t *page)
{
    lv_anim_t a;

    if(page->date_wheel_group == NULL || page->time_source != 2U) {
        return;
    }

    lv_anim_delete(page->date_wheel_group, date_wheel_scale_cb);
    lv_anim_init(&a);
    lv_anim_set_var(&a, page->date_wheel_group);
    lv_anim_set_exec_cb(&a, date_wheel_scale_cb);
    lv_anim_set_values(&a, 204, 256); /* 0.8x → 1x（256 = 100%） */
    lv_anim_set_duration(&a, 400);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void date_wheel_refresh(ui_settings_page_t *page)
{
    char text[32];
    uint8_t focus = page->st.date_focus;
    uint8_t item = page->st.date_item;
    bool wheel_on = page->time_source == 2U;

    /* Qt dateTimeItem/timeFormatItem.visible：非仪表时间源整块隐藏 */
    if(wheel_on) {
        lv_obj_remove_flag(page->date_wheel_box, LV_OBJ_FLAG_HIDDEN);
        for(uint8_t col = 0U; col < 5U; ++col) {
            lv_obj_remove_flag(page->date_wheel_cols[col], LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_remove_flag(page->date_wheel_confirm, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(page->date_wheel_format, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(page->date_wheel_box, LV_OBJ_FLAG_HIDDEN);
        for(uint8_t col = 0U; col < 5U; ++col) {
            lv_obj_add_flag(page->date_wheel_cols[col], LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_add_flag(page->date_wheel_confirm, LV_OBJ_FLAG_HIDDEN);
        focus = NONE;
    }

    for(uint8_t col = 0U; col < 5U; ++col) {
        uint16_t count = date_column_count(col);
        uint16_t value = page->st.date_values[col];

        if(value >= count) {
            value = (uint16_t)(count - 1U);
        }
        for(uint8_t cell = 0U; cell < 3U; ++cell) {
            int32_t offset = (int32_t)cell - 1; /* 上/当前/下 */
            int32_t raw = (int32_t)value + offset;
            uint32_t color = UI_COLOR_WHITE;

            if(col == 0U && (raw < 0 || raw >= (int32_t)count)) {
                ui_settings_label_set_text(page->date_wheel_cells[col][cell], "");
                continue;
            }
            raw = ((raw % (int32_t)count) + (int32_t)count) % (int32_t)count;
            /* 月/日草稿为 0 基索引，显示时 +1 还原真实月份/日期 */
            date_wheel_cell_text(text, sizeof(text), page, col,
                                 (uint16_t)(col == 1U || col == 2U ? raw + 1 : raw));
            ui_settings_label_set_text(page->date_wheel_cells[col][cell], text);
            if(offset != 0) {
                color = DATE_REF_GRAY;
            }
            else if(focus == col) {
                /* Qt：光标所在列（且滚轮项选中）显示红色 */
                color = UI_COLOR_RED;
            }
            lv_obj_set_style_text_color(page->date_wheel_cells[col][cell],
                                        lv_color_hex(color), 0);
        }
    }

    row_style(page->date_wheel_confirm, item == 0U && focus == 5U, wheel_on);

    if(wheel_on) {
        bool valid = page->date_units.valid;

        (void)snprintf(text, sizeof(text), "TIME FORMAT: %s",
                       valid ? (page->date_units.hour24 ? "24h" : "12h") : "---");
        ui_settings_label_set_text(lv_obj_get_child(page->date_wheel_format, 0), text);
        row_style(page->date_wheel_format, item == 1U, valid);
    }
    else {
        /* Qt 非仪表模式首行提示 Text：“请在音响里操作”，不可选 */
        ui_settings_label_set_text(lv_obj_get_child(page->date_wheel_format, 0),
                          "OPERATE ON AUDIO UNIT");
        row_style(page->date_wheel_format, false, false);
    }
    (void)snprintf(text, sizeof(text), "TIME SOURCE: %s",
                   page->time_source == 1U ? "CENTRAL" :
                   page->time_source == 2U ? "INSTRUMENT" : "UNKNOWN");
    ui_settings_label_set_text(lv_obj_get_child(page->date_wheel_source, 0), text);
    /* 非仪表行号：0=时间源、1=返回（Qt sourceIndex/backIndex） */
    row_style(page->date_wheel_source, item == (wheel_on ? 2U : 0U),
              page->time_source != 0U);
    row_style(page->date_wheel_back, item == (wheel_on ? 3U : 1U), true);
}

/*
 * 滚轮两套布局（Qt instrumentMode）：
 *   timeSource=2 → 110 高滚轮条 + 确认 + 时间制式 + 时间源 + 返回；
 *   其它（中控供时/未知）→ “请在音响里操作” + 时间源 + 返回。
 */
static void date_wheel_create(ui_settings_page_t *page)
{
    static const int32_t col_x[5] = DATE_WHEEL_COL_X;
    static const int32_t col_w[5] = DATE_WHEEL_COL_W;
    lv_obj_t *list = make_list(page->body);

    page->date_wheel_list = list;
    lv_obj_set_height(list, SETTINGS_H - SUBPAGE_BODY_Y);

    /* Qt dateTimeItem：背景 + 5 列整体承载，作为 scale 动画的容器 */
    page->date_wheel_group = ui_blank_container(list);
    lv_obj_set_pos(page->date_wheel_group, DATE_WHEEL_X, DATE_WHEEL_GROUP_Y);
    lv_obj_set_size(page->date_wheel_group, DATE_WHEEL_W, DATE_WHEEL_H);

    page->date_wheel_box = ui_blank_container(page->date_wheel_group);
    lv_obj_set_pos(page->date_wheel_box, (DATE_WHEEL_W - DATE_WHEEL_BOX_W) / 2,
                   (DATE_WHEEL_H - DATE_WHEEL_BOX_H) / 2);
    lv_obj_set_size(page->date_wheel_box, DATE_WHEEL_BOX_W, DATE_WHEEL_BOX_H);
    lv_obj_set_style_bg_color(page->date_wheel_box, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(page->date_wheel_box, LV_OPA_80, 0);
    lv_obj_set_style_radius(page->date_wheel_box, 10, 0);
    lv_obj_set_style_border_width(page->date_wheel_box, 0, 0);

    for(uint8_t col = 0U; col < 5U; ++col) {
        page->date_wheel_cols[col] = ui_blank_container(page->date_wheel_group);
        lv_obj_set_pos(page->date_wheel_cols[col], col_x[col], 0);
        lv_obj_set_size(page->date_wheel_cols[col], col_w[col], DATE_WHEEL_H);
        for(uint8_t cell = 0U; cell < 3U; ++cell) {
            lv_obj_t *label = make_label(page->date_wheel_cols[col], "", UI_FONT_LG,
                                         UI_COLOR_WHITE);

            lv_obj_set_width(label, col_w[col]);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_pos(label, 0, (int32_t)cell * DATE_WHEEL_CELL_H +
                                        (DATE_WHEEL_CELL_H -
                                         lv_font_get_line_height(UI_FONT_LG)) / 2);
            page->date_wheel_cells[col][cell] = label;
        }
    }

    page->date_wheel_confirm = make_row(list,
        DATE_WHEEL_X + DATE_WHEEL_W - DATE_WHEEL_CONFIRM_W,
        DATE_WHEEL_GROUP_Y + (DATE_WHEEL_H - DATE_WHEEL_CELL_H) / 2, DATE_WHEEL_CONFIRM_W,
        DATE_WHEEL_CELL_H, "CONFIRM");

    /* 时间制式 / 时间源 / 返回：与滚轮同宽、58 高（返回行 Qt y:-20） */
    page->date_wheel_rows[0] = make_row(list, DATE_WHEEL_X, 176, DATE_WHEEL_W,
                                        DATE_WHEEL_ROW_H, "");
    page->date_wheel_rows[1] = make_row(list, DATE_WHEEL_X, 236, DATE_WHEEL_W,
                                        DATE_WHEEL_ROW_H, "");
    page->date_wheel_rows[2] = make_row(list, DATE_WHEEL_X, 296, DATE_WHEEL_W,
                                        DATE_WHEEL_ROW_H, "BACK");
    page->date_wheel_format = page->date_wheel_rows[0];
    page->date_wheel_source = page->date_wheel_rows[1];
    page->date_wheel_back = page->date_wheel_rows[2];
}

#include "ui_settings_factory.inc"

static bool feature_on(unsigned bit)
{
    return !s_settings || (s_settings->values.factory_features & (1U << bit));
}

static void feature_visibility(ui_settings_page_t *p)
{
    lv_obj_t *rows[] = {p->vehicle_rows[5], p->door_rows[4], p->door_rows[5],
        p->door_rows[2], p->door_rows[3], p->light_rows[2], p->light_rows[3],
        p->drv_assist_rows[0], p->assist_rows[UI_SETTINGS_ASSIST_EPS],
        p->parking_rows[UI_SETTINGS_PARK_REAR_CROSS]};
    for(unsigned i = 0; i < 10; ++i) {
        if(feature_on(i + 1)) lv_obj_remove_flag(rows[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(rows[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static bool feature_selection_enabled(const ui_settings_state_t *st)
{
    if(st->view == UI_SETTINGS_VIEW_VEHICLE && st->vehicle_index == 5) return feature_on(1);
    if(st->view == UI_SETTINGS_VIEW_DOOR) {
        if(st->door_index == 4) return feature_on(2);
        if(st->door_index == 5) return feature_on(3);
        if(st->door_index == 2) return feature_on(4);
        if(st->door_index == 3) return feature_on(5);
    }
    if(st->view == UI_SETTINGS_VIEW_LIGHT && st->light_index == 2) return feature_on(6);
    if(st->view == UI_SETTINGS_VIEW_LIGHT && st->light_index == 3) return feature_on(7);
    if(st->view == UI_SETTINGS_VIEW_DRIVING_ASSIST && st->driving_assist_index == 0) return feature_on(8);
    if(st->view == UI_SETTINGS_VIEW_PARKING && st->parking_index == UI_SETTINGS_PARK_REAR_CROSS) return feature_on(9);
    return true;
}
static void connect_rows_render(ui_settings_page_t *page, uint8_t select_index)
{
    static const char *const modes[] = {"ANDROID AUTO", "APPLE CARPLAY", "HICAR"};
    lv_obj_remove_flag(page->date_list, LV_OBJ_FLAG_HIDDEN);
    for(unsigned i = 0; i < 9; ++i) {
        if(i >= 4) lv_obj_add_flag(page->date_rows[i], LV_OBJ_FLAG_HIDDEN);
        else {
            lv_obj_remove_flag(page->date_rows[i], LV_OBJ_FLAG_HIDDEN);
            const char *text = i == 0 ? (s_settings && s_settings->values.connect_enabled ? "ENABLE: ON (CONFIG ONLY)" : "ENABLE: OFF") :
                i == 1 ? modes[s_settings && s_settings->values.connect_mode <= 2U ? s_settings->values.connect_mode : 0U] :
                i == 2 ? "CONFIRM / BACK" : page->connect_name;
            ui_settings_label_set_text(lv_obj_get_child(page->date_rows[i], 0), text);
            row_style(page->date_rows[i], select_index == i, true);
        }
    }
    list_center(page->date_list, select_index, ROW_H);
}
/**
 * @brief 同步当前视图的列表可见性
 * @param page 当前设置页面
 */
static void view_sync(ui_settings_page_t *page)
{
    lv_obj_t *lists[26];
    const char *title = "";

    lists[0] = page->mycar_list;
    lists[1] = page->style_list;
    lists[2] = page->unit_list;
    lists[3] = page->card_list;
    lists[4] = page->info_list;
    lists[5] = page->drv_list;
    lists[6] = page->routine_list;
    lists[7] = page->eco_list;
    lists[8] = page->eco_hist_list;
    lists[9] = page->it_list;
    lists[10] = page->assist_list;
    lists[11] = page->drv_assist_list;
    lists[12] = page->system_list;
    lists[13] = page->version_list;
    lists[14] = page->vehicle_list;
    lists[15] = page->light_list;
    lists[16] = page->door_list;
    lists[17] = page->turn_list;
    lists[18] = page->wiper_list;
    lists[19] = page->service_list;
    lists[20] = page->service_item_list;
    lists[21] = page->service_edit_list;
    lists[22] = page->select_list;
    lists[23] = page->eps_list;
    lists[24] = page->parking_list;
    lists[25] = page->chassis_list;

    /* Only hide the previous branch when navigation changes. Hiding and showing
     * the current list on every key invalidates its entire viewport. */
    if(!page->view_ready || page->shown_view != page->st.view ||
       (page->st.view == UI_SETTINGS_VIEW_ROOT && page->shown_root != page->st.root_index)) {
        for(uint8_t i = 0U; i < 26U; ++i)
            lv_obj_add_flag(lists[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page->speed_warn_list, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page->date_list, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page->date_wheel_list, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page->factory_list, LV_OBJ_FLAG_HIDDEN);
        page->view_ready = true;
        page->shown_view = page->st.view;
        page->shown_root = page->st.root_index;
    }
    switch(page->st.view) {
    case UI_SETTINGS_VIEW_LANGUAGE:
    case UI_SETTINGS_VIEW_FACTORY_PASSWORD:
    case UI_SETTINGS_VIEW_FACTORY:
    case UI_SETTINGS_VIEW_FACTORY_FEATURES:
    case UI_SETTINGS_VIEW_FACTORY_ODO:
    case UI_SETTINGS_VIEW_FACTORY_TANK:
    case UI_SETTINGS_VIEW_FACTORY_MODEL:
        title = page->st.view == UI_SETTINGS_VIEW_LANGUAGE ? "LANGUAGE" :
            page->st.view == UI_SETTINGS_VIEW_FACTORY_PASSWORD ? "PASSWORD" :
            !s_cmd_cb ? "PLATFORM UNAVAILABLE" : "FACTORY SETTINGS";
        factory_render(page);
        break;
    case UI_SETTINGS_VIEW_CLEAR_LOG: {
        unsigned status = cleanup_status();
        page->shown_cleanup_status = status;
        const char *message = status == CAR_LOG_CLEANUP_RUNNING ? "CLEANING LOGS..." :
            status == CAR_LOG_CLEANUP_DONE ? "LOGS CLEARED" :
            status == CAR_LOG_CLEANUP_FAILED ? "FAILED / PARTIAL CLEANUP" :
            status == CAR_LOG_CLEANUP_UNAVAILABLE ? "PLATFORM UNAVAILABLE" : "LOG DIRECTORY CONTENTS ONLY";
        title = "CLEAR LOGS";
        lv_obj_remove_flag(page->date_list, LV_OBJ_FLAG_HIDDEN);
        for(unsigned i = 0; i < 9; ++i) {
            if(i >= 4) lv_obj_add_flag(page->date_rows[i], LV_OBJ_FLAG_HIDDEN);
            else {
                lv_obj_remove_flag(page->date_rows[i], LV_OBJ_FLAG_HIDDEN);
                ui_settings_label_set_text(lv_obj_get_child(page->date_rows[i], 0),
                    i == 0 ? "CONFIRM CLEAR LOGS" : i == 1 ? "CANCEL / BACK" :
                    i == 2 ? message : "CONFIG / RESOURCES / MD5 KEPT");
                row_style(page->date_rows[i], page->st.date_index == i,
                    i == 1 || (i == 0 && status != CAR_LOG_CLEANUP_RUNNING && status != CAR_LOG_CLEANUP_UNAVAILABLE));
            }
        }
        break;
    }
    case UI_SETTINGS_VIEW_CONNECT:
        title = "CONNECT: SDK UNAVAILABLE";
        connect_rows_render(page, page->st.date_index);
        break;
    case UI_SETTINGS_VIEW_DATE:
        /* Qt CentralTemplate.title：“时间设置” */
        title = "TIME SETTINGS";
        lv_obj_remove_flag(page->date_wheel_list, LV_OBJ_FLAG_HIDDEN);
        date_wheel_refresh(page);
        break;
    case UI_SETTINGS_VIEW_SYSTEM:
        lv_obj_remove_flag(page->system_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_VERSION:
        title = "VERSION INFO";
        lv_obj_remove_flag(page->version_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_MY_CAR:
        lv_obj_remove_flag(page->mycar_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_STYLE:
        title = "DIAL STYLE";
        lv_obj_remove_flag(page->style_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_UNIT:
        title = "UNITS";
        lv_obj_remove_flag(page->unit_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_CENTER_CARDS:
        title = "CENTER CARDS";
        lv_obj_remove_flag(page->card_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_DRIVING_INFO:
        title = "DRIVING INFO";
        lv_obj_remove_flag(page->drv_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_ROUTINE:
        title = "GENERAL SETTINGS";
        lv_obj_remove_flag(page->routine_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_ECO:
        title = "ECO SETTINGS";
        lv_obj_remove_flag(page->eco_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_ECO_HISTORY:
        /* 历史柱状图自带标题（Qt CentralTemplate.title“历史消耗”） */
        lv_obj_remove_flag(page->eco_hist_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_ITINERARY_ASSIST:
        title = "DRIVER ASSIST";
        lv_obj_remove_flag(page->assist_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_DRIVING_ASSIST:
        title = "DRIVING ASSIST";
        lv_obj_remove_flag(page->drv_assist_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_EPS:
        title = "ELECTRIC POWER STEERING";
        lv_obj_remove_flag(page->eps_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_PARKING:
        title = "PARKING ASSIST";
        lv_obj_remove_flag(page->parking_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_CHASSIS:
        title = "CHASSIS CONTROL";
        lv_obj_remove_flag(page->chassis_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_SPEED_WARN:
        title = "SPEED WARNING";
        lv_obj_remove_flag(page->speed_warn_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_VEHICLE:
        title = "VEHICLE SETTINGS";
        lv_obj_remove_flag(page->vehicle_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_LIGHT:
        title = "LIGHT SETTINGS";
        lv_obj_remove_flag(page->light_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_DOOR:
        title = "DOOR SETTINGS";
        lv_obj_remove_flag(page->door_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_TURN:
        title = "TURN LIGHT";
        lv_obj_remove_flag(page->turn_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_WIPER:
        title = "WIPER";
        lv_obj_remove_flag(page->wiper_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_SERVICE:
        title = "MAINTENANCE";
        lv_obj_remove_flag(page->service_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_SERVICE_ITEM:
        title = "MAINTENANCE";
        lv_obj_remove_flag(page->service_item_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_SERVICE_EDIT:
        title = "MAINTENANCE";
        lv_obj_remove_flag(page->service_edit_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_SELECT:
        if(page->st.select_kind == UI_SETTINGS_SELECT_SENSITIVITY) {
            title = "LIGHT SENSITIVITY";
        }
        else if(page->st.select_kind == UI_SETTINGS_SELECT_DELAY) {
            title = "LIGHT OFF DELAY";
        }
        else if(page->st.select_kind == UI_SETTINGS_SELECT_ANIMATION) {
            title = "BOOT ANIMATION - PLAYER UNAVAILABLE";
        }
        else {
            title = "AUTO DOOR LOCK";
        }
        lv_obj_remove_flag(page->select_list, LV_OBJ_FLAG_HIDDEN);
        break;
    case UI_SETTINGS_VIEW_ROOT:
    default:
        /* 焦点在根菜单分类上时，预览该分类的列表 */
        if(page->st.root_index == UI_SETTINGS_ROOT_MY_CAR) {
            lv_obj_remove_flag(page->mycar_list, LV_OBJ_FLAG_HIDDEN);
        }
        else if(page->st.root_index == UI_SETTINGS_ROOT_INFO) {
            lv_obj_remove_flag(page->info_list, LV_OBJ_FLAG_HIDDEN);
        }
        else if(page->st.root_index == UI_SETTINGS_ROOT_ITINERARY) {
            lv_obj_remove_flag(page->it_list, LV_OBJ_FLAG_HIDDEN);
        }
        else if(page->st.root_index == UI_SETTINGS_ROOT_SYSTEM) {
            lv_obj_remove_flag(page->system_list, LV_OBJ_FLAG_HIDDEN);
        }
        else if(page->st.root_index == UI_SETTINGS_ROOT_CONNECT) {
            if(page->connect_name[0] == '\0') {
                connect_read_name(page);
            }
            connect_rows_render(page, NONE);
        }
        break;
    }

    bool category_page = page->st.view == UI_SETTINGS_VIEW_ROOT ||
                         page->st.view == UI_SETTINGS_VIEW_MY_CAR ||
                         page->st.view == UI_SETTINGS_VIEW_SYSTEM ||
                         page->st.view == UI_SETTINGS_VIEW_CONNECT;
    int32_t content_y = category_page ? BODY_Y : SUBPAGE_BODY_Y;
    lv_obj_set_y(page->body, content_y);
    lv_obj_set_height(page->body, SETTINGS_H - content_y);
    if(category_page) {
        lv_obj_remove_flag(page->root_bar, LV_OBJ_FLAG_HIDDEN);
        title = "";
    }
    else {
        lv_obj_add_flag(page->root_bar, LV_OBJ_FLAG_HIDDEN);
    }

    if(strcmp(lv_label_get_text(page->title), title) != 0)
        ui_settings_label_set_text(page->title, title);
}

/* 选中项居中显示：Qt positionViewAtIndex(Center) */
static void list_center(lv_obj_t *list, uint8_t index, int32_t row_h)
{
    if(index == NONE) index = 0U;
    int32_t target = row_h * (int32_t)index - (lv_obj_get_height(list) - row_h) / 2;

    if(target < 0) {
        target = 0;
    }
    /* Keep the viewport fixed; translating it also moves its clipping rectangle. */
    lv_obj_scroll_to_y(list, target, LV_ANIM_OFF);
}

static void list_center_current(ui_settings_page_t *page)
{
    switch(page->st.view) {
    case UI_SETTINGS_VIEW_SYSTEM:
        list_center(page->system_list,
                    (page->st.system_index == NONE) ? 0U : page->st.system_index,
                    ROW_H);
        break;
    case UI_SETTINGS_VIEW_VERSION:
        break;
    case UI_SETTINGS_VIEW_MY_CAR:
        list_center(page->mycar_list, page->st.mycar_index, ROW_H);
        break;
    case UI_SETTINGS_VIEW_STYLE:
        list_center(page->style_list, page->st.style_index, STYLE_ROW_H);
        break;
    case UI_SETTINGS_VIEW_UNIT:
        list_center(page->unit_list,
                    (page->st.unit_index == NONE) ? 0U : page->st.unit_index,
                    UNIT_ROW_H);
        break;
    case UI_SETTINGS_VIEW_CENTER_CARDS:
        list_center(page->card_list, page->st.card_index, CARD_ROW_H);
        break;
    case UI_SETTINGS_VIEW_DRIVING_INFO:
        list_center(page->drv_list,
                    (page->st.drv_index == NONE) ? 0U : page->st.drv_index,
                    UNIT_ROW_H);
        break;
    case UI_SETTINGS_VIEW_ROUTINE:
        list_center(page->routine_list,
                    (page->st.routine_index == NONE) ? 0U : page->st.routine_index,
                    UNIT_ROW_H);
        break;
    case UI_SETTINGS_VIEW_ECO:
        list_center(page->eco_list,
                    (page->st.eco_index == NONE) ? 0U : page->st.eco_index,
                    UNIT_ROW_H);
        break;
    case UI_SETTINGS_VIEW_ECO_HISTORY:
        /* 单张卡片，不需要滚动居中 */
        break;
    case UI_SETTINGS_VIEW_ITINERARY_ASSIST:
        list_center(page->assist_list,
                    (page->st.assist_index == NONE) ? 0U : page->st.assist_index,
                    UNIT_ROW_H);
        break;
    case UI_SETTINGS_VIEW_DRIVING_ASSIST:
        list_center(page->drv_assist_list,
                    (page->st.driving_assist_index == NONE)
                        ? 0U : page->st.driving_assist_index,
                    UNIT_ROW_H);
        break;
    case UI_SETTINGS_VIEW_EPS:
        list_center(page->eps_list,
                    (page->st.eps_index == NONE) ? 0U : page->st.eps_index,
                    UNIT_ROW_H);
        break;
    case UI_SETTINGS_VIEW_PARKING:
        list_center(page->parking_list,
                    (page->st.parking_index == NONE) ? 0U
                                                     : page->st.parking_index,
                    UNIT_ROW_H);
        break;
    case UI_SETTINGS_VIEW_CHASSIS:
        list_center(page->chassis_list,
                    (page->st.chassis_index == NONE) ? 0U
                                                     : page->st.chassis_index,
                    UNIT_ROW_H);
        break;
    case UI_SETTINGS_VIEW_VEHICLE:
        list_center(page->vehicle_list,
                    (page->st.vehicle_index == NONE) ? 0U
                                                     : page->st.vehicle_index,
                    UNIT_ROW_H);
        break;
    case UI_SETTINGS_VIEW_LIGHT:
        list_center(page->light_list,
                    (page->st.light_index == NONE) ? 0U : page->st.light_index,
                    UNIT_ROW_H);
        break;
    case UI_SETTINGS_VIEW_DOOR:
        list_center(page->door_list,
                    (page->st.door_index == NONE) ? 0U : page->st.door_index,
                    UNIT_ROW_H);
        break;
    case UI_SETTINGS_VIEW_TURN:
        list_center(page->turn_list,
                    (page->st.turn_index == NONE) ? 0U : page->st.turn_index,
                    UNIT_ROW_H);
        break;
    case UI_SETTINGS_VIEW_WIPER:
        list_center(page->wiper_list,
                    (page->st.wiper_index == NONE) ? 0U : page->st.wiper_index,
                    UNIT_ROW_H);
        break;
    case UI_SETTINGS_VIEW_SERVICE:
        list_center(page->service_list,
                    (page->st.service_index == NONE) ? 0U : page->st.service_index,
                    ROW_H);
        break;
    case UI_SETTINGS_VIEW_SERVICE_ITEM:
        list_center(page->service_item_list,
                    (page->st.service_row == NONE) ? 0U : page->st.service_row,
                    UNIT_ROW_H);
        break;
    case UI_SETTINGS_VIEW_SERVICE_EDIT:
        /* 静态数字布局，不滚动 */
        break;
    case UI_SETTINGS_VIEW_SELECT:
        list_center(page->select_list,
                    (page->st.select_index == NONE) ? 0U : page->st.select_index,
                    UNIT_ROW_H);
        break;
    case UI_SETTINGS_VIEW_ROOT:
    default:
        /* 未锁定预览：列表从第一项开始 */
        list_center(page->mycar_list, 0U, ROW_H);
        list_center(page->info_list, 0U, ROW_H);
        list_center(page->it_list, 0U, ROW_H);
        break;
    }
}

void ui_settings_page_apply(ui_settings_page_t *page)
{
    if(page == NULL || page->root == NULL) {
        return;
    }

    view_sync(page);
    if(!lv_obj_has_flag(page->root_bar, LV_OBJ_FLAG_HIDDEN)) root_style(page);
#define UPDATE_VISIBLE(list, update) \
    do { if(!lv_obj_has_flag(page->list, LV_OBJ_FLAG_HIDDEN)) update(page); } while(0)
    UPDATE_VISIBLE(system_list, system_style);
    UPDATE_VISIBLE(mycar_list, mycar_style);
    UPDATE_VISIBLE(style_list, style_page_style);
    UPDATE_VISIBLE(unit_list, unit_page_style);
    UPDATE_VISIBLE(card_list, card_page_style);
    UPDATE_VISIBLE(info_list, info_page_style);
    UPDATE_VISIBLE(drv_list, drv_page_style);
    UPDATE_VISIBLE(routine_list, routine_page_style);
    UPDATE_VISIBLE(eco_list, eco_page_style);
    UPDATE_VISIBLE(eco_hist_list, eco_history_refresh);
    UPDATE_VISIBLE(it_list, it_page_style);
    UPDATE_VISIBLE(assist_list, assist_page_style);
    UPDATE_VISIBLE(drv_assist_list, drv_assist_page_style);
    UPDATE_VISIBLE(eps_list, eps_page_style);
    UPDATE_VISIBLE(parking_list, parking_page_style);
    UPDATE_VISIBLE(chassis_list, chassis_page_style);
    UPDATE_VISIBLE(vehicle_list, vehicle_page_style);
    UPDATE_VISIBLE(light_list, light_page_style);
    UPDATE_VISIBLE(door_list, door_page_style);
    UPDATE_VISIBLE(turn_list, turn_page_style);
    UPDATE_VISIBLE(wiper_list, wiper_page_style);
    UPDATE_VISIBLE(service_list, service_page_style);
    UPDATE_VISIBLE(service_item_list, service_item_page_style);
    UPDATE_VISIBLE(service_edit_list, service_edit_page_style);
    UPDATE_VISIBLE(select_list, select_page_style);
    UPDATE_VISIBLE(speed_warn_list, speed_warn_page_style);
    UPDATE_VISIBLE(version_list, version_refresh);
    UPDATE_VISIBLE(system_list, version_refresh);
#undef UPDATE_VISIBLE
    list_center_current(page);
    feature_visibility(page);
    ui_settings_translate(page->root, s_settings && s_settings->values.language == 0);
}

/* ---------------- 设置写入 ---------------- */

static void style_index_from_settings(ui_settings_page_t *page)
{
    page->st.style_index = 0U;

    if(s_settings == NULL) {
        return;
    }

    for(uint8_t i = 0U; i < style_back(); ++i) {
        const ui_style_desc_t *desc = ui_app_style_option_at(i);

        if(desc != NULL && strcmp(desc->id, s_settings->values.dial_style) == 0) {
            page->st.style_index = i;
        }
    }
}

/* Qt PageMyCarStyle：确认后 SettingsMgr.dialStyle = i，并重载表盘 */
static void apply_style(ui_settings_page_t *page)
{
    const ui_style_desc_t *desc;

    if(s_settings == NULL || page->st.style_index >= style_back()) {
        return;
    }

    desc = ui_app_style_option_at(page->st.style_index);
    if(desc == NULL) {
        return;
    }

    /* 与 31 共用同一个入口：保存设置 + 重载表盘 + MCU 重取数 */
    (void)ui_app_switch_style(desc->id);
}

/* Qt PageUnitSettings：OK 在选项间循环切换（立即改内存），确定返回 */
static void toggle_option(uint8_t row)
{
    uint8_t mileage = s_settings->values.unit_mileage;
    uint8_t temp = s_settings->values.unit_temperature;

    if(row == UNIT_ROW_MILEAGE) {
        for(uint8_t i = 0U; i < MILEAGE_COUNT; ++i) {
            if(s_mileage_options[i].value == mileage) {
                mileage = s_mileage_options[(i + 1U) % MILEAGE_COUNT].value;
                break;
            }
        }
    }
    else if(row == UNIT_ROW_TEMP) {
        for(uint8_t i = 0U; i < TEMP_COUNT; ++i) {
            if(s_temp_options[i].value == temp) {
                temp = s_temp_options[(i + 1U) % TEMP_COUNT].value;
                break;
            }
        }
    }
    else {
        return;
    }

    (void)car_settings_set_unit(s_settings, mileage, temp);
}

static void toggle_card(ui_settings_page_t *page)
{
    uint32_t mask;

    if(page->st.card_index >= CARD_BACK) {
        return;
    }

    mask = s_settings->values.center_card_mask ^ s_card_bits[page->st.card_index];
    (void)car_settings_set_center_card_mask(s_settings, mask);
}

/*
 * Qt PageGeneralSettings.adjustBrightness：
 * 每次 OK 加 10%，超过 100 回到 0（0~100 循环）。
 */
static uint8_t brightness_next(uint8_t value)
{
    return (value + 10U > 100U) ? 0U : (uint8_t)(value + 10U);
}

/* Qt sendBacklight(2, totalvalue, nightlightvalue)：mode 固定 2 */
static void routine_send_backlight(void)
{
    car_cmd_t cmd;

    if(s_cmd_cb == NULL || s_settings == NULL) {
        return;
    }
    if(car_cmd_set_backlight(&cmd, 2U, s_settings->values.daylight,
                             s_settings->values.nightlight)) {
        s_cmd_cb(&cmd);
    }
}

/* Qt sendWarnSpeed(0, on ? 1 : 0) */
static void routine_send_speed_limit(bool on)
{
    car_cmd_t cmd;

    if(s_cmd_cb == NULL) {
        return;
    }
    if(car_cmd_set_speed_warning(&cmd, 0U, on ? 1U : 0U)) {
        s_cmd_cb(&cmd);
    }
}

/* ---------------- 层级切换 ---------------- */

/* 回到根页初始状态（Qt reSettings 的 pageSettings.reset() 部分） */
static void pop_to_root(ui_settings_state_t *st)
{
    st->view = UI_SETTINGS_VIEW_ROOT;
    st->root_locked = false;
    st->root_index = NONE;
    st->mycar_locked = false;
    st->mycar_index = NONE;
    st->mycar_child = NONE;
    st->info_index = NONE;
    st->drv_index = NONE;
    st->routine_index = NONE;
    st->eco_index = NONE;
    st->it_index = NONE;
    st->assist_index = NONE;
    st->driving_assist_index = NONE;
    st->parking_index = NONE;
    st->chassis_index = NONE;
    st->vehicle_index = NONE;
    st->light_index = NONE;
    st->door_index = NONE;
    st->turn_index = NONE;
    st->wiper_index = NONE;
    st->service_index = NONE;
    st->service_row = NONE;
    st->service_digit_index = 0U;
    st->service_editing = false;
    st->select_index = NONE;
}

/* 子页弹出到我的座驾：childIndex = -1 */
static void pop_to_mycar(ui_settings_state_t *st)
{
    st->view = UI_SETTINGS_VIEW_MY_CAR;
    st->mycar_child = NONE;
}

bool ui_settings_page_back(ui_settings_page_t *page)
{
    ui_settings_state_t *st;

    if(page == NULL) {
        return false;
    }
    st = &page->st;

    switch(st->view) {
    case UI_SETTINGS_VIEW_FACTORY_FEATURES:
    case UI_SETTINGS_VIEW_FACTORY_ODO:
    case UI_SETTINGS_VIEW_FACTORY_TANK:
    case UI_SETTINGS_VIEW_FACTORY_MODEL:
        st->view = UI_SETTINGS_VIEW_FACTORY;
        st->factory_index = 0;
        break;
    case UI_SETTINGS_VIEW_LANGUAGE:
    case UI_SETTINGS_VIEW_FACTORY_PASSWORD:
    case UI_SETTINGS_VIEW_FACTORY:
        st->view = UI_SETTINGS_VIEW_SYSTEM;
        st->factory_unlocked = false;
        break;
    case UI_SETTINGS_VIEW_DRIVING_INFO:
        st->drv_index = NONE;
        /* 返回信息选项列表（Qt 顶部列表），焦点回到 DRIVING INFO */
        st->root_locked = true;
        st->info_index = UI_SETTINGS_INFO_DRIVING;
        break;
    case UI_SETTINGS_VIEW_DRIVING_ASSIST:
        /* 返回驾驶员辅助列表，焦点回到 DRIVING ASSIST */
        st->driving_assist_index = NONE;
        st->assist_index = UI_SETTINGS_ASSIST_DRIVING;
        st->view = UI_SETTINGS_VIEW_ITINERARY_ASSIST;
        break;
    case UI_SETTINGS_VIEW_EPS:
        /* 返回驾驶员辅助列表，焦点回到 ELECTRIC POWER STEERING */
        st->eps_index = NONE;
        st->assist_index = UI_SETTINGS_ASSIST_EPS;
        st->view = UI_SETTINGS_VIEW_ITINERARY_ASSIST;
        break;
    case UI_SETTINGS_VIEW_PARKING:
        /* 返回驾驶员辅助列表，焦点回到 PARKING ASSIST */
        st->parking_index = NONE;
        st->assist_index = UI_SETTINGS_ASSIST_PARKING;
        st->view = UI_SETTINGS_VIEW_ITINERARY_ASSIST;
        break;
    case UI_SETTINGS_VIEW_CHASSIS:
        /* 返回驾驶员辅助列表，焦点回到 CHASSIS CONTROL */
        st->chassis_index = NONE;
        st->assist_index = UI_SETTINGS_ASSIST_CHASSIS;
        st->view = UI_SETTINGS_VIEW_ITINERARY_ASSIST;
        break;
    case UI_SETTINGS_VIEW_ITINERARY_ASSIST:
        /* 返回行程资讯列表，焦点回到 DRIVING ASSIST */
        st->assist_index = NONE;
        st->root_locked = true;
        st->it_index = UI_SETTINGS_IT_ASSIST;
        st->view = UI_SETTINGS_VIEW_ROOT;
        break;
    case UI_SETTINGS_VIEW_LIGHT:
        /* 返回车辆设置，焦点回到 LIGHT SETTINGS */
        st->light_index = NONE;
        st->vehicle_index = UI_SETTINGS_VEH_LIGHT;
        st->view = UI_SETTINGS_VIEW_VEHICLE;
        break;
    case UI_SETTINGS_VIEW_DOOR:
        /* 返回车辆设置，焦点回到 DOORS */
        st->door_index = NONE;
        st->vehicle_index = UI_SETTINGS_VEH_DOOR;
        st->view = UI_SETTINGS_VIEW_VEHICLE;
        break;
    case UI_SETTINGS_VIEW_TURN:
        /* 返回车辆设置，焦点回到 TURN LIGHT */
        st->turn_index = NONE;
        st->vehicle_index = UI_SETTINGS_VEH_TURN;
        st->view = UI_SETTINGS_VIEW_VEHICLE;
        break;
    case UI_SETTINGS_VIEW_WIPER:
        /* 返回车辆设置，焦点回到 WIPER */
        st->wiper_index = NONE;
        st->vehicle_index = UI_SETTINGS_VEH_WIPER;
        st->view = UI_SETTINGS_VIEW_VEHICLE;
        break;
    case UI_SETTINGS_VIEW_SERVICE:
        /* 返回车辆设置，焦点回到 SERVICE */
        st->service_index = NONE;
        st->vehicle_index = UI_SETTINGS_VEH_SERVICE;
        st->view = UI_SETTINGS_VIEW_VEHICLE;
        break;
    case UI_SETTINGS_VIEW_SERVICE_ITEM:
        /* 返回保养主列表，焦点回到进入时的项目 */
        st->service_index = st->service_type;
        st->service_row = NONE;
        st->view = UI_SETTINGS_VIEW_SERVICE;
        break;
    case UI_SETTINGS_VIEW_SERVICE_EDIT:
        /* 退出数字编辑，回到保养子页 */
        st->service_editing = false;
        st->service_digit_index = 0U;
        st->view = UI_SETTINGS_VIEW_SERVICE_ITEM;
        break;
    case UI_SETTINGS_VIEW_SELECT:
        /* 返回父页，不改动设置 */
        if(st->select_kind == UI_SETTINGS_SELECT_ANIMATION) {
            st->view = UI_SETTINGS_VIEW_SYSTEM;
            st->system_index = 5U;
        }
        else if(st->select_kind == UI_SETTINGS_SELECT_AUTO_LOCK) {
            st->door_index = UI_SETTINGS_DOOR_AUTO_LOCK;
            st->view = UI_SETTINGS_VIEW_DOOR;
        }
        else {
            st->light_index = (st->select_kind == UI_SETTINGS_SELECT_SENSITIVITY)
                                  ? UI_SETTINGS_LIGHT_SENSITIVITY
                                  : UI_SETTINGS_LIGHT_DELAY;
            st->view = UI_SETTINGS_VIEW_LIGHT;
        }
        break;
    case UI_SETTINGS_VIEW_VEHICLE:
        /* 返回行程资讯列表，焦点回到 VEHICLE */
        st->vehicle_index = NONE;
        st->root_locked = true;
        st->it_index = UI_SETTINGS_IT_VEHICLE;
        st->view = UI_SETTINGS_VIEW_ROOT;
        break;
    case UI_SETTINGS_VIEW_SPEED_WARN:
        st->view = UI_SETTINGS_VIEW_ROUTINE;
        break;
    case UI_SETTINGS_VIEW_STYLE:
    case UI_SETTINGS_VIEW_UNIT:
    case UI_SETTINGS_VIEW_CENTER_CARDS:
    case UI_SETTINGS_VIEW_ROUTINE:
    case UI_SETTINGS_VIEW_ECO:
    case UI_SETTINGS_VIEW_ECO_HISTORY:
        pop_to_mycar(st);
        break;
    case UI_SETTINGS_VIEW_CONNECT:
        pop_to_root(st);
        break;
    case UI_SETTINGS_VIEW_CLEAR_LOG:
        st->view = UI_SETTINGS_VIEW_SYSTEM;
        st->system_index = 4U;
        return true;
    case UI_SETTINGS_VIEW_DATE:
        st->view = UI_SETTINGS_VIEW_SYSTEM;
        st->system_index = 1U;
        break;
    case UI_SETTINGS_VIEW_VERSION:
        st->view = UI_SETTINGS_VIEW_SYSTEM;
        st->system_index = UI_SETTINGS_SYSTEM_VERSION;
        break;
    case UI_SETTINGS_VIEW_SYSTEM:
        pop_to_root(st);
        st->system_index = NONE;
        break;
    case UI_SETTINGS_VIEW_MY_CAR:
        st->mycar_locked = false;
        st->mycar_index = NONE;
        pop_to_root(st);
        break;
    case UI_SETTINGS_VIEW_ROOT:
    default:
        /* 根页没有上一级：Qt 直接 reSettings 关闭设置 */
        return false;
    }

    ui_settings_page_apply(page);
    return true;
}

/* ---------------- Qt PageMainSettings ---------------- */

static void root_step(ui_settings_state_t *st, int8_t step)
{
    if(st->root_index == NONE) {
        st->root_index = 0U;
    }
    else {
        st->root_index = (uint8_t)(((int32_t)st->root_index + step +
                                    (int32_t)UI_SETTINGS_ROOT_COUNT) %
                                   (int32_t)UI_SETTINGS_ROOT_COUNT);
    }
}

static bool mycar_enabled(uint8_t index)
{
    return (index < UI_SETTINGS_MYCAR_COUNT) && s_mycar_enabled[index];
}

/* 我的座驾：跳过置灰项（Qt 隐藏项不参与焦点） */
static void mycar_step(ui_settings_state_t *st, int8_t step)
{
    uint8_t i;
    uint8_t index = st->mycar_index;

    if(!st->mycar_locked) {
        return;
    }

    if(index == NONE) {
        index = (step > 0) ? (uint8_t)(UI_SETTINGS_MYCAR_COUNT - 1U) : 0U;
    }

    for(i = 0U; i < UI_SETTINGS_MYCAR_COUNT; ++i) {
        index = (uint8_t)(((int32_t)index + step +
                           (int32_t)UI_SETTINGS_MYCAR_COUNT) %
                          (int32_t)UI_SETTINGS_MYCAR_COUNT);
        if(mycar_enabled(index)) {
            break;
        }
    }

    st->mycar_index = index;
}

/* 打开我的座驾子项；返回 true 表示进入子页 */
static bool mycar_open(ui_settings_page_t *page)
{
    ui_settings_state_t *st = &page->st;

    if(!mycar_enabled(st->mycar_index)) {
        return false;
    }

    st->mycar_child = st->mycar_index;

    if(st->mycar_index == UI_SETTINGS_MYCAR_BACK) {
        st->mycar_locked = false;
        st->mycar_index = NONE;
        st->mycar_child = NONE;
        pop_to_root(st);
        return false;
    }

    if(st->mycar_index == UI_SETTINGS_MYCAR_STYLE) {
        style_index_from_settings(page);
        st->view = UI_SETTINGS_VIEW_STYLE;
    }
    else if(st->mycar_index == UI_SETTINGS_MYCAR_UNIT) {
        st->unit_index = NONE;
        st->view = UI_SETTINGS_VIEW_UNIT;
    }
    else if(st->mycar_index == UI_SETTINGS_MYCAR_ROUTINE) {
        st->routine_index = NONE;
        st->view = UI_SETTINGS_VIEW_ROUTINE;
    }
    else if(st->mycar_index == UI_SETTINGS_MYCAR_ECO) {
        st->eco_index = NONE;
        st->view = UI_SETTINGS_VIEW_ECO;
    }
    else if(st->mycar_index == UI_SETTINGS_MYCAR_ECO_HISTORY) {
        st->view = UI_SETTINGS_VIEW_ECO_HISTORY;
    }
    else {
        st->card_index = 0U;
        st->view = UI_SETTINGS_VIEW_CENTER_CARDS;
    }

    return true;
}

/* 信息选项：2 项全部可用，-1 时按++落到第一项 */
static void info_step(ui_settings_state_t *st, int8_t step)
{
    if(!st->root_locked) {
        return;
    }

    st->info_index = (st->info_index == NONE)
                         ? 0U
                         : (uint8_t)(((int32_t)st->info_index + step +
                                      (int32_t)UI_SETTINGS_INFO_COUNT) %
                                     (int32_t)UI_SETTINGS_INFO_COUNT);
}

static bool it_enabled(uint8_t index)
{
    return (index < UI_SETTINGS_IT_COUNT) && s_it_enabled[index];
}

/* 行程资讯：跳过置灰项（Qt 隐藏项不参与焦点） */
static void it_step(ui_settings_state_t *st, int8_t step)
{
    uint8_t index;
    uint8_t i;

    if(!st->root_locked) {
        return;
    }

    index = st->it_index;
    if(index == NONE) {
        index = (step > 0) ? (uint8_t)(UI_SETTINGS_IT_COUNT - 1U) : 0U;
    }

    for(i = 0U; i < UI_SETTINGS_IT_COUNT; ++i) {
        index = (uint8_t)(((int32_t)index + step +
                           (int32_t)UI_SETTINGS_IT_COUNT) %
                          (int32_t)UI_SETTINGS_IT_COUNT);
        if(it_enabled(index)) {
            break;
        }
    }

    st->it_index = index;
}

static bool assist_enabled(uint8_t index)
{
    /* 电动助力转向按工厂开关 enableElectricPowerSteering（bit6）显示 */
    if(index == UI_SETTINGS_ASSIST_EPS) {
        return feature_on(6);
    }
    return (index < UI_SETTINGS_ASSIST_COUNT) && s_assist_enabled[index];
}

/* 驾驶员辅助：跳过工厂位关闭的行 */
static void assist_step(ui_settings_state_t *st, int8_t step)
{
    uint8_t index = st->assist_index;
    uint8_t i;

    if(index == NONE) {
        index = (step > 0) ? (uint8_t)(UI_SETTINGS_ASSIST_COUNT - 1U) : 0U;
    }

    for(i = 0U; i < UI_SETTINGS_ASSIST_COUNT; ++i) {
        index = (uint8_t)(((int32_t)index + step +
                           (int32_t)UI_SETTINGS_ASSIST_COUNT) %
                          (int32_t)UI_SETTINGS_ASSIST_COUNT);
        if(assist_enabled(index)) {
            break;
        }
    }

    st->assist_index = index;
}

/* Qt DrivingHelp 的 sendLightset：只发 0x16 SetGet，不在本地翻转 */
static void drv_assist_send(uint8_t index, bool on)
{
    car_cmd_t cmd;

    if(s_cmd_cb == NULL) {
        return;
    }
    if(car_cmd_set_driver_assist(&cmd, index, on)) {
        s_cmd_cb(&cmd);
    }
}

/* Qt sendLightset：车辆设置项只发 0x16 SetGet，显示等 0x86 回读 */
static void vehicle_send(uint8_t index, uint8_t status)
{
    car_cmd_t cmd;

    if(s_cmd_cb == NULL) {
        return;
    }
    if(car_cmd_set_vehicle_setting(&cmd, index, status)) {
        s_cmd_cb(&cmd);
    }
}

static bool veh_enabled(uint8_t index)
{
    return (index < UI_SETTINGS_VEH_COUNT) && s_veh_enabled[index];
}

/* 车辆设置：跳过置灰的未迁移项 */
static void veh_step(ui_settings_state_t *st, int8_t step)
{
    uint8_t index = st->vehicle_index;
    uint8_t i;

    if(index == NONE) {
        index = (step > 0) ? (uint8_t)(UI_SETTINGS_VEH_COUNT - 1U) : 0U;
    }

    for(i = 0U; i < UI_SETTINGS_VEH_COUNT; ++i) {
        index = (uint8_t)(((int32_t)index + step +
                           (int32_t)UI_SETTINGS_VEH_COUNT) %
                          (int32_t)UI_SETTINGS_VEH_COUNT);
        if(veh_enabled(index)) {
            break;
        }
    }

    st->vehicle_index = index;
}

/* 灯光设置/车门设置都是 3 行循环 */
static void light_step(ui_settings_state_t *st, int8_t step)
{
    st->light_index = (st->light_index == NONE)
                          ? ((step > 0) ? 0U : (uint8_t)(UI_SETTINGS_LIGHT_COUNT - 1U))
                          : (uint8_t)(((int32_t)st->light_index + step +
                                       (int32_t)UI_SETTINGS_LIGHT_COUNT) %
                                      (int32_t)UI_SETTINGS_LIGHT_COUNT);
}

static void door_step(ui_settings_state_t *st, int8_t step)
{
    st->door_index = (st->door_index == NONE)
                         ? ((step > 0) ? 0U : (uint8_t)(UI_SETTINGS_DOOR_COUNT - 1U))
                         : (uint8_t)(((int32_t)st->door_index + step +
                                      (int32_t)UI_SETTINGS_DOOR_COUNT) %
                                     (int32_t)UI_SETTINGS_DOOR_COUNT);
}

static void turn_step(ui_settings_state_t *st, int8_t step)
{
    st->turn_index = (st->turn_index == NONE)
                         ? ((step > 0) ? 0U : (uint8_t)(UI_SETTINGS_TURN_COUNT - 1U))
                         : (uint8_t)(((int32_t)st->turn_index + step +
                                      (int32_t)UI_SETTINGS_TURN_COUNT) %
                                     (int32_t)UI_SETTINGS_TURN_COUNT);
}

static void wiper_step(ui_settings_state_t *st, int8_t step)
{
    st->wiper_index = (st->wiper_index == NONE)
                          ? ((step > 0) ? 0U : (uint8_t)(UI_SETTINGS_WIPER_COUNT - 1U))
                          : (uint8_t)(((int32_t)st->wiper_index + step +
                                       (int32_t)UI_SETTINGS_WIPER_COUNT) %
                                      (int32_t)UI_SETTINGS_WIPER_COUNT);
}

static void service_step(ui_settings_state_t *st, int8_t step)
{
    st->service_index = (st->service_index == NONE)
                            ? ((step > 0) ? 0U
                                          : (uint8_t)(UI_SETTINGS_SERVICE_COUNT - 1U))
                            : (uint8_t)(((int32_t)st->service_index + step +
                                         (int32_t)UI_SETTINGS_SERVICE_COUNT) %
                                        (int32_t)UI_SETTINGS_SERVICE_COUNT);
}

static void service_row_step(ui_settings_state_t *st, int8_t step)
{
    st->service_row = (st->service_row == NONE)
                          ? ((step > 0) ? 0U
                                        : (uint8_t)(UI_SETTINGS_SVC_COUNT - 1U))
                          : (uint8_t)(((int32_t)st->service_row + step +
                                       (int32_t)UI_SETTINGS_SVC_COUNT) %
                                      (int32_t)UI_SETTINGS_SVC_COUNT);
}

/* 多选页步进：选项 + 末尾“返回” */
static void select_step(ui_settings_state_t *st, int8_t step)
{
    uint8_t count = 0U;
    uint8_t total;

    (void)select_options(st->select_kind, &count);
    total = (uint8_t)(count + 1U);
    st->select_index = (st->select_index == NONE)
                           ? ((step > 0) ? 0U : (uint8_t)(total - 1U))
                           : (uint8_t)(((int32_t)st->select_index + step +
                                        (int32_t)total) % (int32_t)total);
}

static bool handle_it_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        it_step(st, -1);
        break;
    case UI_KEY_DOWN:
        it_step(st, 1);
        break;
    case UI_KEY_OK:
        if(!st->root_locked) {
            st->root_locked = true;
            st->it_index = NONE;
            it_step(st, 1);
            break;
        }
        if(st->it_index == UI_SETTINGS_IT_BACK) {
            st->root_locked = false;
            st->it_index = NONE;
            pop_to_root(st);
        }
        else if(st->it_index == UI_SETTINGS_IT_ASSIST) {
            /* Qt PageItineraryItem → DrivePeopleHelp */
            st->assist_index = NONE;
            st->view = UI_SETTINGS_VIEW_ITINERARY_ASSIST;
        }
        else if(st->it_index == UI_SETTINGS_IT_VEHICLE) {
            /* Qt PageItineraryItem → CarSettings */
            st->vehicle_index = NONE;
            st->view = UI_SETTINGS_VIEW_VEHICLE;
        }
        break;
    default:
        return false;
    }

    return true;
}

static bool handle_it_assist_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        assist_step(st, -1);
        break;
    case UI_KEY_DOWN:
        assist_step(st, 1);
        break;
    case UI_KEY_OK:
        if(st->assist_index == NONE) {
            st->assist_index = UI_SETTINGS_ASSIST_DRIVING;
            break;
        }
        if(st->assist_index == UI_SETTINGS_ASSIST_BACK) {
            /* 返回行程资讯列表，焦点回到 DRIVING ASSIST */
            st->assist_index = NONE;
            st->root_locked = true;
            st->it_index = UI_SETTINGS_IT_ASSIST;
            st->view = UI_SETTINGS_VIEW_ROOT;
        }
        else if(st->assist_index == UI_SETTINGS_ASSIST_DRIVING) {
            /* Qt DrivePeopleHelp → DrivingHelp */
            st->driving_assist_index = NONE;
            st->view = UI_SETTINGS_VIEW_DRIVING_ASSIST;
        }
        else if(st->assist_index == UI_SETTINGS_ASSIST_EPS) {
            /* Qt DrivePeopleHelp → ElectricPowerStepping（工厂开关控制可见） */
            st->eps_index = NONE;
            st->view = UI_SETTINGS_VIEW_EPS;
        }
        else if(st->assist_index == UI_SETTINGS_ASSIST_PARKING) {
            /* Qt DrivePeopleHelp → ParkAssist */
            st->parking_index = NONE;
            st->view = UI_SETTINGS_VIEW_PARKING;
        }
        else if(st->assist_index == UI_SETTINGS_ASSIST_CHASSIS) {
            /* Qt DrivePeopleHelp → ChassistControl */
            st->chassis_index = NONE;
            st->view = UI_SETTINGS_VIEW_CHASSIS;
        }
        break;
    default:
        return false;
    }

    return true;
}

/* 电动助力转向：OK 按 MCU 回读值切换 0 常规 / 1 运动，显示等回读刷新 */
static bool handle_eps_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        st->eps_index = (st->eps_index == NONE || st->eps_index == 0U)
                            ? (uint8_t)(UI_SETTINGS_EPS_COUNT - 1U)
                            : (uint8_t)(st->eps_index - 1U);
        break;
    case UI_KEY_DOWN:
        st->eps_index = (st->eps_index == NONE)
                            ? 0U
                            : (uint8_t)((st->eps_index + 1U) %
                                        UI_SETTINGS_EPS_COUNT);
        break;
    case UI_KEY_OK:
        if(st->eps_index == NONE) {
            st->eps_index = 0U;
        }
        else if(st->eps_index == UI_SETTINGS_EPS_BACK) {
            st->eps_index = NONE;
            st->assist_index = UI_SETTINGS_ASSIST_EPS;
            st->view = UI_SETTINGS_VIEW_ITINERARY_ASSIST;
        }
        else if(st->eps_index == UI_SETTINGS_EPS_MODE) {
            vehicle_send(9U, (uint8_t)((s_eps_mode == 1U) ? 0U : 1U));
        }
        break;
    default:
        return false;
    }

    return true;
}

/* 驻车辅助：开关行取反，音量/距离/传感器 3 档循环，显示等 0x86 回读刷新 */
static bool handle_parking_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        st->parking_index = (st->parking_index == NONE || st->parking_index == 0U)
                                ? (uint8_t)(UI_SETTINGS_PARK_COUNT - 1U)
                                : (uint8_t)(st->parking_index - 1U);
        break;
    case UI_KEY_DOWN:
        st->parking_index = (st->parking_index == NONE)
                                ? 0U
                                : (uint8_t)((st->parking_index + 1U) %
                                            UI_SETTINGS_PARK_COUNT);
        break;
    case UI_KEY_OK:
        if(st->parking_index == NONE) {
            st->parking_index = 0U;
        }
        else if(st->parking_index == UI_SETTINGS_PARK_BACK) {
            st->parking_index = NONE;
            st->assist_index = UI_SETTINGS_ASSIST_PARKING;
            st->view = UI_SETTINGS_VIEW_ITINERARY_ASSIST;
        }
        else if(st->parking_index == UI_SETTINGS_PARK_MOVING_OBJECT) {
            vehicle_send(4U, (uint8_t)(s_pc_moving_object ? 0U : 1U));
        }
        else if(st->parking_index == UI_SETTINGS_PARK_FRONT_RADAR) {
            vehicle_send(5U, (uint8_t)(s_pc_front_radar ? 0U : 1U));
        }
        else if(st->parking_index == UI_SETTINGS_PARK_REAR_RADAR) {
            vehicle_send(6U, (uint8_t)(s_pc_rear_radar ? 0U : 1U));
        }
        else if(st->parking_index == UI_SETTINGS_PARK_DISPLAY) {
            vehicle_send(7U, (uint8_t)(s_pc_display ? 0U : 1U));
        }
        else if(st->parking_index == UI_SETTINGS_PARK_REAR_CROSS) {
            vehicle_send(26U, (uint8_t)(s_pc_rear_cross ? 0U : 1U));
        }
        else if(st->parking_index == UI_SETTINGS_PARK_VOLUME) {
            vehicle_send(17U, (uint8_t)((s_pc_radar_volume + 1U) % 3U));
        }
        else if(st->parking_index == UI_SETTINGS_PARK_DISTANCE) {
            vehicle_send(18U, (uint8_t)((s_pc_radar_distance + 1U) % 3U));
        }
        else if(st->parking_index == UI_SETTINGS_PARK_SENSOR) {
            vehicle_send(27U, (uint8_t)((s_pc_parking_sensor + 1U) % 3U));
        }
        break;
    default:
        return false;
    }

    return true;
}

/* 底盘控制：OK 取反发命令，显示等 0x86 回读刷新 */
static bool handle_chassis_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        st->chassis_index = (st->chassis_index == NONE || st->chassis_index == 0U)
                                ? (uint8_t)(UI_SETTINGS_CHASSIS_COUNT - 1U)
                                : (uint8_t)(st->chassis_index - 1U);
        break;
    case UI_KEY_DOWN:
        st->chassis_index = (st->chassis_index == NONE)
                                ? 0U
                                : (uint8_t)((st->chassis_index + 1U) %
                                            UI_SETTINGS_CHASSIS_COUNT);
        break;
    case UI_KEY_OK:
        if(st->chassis_index == NONE) {
            st->chassis_index = 0U;
        }
        else if(st->chassis_index == UI_SETTINGS_CHASSIS_BACK) {
            st->chassis_index = NONE;
            st->assist_index = UI_SETTINGS_ASSIST_CHASSIS;
            st->view = UI_SETTINGS_VIEW_ITINERARY_ASSIST;
        }
        else if(st->chassis_index == UI_SETTINGS_CHASSIS_TRACE) {
            vehicle_send(8U, (uint8_t)(s_pc_trace ? 0U : 1U));
        }
        else if(st->chassis_index == UI_SETTINGS_CHASSIS_BRAKE) {
            vehicle_send(19U, (uint8_t)(s_pc_engine_brake ? 0U : 1U));
        }
        break;
    default:
        return false;
    }

    return true;
}

/* 驾驶辅助：OK 按 MCU 当前值取反发命令；显示等 0x16 Status 回读刷新 */
static bool handle_driving_assist_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        st->driving_assist_index =
            (st->driving_assist_index == NONE ||
             st->driving_assist_index == 0U)
                ? (uint8_t)(UI_SETTINGS_DA_COUNT - 1U)
                : (uint8_t)(st->driving_assist_index - 1U);
        break;
    case UI_KEY_DOWN:
        st->driving_assist_index =
            (st->driving_assist_index == NONE)
                ? 0U
                : (uint8_t)((st->driving_assist_index + 1U) %
                            UI_SETTINGS_DA_COUNT);
        break;
    case UI_KEY_OK:
        if(st->driving_assist_index == NONE) {
            st->driving_assist_index = 0U;
        }
        else if(st->driving_assist_index == UI_SETTINGS_DA_BACK) {
            st->driving_assist_index = NONE;
            st->assist_index = UI_SETTINGS_ASSIST_DRIVING;
            st->view = UI_SETTINGS_VIEW_ITINERARY_ASSIST;
        }
        else if(st->driving_assist_index == UI_SETTINGS_DA_EMERGENCY) {
            drv_assist_send(1U, !s_da_emergency);
        }
        else if(st->driving_assist_index == UI_SETTINGS_DA_LANE) {
            drv_assist_send(2U, !s_da_lane);
        }
        else if(st->driving_assist_index == UI_SETTINGS_DA_BLIND) {
            drv_assist_send(3U, !s_da_blind);
        }
        break;
    default:
        return false;
    }

    return true;
}

/* 车辆设置：灯光设置/车门进入下一级，返回行程资讯列表 */
static bool handle_vehicle_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        veh_step(st, -1);
        break;
    case UI_KEY_DOWN:
        veh_step(st, 1);
        break;
    case UI_KEY_OK:
        if(st->vehicle_index == NONE) {
            st->vehicle_index = UI_SETTINGS_VEH_LIGHT;
            break;
        }
        if(st->vehicle_index == UI_SETTINGS_VEH_BACK) {
            /* 返回行程资讯列表，焦点回到 VEHICLE */
            st->vehicle_index = NONE;
            st->root_locked = true;
            st->it_index = UI_SETTINGS_IT_VEHICLE;
            st->view = UI_SETTINGS_VIEW_ROOT;
        }
        else if(st->vehicle_index == UI_SETTINGS_VEH_LIGHT) {
            /* Qt CarSettings → LightSettings */
            st->light_index = NONE;
            st->view = UI_SETTINGS_VIEW_LIGHT;
        }
        else if(st->vehicle_index == UI_SETTINGS_VEH_DOOR) {
            /* Qt CarSettings → DoorSetting */
            st->door_index = NONE;
            st->view = UI_SETTINGS_VIEW_DOOR;
        }
        else if(st->vehicle_index == UI_SETTINGS_VEH_TURN) {
            /* Qt CarSettings → TurnLightSet */
            st->turn_index = NONE;
            st->view = UI_SETTINGS_VIEW_TURN;
        }
        else if(st->vehicle_index == UI_SETTINGS_VEH_WIPER) {
            /* Qt CarSettings → WiperSet */
            st->wiper_index = NONE;
            st->view = UI_SETTINGS_VIEW_WIPER;
        }
        else if(st->vehicle_index == UI_SETTINGS_VEH_SERVICE) {
            /* Qt CarSettings → Service */
            st->service_index = NONE;
            st->view = UI_SETTINGS_VIEW_SERVICE;
        }
        else if(st->vehicle_index == UI_SETTINGS_VEH_REMOTE_START) {
            /* Qt CarSettings 顶部列表的远程启动发动机，0x16 index 23 */
            vehicle_send(23U, s_vs_remote_start ? 0U : 1U);
        }
        break;
    default:
        return false;
    }

    return true;
}

/* 灯光设置：OK 按 MCU 回读值取反下发 0x16，显示等 0x86 回读 */
static bool handle_light_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        light_step(st, -1);
        break;
    case UI_KEY_DOWN:
        light_step(st, 1);
        break;
    case UI_KEY_OK:
        if(st->light_index == NONE) {
            st->light_index = UI_SETTINGS_LIGHT_WELCOME;
        }
        else if(st->light_index == UI_SETTINGS_LIGHT_BACK) {
            st->light_index = NONE;
            st->vehicle_index = UI_SETTINGS_VEH_LIGHT;
            st->view = UI_SETTINGS_VIEW_VEHICLE;
        }
        else if(st->light_index == UI_SETTINGS_LIGHT_WELCOME) {
            vehicle_send(10U, s_vs_welcome ? 0U : 1U);
        }
        else if(st->light_index == UI_SETTINGS_LIGHT_AUTO_INTERIOR) {
            vehicle_send(11U, s_vs_interior ? 0U : 1U);
        }
        else if(st->light_index == UI_SETTINGS_LIGHT_SENSITIVITY) {
            /* Qt PageListSelect，4 个选项 -> 扩展 control 2 */
            st->select_kind = UI_SETTINGS_SELECT_SENSITIVITY;
            st->select_saved_index = s_vs_headlight_sensitivity;
            st->select_index = s_vs_headlight_sensitivity;
            st->view = UI_SETTINGS_VIEW_SELECT;
        }
        else if(st->light_index == UI_SETTINGS_LIGHT_DELAY) {
            /* Qt PageListSelect，8 个选项 -> 扩展 control 1 */
            st->select_kind = UI_SETTINGS_SELECT_DELAY;
            st->select_saved_index = s_vs_headlight_delay;
            st->select_index = s_vs_headlight_delay;
            st->view = UI_SETTINGS_VIEW_SELECT;
        }
        break;
    default:
        return false;
    }

    return true;
}

/*
 * 车门设置：开关项取反下发；自动解锁按 3 个选项循环，
 * 自动上锁进入通用多选页（对齐 Qt PageListSelect）。
 */
static bool handle_door_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        door_step(st, -1);
        break;
    case UI_KEY_DOWN:
        door_step(st, 1);
        break;
    case UI_KEY_OK:
        if(st->door_index == NONE) {
            st->door_index = UI_SETTINGS_DOOR_HORN;
        }
        else if(st->door_index == UI_SETTINGS_DOOR_BACK) {
            st->door_index = NONE;
            st->vehicle_index = UI_SETTINGS_VEH_DOOR;
            st->view = UI_SETTINGS_VIEW_VEHICLE;
        }
        else if(st->door_index == UI_SETTINGS_DOOR_HORN) {
            vehicle_send(15U, s_vs_horn ? 0U : 1U);
        }
        else if(st->door_index == UI_SETTINGS_DOOR_AUTO_UNLOCK) {
            vehicle_send(16U, (uint8_t)((s_vs_auto_unlock + 1U) % 3U));
        }
        else if(st->door_index == UI_SETTINGS_DOOR_EXTERNAL) {
            vehicle_send(13U, s_vs_external_doors ? 0U : 1U);
        }
        else if(st->door_index == UI_SETTINGS_DOOR_SINGLE_UNLOCK) {
            vehicle_send(14U, s_vs_single_unlock ? 0U : 1U);
        }
        else if(st->door_index == UI_SETTINGS_DOOR_SMART_KEY) {
            vehicle_send(24U, s_vs_smart_key ? 0U : 1U);
        }
        else if(st->door_index == UI_SETTINGS_DOOR_SELECT_UNLOCK) {
            vehicle_send(25U, s_vs_select_unlock ? 0U : 1U);
        }
        else if(st->door_index == UI_SETTINGS_DOOR_AUTO_LOCK) {
            /* Qt PageListSelect，3 个选项 -> index 28 */
            st->select_kind = UI_SETTINGS_SELECT_AUTO_LOCK;
            st->select_saved_index = s_vs_auto_lock;
            st->select_index = s_vs_auto_lock;
            st->view = UI_SETTINGS_VIEW_SELECT;
        }
        break;
    default:
        return false;
    }

    return true;
}

/* 多选确认：灵敏度/延时走扩展帧，自动上锁走 index 28 */
static void select_send(uint8_t kind, uint8_t index)
{
    car_cmd_t cmd;

    if(s_cmd_cb == NULL) {
        return;
    }
    if(kind == UI_SETTINGS_SELECT_AUTO_LOCK) {
        if(car_cmd_set_vehicle_setting(&cmd, 28U, index)) {
            s_cmd_cb(&cmd);
        }
        return;
    }

    uint8_t control = (kind == UI_SETTINGS_SELECT_SENSITIVITY) ? 2U : 1U;
    if(car_cmd_set_vehicle_setting_ext(&cmd, control, index)) {
        s_cmd_cb(&cmd);
    }
}

/* 通用多选页：OK 选中并立即返回父页（Qt confirmSelection -> closePage） */
static bool handle_select_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;
    uint8_t count = 0U;

    (void)select_options(st->select_kind, &count);

    switch(code) {
    case UI_KEY_UP:
        select_step(st, -1);
        break;
    case UI_KEY_DOWN:
        select_step(st, 1);
        break;
    case UI_KEY_OK:
        if(st->select_index == NONE) {
            st->select_index = st->select_saved_index;
        }
        else {
            if(st->select_index < count) {
                if(st->select_kind == UI_SETTINGS_SELECT_ANIMATION) {
                    (void)car_settings_set_animation_style(s_settings, st->select_index);
                }
                else {
                    select_send(st->select_kind, st->select_index);
                }
                st->select_saved_index = st->select_index;
            }
            if(st->select_kind == UI_SETTINGS_SELECT_ANIMATION) {
                st->view = UI_SETTINGS_VIEW_SYSTEM;
                st->system_index = 5U;
            }
            else if(st->select_kind == UI_SETTINGS_SELECT_AUTO_LOCK) {
                st->door_index = UI_SETTINGS_DOOR_AUTO_LOCK;
                st->view = UI_SETTINGS_VIEW_DOOR;
            }
            else {
                st->light_index =
                    (st->select_kind == UI_SETTINGS_SELECT_SENSITIVITY)
                        ? UI_SETTINGS_LIGHT_SENSITIVITY
                        : UI_SETTINGS_LIGHT_DELAY;
                st->view = UI_SETTINGS_VIEW_LIGHT;
            }
        }
        break;
    default:
        return false;
    }

    return true;
}

/* Qt sendServiceKM：0x36 保养命令，set_km 时带 3 字节公里数 */
static void service_send(uint8_t type, uint32_t km, bool set_km)
{
    car_cmd_t cmd;

    if(s_cmd_cb == NULL) {
        return;
    }
    if(car_cmd_set_service_km(&cmd, type, km, set_km)) {
        s_cmd_cb(&cmd);
    }
}

/* 转向灯：OK 按 MCU 回读值取反下发 0x16 index 12 */
static bool handle_turn_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        turn_step(st, -1);
        break;
    case UI_KEY_DOWN:
        turn_step(st, 1);
        break;
    case UI_KEY_OK:
        if(st->turn_index == NONE) {
            st->turn_index = UI_SETTINGS_TURN_LANE_CHANGE;
        }
        else if(st->turn_index == UI_SETTINGS_TURN_BACK) {
            st->turn_index = NONE;
            st->vehicle_index = UI_SETTINGS_VEH_TURN;
            st->view = UI_SETTINGS_VIEW_VEHICLE;
        }
        else if(st->turn_index == UI_SETTINGS_TURN_LANE_CHANGE) {
            vehicle_send(12U, s_vs_lane_change ? 0U : 1U);
        }
        break;
    default:
        return false;
    }

    return true;
}

/* 雨刷器：OK 按 MCU 回读值取反下发 0x16 index 20 / 21 / 22 */
static bool handle_wiper_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        wiper_step(st, -1);
        break;
    case UI_KEY_DOWN:
        wiper_step(st, 1);
        break;
    case UI_KEY_OK:
        if(st->wiper_index == NONE) {
            st->wiper_index = UI_SETTINGS_WIPER_SPEED_SENSE;
        }
        else if(st->wiper_index == UI_SETTINGS_WIPER_BACK) {
            st->wiper_index = NONE;
            st->vehicle_index = UI_SETTINGS_VEH_WIPER;
            st->view = UI_SETTINGS_VIEW_VEHICLE;
        }
        else if(st->wiper_index == UI_SETTINGS_WIPER_SPEED_SENSE) {
            vehicle_send(20U, s_vs_wiper_speed ? 0U : 1U);
        }
        else if(st->wiper_index == UI_SETTINGS_WIPER_REAR_LINKAGE) {
            vehicle_send(21U, s_vs_wiper_rear ? 0U : 1U);
        }
        else if(st->wiper_index == UI_SETTINGS_WIPER_MIST) {
            vehicle_send(22U, s_vs_wiper_mist ? 0U : 1U);
        }
        break;
    default:
        return false;
    }

    return true;
}

/* 保养主列表：进入机油 / 轮胎 / 自定义子页 */
static bool handle_service_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        service_step(st, -1);
        break;
    case UI_KEY_DOWN:
        service_step(st, 1);
        break;
    case UI_KEY_OK:
        if(st->service_index == NONE) {
            st->service_index = UI_SETTINGS_SERVICE_OIL;
        }
        else if(st->service_index == UI_SETTINGS_SERVICE_BACK) {
            st->service_index = NONE;
            st->vehicle_index = UI_SETTINGS_VEH_SERVICE;
            st->view = UI_SETTINGS_VIEW_VEHICLE;
        }
        else if(st->service_index <= UI_SETTINGS_SERVICE_CUSTOM) {
            st->service_type = st->service_index;
            st->service_row = NONE;
            st->view = UI_SETTINGS_VIEW_SERVICE_ITEM;
        }
        break;
    default:
        return false;
    }

    return true;
}

/* 保养子页：对齐 Qt PageOilChangeSetting 第一层 */
static bool handle_service_item_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        service_row_step(st, -1);
        break;
    case UI_KEY_DOWN:
        service_row_step(st, 1);
        break;
    case UI_KEY_OK:
        if(st->service_row == NONE) {
            st->service_row = UI_SETTINGS_SVC_SET_KM;
        }
        else if(st->service_row == UI_SETTINGS_SVC_BACK) {
            st->service_index = st->service_type;
            st->service_row = NONE;
            st->view = UI_SETTINGS_VIEW_SERVICE;
        }
        else if(st->service_row == UI_SETTINGS_SVC_SET_KM) {
            uint32_t km = s_svc_set_km[st->service_type % 3U];

            for(uint8_t i = 0U; i < UI_SETTINGS_SVC_DIGITS; ++i) {
                uint32_t div = 1U;

                for(uint8_t j = 0U; j < (UI_SETTINGS_SVC_DIGITS - 1U - i); ++j) {
                    div *= 10U;
                }
                st->service_digits[i] = (uint8_t)((km / div) % 10U);
            }
            st->service_digit_index = 0U;
            st->service_editing = true;
            st->view = UI_SETTINGS_VIEW_SERVICE_EDIT;
        }
        else if(st->service_row == UI_SETTINGS_SVC_REMAIN_KM) {
            service_send(st->service_type, 0U, false);
        }
        else if(st->service_row == UI_SETTINGS_SVC_RESET) {
            service_send(st->service_type, 1U, false);
        }
        break;
    default:
        return false;
    }

    return true;
}

/* 保养数字编辑：OK 选中的位 +1；确认累加后下发 0x36 */
static bool handle_service_edit_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        st->service_digit_index =
            (st->service_digit_index == 0U)
                ? (uint8_t)(UI_SETTINGS_SVC_EDIT_COUNT - 1U)
                : (uint8_t)(st->service_digit_index - 1U);
        break;
    case UI_KEY_DOWN:
        st->service_digit_index =
            (uint8_t)((st->service_digit_index + 1U) % UI_SETTINGS_SVC_EDIT_COUNT);
        break;
    case UI_KEY_OK:
        if(st->service_digit_index < UI_SETTINGS_SVC_DIGITS) {
            uint8_t *digit = &st->service_digits[st->service_digit_index];

            *digit = (uint8_t)((*digit + 1U) % 10U);
        }
        else if(st->service_digit_index == UI_SETTINGS_SVC_EDIT_CONFIRM) {
            uint32_t km = 0U;

            for(uint8_t i = 0U; i < UI_SETTINGS_SVC_DIGITS; ++i) {
                km = km * 10U + st->service_digits[i];
            }
            s_svc_set_km[st->service_type % 3U] = km;
            service_send(st->service_type, km, true);
            st->service_editing = false;
            st->service_digit_index = 0U;
            st->service_row = UI_SETTINGS_SVC_SET_KM;
            st->view = UI_SETTINGS_VIEW_SERVICE_ITEM;
        }
        else {
            st->service_editing = false;
            st->service_digit_index = 0U;
            st->view = UI_SETTINGS_VIEW_SERVICE_ITEM;
        }
        break;
    default:
        return false;
    }

    return true;
}

static void system_style(ui_settings_page_t *page)
{
    char text[48];
    const car_clock_t *clock = &page->date_clock;

    /* Qt ItemDetails：datetime 行 desc 显示 YYYY.M.D HH:MM / HH:MM / --.-- */
    if(clock->valid && page->time_source == 2U) {
        (void)snprintf(text, sizeof(text), "%u.%u.%u %02u:%02u", (unsigned)clock->year,
                       (unsigned)clock->month, (unsigned)clock->day,
                       (unsigned)clock->hour, (unsigned)clock->minute);
    }
    else if(clock->valid && page->time_source == 1U) {
        (void)snprintf(text, sizeof(text), "%02u:%02u", (unsigned)clock->hour,
                       (unsigned)clock->minute);
    }
    else {
        (void)strcpy(text, "--.--");
    }
    ui_settings_label_set_text(lv_obj_get_child(page->system_rows[1], 1), text);
    unsigned language = s_settings ? s_settings->values.language : 1U;
    ui_settings_label_set_text(lv_obj_get_child(page->system_rows[0], 1),
                      language == 0U ? "简体中文" : language_names[language < 19U ? language : 1U]);
    unsigned animation = s_settings ? s_settings->values.animation_style : 0U;
    ui_settings_label_set_text(lv_obj_get_child(page->system_rows[5], 1),
                      animation == 2U ? "OFF" : animation == 1U ? "ANIMATION 1" : "DEFAULT");

    for(uint8_t i = 0U; i < UI_SETTINGS_SYSTEM_COUNT; ++i) {
        row_style(page->system_rows[i], page->st.system_index == i,
                  true);
        lv_obj_t *summary = lv_obj_get_child(page->system_rows[i], 1);
        lv_color_t color = lv_color_hex(page->st.system_index == i ? COLOR_YELLOW : UI_COLOR_WHITE);
        if(summary && lv_color_to_u32(lv_obj_get_style_text_color(summary, 0)) != lv_color_to_u32(color))
            lv_obj_set_style_text_color(summary, color, 0);
    }
}

static void system_step(ui_settings_state_t *st, int8_t step)
{
    uint8_t index = st->system_index;

    if(index == NONE) {
        st->system_index = (step > 0) ? 0U :
                                        UI_SETTINGS_SYSTEM_BACK;
        return;
    }
    do {
        index = (uint8_t)(((int32_t)index + step + UI_SETTINGS_SYSTEM_COUNT) %
                          UI_SETTINGS_SYSTEM_COUNT);
    } while(false);
    st->system_index = index;
}

/*
 * 进入日期时间页（Qt PageDateSettings.Component.onCompleted +
 * SettingsMgr.onTimeSourceChanged）：时间源=仪表时初始化 5 列滚轮草稿，
 * 光标停在时间源行；其它时间源只有时间源/返回两项，光标停在第一项。
 */
static void date_page_enter(ui_settings_page_t *page)
{
    ui_settings_state_t *st = &page->st;

    st->view = UI_SETTINGS_VIEW_DATE;
    st->date_focus = NONE;
    st->date_child_back = false;
    /* 草稿内部全部 0 基：年索引、月索引、日索引、时、分（Qt Tumbler 数组下标） */
    st->date_values[0] = page->date_clock.valid ? page->date_clock.year : 2026U;
    st->date_values[1] = (uint16_t)((page->date_clock.valid ? page->date_clock.month : 1U) - 1U);
    st->date_values[2] = (uint16_t)((page->date_clock.valid ? page->date_clock.day : 1U) - 1U);
    st->date_values[3] = page->date_clock.valid ? page->date_clock.hour : 0U;
    st->date_values[4] = page->date_clock.valid ? page->date_clock.minute : 0U;
    date_wheel_reset(page, st->date_values[0]);
    st->date_values[0] = (uint16_t)(st->date_values[0] - st->date_year_base);
    date_day_clamp(page);
    /* Qt sourceIndex：仪表模式为 2；否则为 0（首项） */
    st->date_item = page->time_source == 2U ? 2U : 0U;
}

static bool handle_system_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
    case UI_KEY_LEFT:
        system_step(st, -1);
        break;
    case UI_KEY_DOWN:
    case UI_KEY_RIGHT:
        system_step(st, 1);
        break;
    case UI_KEY_OK:
        if(st->system_index == NONE) {
            system_step(st, 1);
        }
        else if(st->system_index == 0U || st->system_index == 3U) {
            st->view = st->system_index == 0U ? UI_SETTINGS_VIEW_LANGUAGE : UI_SETTINGS_VIEW_FACTORY_PASSWORD;
            st->factory_index = 0;
            st->factory_unlocked = false;
            st->factory_error = false;
            memset(st->factory_digits, 0, sizeof(st->factory_digits));
        }
        else if(st->system_index == 1U) {
            date_page_enter(page);
        }
        else if(st->system_index == UI_SETTINGS_SYSTEM_VERSION) {
            st->view = UI_SETTINGS_VIEW_VERSION;
        }
        else if(st->system_index == 4U) {
            st->view = UI_SETTINGS_VIEW_CLEAR_LOG;
            st->date_index = 1U;
        }
        else if(st->system_index == 5U) {
            st->select_kind = UI_SETTINGS_SELECT_ANIMATION;
            st->select_saved_index = s_settings ? s_settings->values.animation_style : 0U;
            st->select_index = st->select_saved_index;
            st->view = UI_SETTINGS_VIEW_SELECT;
        }
        else if(st->system_index == UI_SETTINGS_SYSTEM_BACK) {
            pop_to_root(st);
            st->system_index = NONE;
        }
        break;
    default:
        return false;
    }
    return true;
}

static void connect_read_name(ui_settings_page_t *page)
{
    char line[256];
    snprintf(page->connect_name, sizeof(page->connect_name), "CAR-KIT");
    FILE *file = fopen("/usr/goc/bt_conf.ini", "r");
    if(!file) return;
    while(fgets(line, sizeof(line), file)) {
        char *key = line;
        while(*key == ' ' || *key == '\t') ++key;
        if(*key == '#' || *key == ';') continue;
        char *equal = strchr(key, '=');
        if(!equal) continue;
        char *end = equal;
        while(end > key && (end[-1] == ' ' || end[-1] == '\t')) --end;
        *end = '\0';
        if(strcmp(key, "localname") != 0) continue;
        char *value = equal + 1;
        while(*value == ' ' || *value == '\t') ++value;
        end = value + strlen(value);
        while(end > value && (end[-1] == ' ' || end[-1] == '\t' ||
                             end[-1] == '\r' || end[-1] == '\n')) --end;
        *end = '\0';
        if(*value) snprintf(page->connect_name, sizeof(page->connect_name), "%s", value);
        break;
    }
    fclose(file);
}

static bool handle_root_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
    case UI_KEY_LEFT:
        root_step(st, -1);
        break;
    case UI_KEY_DOWN:
    case UI_KEY_RIGHT:
        root_step(st, 1);
        break;
    case UI_KEY_OK:
        /* 第一次 OK 只是进入分类；未迁移分类不进，避免假入口 */
        if(st->root_index == UI_SETTINGS_ROOT_MY_CAR) {
            st->root_locked = true;
            st->view = UI_SETTINGS_VIEW_MY_CAR;
            st->mycar_locked = true;
            st->mycar_index = NONE;
            st->mycar_child = NONE;
            /* Qt：-1 时按++取模落到第一项 */
            mycar_step(st, 1);
        }
        else if(st->root_index == UI_SETTINGS_ROOT_INFO) {
            st->root_locked = true;
            st->view = UI_SETTINGS_VIEW_ROOT;
            st->info_index = NONE;
            info_step(st, 1);
        }
        else if(st->root_index == UI_SETTINGS_ROOT_CONNECT) {
            connect_read_name(page);
            st->view = UI_SETTINGS_VIEW_CONNECT;
            st->date_index = 0U;
            st->date_values[0] = s_settings ? s_settings->values.connect_mode : 0U;
        }
        else if(st->root_index == UI_SETTINGS_ROOT_SYSTEM) {
            st->view = UI_SETTINGS_VIEW_SYSTEM;
            st->system_index = NONE;
        }
        else if(st->root_index == UI_SETTINGS_ROOT_ITINERARY) {
            /* Qt PageMainSettings → PageItineraryItem（列表仍在根页内） */
            st->root_locked = true;
            st->view = UI_SETTINGS_VIEW_ROOT;
            st->it_index = NONE;
            it_step(st, 1);
        }
        break;
    default:
        return false;
    }

    return true;
}

static bool handle_mycar_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        mycar_step(st, -1);
        break;
    case UI_KEY_DOWN:
        mycar_step(st, 1);
        break;
    case UI_KEY_OK:
        if(!st->mycar_locked) {
            st->mycar_locked = true;
            st->mycar_index = NONE;
            mycar_step(st, 1);
            break;
        }
        (void)mycar_open(page);
        break;
    default:
        return false;
    }

    return true;
}

static bool handle_style_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        st->style_index = (st->style_index == 0U)
                              ? style_back()
                              : (uint8_t)(st->style_index - 1U);
        break;
    case UI_KEY_DOWN:
        st->style_index = (uint8_t)((st->style_index + 1U) % (style_back() + 1U));
        break;
    case UI_KEY_OK:
        if(st->style_index >= style_back()) {
            pop_to_mycar(st);
        }
        else {
            apply_style(page);
        }
        break;
    default:
        return false;
    }

    return true;
}

static bool handle_unit_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        st->unit_index = (st->unit_index == NONE || st->unit_index == 0U)
                             ? (uint8_t)(UNIT_ROW_COUNT - 1U)
                             : (uint8_t)(st->unit_index - 1U);
        break;
    case UI_KEY_DOWN:
        st->unit_index = (st->unit_index == NONE)
                             ? 0U
                             : (uint8_t)((st->unit_index + 1U) % UNIT_ROW_COUNT);
        break;
    case UI_KEY_OK:
        if(st->unit_index == NONE) {
            st->unit_index = 0U;
        }
        else if(st->unit_index == UNIT_ROW_CONFIRM) {
            /* 切换时已写入设置模型，确定只负责返回（Qt goBack） */
            pop_to_mycar(st);
        }
        else {
            toggle_option(st->unit_index);
        }
        break;
    default:
        return false;
    }

    return true;
}

static bool handle_card_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        st->card_index = (st->card_index == 0U) ? CARD_BACK
                                                : (uint8_t)(st->card_index - 1U);
        break;
    case UI_KEY_DOWN:
        st->card_index = (uint8_t)((st->card_index + 1U) % (CARD_BACK + 1U));
        break;
    case UI_KEY_OK:
        if(st->card_index >= CARD_BACK) {
            pop_to_mycar(st);
        }
        else {
            toggle_card(page);
        }
        break;
    default:
        return false;
    }

    return true;
}

static bool handle_info_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        info_step(st, -1);
        break;
    case UI_KEY_DOWN:
        info_step(st, 1);
        break;
    case UI_KEY_OK:
        if(!st->root_locked) {
            st->root_locked = true;
            st->info_index = NONE;
            info_step(st, 1);
            break;
        }
        if(st->info_index == UI_SETTINGS_INFO_BACK) {
            st->root_locked = false;
            st->info_index = NONE;
            pop_to_root(st);
        }
        else {
            /* Qt PageSettingDrivingInfo：顶部列表选下一级，-1 时落到第一项 */
            st->drv_index = NONE;
            st->view = UI_SETTINGS_VIEW_DRIVING_INFO;
        }
        break;
    default:
        return false;
    }

    return true;
}
//调用 Qt CarSettings 接口，0x16 index 0/1/2，取反下发
static bool handle_driving_info_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        st->drv_index = (st->drv_index == NONE || st->drv_index == 0U)
                            ? (uint8_t)(UI_SETTINGS_DRV_COUNT - 1U)
                            : (uint8_t)(st->drv_index - 1U);
        break;
    case UI_KEY_DOWN:
        st->drv_index = (st->drv_index == NONE)
                            ? 0U
                            : (uint8_t)((st->drv_index + 1U) % UI_SETTINGS_DRV_COUNT);
        break;
    case UI_KEY_OK:
        if(st->drv_index == NONE) {
            st->drv_index = 0U;
        }
        else if(st->drv_index == UI_SETTINGS_DRV_BACK) {
            st->drv_index = NONE;
            st->view = UI_SETTINGS_VIEW_ROOT;
        }
        else if(s_settings != NULL) {
            if(st->drv_index == UI_SETTINGS_DRV_OUT_TEMP) {
                (void)car_settings_set_out_temperature(
                    s_settings, s_settings->values.out_temperature == 0U);
            }
            else if(st->drv_index == UI_SETTINGS_DRV_GEAR) {
                (void)car_settings_set_gear_on(
                    s_settings, s_settings->values.gear_on == 0U);
            }
            else {
                (void)car_settings_set_avg_fuel_on(
                    s_settings, s_settings->values.avg_fuel_on == 0U);
            }
        }
        break;
    default:
        return false;
    }

    return true;
}

/* 常规设置：上下循环 5 项；OK 对亮度步进、对超速开关取反 */
static void speed_warn_open(ui_settings_page_t *page)
{
    uint8_t speed = page->speed_warn_kph;

    page->st.speed_warn_digits[0] = (uint8_t)(speed / 100U);
    page->st.speed_warn_digits[1] = (uint8_t)((speed / 10U) % 10U);
    page->st.speed_warn_digits[2] = (uint8_t)(speed % 10U);
    page->st.speed_warn_digit_index = 0U;
    page->st.view = UI_SETTINGS_VIEW_SPEED_WARN;
}

static bool handle_speed_warn_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;
    uint8_t i = st->speed_warn_digit_index;

    switch(code) {
    case UI_KEY_LEFT:
        st->speed_warn_digit_index = (uint8_t)((i + 2U) % 3U);
        break;
    case UI_KEY_RIGHT:
        st->speed_warn_digit_index = (uint8_t)((i + 1U) % 3U);
        break;
    case UI_KEY_UP: {
        uint8_t max = (i == 0U) ? 2U : 9U;

        if(i == 1U && st->speed_warn_digits[0] == 2U) {
            max = 5U;
        }
        st->speed_warn_digits[i] = (st->speed_warn_digits[i] >= max)
                                       ? 0U
                                       : (uint8_t)(st->speed_warn_digits[i] + 1U);
        if(i == 0U && st->speed_warn_digits[0] == 2U &&
           st->speed_warn_digits[1] > 5U) {
            st->speed_warn_digits[1] = 5U;
        }
        break;
    }
    case UI_KEY_DOWN: {
        uint8_t max = (i == 0U) ? 2U : 9U;

        if(i == 1U && st->speed_warn_digits[0] == 2U) {
            max = 5U;
        }
        st->speed_warn_digits[i] = (st->speed_warn_digits[i] == 0U)
                                       ? max
                                       : (uint8_t)(st->speed_warn_digits[i] - 1U);
        break;
    }
    case UI_KEY_OK: {
        car_cmd_t cmd;
        uint8_t speed = (uint8_t)(st->speed_warn_digits[0] * 100U +
                                  st->speed_warn_digits[1] * 10U +
                                  st->speed_warn_digits[2]);

        if(s_cmd_cb != NULL && car_cmd_set_speed_warning(&cmd, 1U, speed)) {
            s_cmd_cb(&cmd);
            page->speed_warn_kph = speed;
            s_speed_warn_valid = true;
        }
        st->view = UI_SETTINGS_VIEW_ROUTINE;
        break;
    }
    default:
        return false;
    }
    return true;
}

static bool handle_routine_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        st->routine_index = (st->routine_index == NONE || st->routine_index == 0U)
                                ? (uint8_t)(UI_SETTINGS_ROUTINE_COUNT - 1U)
                                : (uint8_t)(st->routine_index - 1U);
        break;
    case UI_KEY_DOWN:
        st->routine_index =
            (st->routine_index == NONE)
                ? 0U
                : (uint8_t)((st->routine_index + 1U) % UI_SETTINGS_ROUTINE_COUNT);
        break;
    case UI_KEY_OK:
        if(st->routine_index == NONE) {
            st->routine_index = 0U;
        }
        else if(st->routine_index == UI_SETTINGS_ROUTINE_BACK) {
            pop_to_mycar(st);
        }
        else if(st->routine_index == UI_SETTINGS_ROUTINE_SPEED_WARN) {
            if(s_settings != NULL && s_settings->values.speed_limit_on != 0U) {
                speed_warn_open(page);
            }
            else {
                st->routine_index = UI_SETTINGS_ROUTINE_SPEED_LIMIT;
            }
        }
        else if(s_settings != NULL) {
            if(st->routine_index == UI_SETTINGS_ROUTINE_DAYLIGHT) {
                (void)car_settings_set_brightness(
                    s_settings, brightness_next(s_settings->values.daylight),
                    s_settings->values.nightlight);
                routine_send_backlight();
            }
            else if(st->routine_index == UI_SETTINGS_ROUTINE_NIGHTLIGHT) {
                (void)car_settings_set_brightness(
                    s_settings, s_settings->values.daylight,
                    brightness_next(s_settings->values.nightlight));
                routine_send_backlight();
            }
            else if(st->routine_index == UI_SETTINGS_ROUTINE_SPEED_LIMIT) {
                bool on = s_settings->values.speed_limit_on == 0U;

                (void)car_settings_set_speed_limit_on(s_settings, on);
                routine_send_speed_limit(on);
            }
        }
        break;
    default:
        return false;
    }

    return true;
}

/*
 * ECO 设置：显示（油门引导 / 油耗）+ ECO 指示 + 返回。
 * Qt 的“显示”是 PageListSelect 子页，选项只有两项，这里左右键直接切换，
 * OK 在两项之间翻转。
 */
static bool handle_eco_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;

    switch(code) {
    case UI_KEY_UP:
        st->eco_index = (st->eco_index == NONE || st->eco_index == 0U)
                            ? (uint8_t)(UI_SETTINGS_ECO_COUNT - 1U)
                            : (uint8_t)(st->eco_index - 1U);
        break;
    case UI_KEY_DOWN:
        st->eco_index = (st->eco_index == NONE)
                            ? 0U
                            : (uint8_t)((st->eco_index + 1U) % UI_SETTINGS_ECO_COUNT);
        break;
    case UI_KEY_OK:
        if(st->eco_index == NONE) {
            st->eco_index = 0U;
        }
        else if(st->eco_index == UI_SETTINGS_ECO_BACK) {
            pop_to_mycar(st);
        }
        else if(s_settings != NULL) {
            if(st->eco_index == UI_SETTINGS_ECO_DISPLAY) {
                (void)car_settings_set_eco_or_fuel(
                    s_settings, s_settings->values.eco_or_fuel == 0U);
            }
            else {
                (void)car_settings_set_eco_indicator(
                    s_settings, s_settings->values.eco_indicator == 0U);
            }
        }
        break;
    default:
        return false;
    }

    return true;
}

/*
 * ECO 历史信息（Qt EcoavgShow）：SigNext / SigBack 都直接 goBack；
 * SigOk 调 resetAllData —— 这里改为“记录当前值”更符合历史页语义：
 * 当前平均油耗非 0 时按 Qt updateHistory 推入并落盘，全 0 时等同于重置。
 */
static bool handle_eco_history_key(ui_settings_page_t *page, uint8_t code)
{
    (void)page;

    switch(code) {
    case UI_KEY_UP:
    case UI_KEY_DOWN:
        /* 单页无列表，方向键无操作 */
        return true;
    case UI_KEY_OK:
        if(s_settings != NULL) {
            (void)car_settings_eco_history_push(s_settings, s_eco_current_x10);
        }
        return true;
    default:
        return false;
    }
}

/* Qt nextTumblerIndex：光标 0→1→…→5，到端点后反向 */
static void date_focus_next(ui_settings_state_t *st)
{
    if(st->date_child_back) {
        if(st->date_focus != NONE && st->date_focus > 0U) {
            st->date_focus--;
        }
    }
    else {
        st->date_focus = (uint8_t)((st->date_focus == NONE) ? 0U : st->date_focus + 1U);
    }
    if(st->date_focus <= 0U) {
        st->date_child_back = false;
    }
    if(st->date_focus >= 5U) {
        st->date_focus = 5U;
        st->date_child_back = true;
    }
}

/*
 * Qt SettingsManager::setSystemTimer：date -s + hwclock -w + sync。
 * 仅板端写系统时钟/RTC，PC 演示不改宿主机时间。
 */
static void date_apply_system_time(uint16_t year, uint16_t month, uint16_t day,
                                   uint16_t hour, uint16_t minute)
{
#if defined(CARMW_PLATFORM_BOARD)
    struct tm tm_time = {0};
    struct timespec ts;
    time_t sec;

    tm_time.tm_year = (int)year - 1900;
    tm_time.tm_mon = (int)month - 1;
    tm_time.tm_mday = (int)day;
    tm_time.tm_hour = (int)hour;
    tm_time.tm_min = (int)minute;
    tm_time.tm_isdst = -1;
    sec = mktime(&tm_time);
    if(sec == (time_t)-1) {
        return;
    }
    ts.tv_sec = sec;
    ts.tv_nsec = 0;
    if(clock_settime(CLOCK_REALTIME, &ts) != 0) {
        return;
    }
    /* 与 Qt 一致：把系统时间写回 RTC */
    (void)system("hwclock -w");
#else
    (void)year;
    (void)month;
    (void)day;
    (void)hour;
    (void)minute;
#endif
}

/* Qt saveDateTime：下发 0x32 并退出滚轮光标 */
static bool date_save(ui_settings_page_t *page)
{
    const uint16_t *v = page->st.date_values;
    car_cmd_t cmd;
    uint16_t year = (uint16_t)(page->st.date_year_base + v[0]);
    uint16_t month;
    uint16_t day;
    unsigned limit;

    /* 草稿月/日为 0 基索引，转真实月份/日期后校验范围 */
    if(v[1] > 11U || v[2] > 30U || v[3] > 23U || v[4] > 59U) {
        return false;
    }
    month = (uint16_t)(v[1] + 1U);
    day = (uint16_t)(v[2] + 1U);
    limit = s_date_days[month - 1U] + (month == 2U && date_leap(year) ? 1U : 0U);

    if(day > limit) {
        return false;
    }
    if(s_cmd_cb == NULL || !car_cmd_set_time(&cmd, year, month, day, v[3], v[4])) {
        return false;
    }
    s_cmd_cb(&cmd);
    /* Qt saveDateTime 同时同步 Linux 系统时间（RTC） */
    date_apply_system_time(year, month, day, v[3], v[4]);
    return true;
}

/* Qt sendTimeSource：PageListSelect 的 timeSource 选项（1=中控 2=仪表） */
static bool date_send_source(ui_settings_page_t *page)
{
    car_cmd_t cmd;
    uint8_t payload[] = {6U, page->time_source == 1U ? 2U : 1U};

    if((page->time_source != 1U && page->time_source != 2U) || s_cmd_cb == NULL) {
        return false;
    }
    if(!car_cmd_set_payload(&cmd, 0x05U, CAR_OP_SET_GET, payload, 2U)) {
        return false;
    }
    s_cmd_cb(&cmd);
    return true;
}

/*
 * 按键对齐 Qt PageDateSettings：
 *   UP/NEXT  → 滚轮内 childIndex>=0 时 okTumblerIndex（当前列加一，
 *              确认项回退光标），否则 itemIndex+1；
 *   DOWN/PREV→ 滚轮内 childIndex>=0 时 backTumblerIndex（当前列减一），
 *              否则 itemIndex-1；
 *   OK       → 滚轮项：未进入光标时进入第一列，确认项保存并退出；
 *              时间制式：24h/12h 切换；时间源：下发 0x05；返回：回系统设置。
 * 仪表模式行数 4，其它时间源行数 2（与 Qt rowCount 一致）。
 */
static bool handle_date_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *st = &page->st;
    bool wheel = page->time_source == 2U;
    uint8_t rows = wheel ? 4U : 2U;
    uint8_t back = wheel ? 3U : 1U;
    bool next = (code == UI_KEY_UP || code == UI_KEY_RIGHT);
    bool prev = (code == UI_KEY_DOWN || code == UI_KEY_LEFT);

    /* Qt onItemSelected：越界行钳到返回行（Qt itemIndex 只在 0~rowCount-1） */
    if(st->date_item >= rows) {
        st->date_item = back;
    }

    if(next || prev) {
        if(wheel && st->date_item == 0U && st->date_focus != NONE) {
            uint8_t col = st->date_focus;

            if(col == 5U) {
                date_focus_next(st);
            }
            else {
                date_wheel_step_value(page, col, (int8_t)(next ? 1 : -1));
            }
            return true;
        }
        st->date_item = (uint8_t)((st->date_item + (next ? 1 : rows - 1U)) % rows);
        /* Qt：itemIndex 变为 0 时滚轮条重播 scale 0.8→1 */
        if(wheel && st->date_item == 0U) {
            date_wheel_animate(page);
        }
        return true;
    }

    if(code != UI_KEY_OK) {
        return false;
    }

    if(wheel && st->date_item == 0U) {
        if(st->date_focus == NONE) {
            date_focus_next(st);
        }
        else if(st->date_focus == 5U) {
            (void)date_save(page);
            st->date_focus = NONE;
            st->date_child_back = false;
        }
        else {
            date_focus_next(st);
        }
        return true;
    }
    if(wheel && st->date_item == 1U) {
        if(page->date_units.valid) {
            car_cmd_t cmd;

            if(s_cmd_cb != NULL &&
               car_cmd_set_units(&cmd, page->date_units.mile, !page->date_units.hour24,
                                 page->date_units.fahrenheit, page->date_units.fuel_unit,
                                 page->date_units.pressure_unit)) {
                s_cmd_cb(&cmd);
            }
        }
        return true;
    }
    if(st->date_item == back) {
        st->view = UI_SETTINGS_VIEW_SYSTEM;
        st->system_index = 1U;
        return true;
    }
    (void)date_send_source(page);
    return true;
}
//根据当前页面状态和按键事件处理设置页面的按键输入，返回是否处理了该按键事件。
bool ui_settings_page_key(ui_settings_page_t *page, const ui_key_event_t *evt)
{
    bool handled;

    if(page == NULL || evt == NULL || evt->pressed == 0U || !page->active) {
        return false;
    }

    if(evt->code == UI_KEY_OK && !feature_selection_enabled(&page->st)) return true;
    switch(page->st.view) {
    case UI_SETTINGS_VIEW_LANGUAGE:
    case UI_SETTINGS_VIEW_FACTORY_PASSWORD:
    case UI_SETTINGS_VIEW_FACTORY:
    case UI_SETTINGS_VIEW_FACTORY_FEATURES:
    case UI_SETTINGS_VIEW_FACTORY_ODO:
    case UI_SETTINGS_VIEW_FACTORY_TANK:
    case UI_SETTINGS_VIEW_FACTORY_MODEL:
        handled = factory_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_CLEAR_LOG:
        handled = true;
        if(evt->code == UI_KEY_UP || evt->code == UI_KEY_DOWN ||
           evt->code == UI_KEY_LEFT || evt->code == UI_KEY_RIGHT)
            page->st.date_index ^= 1U;
        else if(evt->code == UI_KEY_OK) {
            if(page->st.date_index == 1U) page->st.view = UI_SETTINGS_VIEW_SYSTEM;
            else if(s_cleanup_start && s_cleanup_status && cleanup_status() != CAR_LOG_CLEANUP_RUNNING) {
                s_cleanup_failed = !s_cleanup_start();
            }
        }
        else handled = false;
        break;
    case UI_SETTINGS_VIEW_CONNECT:
        handled = true;
        if(evt->code == UI_KEY_UP || evt->code == UI_KEY_DOWN)
            page->st.date_index = (page->st.date_index + (evt->code == UI_KEY_UP ? 2 : 1)) % 3;
        else if(evt->code == UI_KEY_OK) {
            if(page->st.date_index == 2) pop_to_root(&page->st);
            else if(s_settings) {
                bool enabled = s_settings->values.connect_enabled;
                uint8_t mode = s_settings->values.connect_mode;
                if(page->st.date_index == 0) enabled = !enabled;
                else if(enabled) mode = (mode + 1) % 3;
                car_settings_set_connect(s_settings, enabled, mode);
            }
        }
        else handled = false;
        break;
    case UI_SETTINGS_VIEW_DATE:
        handled = handle_date_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_SYSTEM:
        handled = handle_system_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_VERSION:
        handled = (evt->code == UI_KEY_UP || evt->code == UI_KEY_DOWN);
        break;
    case UI_SETTINGS_VIEW_ROOT:
        if(page->st.root_locked) {
            handled = (page->st.root_index == UI_SETTINGS_ROOT_ITINERARY)
                          ? handle_it_key(page, evt->code)
                          : handle_info_key(page, evt->code);
        }
        else {
            handled = handle_root_key(page, evt->code);
        }
        break;
    case UI_SETTINGS_VIEW_MY_CAR:
        handled = handle_mycar_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_STYLE:
        handled = handle_style_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_UNIT:
        handled = handle_unit_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_CENTER_CARDS:
        handled = handle_card_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_DRIVING_INFO:
        handled = handle_driving_info_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_ROUTINE:
        handled = handle_routine_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_SPEED_WARN:
        handled = handle_speed_warn_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_ECO:
        handled = handle_eco_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_ECO_HISTORY:
        handled = handle_eco_history_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_ITINERARY_ASSIST:
        handled = handle_it_assist_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_DRIVING_ASSIST:
        handled = handle_driving_assist_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_EPS:
        handled = handle_eps_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_PARKING:
        handled = handle_parking_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_CHASSIS:
        handled = handle_chassis_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_VEHICLE:
        handled = handle_vehicle_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_LIGHT:
        handled = handle_light_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_DOOR:
        handled = handle_door_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_TURN:
        handled = handle_turn_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_WIPER:
        handled = handle_wiper_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_SERVICE:
        handled = handle_service_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_SERVICE_ITEM:
        handled = handle_service_item_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_SERVICE_EDIT:
        handled = handle_service_edit_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_SELECT:
        handled = handle_select_key(page, evt->code);
        break;
    default:
        handled = false;
        break;
    }

    if(handled) {
        if((evt->code == UI_KEY_UP || evt->code == UI_KEY_DOWN) && !feature_selection_enabled(&page->st))
            return ui_settings_page_key(page, evt);
        ui_settings_page_apply(page);
    }

    return handled;
}

/* Qt setNextOrBackIndex：子页返回，根页/我的座驾按序号前进 */
void ui_settings_page_next_or_back(ui_settings_page_t *page)
{
    ui_settings_state_t *st;

    if(page == NULL) {
        return;
    }
    st = &page->st;

    switch(st->view) {
    case UI_SETTINGS_VIEW_ROOT:
        /* 信息选项/行程资讯列表锁定在根页内，按序号前进 */
        if(st->root_locked) {
            if(st->root_index == UI_SETTINGS_ROOT_ITINERARY) {
                it_step(st, 1);
            }
            else {
                info_step(st, 1);
            }
            break;
        }
        if(st->root_index == UI_SETTINGS_ROOT_COUNT - 1U) {
            st->root_index = NONE;
        }
        st->root_index = (st->root_index == NONE)
                             ? 0U
                             : (uint8_t)(st->root_index + 1U);
        break;
    case UI_SETTINGS_VIEW_MY_CAR:
        mycar_step(st, 1);
        break;
    case UI_SETTINGS_VIEW_CONNECT:
        st->date_index = (uint8_t)((st->date_index + 1U) % 3U);
        break;
    case UI_SETTINGS_VIEW_DATE:
        if(page->time_source == 2U && st->date_item == 0U && st->date_focus != NONE) {
            /* Qt okTumblerIndex：滚轮内前进到当前列的下一项 */
            uint8_t col = st->date_focus;

            if(col < 5U) {
                date_wheel_step_value(page, col, 1);
            }
            break;
        }
        (void)ui_settings_page_back(page);
        return;
    case UI_SETTINGS_VIEW_SELECT:
        if(st->select_kind == UI_SETTINGS_SELECT_ANIMATION) {
            select_step(st, 1);
            break;
        }
        (void)ui_settings_page_back(page);
        return;
    default:
        (void)ui_settings_page_back(page);
        return;
    }

    ui_settings_page_apply(page);
}

/* ---------------- 生命周期 ---------------- */

static void version_refresh(ui_settings_page_t *page)
{
    const char *fallback = "---";
    const char *values[3];
    char *system_version = page->system_version;
    FILE *file = NULL;

    /* This file describes the installed system, not the current selection. */
    if(!page->system_version_loaded) {
        page->system_version_loaded = true;
        (void)strcpy(system_version, "READ FAILED");
        file = fopen("/etc/dash_ver", "r");
    }

    if(file != NULL) {
        if(fgets(system_version, sizeof(page->system_version), file) == NULL ||
           system_version[0] == '\0' || system_version[0] == '\n' ||
           system_version[0] == '\r') {
            (void)strcpy(system_version, "READ FAILED");
        }
        else {
            size_t len = strlen(system_version);
            while(len > 0U && (system_version[len - 1U] == '\n' ||
                               system_version[len - 1U] == '\r')) {
                system_version[--len] = '\0';
            }
            if(len == 0U) {
                (void)strcpy(system_version, "READ FAILED");
            }
        }
        (void)fclose(file);
    }

    ui_settings_label_set_text(page->version_values[0], fallback);
    ui_settings_label_set_text(page->version_updated, fallback);
    values[0] = page->version_valid[0] ? page->version_text[0] : fallback;
    values[1] = page->version_valid[1] ? page->version_text[1] : fallback;
    values[2] = page->version_valid[2] ? page->version_text[2] : fallback;
    for(uint8_t i = 0U; i < 3U; ++i) {
        ui_settings_label_set_text(page->version_values[i + 1U], values[i]);
    }
    ui_settings_label_set_text(page->version_values[4], system_version);
    ui_settings_label_set_text(lv_obj_get_child(page->system_rows[2], 1), system_version);
}

void ui_settings_page_set_date(ui_settings_page_t *page, const car_clock_t *clock,
                               const car_units_t *units, uint8_t source)
{
    bool source_changed;

    if(!page || !clock || !units) return;
    source_changed = page->time_source != source;
    bool changed = source_changed || page->date_clock.year != clock->year ||
        page->date_clock.month != clock->month || page->date_clock.day != clock->day ||
        page->date_clock.hour != clock->hour || page->date_clock.minute != clock->minute ||
        page->date_clock.valid != clock->valid || page->date_units.hour24 != units->hour24 ||
        page->date_units.valid != units->valid;
    page->date_clock = *clock;
    page->date_units = *units;
    page->time_source = source;
    if(changed && page->active && page->st.view == UI_SETTINGS_VIEW_DATE) {
        if(source_changed) {
            /* Qt onTimeSourceChanged：重进滚轮，草稿取最新回读值 */
            date_page_enter(page);
        }
        /* 数据刷新后立即同步文案/可见性（Qt 属性绑定自动更新） */
        ui_settings_page_apply(page);
    }
    else if(changed && page->active && !lv_obj_has_flag(page->system_list, LV_OBJ_FLAG_HIDDEN)) {
        ui_settings_page_apply(page);
    }
}

void ui_settings_page_set_version(ui_settings_page_t *page,
                                  const char *main_version, bool main_valid,
                                  const char *chip_version, bool chip_valid,
                                  const char *wired_version, bool wired_valid)
{
    const char *source[3] = {main_version, chip_version, wired_version};
    bool source_valid[3] = {main_valid, chip_valid, wired_valid};
    bool changed = false;

    if(page == NULL) {
        return;
    }
    for(uint8_t i = 0U; i < 3U; ++i) {
        size_t len = (source[i] != NULL) ? strlen(source[i]) : 0U;
        bool valid = source_valid[i] && len > 0U && len < CAR_TEXT_MAX;
        for(size_t j = 0U; valid && j < len; ++j) {
            if((unsigned char)source[i][j] < 0x20U ||
               (unsigned char)source[i][j] > 0x7EU) {
                valid = false;
            }
        }
        if(page->version_valid[i] != valid ||
           strcmp(page->version_text[i], valid ? source[i] : "") != 0)
            changed = true;
        page->version_valid[i] = valid;
        if(valid) {
            (void)snprintf(page->version_text[i], sizeof(page->version_text[i]),
                           "%s", source[i]);
        }
        else {
            page->version_text[i][0] = '\0';
        }
    }
    if(changed && page->root != NULL) {
        version_refresh(page);
    }
}

void ui_settings_page_refresh(ui_settings_page_t *page)
{
    if(page == NULL || page->root == NULL) {
        return;
    }

    /* 不在风格选项中操作时，跟随设置里的当前样式 */
    if(page->st.view == UI_SETTINGS_VIEW_STYLE) {
        style_index_from_settings(page);
    }
    ui_settings_page_apply(page);
}

void ui_settings_page_set_eco_current(ui_settings_page_t *page, uint16_t avg_x10)
{
    bool changed = s_eco_current_x10 != avg_x10;
    s_eco_current_x10 = avg_x10;

    if(page && page->root && page->active && page->st.view == UI_SETTINGS_VIEW_CLEAR_LOG &&
       page->shown_cleanup_status != cleanup_status())
        ui_settings_page_apply(page);
    if(changed && page != NULL && page->root != NULL && page->active &&
       page->st.view == UI_SETTINGS_VIEW_ECO_HISTORY) {
        eco_history_refresh(page);
    }
}

void ui_settings_page_set_speed_warning(ui_settings_page_t *page, bool valid,
                                        uint8_t speed_kph)
{
    if(page == NULL) {
        return;
    }

    bool changed = s_speed_warn_valid != valid || (valid && page->speed_warn_kph != speed_kph);
    s_speed_warn_valid = valid;
    if(valid) {
        page->speed_warn_kph = speed_kph;
    }
    if(changed && page->root != NULL && page->active && page->st.view == UI_SETTINGS_VIEW_ROUTINE) {
        ui_settings_page_apply(page);
    }
}

void ui_settings_page_set_driver_assist(ui_settings_page_t *page,
                                        bool emergency,
                                        bool lane,
                                        bool blind)
{
    if(s_da_emergency == emergency && s_da_lane == lane &&
       s_da_blind == blind) {
        return;
    }

    s_da_emergency = emergency;
    s_da_lane = lane;
    s_da_blind = blind;

    /* 只刷三行 ON/OFF，避免每帧全量 apply */
    if(page != NULL && page->root != NULL) {
        drv_assist_page_style(page);
    }
}

void ui_settings_page_set_steering_mode(ui_settings_page_t *page, uint8_t mode)
{
    mode = (uint8_t)((mode == 1U) ? 1U : 0U);
    if(s_eps_mode == mode) {
        return;
    }

    s_eps_mode = mode;

    /* 只刷模式文字，避免每帧全量 apply */
    if(page != NULL && page->root != NULL) {
        eps_page_style(page);
    }
}

void ui_settings_page_set_vehicle_settings(ui_settings_page_t *page,
                                           const car_vehicle_setting_t *vs)
{
    bool changed;

    if(vs == NULL) {
        return;
    }

    changed = (s_vs_welcome != vs->welcome_light) ||
              (s_vs_interior != vs->auto_interior_light) ||
              (s_vs_horn != vs->horn_feedback) ||
              (s_vs_auto_unlock != vs->auto_door_unlock) ||
              (s_vs_lane_change != vs->lane_change_flash) ||
              (s_vs_wiper_speed != vs->rain_sensing_wipers) ||
              (s_vs_wiper_rear != vs->rear_wiper_reverse_linkage) ||
              (s_vs_wiper_mist != vs->wiper_mist_mode) ||
              (s_vs_external_doors != vs->external_doors) ||
              (s_vs_single_unlock != vs->single_door_unlock) ||
              (s_vs_smart_key != vs->smart_door_unlock) ||
              (s_vs_select_unlock != vs->select_unlock) ||
              (s_vs_auto_lock != vs->auto_door_lock) ||
              (s_vs_headlight_sensitivity != vs->headlight_sensitivity) ||
              (s_vs_headlight_delay != vs->headlight_off_delay) ||
              (s_vs_remote_start != vs->remote_engine_start);

    if(!changed) {
        return;
    }

    s_vs_welcome = vs->welcome_light;
    s_vs_interior = vs->auto_interior_light;
    s_vs_horn = vs->horn_feedback;
    s_vs_auto_unlock = vs->auto_door_unlock;
    s_vs_lane_change = vs->lane_change_flash;
    s_vs_wiper_speed = vs->rain_sensing_wipers;
    s_vs_wiper_rear = vs->rear_wiper_reverse_linkage;
    s_vs_wiper_mist = vs->wiper_mist_mode;
    s_vs_external_doors = vs->external_doors;
    s_vs_single_unlock = vs->single_door_unlock;
    s_vs_smart_key = vs->smart_door_unlock;
    s_vs_select_unlock = vs->select_unlock;
    s_vs_auto_lock = vs->auto_door_lock;
    s_vs_headlight_sensitivity = vs->headlight_sensitivity;
    s_vs_headlight_delay = vs->headlight_off_delay;
    s_vs_remote_start = vs->remote_engine_start;

    if(page != NULL && page->root != NULL) {
        light_page_style(page);
        door_page_style(page);
        turn_page_style(page);
        wiper_page_style(page);
        vehicle_page_style(page);
        select_page_style(page);
    }
}

void ui_settings_page_set_parking_chassis(ui_settings_page_t *page,
                                          const car_parking_chassis_t *pc)
{
    bool changed;

    if(pc == NULL) {
        return;
    }

    changed = (s_pc_moving_object != pc->moving_object) ||
              (s_pc_front_radar != pc->front_radar) ||
              (s_pc_rear_radar != pc->rear_radar) ||
              (s_pc_display != pc->radar_display) ||
              (s_pc_rear_cross != pc->rear_cross_traffic) ||
              (s_pc_radar_volume != pc->radar_volume) ||
              (s_pc_radar_distance != pc->radar_distance) ||
              (s_pc_parking_sensor != pc->parking_sensor) ||
              (s_pc_trace != pc->active_trace_control) ||
              (s_pc_engine_brake != pc->active_engine_brake);

    if(!changed) {
        return;
    }

    s_pc_moving_object = pc->moving_object;
    s_pc_front_radar = pc->front_radar;
    s_pc_rear_radar = pc->rear_radar;
    s_pc_display = pc->radar_display;
    s_pc_rear_cross = pc->rear_cross_traffic;
    s_pc_radar_volume = pc->radar_volume;
    s_pc_radar_distance = pc->radar_distance;
    s_pc_parking_sensor = pc->parking_sensor;
    s_pc_trace = pc->active_trace_control;
    s_pc_engine_brake = pc->active_engine_brake;

    /* 只刷驻车辅助/底盘控制相关行，避免每帧全量 apply */
    if(page != NULL && page->root != NULL) {
        parking_page_style(page);
        chassis_page_style(page);
    }
}

void ui_settings_page_set_service(ui_settings_page_t *page,
                                  const car_maintenance_t *maint)
{
    bool changed = false;

    if(maint == NULL) {
        return;
    }

    for(uint8_t i = 0U; i < 3U; ++i) {
        if(!maint->item_valid[i]) {
            continue;
        }
        if(s_svc_set_km[i] != maint->set_km_by_item[i] ||
           s_svc_remain_km[i] != maint->remain_km_by_item[i]) {
            s_svc_set_km[i] = maint->set_km_by_item[i];
            s_svc_remain_km[i] = maint->remain_km_by_item[i];
            changed = true;
        }
    }

    if(changed && page != NULL && page->root != NULL) {
        service_item_page_style(page);
    }
}

void ui_settings_page_set_pending(ui_settings_page_t *page, bool pending)
{
    if(page == NULL || page->pending_hint == NULL) {
        return;
    }

    if(pending) {
        lv_obj_remove_flag(page->pending_hint, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(page->pending_hint, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_settings_page_set_active(ui_settings_page_t *page, bool active)
{
    if(page == NULL || page->root == NULL) {
        return;
    }

    page->active = active;
    if(!active) {
        page->st.factory_unlocked = false;
        memset(page->st.factory_digits, 0, sizeof(page->st.factory_digits));
    }
    if(active) {
        lv_obj_remove_flag(page->root, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(page->root);
        ui_settings_page_apply(page);
    }
    else {
        lv_obj_add_flag(page->root, LV_OBJ_FLAG_HIDDEN);
    }
}

bool ui_settings_page_is_active(const ui_settings_page_t *page)
{
    return (page != NULL) && page->active;
}
/**
 * @brief 创建设置页面
 * @param page 设置页面对象
 * @param parent 父容器
 * @return true 成功
 * @return false 失败
 */
bool ui_settings_page_create(ui_settings_page_t *page, lv_obj_t *parent)
{
    uint8_t i;

    if(page == NULL || parent == NULL) {
        return false;
    }

    memset(page, 0, sizeof(*page));

    page->root = ui_blank_container(parent);
    lv_obj_add_flag(page->root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(page->root, SETTINGS_X, SETTINGS_Y);
    lv_obj_set_size(page->root, SETTINGS_W, SETTINGS_H);
    /* Qt 设置页背景透明，显示所在仪表的背景。 */

    page->title = make_label(page->root, "", UI_FONT_XL, UI_COLOR_WHITE);
    lv_obj_set_pos(page->title, TITLE_X, TITLE_Y);
    lv_obj_set_width(page->title, TITLE_W);
    lv_obj_set_style_text_align(page->title, LV_TEXT_ALIGN_CENTER, 0);

    page->root_bar = ui_blank_container(page->root);
    lv_obj_set_pos(page->root_bar, 0, ROOT_BAR_Y);
    lv_obj_set_size(page->root_bar, SETTINGS_W, ROOT_BAR_H);
    for(i = 0U; i < UI_SETTINGS_ROOT_COUNT; ++i) {
        root_menu_create(page, i);
    }
    page->body = ui_blank_container(page->root);
    lv_obj_set_pos(page->body, 0, BODY_Y);
    lv_obj_set_size(page->body, SETTINGS_W, BODY_H);

    page->factory_list = make_list(page->body);
    for(i = 0; i < 20; ++i)
        page->factory_rows[i] = make_row(page->factory_list, LIST_X, i * ROW_H, LIST_W, ROW_H, "");
    page->system_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_SYSTEM_COUNT; ++i) {
        lv_obj_t *row = make_row(page->system_list, (SETTINGS_W - 450) / 2,
                                  ROW_H * (int32_t)i, 450, ROW_H, s_system_labels[i]);
        if(i != UI_SETTINGS_SYSTEM_BACK) {
            /* Qt ItemDetails：左名称 26px，右摘要 20px；名称不可被摘要覆盖。 */
            lv_obj_t *name = lv_obj_get_child(row, 0);
            lv_obj_set_width(name, 450 * 55 / 100);
            lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_align(name, LV_ALIGN_LEFT_MID, 20, 0);
            lv_obj_t *summary = make_label(row, "", UI_TEXT(20), UI_COLOR_WHITE);
            lv_obj_set_width(summary, 450 * 40 / 100);
            lv_label_set_long_mode(summary, LV_LABEL_LONG_MODE_DOTS);
            lv_obj_set_style_text_align(summary, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(summary, LV_ALIGN_BOTTOM_RIGHT, -20, -14);
        }
        page->system_rows[i] = row;
    }

    page->date_list = make_list(page->body);
    for(i = 0U; i < 9U; ++i) {
        page->date_rows[i] = make_row(page->date_list, LIST_X,
            ROW_H * (int32_t)i, LIST_W, ROW_H, "");
    }
    date_wheel_create(page);
    page->version_list = make_list(page->body);
    {
        static const char *const labels[5] = {
            "APP VERSION", "MAIN FIRMWARE", "SMALL CHIP", "WIRED", "SYSTEM VERSION",
        };
        for(i = 0U; i < 5U; ++i) {
            lv_obj_t *row = make_row(page->version_list, 20, 60 * (int32_t)i,
                                     SETTINGS_W - 40, 60, labels[i]);
            lv_obj_t *label = lv_obj_get_child(row, 0);
            lv_obj_t *value = make_label(row, "---", UI_FONT_LG, UI_COLOR_WHITE);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_width(label, 250);
            lv_obj_set_pos(label, ROW_OFFSET_X, 16);
            lv_obj_align(value, LV_ALIGN_RIGHT_MID, -20, 0);
            page->version_values[i] = value;
        }
        {
            page->version_updated = make_label(page->version_list, "---", UI_FONT_SM,
                                                UI_COLOR_WHITE);
            lv_obj_align(page->version_updated, LV_ALIGN_TOP_RIGHT, -36, 38);
            lv_obj_set_width(page->version_updated, 140);
            lv_obj_set_style_text_align(page->version_updated, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_set_pos(page->version_values[0], 390, 16);
        }
    }

    /* 我的座驾：400 宽、行高 60（Qt ButtonEx） */
    page->mycar_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_MYCAR_COUNT; ++i) {
        page->mycar_rows[i] = make_row(page->mycar_list, LIST_X,
                                       ROW_H * (int32_t)i, LIST_W, ROW_H,
                                       s_mycar_labels[i]);
    }

    /* 仪表风格：选项 + 返回 */
    page->style_list = make_list(page->body);
    for(i = 0U; i < style_back(); ++i) {
        const ui_style_desc_t *desc = ui_app_style_option_at(i);

        (void)make_row(page->style_list, STYLE_ROW_X,
                       STYLE_ROW_H * (int32_t)i, STYLE_ROW_W, STYLE_ROW_H,
                       (desc != NULL && desc->name != NULL) ? desc->name
                                                           : "UNKNOWN");
    }
    (void)make_row(page->style_list, LIST_X, STYLE_ROW_H * (int32_t)style_back(),
                   LIST_W, ROW_H, "BACK");

    /* 计量单位：里程 / 温度 / 确定 */
    page->unit_list = make_list(page->body);
    for(i = 0U; i < UNIT_ROW_COUNT; ++i) {
        (void)make_row(page->unit_list, LIST_X, UNIT_ROW_H * (int32_t)i,
                       LIST_W, UNIT_ROW_H, "");
    }

    /* 中间卡片：450 宽、行高 55，右侧勾选框 */
    page->card_list = make_list(page->body);
    lv_obj_set_height(page->card_list, 285);
    for(i = 0U; i <= CARD_BACK; ++i) {
        bool back = (i == CARD_BACK);
        lv_obj_t *row = make_row(page->card_list, CARD_LIST_X,
                                 CARD_ROW_H * (int32_t)i, CARD_LIST_W,
                                 CARD_ROW_H, back ? "BACK" : s_card_labels[i]);

        if(back) {
            continue;
        }

        {
            lv_obj_t *label = lv_obj_get_child(row, 0);
            lv_obj_t *box = ui_blank_container(row);
            lv_obj_t *tick;

            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_width(label, CARD_LIST_W - ROW_OFFSET_X - CARD_BOX - 40);
            lv_obj_set_pos(label, ROW_OFFSET_X,
                           (CARD_ROW_H - lv_font_get_line_height(UI_FONT_LG)) / 2);

            lv_obj_set_size(box, CARD_BOX, CARD_BOX);
            lv_obj_align(box, LV_ALIGN_RIGHT_MID, -20, 0);
            lv_obj_set_style_border_width(box, 2, 0);
            lv_obj_set_style_border_color(box, lv_color_hex(UI_COLOR_WHITE), 0);
            lv_obj_set_style_radius(box, 4, 0);

            static const lv_point_precise_t check_points[] = {{4, 12}, {9, 17}, {19, 6}};
            tick = lv_line_create(box);
            lv_line_set_points(tick, check_points, 3);
            lv_obj_set_pos(tick, 0, 0);
            lv_obj_set_style_line_width(tick, 2, 0);
            lv_obj_set_style_line_color(tick, lv_color_hex(COLOR_YELLOW), 0);
        }
    }

    /* 信息选项：进入行车信息 / 返回 */
    page->info_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_INFO_COUNT; ++i) {
        page->info_rows[i] = make_row(page->info_list, LIST_X,
                                      ROW_H * (int32_t)i, LIST_W, ROW_H,
                                      s_info_labels[i]);
    }

    /* 行车信息：3 个开关 + 返回（右侧 ON/OFF） */
    page->drv_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_DRV_COUNT; ++i) {
        lv_obj_t *row = make_row(page->drv_list, LIST_X,
                                 UNIT_ROW_H * (int32_t)i, LIST_W, UNIT_ROW_H,
                                 s_drv_labels[i]);

        if(i != UI_SETTINGS_DRV_BACK) {
            lv_obj_t *label = lv_obj_get_child(row, 0);
            lv_obj_t *state = make_label(row, "OFF", UI_FONT_LG, UI_COLOR_WHITE);

            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_width(label, LIST_W - ROW_OFFSET_X - 60);
            lv_obj_set_pos(label, ROW_OFFSET_X,
                           (UNIT_ROW_H - lv_font_get_line_height(UI_FONT_LG)) / 2);
            lv_obj_align(state, LV_ALIGN_RIGHT_MID, -20, 0);
        }

        page->drv_rows[i] = row;
    }

    /* 常规设置：亮度、超速报警、超速预警值 + 返回。 */
    page->routine_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_ROUTINE_COUNT; ++i) {
        lv_obj_t *row = make_row(page->routine_list, LIST_X,
                                 UNIT_ROW_H * (int32_t)i, LIST_W, UNIT_ROW_H,
                                 s_routine_labels[i]);

        if(i != UI_SETTINGS_ROUTINE_BACK) {
            lv_obj_t *label = lv_obj_get_child(row, 0);
            lv_obj_t *state = make_label(row, "", UI_FONT_LG, UI_COLOR_WHITE);

            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_width(label, LIST_W - ROW_OFFSET_X - 60);
            lv_obj_set_pos(label, ROW_OFFSET_X,
                           (UNIT_ROW_H - lv_font_get_line_height(UI_FONT_LG)) / 2);
            lv_obj_align(state, LV_ALIGN_RIGHT_MID, -20, 0);
        }

        page->routine_rows[i] = row;
    }

    /* ECO 设置：显示 THROTTLE/FUEL、ECO 指示 ON/OFF + 返回 */
    page->eco_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_ECO_COUNT; ++i) {
        lv_obj_t *row = make_row(page->eco_list, LIST_X,
                                 UNIT_ROW_H * (int32_t)i, LIST_W, UNIT_ROW_H,
                                 s_eco_labels[i]);

        if(i != UI_SETTINGS_ECO_BACK) {
            lv_obj_t *label = lv_obj_get_child(row, 0);
            lv_obj_t *state = make_label(row, "", UI_FONT_LG, UI_COLOR_WHITE);

            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_width(label, LIST_W - ROW_OFFSET_X - 60);
            lv_obj_set_pos(label, ROW_OFFSET_X,
                           (UNIT_ROW_H - lv_font_get_line_height(UI_FONT_LG)) / 2);
            lv_obj_align(state, LV_ALIGN_RIGHT_MID, -20, 0);
        }

        page->eco_rows[i] = row;
    }

    eco_history_create(page);

    /* 行程资讯：驾驶员辅助 / 返回（行高 60，与 Qt 二级列表一致） */
    page->it_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_IT_COUNT; ++i) {
        page->it_rows[i] = make_row(page->it_list, LIST_X,
                                    ROW_H * (int32_t)i, LIST_W, ROW_H,
                                    s_it_labels[i]);
    }

    /* 驾驶员辅助：驾驶辅助 / 返回 */
    page->assist_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_ASSIST_COUNT; ++i) {
        page->assist_rows[i] = make_row(page->assist_list, LIST_X,
                                        UNIT_ROW_H * (int32_t)i, LIST_W,
                                        UNIT_ROW_H, s_assist_labels[i]);
    }

    /* 超速预警值：百/十/个位编辑，按键确认后下发 MCU。 */
    page->speed_warn_list = make_list(page->body);
    {
        lv_obj_t *digits = ui_blank_container(page->speed_warn_list);
        lv_obj_t *unit;

        lv_obj_set_size(digits, 240, 100);
        lv_obj_align(digits, LV_ALIGN_CENTER, 0, -10);
        lv_obj_set_style_bg_opa(digits, LV_OPA_TRANSP, 0);
        for(i = 0U; i < 3U; ++i) {
            page->speed_warn_digits[i] = make_label(digits, "0", UI_FONT_XL,
                                                    UI_COLOR_WHITE);
            lv_obj_set_width(page->speed_warn_digits[i], 64);
            lv_obj_set_style_text_align(page->speed_warn_digits[i],
                                        LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(page->speed_warn_digits[i], LV_ALIGN_LEFT_MID,
                         12 + (int32_t)i * 72, 0);
        }
        unit = make_label(page->speed_warn_list, "km/h", UI_FONT_LG, UI_COLOR_WHITE);
        lv_obj_align(unit, LV_ALIGN_CENTER, 0, 70);
    }

    page->drv_assist_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_DA_COUNT; ++i) {
        lv_obj_t *row = make_row(page->drv_assist_list, LIST_X,
                                 UNIT_ROW_H * (int32_t)i, LIST_W, UNIT_ROW_H,
                                 s_da_labels[i]);

        if(i != UI_SETTINGS_DA_BACK) {
            lv_obj_t *label = lv_obj_get_child(row, 0);
            lv_obj_t *state = make_label(row, "OFF", UI_FONT_LG, UI_COLOR_WHITE);

            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_width(label, LIST_W - ROW_OFFSET_X - 60);
            lv_obj_set_pos(label, ROW_OFFSET_X,
                           (UNIT_ROW_H - lv_font_get_line_height(UI_FONT_LG)) / 2);
            lv_obj_align(state, LV_ALIGN_RIGHT_MID, -20, 0);
        }

        page->drv_assist_rows[i] = row;
    }

    /* 电动助力转向：模式（NORMAL/SPORT）+ 返回 */
    page->eps_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_EPS_COUNT; ++i) {
        lv_obj_t *row = make_row(page->eps_list, LIST_X,
                                 UNIT_ROW_H * (int32_t)i, LIST_W, UNIT_ROW_H,
                                 s_eps_labels[i]);

        if(i == UI_SETTINGS_EPS_MODE) {
            lv_obj_t *label = lv_obj_get_child(row, 0);
            lv_obj_t *state = make_label(row, "NORMAL", UI_FONT_LG, UI_COLOR_WHITE);

            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_width(label, LIST_W - ROW_OFFSET_X - 60);
            lv_obj_set_pos(label, ROW_OFFSET_X,
                           (UNIT_ROW_H - lv_font_get_line_height(UI_FONT_LG)) / 2);
            lv_obj_align(state, LV_ALIGN_RIGHT_MID, -20, 0);
        }

        page->eps_rows[i] = row;
    }

    /* 驻车辅助：5 个开关行 + 音量/距离/传感器 3 档 + 返回 */
    page->parking_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_PARK_COUNT; ++i) {
        lv_obj_t *row = make_row(page->parking_list, LIST_X,
                                 UNIT_ROW_H * (int32_t)i, LIST_W, UNIT_ROW_H,
                                 s_park_labels[i]);

        if(i != UI_SETTINGS_PARK_BACK) {
            lv_obj_t *label = lv_obj_get_child(row, 0);
            lv_obj_t *state = make_label(row, "ON", UI_FONT_LG, UI_COLOR_WHITE);

            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_width(label, LIST_W - ROW_OFFSET_X - 60);
            lv_obj_set_pos(label, ROW_OFFSET_X,
                           (UNIT_ROW_H - lv_font_get_line_height(UI_FONT_LG)) / 2);
            lv_obj_align(state, LV_ALIGN_RIGHT_MID, -20, 0);
        }

        page->parking_rows[i] = row;
    }

    /* 底盘控制：主动弯道控制 / 主动发动机制动 + 返回 */
    page->chassis_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_CHASSIS_COUNT; ++i) {
        lv_obj_t *row = make_row(page->chassis_list, LIST_X,
                                 UNIT_ROW_H * (int32_t)i, LIST_W, UNIT_ROW_H,
                                 s_chassis_labels[i]);

        if(i != UI_SETTINGS_CHASSIS_BACK) {
            lv_obj_t *label = lv_obj_get_child(row, 0);
            lv_obj_t *state = make_label(row, "ON", UI_FONT_LG, UI_COLOR_WHITE);

            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_width(label, LIST_W - ROW_OFFSET_X - 60);
            lv_obj_set_pos(label, ROW_OFFSET_X,
                           (UNIT_ROW_H - lv_font_get_line_height(UI_FONT_LG)) / 2);
            lv_obj_align(state, LV_ALIGN_RIGHT_MID, -20, 0);
        }

        page->chassis_rows[i] = row;
    }

    /* 车辆设置：灯光设置 / 车门 / 返回（行高 60） */
    page->vehicle_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_VEH_COUNT; ++i) {
        lv_obj_t *row = make_row(page->vehicle_list, LIST_X,
                                 ROW_H * (int32_t)i, LIST_W, ROW_H,
                                 s_veh_labels[i]);

        if(i == UI_SETTINGS_VEH_REMOTE_START) {
            lv_obj_t *label = lv_obj_get_child(row, 0);
            lv_obj_t *state = make_label(row, "OFF", UI_FONT_LG, UI_COLOR_WHITE);

            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_width(label, LIST_W - ROW_OFFSET_X - 60);
            lv_obj_set_pos(label, ROW_OFFSET_X,
                           (ROW_H - lv_font_get_line_height(UI_FONT_LG)) / 2);
            lv_obj_align(state, LV_ALIGN_RIGHT_MID, -20, 0);
        }
        page->vehicle_rows[i] = row;
    }

    /* 灯光设置：迎宾灯 / 室内自动点灯 ON/OFF + 返回 */
    page->light_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_LIGHT_COUNT; ++i) {
        lv_obj_t *row = make_row(page->light_list, LIST_X,
                                 UNIT_ROW_H * (int32_t)i, LIST_W, UNIT_ROW_H,
                                 s_light_labels[i]);

        if(i != UI_SETTINGS_LIGHT_BACK) {
            lv_obj_t *label = lv_obj_get_child(row, 0);
            lv_obj_t *state = make_label(row, "ON", UI_FONT_LG, UI_COLOR_WHITE);

            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_width(label, LIST_W - ROW_OFFSET_X - 60);
            lv_obj_set_pos(label, ROW_OFFSET_X,
                           (UNIT_ROW_H - lv_font_get_line_height(UI_FONT_LG)) / 2);
            lv_obj_align(state, LV_ALIGN_RIGHT_MID, -20, 0);
        }

        page->light_rows[i] = row;
    }

    /* 车门设置：喇叭响应 ON/OFF、车门自动解锁选项 + 返回 */
    page->door_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_DOOR_COUNT; ++i) {
        lv_obj_t *row = make_row(page->door_list, LIST_X,
                                 UNIT_ROW_H * (int32_t)i, LIST_W, UNIT_ROW_H,
                                 s_door_labels[i]);

        if(i != UI_SETTINGS_DOOR_BACK) {
            lv_obj_t *label = lv_obj_get_child(row, 0);
            lv_obj_t *state = make_label(row, "ON", UI_FONT_LG, UI_COLOR_WHITE);

            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_width(label, LIST_W - ROW_OFFSET_X - 60);
            lv_obj_set_pos(label, ROW_OFFSET_X,
                           (UNIT_ROW_H - lv_font_get_line_height(UI_FONT_LG)) / 2);
            lv_obj_align(state, LV_ALIGN_RIGHT_MID, -20, 0);
        }

        page->door_rows[i] = row;
    }

    /* 转向灯：变更车道闪灯 3 次 ON/OFF + 返回 */
    page->turn_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_TURN_COUNT; ++i) {
        lv_obj_t *row = make_row(page->turn_list, LIST_X,
                                 UNIT_ROW_H * (int32_t)i, LIST_W, UNIT_ROW_H,
                                 s_turn_labels[i]);

        if(i != UI_SETTINGS_TURN_BACK) {
            lv_obj_t *label = lv_obj_get_child(row, 0);
            lv_obj_t *state = make_label(row, "ON", UI_FONT_LG, UI_COLOR_WHITE);

            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_width(label, LIST_W - ROW_OFFSET_X - 60);
            lv_obj_set_pos(label, ROW_OFFSET_X,
                           (UNIT_ROW_H - lv_font_get_line_height(UI_FONT_LG)) / 2);
            lv_obj_align(state, LV_ALIGN_RIGHT_MID, -20, 0);
        }

        page->turn_rows[i] = row;
    }

    /* 雨刷器：三项 ON/OFF + 返回 */
    page->wiper_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_WIPER_COUNT; ++i) {
        lv_obj_t *row = make_row(page->wiper_list, LIST_X,
                                 UNIT_ROW_H * (int32_t)i, LIST_W, UNIT_ROW_H,
                                 s_wiper_labels[i]);

        if(i != UI_SETTINGS_WIPER_BACK) {
            lv_obj_t *label = lv_obj_get_child(row, 0);
            lv_obj_t *state = make_label(row, "ON", UI_FONT_LG, UI_COLOR_WHITE);

            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_width(label, LIST_W - ROW_OFFSET_X - 60);
            lv_obj_set_pos(label, ROW_OFFSET_X,
                           (UNIT_ROW_H - lv_font_get_line_height(UI_FONT_LG)) / 2);
            lv_obj_align(state, LV_ALIGN_RIGHT_MID, -20, 0);
        }

        page->wiper_rows[i] = row;
    }

    /* 保养主列表：机油 / 轮胎 / 自定义 / 返回（行高 60） */
    page->service_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_SERVICE_COUNT; ++i) {
        page->service_rows[i] = make_row(page->service_list, LIST_X,
                                         ROW_H * (int32_t)i, LIST_W, ROW_H,
                                         s_service_labels[i]);
    }

    /* 保养子页：设置公里数 / 剩余公里数带数值，重置/返回 */
    page->service_item_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_SVC_COUNT; ++i) {
        lv_obj_t *row = make_row(page->service_item_list, LIST_X,
                                 UNIT_ROW_H * (int32_t)i, LIST_W, UNIT_ROW_H,
                                 s_service_row_labels[i]);

        if(i == UI_SETTINGS_SVC_SET_KM || i == UI_SETTINGS_SVC_REMAIN_KM) {
            lv_obj_t *label = lv_obj_get_child(row, 0);
            lv_obj_t *state = make_label(row, "0km", UI_FONT_LG, UI_COLOR_WHITE);

            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_width(label, LIST_W - ROW_OFFSET_X - 90);
            lv_obj_set_pos(label, ROW_OFFSET_X,
                           (UNIT_ROW_H - lv_font_get_line_height(UI_FONT_LG)) / 2);
            lv_obj_align(state, LV_ALIGN_RIGHT_MID, -20, 0);
        }

        page->service_item_rows[i] = row;
    }

    /* 保养数字编辑：6 位数字 + 确认/返回 */
    page->service_edit_list = make_list(page->body);
    {
        lv_obj_t *digits = ui_blank_container(page->service_edit_list);

        lv_obj_set_size(digits, 420, 100);
        lv_obj_align(digits, LV_ALIGN_TOP_MID, 0, 10);
        for(i = 0U; i < UI_SETTINGS_SVC_DIGITS; ++i) {
            page->service_edit_digits[i] = make_label(digits, "0", UI_FONT_XL,
                                                      UI_COLOR_WHITE);
            lv_obj_set_width(page->service_edit_digits[i], 56);
            lv_obj_set_style_text_align(page->service_edit_digits[i],
                                        LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(page->service_edit_digits[i], LV_ALIGN_TOP_LEFT,
                         42 + (int32_t)i * 56, 25);
        }

        page->service_edit_rows[0] = make_row(page->service_edit_list, LIST_X,
                                              140, LIST_W, UNIT_ROW_H, "CONFIRM");
        page->service_edit_rows[1] = make_row(page->service_edit_list, LIST_X,
                                              140 + UNIT_ROW_H, LIST_W,
                                              UNIT_ROW_H, "BACK");
    }

    /* 通用多选页：最多 8 个选项 + 返回，右侧 * 标记当前回读值 */
    page->select_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_SELECT_MAX; ++i) {
        lv_obj_t *row = make_row(page->select_list, LIST_X,
                                 UNIT_ROW_H * (int32_t)i, LIST_W, UNIT_ROW_H,
                                 "");
        lv_obj_t *label = lv_obj_get_child(row, 0);
        lv_obj_t *mark = make_label(row, "", UI_FONT_LG, COLOR_YELLOW);

        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_width(label, LIST_W - ROW_OFFSET_X - 60);
        lv_obj_set_pos(label, ROW_OFFSET_X,
                       (UNIT_ROW_H - lv_font_get_line_height(UI_FONT_LG)) / 2);
        lv_obj_align(mark, LV_ALIGN_RIGHT_MID, -20, 0);
        page->select_rows[i] = row;
    }

    /* 底部提示：改动先留在内存，ACC 关闭时统一写入 */
    page->pending_hint = make_label(page->root, "CHANGES SAVED ON ACC OFF",
                                    UI_FONT_SM, COLOR_GRAY);
    lv_obj_set_width(page->pending_hint, SETTINGS_W);
    lv_obj_set_style_text_align(page->pending_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(page->pending_hint, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_add_flag(page->pending_hint, LV_OBJ_FLAG_HIDDEN);

    ui_settings_state_init(&page->st);
    ui_settings_page_apply(page);
    lv_obj_add_flag(page->root, LV_OBJ_FLAG_HIDDEN);

    return true;
}

void ui_settings_page_destroy(ui_settings_page_t *page)
{
    if(page == NULL) {
        return;
    }

    if(page->root != NULL) {
        lv_obj_delete(page->root);
    }

    memset(page, 0, sizeof(*page));
}
