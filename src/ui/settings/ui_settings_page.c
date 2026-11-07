/*
 * ui_settings_page.c - 设置菜单覆盖层
 *
 * 布局与状态机都照搬 Qt：
 *   qml/main.qml            settingStackView 594×504，x=663，y=120；
 *                           8 秒无按键自动关闭（reSettings），由 ui_app 计时。
 *   PageMainSettings.qml    5 个分类（Qt 为横排图标，这里画成环形卡片轮盘，
 *                           选中项转到正前方；按键与状态机不变），
 *                           selectedIndex 初值 -1，第一次 OK 只是进入分类。
 *   PageMyCarItem.qml       第一次 OK 锁定列表（isOk），之后 OK 才执行子项。
 *   PageUnitSettings.qml    里程/温度按 OK 循环切换，确定写回并返回。
 *   PageCenterCards.qml     OK 切换勾选，返回项弹出子页。
 *
 * 本工程只有 Montserrat 字体（无中文字形），标签一律用英文，
 * 与 Qt 中文文案的对应关系记录在 docs/UI_REWRITE_HANDOFF.md。
 */
#include "ui/settings/ui_settings_page.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ui/ui_app.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"

#define SETTINGS_W 594
#define SETTINGS_H 504
#define SETTINGS_X ((UI_DESIGN_W - SETTINGS_W) / 2)
#define SETTINGS_Y 120

#define ROOT_BAR_Y 8   /* 轮盘区：y=8..160，紧贴子页内容区上沿 */
#define ROOT_BAR_H 152
#define BODY_Y 160
#define BODY_H 370

#define LIST_X 97      /* 子列表水平居中：(594 - 400) / 2 */
#define LIST_W 400
#define ROW_H 60
#define ROW_OFFSET_X 24
#define ROW_TEXT_PAD 10

#define TITLE_X 147    /* 标题居中：(594 - 300) / 2 */
#define TITLE_W 300
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

static const char *const s_root_names[UI_SETTINGS_ROOT_COUNT] = {
    "ITINERARY", "INFORMATION", "MY CAR", "SYSTEM", "CONNECT",
};

static const char *const s_system_labels[UI_SETTINGS_SYSTEM_COUNT] = {
    "LANGUAGE", "DATE & TIME", "VERSION INFO", "FACTORY SETTINGS",
    "CLEAR CACHE", "BOOT ANIMATION", "BACK",
};

/*
 * 环形轮盘：5 张卡片均布在水平圆环上，正前方一张最大最亮。
 * 投影：th = (i - pos)·2π/5，z = R(1 - cos th)，缩放 F / (F + z)。
 * 卡片按尺寸缩放（图标保持原尺寸，不走图片变换），亮度靠混黑压暗。
 */
#define CARD_TOP 0x2B3947
#define CARD_BOTTOM 0x1A2331
#define CARD_EDGE 0x4A5A6C
#define CARD_W 130
#define CARD_H 100
#define CARD_RADIUS 14
#define RING_CX (SETTINGS_W / 2)
#define RING_CY 62     /* 正前方卡片中心（root_bar 内坐标） */
#define RING_R 190.0f  /* 圆环半径：决定左右展开宽度 */
#define RING_F 330.0f  /* 视距：R/F ≈ 0.58 */
#define RING_LIFT 14.0f /* 越靠后越往上抬，形成俯视感 */
#define RING_DIM 150   /* 最远处压暗量（/256） */
#define RING_ANIM_MS 300U
#define ROOT_LABEL_Y 122

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

/* 驾驶员辅助子项：驻车辅助/底盘控制未迁移，置灰 */
static const char *const s_assist_labels[UI_SETTINGS_ASSIST_COUNT] = {
    "DRIVING ASSIST", "PARKING ASSIST", "CHASSIS CONTROL", "BACK",
};
static const bool s_assist_enabled[UI_SETTINGS_ASSIST_COUNT] = {
    true, false, false, true,
};

/* 驾驶辅助三项：值来自 MCU 0x16 回读，OK 只下发命令 */
static const char *const s_da_labels[UI_SETTINGS_DA_COUNT] = {
    "EMERGENCY BRAKING ", "LANE DEPARTURE ", "BLIND SPOT ", "BACK",
};

/* 车辆设置子项（Qt CarSettings.leftModel 顺序） */
static const char *const s_veh_labels[UI_SETTINGS_VEH_COUNT] = {
    "LIGHT SETTINGS", "TURN LIGHT", "DOORS", "WIPER", "SERVICE", "BACK",
};
static const bool s_veh_enabled[UI_SETTINGS_VEH_COUNT] = {
    true, false, true, false, false, true,
};

/* 灯光设置（Qt LightSettings）：工厂配置门控项未迁移 */
static const char *const s_light_labels[UI_SETTINGS_LIGHT_COUNT] = {
    "WELCOME LIGHT ", "AUTO INTERIOR LIGHT ", "BACK",
};

/* 车门设置（Qt DoorSetting）：工厂配置门控项未迁移 */
static const char *const s_door_labels[UI_SETTINGS_DOOR_COUNT] = {
    "HORN FEEDBACK ", "AUTO UNLOCK ", "BACK",
};

/* Qt DoorSetting.autolock 的 3 个选项 */
static const char *const s_auto_unlock_options[3] = {
    "IGN OFF", "P SHIFT", "OFF",
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
static bool s_speed_warn_valid;

/* 车辆设置项的 MCU 回读缓存（Qt SettingsMgr 缺省：开关 true / 选项 0） */
static bool s_vs_welcome = true;
static bool s_vs_interior = true;
static bool s_vs_horn = true;
static uint8_t s_vs_auto_unlock;

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
    st->vehicle_index = NONE;
    st->light_index = NONE;
    st->door_index = NONE;
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
    lv_label_set_text(label, text);

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
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(label, w);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    return row;
}

/* 分类卡片：渐变圆角底 + 居中图标 + 选中时底部红条（Qt ComSettingMenu） */
static void root_menu_create(ui_settings_page_t *page, uint8_t index)
{
    lv_obj_t *item = ui_blank_container(page->root_bar);
    lv_obj_t *img;
    lv_obj_t *bar;

    lv_obj_set_style_bg_color(item, lv_color_hex(CARD_TOP), 0);
    lv_obj_set_style_bg_grad_color(item, lv_color_hex(CARD_BOTTOM), 0);
    lv_obj_set_style_bg_grad_dir(item, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(item, CARD_RADIUS, 0);
    lv_obj_set_style_border_color(item, lv_color_hex(CARD_EDGE), 0);
    lv_obj_set_style_border_width(item, 1, 0);
    lv_obj_set_style_shadow_color(item, lv_color_hex(UI_COLOR_BLACK), 0);
    lv_obj_set_style_shadow_width(item, 16, 0);
    lv_obj_set_style_shadow_offset_y(item, 6, 0);
    lv_obj_set_style_shadow_opa(item, LV_OPA_60, 0);

    img = lv_image_create(item);
    lv_image_set_src(img, s_root_icons[index][0]);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, -4);

    bar = ui_blank_container(item);
    lv_obj_set_height(bar, 4);
    lv_obj_set_style_radius(bar, 2, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(UI_COLOR_RED), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);

    page->root_menu[index] = item;
}

static float ring_wrap(float th)
{
    while(th > 3.14159265f) {
        th -= 6.28318531f;
    }
    while(th < -3.14159265f) {
        th += 6.28318531f;
    }
    return th;
}

/* 按 root_pos 摆放 5 张卡片：位置、尺寸、压暗、前后层级 */
static void ring_layout(ui_settings_page_t *page)
{
    const float pos = (float)page->root_pos / 1000.0f;
    float depth[UI_SETTINGS_ROOT_COUNT];
    uint8_t order[UI_SETTINGS_ROOT_COUNT];
    uint8_t i;
    uint8_t j;

    for(i = 0U; i < UI_SETTINGS_ROOT_COUNT; ++i) {
        lv_obj_t *item = page->root_menu[i];
        lv_obj_t *bar = lv_obj_get_child(item, 1);
        float th = ring_wrap(((float)i - pos) *
                             (6.28318531f / (float)UI_SETTINGS_ROOT_COUNT));
        float cs = cosf(th);
        float z = RING_R * (1.0f - cs);
        float sc = RING_F / (RING_F + z);
        int32_t w = (int32_t)((float)CARD_W * sc + 0.5f);
        int32_t h = (int32_t)((float)CARD_H * sc + 0.5f);
        int32_t cx = RING_CX + (int32_t)(RING_R * sinf(th) * 1.15f);
        int32_t cy = RING_CY - (int32_t)(RING_LIFT * (1.0f - cs));
        /* 0 = 正前方，1 = 正后方 */
        float far = fabsf(th) / 3.14159265f;
        lv_opa_t dim = (lv_opa_t)(far * (float)RING_DIM);

        lv_obj_set_size(item, w, h);
        lv_obj_set_pos(item, cx - w / 2, cy - h / 2);
        lv_obj_set_style_bg_color(item, lv_color_mix(lv_color_hex(UI_COLOR_BLACK),
                                                     lv_color_hex(CARD_TOP), dim), 0);
        lv_obj_set_style_bg_grad_color(item, lv_color_mix(lv_color_hex(UI_COLOR_BLACK),
                                                          lv_color_hex(CARD_BOTTOM), dim), 0);
        lv_obj_set_style_image_opa(lv_obj_get_child(item, 0),
                                   (lv_opa_t)(LV_OPA_COVER - dim), 0);
        lv_obj_set_width(bar, w - 2 * CARD_RADIUS);
        lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -8);

        depth[i] = z;
        order[i] = i;
    }

    /* 远的先画：按 z 降序依次提到最前 */
    for(i = 1U; i < UI_SETTINGS_ROOT_COUNT; ++i) {
        for(j = i; j > 0U && depth[order[j - 1U]] < depth[order[j]]; --j) {
            uint8_t t = order[j];
            order[j] = order[j - 1U];
            order[j - 1U] = t;
        }
    }
    for(i = 0U; i < UI_SETTINGS_ROOT_COUNT; ++i) {
        lv_obj_move_foreground(page->root_menu[order[i]]);
    }
}

static void ring_anim_cb(void *var, int32_t value)
{
    ui_settings_page_t *page = var;

    page->root_pos = value;
    ring_layout(page);
}

/* 轮盘转到 index：沿最短方向，首次（或重建后）直接落位 */
static void ring_goto(ui_settings_page_t *page, uint8_t index, bool animate)
{
    const int32_t n = (int32_t)UI_SETTINGS_ROOT_COUNT * 1000;
    int32_t cur = page->root_target;
    int32_t delta = ((int32_t)index * 1000 - cur) % n;
    lv_anim_t a;

    if(delta > n / 2) {
        delta -= n;
    }
    else if(delta < -n / 2) {
        delta += n;
    }

    if(!animate) {
        lv_anim_delete(page, ring_anim_cb);
        page->root_target = (int32_t)index * 1000;
        page->root_pos = page->root_target;
        ring_layout(page);
        return;
    }
    if(delta == 0) {
        return;
    }

    page->root_target = cur + delta;
    lv_anim_delete(page, ring_anim_cb);
    lv_anim_init(&a);
    lv_anim_set_var(&a, page);
    lv_anim_set_exec_cb(&a, ring_anim_cb);
    lv_anim_set_values(&a, page->root_pos, page->root_target);
    lv_anim_set_duration(&a, RING_ANIM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

/*
 * 列表容器：尺寸与 body 相同、自身不滚动。
 * 行位置直接按内容顺序排，选中项用“内容偏移”滚动，绝不移动行对象，
 * 否则测试按坐标找行、以及“返回”这类子对象都会错位。
 */
static lv_obj_t *make_list(lv_obj_t *body)
{
    lv_obj_t *list = ui_blank_container(body);

    lv_obj_set_pos(list, 0, 0);
    lv_obj_set_size(list, SETTINGS_W, BODY_H);
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

    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
}

/*
 * Qt：selected = selectedIndex === i；进入子项（isOk）后图标仍保持选中。
 * 不做 Qt 的缩放动画，选中直接换蓝色图标并显示红条。
 */
static void root_style(ui_settings_page_t *page)
{
    uint8_t i;
    uint8_t sel = page->st.root_index;

    /* 未选中（-1）时正前方停在第一项，第一次按键只点亮、不转动 */
    ring_goto(page, (sel == NONE) ? 0U : sel, true);
    lv_label_set_text(page->root_label, (sel == NONE) ? "" : s_root_names[sel]);

    for(i = 0U; i < UI_SETTINGS_ROOT_COUNT; ++i) {
        lv_obj_t *item = page->root_menu[i];
        lv_obj_t *img = lv_obj_get_child(item, 0);
        lv_obj_t *bar = lv_obj_get_child(item, 1);
        bool selected = (page->st.root_index == i);

        lv_image_set_src(img, s_root_icons[i][selected ? 1 : 0]);
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

        lv_label_set_text(label, text);
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
        lv_label_set_text(state, on ? "ON" : "OFF");
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

        lv_label_set_text(page->speed_warn_digits[i], text);
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

/* 车辆设置：只有灯光设置/车门/返回可用，未迁移项置灰 */
static void vehicle_page_style(ui_settings_page_t *page)
{
    for(uint8_t i = 0U; i < UI_SETTINGS_VEH_COUNT; ++i) {
        row_style(page->vehicle_rows[i], page->st.vehicle_index == i,
                  s_veh_enabled[i]);
    }
}

/* 灯光设置：ON/OFF 只反映 MCU 0x16 回读 */
static void light_page_style(ui_settings_page_t *page)
{
    switch_row_state(page->light_rows[UI_SETTINGS_LIGHT_WELCOME],
                     page->st.light_index == UI_SETTINGS_LIGHT_WELCOME,
                     s_vs_welcome);
    switch_row_state(page->light_rows[UI_SETTINGS_LIGHT_AUTO_INTERIOR],
                     page->st.light_index == UI_SETTINGS_LIGHT_AUTO_INTERIOR,
                     s_vs_interior);
    row_style(page->light_rows[UI_SETTINGS_LIGHT_BACK],
              page->st.light_index == UI_SETTINGS_LIGHT_BACK, true);
}

/*
 * 车门设置：喇叭响应 ON/OFF；自动解锁显示当前选项
 * （Qt ItemDetails + PageListSelect，这里按 3 个选项循环，见回填说明）。
 */
static void door_page_style(ui_settings_page_t *page)
{
    lv_obj_t *state;

    switch_row_state(page->door_rows[UI_SETTINGS_DOOR_HORN],
                     page->st.door_index == UI_SETTINGS_DOOR_HORN, s_vs_horn);

    row_style(page->door_rows[UI_SETTINGS_DOOR_AUTO_UNLOCK],
              page->st.door_index == UI_SETTINGS_DOOR_AUTO_UNLOCK, true);
    state = lv_obj_get_child(page->door_rows[UI_SETTINGS_DOOR_AUTO_UNLOCK], 1);
    if(state != NULL) {
        lv_label_set_text(state,
                          s_auto_unlock_options[s_vs_auto_unlock % 3U]);
        lv_obj_set_style_text_color(state,
            lv_color_hex(UI_COLOR_WHITE), 0);
    }

    row_style(page->door_rows[UI_SETTINGS_DOOR_BACK],
              page->st.door_index == UI_SETTINGS_DOOR_BACK, true);
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
                lv_label_set_text(state, text);
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
                lv_label_set_text(state, text);
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
        lv_label_set_text(state, throttle ? "THROTTLE" : "FUEL");
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
    lv_label_set_text(page->eco_hist_cur_label, text);
    (void)snprintf(text, sizeof(text), "%u.%u",
                   (unsigned)(best / 10U), (unsigned)(best % 10U));
    lv_label_set_text(page->eco_hist_best_label, text);
}

/*
 * Qt PageMainSettings：图标行常驻，下方 StackLayout 随 selectedIndex 切换；
 * 我的座驾子项就在根页内（isOk 只是锁定列表），子页才压栈盖住图标行。
 */
static void version_refresh(ui_settings_page_t *page);
static void system_style(ui_settings_page_t *page);

static void view_sync(ui_settings_page_t *page)
{
    lv_obj_t *lists[17];
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

    for(uint8_t i = 0U; i < 17U; ++i) {
        lv_obj_add_flag(lists[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(page->speed_warn_list, LV_OBJ_FLAG_HIDDEN);

    switch(page->st.view) {
    case UI_SETTINGS_VIEW_SYSTEM:
        title = "SYSTEM SETTINGS";
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
    case UI_SETTINGS_VIEW_ROOT:
    default:
        /* 其余分类的子页未迁移，焦点在它们上时内容区留空 */
        if(page->st.root_index == UI_SETTINGS_ROOT_MY_CAR) {
            lv_obj_remove_flag(page->mycar_list, LV_OBJ_FLAG_HIDDEN);
        }
        else if(page->st.root_index == UI_SETTINGS_ROOT_INFO) {
            lv_obj_remove_flag(page->info_list, LV_OBJ_FLAG_HIDDEN);
        }
        else if(page->st.root_index == UI_SETTINGS_ROOT_ITINERARY) {
            lv_obj_remove_flag(page->it_list, LV_OBJ_FLAG_HIDDEN);
        }
        break;
    }

    if(page->st.view == UI_SETTINGS_VIEW_ROOT ||
       page->st.view == UI_SETTINGS_VIEW_MY_CAR) {
        lv_obj_remove_flag(page->root_bar, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(page->root_bar, LV_OBJ_FLAG_HIDDEN);
    }

    lv_label_set_text(page->title, title);
}

/* 选中项居中显示：Qt positionViewAtIndex(Center) */
static void list_center(lv_obj_t *list, uint8_t index, int32_t row_h)
{
    int32_t target = row_h * (int32_t)index - (BODY_H - row_h) / 2;

    if(target < 0) {
        target = 0;
    }
    lv_obj_set_style_translate_y(list, -target, 0);
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
    system_style(page);
    root_style(page);
    mycar_style(page);
    style_page_style(page);
    unit_page_style(page);
    card_page_style(page);
    info_page_style(page);
    drv_page_style(page);
    routine_page_style(page);
    eco_page_style(page);
    eco_history_refresh(page);
    it_page_style(page);
    assist_page_style(page);
    drv_assist_page_style(page);
    vehicle_page_style(page);
    light_page_style(page);
    door_page_style(page);
    speed_warn_page_style(page);
    version_refresh(page);
    list_center_current(page);
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
    st->vehicle_index = NONE;
    st->light_index = NONE;
    st->door_index = NONE;
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
    return (index < UI_SETTINGS_ASSIST_COUNT) && s_assist_enabled[index];
}

/* 驾驶员辅助：跳过置灰的驻车辅助/底盘控制 */
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
        break;
    default:
        return false;
    }

    return true;
}

/*
 * 车门设置：喇叭响应取反下发；
 * 车门自动解锁按 Qt PageListSelect 的 3 个选项循环。
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
        break;
    default:
        return false;
    }

    return true;
}

static void system_style(ui_settings_page_t *page)
{
    for(uint8_t i = 0U; i < UI_SETTINGS_SYSTEM_COUNT; ++i) {
        row_style(page->system_rows[i], page->st.system_index == i,
                  i == UI_SETTINGS_SYSTEM_VERSION || i == UI_SETTINGS_SYSTEM_BACK);
    }
}

static void system_step(ui_settings_state_t *st, int8_t step)
{
    uint8_t index = st->system_index;

    if(index == NONE) {
        st->system_index = (step > 0) ? UI_SETTINGS_SYSTEM_VERSION :
                                        UI_SETTINGS_SYSTEM_BACK;
        return;
    }
    do {
        index = (uint8_t)(((int32_t)index + step + UI_SETTINGS_SYSTEM_COUNT) %
                          UI_SETTINGS_SYSTEM_COUNT);
    } while(index != UI_SETTINGS_SYSTEM_VERSION &&
            index != UI_SETTINGS_SYSTEM_BACK);
    st->system_index = index;
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
        else if(st->system_index == UI_SETTINGS_SYSTEM_VERSION) {
            st->view = UI_SETTINGS_VIEW_VERSION;
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

bool ui_settings_page_key(ui_settings_page_t *page, const ui_key_event_t *evt)
{
    bool handled;

    if(page == NULL || evt == NULL || evt->pressed == 0U || !page->active) {
        return false;
    }

    switch(page->st.view) {
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
    case UI_SETTINGS_VIEW_VEHICLE:
        handled = handle_vehicle_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_LIGHT:
        handled = handle_light_key(page, evt->code);
        break;
    case UI_SETTINGS_VIEW_DOOR:
        handled = handle_door_key(page, evt->code);
        break;
    default:
        handled = false;
        break;
    }

    if(handled) {
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
    char system_version[CAR_TEXT_MAX] = "READ FAILED";
    FILE *file = fopen("/etc/dash_ver", "r");

    if(file != NULL) {
        if(fgets(system_version, sizeof(system_version), file) == NULL ||
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

    lv_label_set_text(page->version_values[0], fallback);
    lv_label_set_text(page->version_updated, fallback);
    values[0] = page->version_valid[0] ? page->version_text[0] : fallback;
    values[1] = page->version_valid[1] ? page->version_text[1] : fallback;
    values[2] = page->version_valid[2] ? page->version_text[2] : fallback;
    for(uint8_t i = 0U; i < 3U; ++i) {
        lv_label_set_text(page->version_values[i + 1U], values[i]);
    }
    lv_label_set_text(page->version_values[4], system_version);
}

void ui_settings_page_set_version(ui_settings_page_t *page,
                                  const char *main_version, bool main_valid,
                                  const char *chip_version, bool chip_valid,
                                  const char *wired_version, bool wired_valid)
{
    const char *source[3] = {main_version, chip_version, wired_version};
    bool source_valid[3] = {main_valid, chip_valid, wired_valid};

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
        page->version_valid[i] = valid;
        if(valid) {
            (void)snprintf(page->version_text[i], sizeof(page->version_text[i]),
                           "%s", source[i]);
        }
        else {
            page->version_text[i][0] = '\0';
        }
    }
    if(page->root != NULL) {
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
    s_eco_current_x10 = avg_x10;

    if(page != NULL && page->root != NULL) {
        eco_history_refresh(page);
    }
}

void ui_settings_page_set_speed_warning(ui_settings_page_t *page, bool valid,
                                        uint8_t speed_kph)
{
    if(page == NULL) {
        return;
    }

    s_speed_warn_valid = valid;
    if(valid) {
        page->speed_warn_kph = speed_kph;
    }
    if(page->root != NULL && page->st.view != UI_SETTINGS_VIEW_SPEED_WARN) {
        routine_page_style(page);
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

void ui_settings_page_set_vehicle_settings(ui_settings_page_t *page,
                                           bool welcome_light,
                                           bool auto_interior_light,
                                           bool horn_feedback,
                                           uint8_t auto_door_unlock)
{
    bool changed = (s_vs_welcome != welcome_light) ||
                   (s_vs_interior != auto_interior_light) ||
                   (s_vs_horn != horn_feedback) ||
                   (s_vs_auto_unlock != auto_door_unlock);

    if(!changed) {
        return;
    }

    s_vs_welcome = welcome_light;
    s_vs_interior = auto_interior_light;
    s_vs_horn = horn_feedback;
    s_vs_auto_unlock = auto_door_unlock;

    if(page != NULL && page->root != NULL) {
        light_page_style(page);
        door_page_style(page);
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

bool ui_settings_page_create(ui_settings_page_t *page, lv_obj_t *parent)
{
    uint8_t i;

    if(page == NULL || parent == NULL) {
        return false;
    }

    memset(page, 0, sizeof(*page));

    page->root = ui_blank_container(parent);
    lv_obj_set_pos(page->root, SETTINGS_X, SETTINGS_Y);
    lv_obj_set_size(page->root, SETTINGS_W, SETTINGS_H);
    /* 面板底色：不加底时只有 5 个 52px 细框图标，板端几乎看不出“已打开” */
    lv_obj_set_style_bg_color(page->root, lv_color_hex(UI_COLOR_BLACK), 0);
    lv_obj_set_style_bg_opa(page->root, LV_OPA_80, 0);
    lv_obj_set_style_radius(page->root, 8, 0);

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
    page->root_label = make_label(page->root_bar, "", UI_FONT_MD, UI_COLOR_WHITE);
    lv_obj_set_width(page->root_label, SETTINGS_W);
    lv_obj_set_style_text_align(page->root_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(page->root_label, 0, ROOT_LABEL_Y);
    ring_goto(page, 0U, false);

    page->body = ui_blank_container(page->root);
    lv_obj_set_pos(page->body, 0, BODY_Y);
    lv_obj_set_size(page->body, SETTINGS_W, BODY_H);

    page->system_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_SYSTEM_COUNT; ++i) {
        page->system_rows[i] = make_row(page->system_list, LIST_X,
                                        ROW_H * (int32_t)i, LIST_W, ROW_H,
                                        s_system_labels[i]);
    }

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

            tick = make_label(box, "X", &lv_font_montserrat_20, COLOR_YELLOW);
            lv_obj_set_style_text_align(tick, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_width(tick, CARD_BOX);
            lv_obj_align(tick, LV_ALIGN_CENTER, 0, 0);
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

    /* 车辆设置：灯光设置 / 车门 / 返回（行高 60） */
    page->vehicle_list = make_list(page->body);
    for(i = 0U; i < UI_SETTINGS_VEH_COUNT; ++i) {
        page->vehicle_rows[i] = make_row(page->vehicle_list, LIST_X,
                                         ROW_H * (int32_t)i, LIST_W, ROW_H,
                                         s_veh_labels[i]);
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

    lv_anim_delete(page, ring_anim_cb);
    if(page->root != NULL) {
        lv_obj_delete(page->root);
    }

    memset(page, 0, sizeof(*page));
}
