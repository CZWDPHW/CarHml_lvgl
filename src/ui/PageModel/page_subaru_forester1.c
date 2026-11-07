/*
 * page_subaru_forester1.c - Style1 主表盘（对应 PageSubaruForester1.qml）
 *
 * 坐标全部来自 Qt 的 PageSubaruForester1.qml，
 * 0.975 的页面缩放由 ui_page_create 统一处理。
 */
#include "ui/ui_node.h"
#include "ui/ui_common.h"
#include "ui/ui_assets.h"
#include "ui/ui_theme.h"
#include "ui/widgets/ui_dial.h"
#include "ui/SpeedDial/speed_forester1.h"
#include "ui/RotationDial/rotate_forester1.h"
#include "ui/FuelTank/fuel_forester1.h"
#include "ui/WaterTemp/water_forester1.h"
#include "ui/ComControl/ui_gear.h"
#include "ui/ComControl/ui_mileage.h"
#include "ui/ComControl/ui_range.h"
#include "ui/ComControl/ui_clock.h"
#include "ui/ComControl/ui_outside_temp.h"
#include "ui/ComControl/ui_turn_signal.h"
#include "ui/ComControl/ui_lamp_row.h"
#include "ui/ComControl/ui_lamp_group.h"
#include "ui/ComControl/ui_cruise.h"
#include "ui/ComControl/ui_small_door.h"
#include "ui/PageModel/page_itinerary.h"
#include "ui/PageModel/page_4i.h"
#include "ui/PageModel/page_tire.h"
#include "ui/PageModel/page_chassis.h"
#include "ui/PageModel/page_short_term.h"
#include "ui/PageModel/page_instant_fuel.h"
#include "ui/PageModel/page_eco.h"
#include "ui/PageModel/page_music.h"
#include "ui/PageModel/page_connect.h"
#include "ui/overlays/ov_door.h"
#include "ui/overlays/ov_tire.h"
#include "ui/overlays/ov_radar.h"
#include "ui/overlays/ov_cruise.h"
#include "ui/overlays/ov_tips.h"
#include "ui/ui_session.h"
#include "ui/ui_app.h"
#include "carmw/car_settings.h"

#include <stddef.h>
/* 中心车辆视图：Qt amCount 1~4 共 4 帧，150ms 一换 */
#define CAR_FRAME_COUNT     4
#define CAR_FRAME_MS        150
/* 中心卡片数量上限（Qt pageModel 最多 10 张） */
#define UI_CARD_MAX  10
#define UI_CARD_NONE 0xFFU

/*
 * 注册序号 → 设置位（CAR_SETTINGS_CARD_*）。
 * 注册序号即创建顺序：导航 / 行程 / 4x4-i / 胎压 / 底盘 / 短期 / 油耗 / 经济 / 多媒体 / 互联，
 * 而设置列表里行程排在 4x4-i、胎压之后，所以 1~3 号需要做置换。
 */
static const uint8_t s_card_setting_bit[UI_CARD_MAX] = {
    0U, 4U, 1U, 2U, 3U, 5U, 6U, 7U, 8U, 9U
};

/* Style1 页面私有状态 */
typedef struct {
    ui_node_t base;
    speed_forester1_t speed;
    rotate_forester1_t rotate;
    fuel_forester1_t fuel;
    water_forester1_t water;
    ui_gear_t gear;
    ui_mileage_t odo;
    ui_mileage_t trip;
    ui_range_t range;
    ui_clock_t clock;
    ui_outside_temp_t outside_temp;
    ui_turn_signal_t turn_signal;
    ui_lamp_row_t top_lamps;
     ui_cruise_t cruise;
    ui_small_door_t mini_door;
    /* 中心车辆视图：4 帧资源常驻，靠 HIDDEN 标志切换 */
    lv_obj_t *car_clip;
    lv_obj_t *car_lane[CAR_FRAME_COUNT];
    lv_obj_t *car_body[CAR_FRAME_COUNT];
    lv_obj_t *car_mask;
    lv_obj_t *car_width_lamp;
    lv_obj_t *car_turn_left;
    lv_obj_t *car_turn_right;
    uint8_t car_frame;        /* 当前帧 0~3 */
    uint8_t car_running;      /* 帧动画是否运行 */
    uint32_t car_frame_tick;  /* 上一帧时间戳 */
    uint8_t car_color_applied; /* 车身染色是否已生效（0 未染色） */
    uint8_t car_color_r;
    uint8_t car_color_g;
    uint8_t car_color_b;

    /* 中心卡片容器（Qt StackLayout 553,140,814,509），卡片逐个挂在这里 */
    lv_obj_t *central_stack;
    itinerary_card_t itin;
    page4i_card_t card4i;
    tire_card_t tire;
    chassis_card_t chassis;
    short_term_card_t short_term;
    instant_fuel_card_t instant_fuel;
    eco_card_t eco;
    music_card_t music;
    connect_card_t connect;
    /* 卡片注册表：顺序即页签顺序，0 = 导航（车辆视图） */
    lv_obj_t *cards[UI_CARD_MAX];
    lv_area_t card_orig[UI_CARD_MAX]; /* 卡片原始区域（x1,y1,x2,y2），隐藏后用于还原 */
    uint8_t card_count;
    uint8_t card_index;     /* 当前卡的注册序号，UI_CARD_NONE 表示无可见卡 */
    uint8_t card_layout_initialized;
    uint32_t card_mask;     /* 已生效的设置位图，变化时重建可见集合 */

    /* ECO 指示图标：Qt IconGeneral(350,380) 在左表盘容器内，无故障码 */
    lv_obj_t *eco_lamp;

    /* Set 徽标：Qt IconSet，白底圆角 + "set"，由 cruise_state bit1 控制显隐 */
    lv_obj_t *set_badge;
    bool set_badge_shown;

    /* 覆盖层：创建在页面最上层，任一激活时隐藏中心卡片容器 */
    ov_door_t ov_door;
    ov_tire_t ov_tire;
    ov_radar_t ov_radar;
    ov_cruise_t ov_cruise;
    ov_tips_t ov_tips;
} s1_page_t;

/*
 * 顶部灯排（Qt Row 位于 720,92）
 * 顺序与宽度对应 QML：自动远光 56、示廓 56、近光 56、远光 56、前雾 40、后雾 40。
 * 近光灯同时接受 15（正常）与 229（远光辅助故障图），有 229 时优先故障图。
 */
static const ui_lamp_row_item_t s_top_lamps[] = {
    {"WarnIcon/auto_lamp.png",       NULL,                            16U, 0U,   56, 56},
    {"WarnIcon/width_lemp.png",      NULL,                            12U, 0U,   56, 56},
    {"WarnIcon/low_beam.png",        "WarnIcon/low_beam_faulty.png",  15U, 229U, 56, 56},
    {"WarnIcon/high_lamp.png",       NULL,                             9U, 0U,   56, 56},
    {"WarnIcon/FrontFogLights.png",  NULL,                            11U, 0U,   40, 40},
    {"WarnIcon/rear_fog_lights.png", NULL,                            10U, 0U,   40, 40},
};

#define TOP_LAMP_COUNT (uint8_t)(sizeof(s_top_lamps) / sizeof(s_top_lamps[0]))

/*
 * 左表盘灯群（Qt 中为 ForesterSpeed1 的子项）
 * 表项顺序与 Qt 声明顺序一致，保证重叠灯的 z 序相同。
 * group 用 static：跨样式切换重建页面时复用已加载的图片 RAM。
 */
static ui_lamp_group_t s_left_lamps;

static const ui_lamp_group_item_t s_left_lamp_items[] = {
    /* 制动液位低（Qt 水平居中 y=226）46×46，码 5 */
    { .variants = {{"WarnIcon/brake_r.png", 5U, 0U}}, .variant_count = 1,
      .x = 453, .y = 305, .w = 46, .h = 46 },

    /* 电子手刹 (340,226) 46×46，码 23 */
    { .variants = {{"WarnIcon/park.png", 23U, 0U}}, .variant_count = 1,
      .x = 506, .y = 305, .w = 46, .h = 46 },

    /* 自动驻车 (400,226) 46×46，码 46 / 91 共用一张图 */
    { .variants = {{"WarnIcon/park_auto.png", 46U, 91U}}, .variant_count = 1,
      .x = 566, .y = 305, .w = 46, .h = 46 },

    /* 胎压 (234,220) 52×52，码 24 */
    { .variants = {{"WarnIcon/tire_faulty.png", 24U, 0U}}, .variant_count = 1,
      .x = 400, .y = 299, .w = 52, .h = 52 },

    /* 前大灯随动转向 (122,216) 64×64，码 210 */
    { .variants = {{"WarnIcon/210.png", 210U, 0U}}, .variant_count = 1,
      .x = 288, .y = 295, .w = 64, .h = 64 },

    /* EyeSight 警告/暂停 (176,216) 64×64，码 216 / 228 */
    { .variants = {{"WarnIcon/216.png", 216U, 0U},
                   {"WarnIcon/228.png", 228U, 0U}},
      .variant_count = 2, .x = 342, .y = 295, .w = 64, .h = 64 },

    /* 防碰撞关闭/警告 (288,112) 52×52，码 107 / 108 */
    { .variants = {{"WarnIcon/anti-collision_r.png", 107U, 0U},
                   {"WarnIcon/anti-collision.png", 108U, 0U}},
      .variant_count = 2, .x = 454, .y = 191, .w = 52, .h = 52 },

    /* 坡路自动驻车报警 (226,112) 64×64，码 207 */
    { .variants = {{"WarnIcon/207.png", 207U, 0U}}, .variant_count = 1,
      .x = 392, .y = 191, .w = 64, .h = 64 },

    /* ESC 故障 (180,162) 52×52，码 19 */
    { .variants = {{"WarnIcon/19.png", 19U, 0U}}, .variant_count = 1,
      .x = 346, .y = 241, .w = 52, .h = 52 },

    /* ESC 关闭 (230,166) 52×52，码 20 */
    { .variants = {{"WarnIcon/ESCOFF.png", 20U, 0U}}, .variant_count = 1,
      .x = 396, .y = 245, .w = 52, .h = 52 },

    /* LAS 车道偏离 (286,166) 52×52，码 49/52 黄、50 灰、51 绿 */
    { .variants = {{"WarnIcon/LAS-y.png", 49U, 52U},
                   {"WarnIcon/LAS-2.png", 50U, 0U},
                   {"WarnIcon/LAS-1.png", 51U, 0U}},
      .variant_count = 3, .x = 452, .y = 245, .w = 52, .h = 52 },

    /* 变速箱温度高 (130,270) 64×64，码 8 */
    { .variants = {{"WarnIcon/8.png", 8U, 0U}}, .variant_count = 1,
      .x = 296, .y = 349, .w = 64, .h = 64 },

    /* 后差速器油温高 (196,270) 64×64，码 204 */
    { .variants = {{"WarnIcon/204.png", 204U, 0U}}, .variant_count = 1,
      .x = 362, .y = 349, .w = 64, .h = 64 },

    /* 智能模式 (136,340) 52×52，码 232 绿 / 233 黄 / 234 红 */
    { .variants = {{"WarnIcon/smart_g.png", 232U, 0U},
                   {"WarnIcon/smart_y.png", 233U, 0U},
                   {"WarnIcon/smart_r.png", 234U, 0U}},
      .variant_count = 3, .x = 302, .y = 419, .w = 52, .h = 52 },

    /* EV 模式 (186,340) 52×52，码 235 */
    { .variants = {{"WarnIcon/EV.png", 235U, 0U}}, .variant_count = 1,
      .x = 352, .y = 419, .w = 52, .h = 52 },

    /* AVH 自动车辆保持 (148,384) 64×64，码 220 */
    { .variants = {{"WarnIcon/220.png", 220U, 0U}}, .variant_count = 1,
      .x = 314, .y = 463, .w = 64, .h = 64 },

    /* 外灯损坏 (188,428) 54×54，码 13 / 14 */
    { .variants = {{"WarnIcon/outer_faulty.png", 13U, 0U},
                   {"WarnIcon/14.png", 14U, 0U}},
      .variant_count = 2, .x = 354, .y = 507, .w = 54, .h = 54 },

    /* EPS 助力转向（Item 40,480 内）52×52，码 6 */
    { .variants = {{"WarnIcon/eps_faulty.png", 6U, 0U}}, .variant_count = 1,
      .x = 40, .y = 480, .w = 52, .h = 52 },

    /* ABS（Item 40,480 内 y=52）46×46，码 4 */
    { .variants = {{"WarnIcon/abs.png", 4U, 0U}}, .variant_count = 1,
      .x = 40, .y = 532, .w = 46, .h = 46 },

    /* 雷达（Item 40,480 内 60,110）52×52，码 59 绿 / 129 黄 / 173 红 */
    { .variants = {{"WarnIcon/P59_g.png", 59U, 0U},
                   {"WarnIcon/P59_y.png", 129U, 0U},
                   {"WarnIcon/P59_r.png", 173U, 0U}},
      .variant_count = 3, .x = 100, .y = 590, .w = 52, .h = 52 },
};

/*
 * 右表盘灯群（Qt 中为 ForesterRotate1 的子项，表盘原点 1134,79）
 * 表项顺序与 Qt 声明顺序一致，保证重叠灯的 z 序相同。
 * 未接入的三项：
 *   baoyang（保养，Qt 码 290 超出 uint8_t 故障码范围）
 *   coolant（冷却液，Qt 中码 232 同时绑定 coolant 与 smart，后定义者生效，实际由智能模式灯占用）
 *   eco（环保图标，由设置项 ecoEnable/ecoIndicator 驱动，不属于故障码）
 */
static ui_lamp_group_t s_right_lamps;

static const ui_lamp_group_item_t s_right_lamp_items[] = {
    /* 机油液位低 (290,216) 64×64，码 206 */
    { .variants = {{"WarnIcon/oil_y.png", 206U, 0U}}, .variant_count = 1,
      .x = 1424, .y = 295, .w = 64, .h = 64 },

    /* 机油压力 (356,216) 64×64，码 25 */
    { .variants = {{"WarnIcon/oil.png", 25U, 0U}}, .variant_count = 1,
      .x = 1490, .y = 295, .w = 64, .h = 64 },

    /* READY 指示灯 (358,266) 64×64，码 139 绿 / 140 红 / 241 白 */
    { .variants = {{"WarnIcon/READY_g.png", 139U, 0U},
                   {"WarnIcon/READY_r.png", 140U, 0U},
                   {"WarnIcon/acc_ready.png", 241U, 0U}},
      .variant_count = 3, .x = 1492, .y = 345, .w = 64, .h = 64 },

    /* 全时四驱 AWD (428,216) 64×64，码 203 */
    { .variants = {{"WarnIcon/203.png", 203U, 0U}}, .variant_count = 1,
      .x = 1562, .y = 295, .w = 64, .h = 64 },

    /* 四驱故障 (286,180) 52×52，码 36；Qt visible: SettingsMgr.show4h */
    { .variants = {{"WarnIcon/4WD_l.png", 36U, 0U}}, .variant_count = 1,
      .gate = (uint8_t)UI_LAMP_GATE_SHOW_4H,
      .x = 1420, .y = 259, .w = 52, .h = 52 },

    /* 四驱打开 (226,180) 52×52，码 33；Qt visible: SettingsMgr.show4h */
    { .variants = {{"WarnIcon/four_g.png", 33U, 0U}}, .variant_count = 1,
      .gate = (uint8_t)UI_LAMP_GATE_SHOW_4H,
      .x = 1360, .y = 259, .w = 52, .h = 52 },

    /* hole 指示灯 (406,180) 52×52，码 238 */
    { .variants = {{"WarnIcon/hole_w.png", 238U, 0U}}, .variant_count = 1,
      .x = 1540, .y = 259, .w = 52, .h = 52 },

    /* 行人接近警告 (434,333) 52×52，码 236 开启 / 237 关闭 */
    { .variants = {{"WarnIcon/people_close_g.png", 236U, 0U},
                   {"WarnIcon/people_close_r.png", 237U, 0U}},
      .variant_count = 2, .x = 1568, .y = 412, .w = 52, .h = 52 },

    /* 自动车辆保持工作 (434,273) 52×52，码 222 */
    { .variants = {{"WarnIcon/222.png", 222U, 0U}}, .variant_count = 1,
      .x = 1568, .y = 352, .w = 52, .h = 52 },

    /* 主驾安全带 (280,442) 42×42，码 17 */
    { .variants = {{"WarnIcon/belt1.png", 17U, 0U}}, .variant_count = 1,
      .x = 1414, .y = 521, .w = 42, .h = 42 },

    /* 副驾安全带 (325,442) 42×42，码 18 */
    { .variants = {{"WarnIcon/belt2.png", 18U, 0U}}, .variant_count = 1,
      .x = 1459, .y = 521, .w = 42, .h = 42 },

    /* 安全气囊 (370,442) 42×42，码 3 */
    { .variants = {{"WarnIcon/airbag_faulty.png", 3U, 0U}}, .variant_count = 1,
      .x = 1504, .y = 521, .w = 42, .h = 42 },
};

/*
 * 底部灯排（Qt Row 位于 660,630）
 * 坐标按 Qt 子项宽度依次累加；XMode/Set 仍未迁移。
 */
static ui_lamp_group_t s_bottom_lamps;

static const ui_lamp_group_item_t s_bottom_lamp_items[] = {
    /* 发动机故障 (0,0) 52×52，码 2 */
    { .variants = {{"WarnIcon/engine_faulty.png", 2U, 0U}}, .variant_count = 1,
      .x = 660, .y = 630, .w = 52, .h = 52 },

    /* 自动启停警告 (64×64 容器内居中) 64×64，码 27 / 219 */
    { .variants = {{"WarnIcon/217.png", 27U, 219U}}, .variant_count = 1,
      .x = 712, .y = 630, .w = 64, .h = 64 },

    /* 自动启停未检测到操作，与上一盏完全重叠，码 28 */
    { .variants = {{"WarnIcon/219.png", 28U, 0U}}, .variant_count = 1,
      .x = 712, .y = 630, .w = 64, .h = 64 },

    /* 智能怠速正在工作 (52×52) ，码 26 */
    { .variants = {{"WarnIcon/26.png", 26U, 0U}}, .variant_count = 1,
      .x = 776, .y = 630, .w = 52, .h = 52 },

    /*
     * 紧急制动 (828,630) 58×58：Qt jinw/jiny，白图常显（受 emergencyBraking 门控），
     * 黄图在 showcar > 0 时覆盖白图。
     */
    { .variants = {{"WarnIcon/jin_w.png", 0U, 0U},
                   {"WarnIcon/jin_y.png", 0U, 0U}},
      .variant_count = 2, .gate = UI_LAMP_GATE_EMERGENCY_BRAKING,
      .x = 828, .y = 630, .w = 58, .h = 58 },

    /* 盲区检测 (886,630) 58×58：Qt hidew/hidey（BSD），黄图在 showarea > 0 时显示 */
    { .variants = {{"WarnIcon/BSD_w.png", 0U, 0U},
                   {"WarnIcon/BSD_y.png", 0U, 0U}},
      .variant_count = 2, .gate = UI_LAMP_GATE_BLIND_SPOT,
      .x = 886, .y = 630, .w = 58, .h = 58 },

    /* 车道偏离开关 (944,630) 58×58：Qt ldaw/lday（real_road），黄图在 carState > 1 时显示 */
    { .variants = {{"WarnIcon/real_road_w_n.png", 0U, 0U},
                   {"WarnIcon/real_road_y_n.png", 0U, 0U}},
      .variant_count = 2, .gate = UI_LAMP_GATE_LANE_DEPARTURE,
      .x = 944, .y = 630, .w = 58, .h = 58 },

    /* XMode (1002,630) 64×64：Qt IconXMode，drive_mode 6/7/8 → xmode_w/snow/deepsnow */
    { .variants = {{"WarnIcon/xmode_w.png", 0U, 0U},
                   {"WarnIcon/snow.png", 0U, 0U},
                   {"WarnIcon/deepsnow.png", 0U, 0U}},
      .variant_count = 3, .gate = UI_LAMP_GATE_XMODE,
      .x = 1002, .y = 630, .w = 64, .h = 64 },

    /* 陡坡缓降 (58×58，行内垂直居中) ，码 30 / 94 */
    { .variants = {{"WarnIcon/steep_y.png", 30U, 94U}}, .variant_count = 1,
      .x = 1066, .y = 633, .w = 58, .h = 58 },
};

/* 在指定位置创建图片控件，返回控件句柄 */
static lv_obj_t *add_image(lv_obj_t *parent, const void *src, int32_t x, int32_t y)
{
    lv_obj_t *img = lv_image_create(parent);

    lv_image_set_src(img, src);
    lv_obj_set_pos(img, x, y);

    return img;
}

/* 创建左侧车速表，具体坐标与刻度由 SpeedDial/speed_forester1 负责 */
static void speed_gauge_create(lv_obj_t *parent, s1_page_t *ui)
{
    (void)speed_forester1_create(&ui->speed, parent, 166, 79);
}

/* 创建右侧转速表，具体坐标与刻度由 RotationDial/rotate_forester1 负责 */
static void tach_gauge_create(lv_obj_t *parent, s1_page_t *ui)
{
    (void)rotate_forester1_create(&ui->rotate, parent, 1134, 79);
}

/*
 * 中心车辆视图（对应 NavigationForester.qml）
 *
 * Qt 侧坐标链：
 *   StackLayout(553,140,814,509) → Loader 居中 900x620 (偏移 -43,-55.5)
 *   → item.x=-40, item.y=-180 → item 左上角 (470,-95.5)
 *   → 图片在 item 内 (170,330)，裁剪 sourceClipRect(170,330,660,340)
 * 换算到页面坐标：容器 (640,235)，尺寸 660x340，源图偏移 (-170,-330)。
 *
 * 图层顺序与 Qt 一致：车道 → 车身 → 颜色遮罩 → 示宽灯 → 左右转向灯。
 * 车色 multiply 与右舵镜像依赖设置项，暂未接入（默认白色 = 无效果）。
 */
static const char *const s_car_lane_paths[CAR_FRAME_COUNT] = {
    UI_IMG("SubaruForester/navigation/CheDao_01.png"),
    UI_IMG("SubaruForester/navigation/CheDao_02.png"),
    UI_IMG("SubaruForester/navigation/CheDao_03.png"),
    UI_IMG("SubaruForester/navigation/CheDao_04.png"),
};

static const char *const s_car_body_paths[CAR_FRAME_COUNT] = {
    UI_IMG("SubaruForester/navigation/lun_01.png"),
    UI_IMG("SubaruForester/navigation/lun_02.png"),
    UI_IMG("SubaruForester/navigation/lun_03.png"),
    UI_IMG("SubaruForester/navigation/lun_04.png"),
};

/* 只显示指定帧：8 张图一次性创建好，切换时仅改可见性，不重建对象 */
static void car_view_show_frame(s1_page_t *ui, uint8_t frame)
{
    uint8_t i;

    for(i = 0U; i < CAR_FRAME_COUNT; ++i) {
        if(i == frame) {
            lv_obj_remove_flag(ui->car_lane[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(ui->car_body[i], LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(ui->car_lane[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui->car_body[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/* 创建中心车辆视图，初始停在第 1 帧（Qt 的 amCount = 1） */
static void central_card_create(lv_obj_t *parent, s1_page_t *ui)
{
    uint8_t i;

    /* 各卡片根对象默认不参与父级 flex 布局，stack 里才不会因隐藏/置 0 而整体位移 */
    ui->car_clip = lv_obj_create(parent);
    lv_obj_add_flag(ui->car_clip, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_remove_style_all(ui->car_clip);
    lv_obj_remove_flag(ui->car_clip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ui->car_clip, 660, 340);
    lv_obj_set_pos(ui->car_clip, 87, 95);   /* 页面 (640,235) 减去容器原点 (553,140) */

    /* 车道帧 → 车身帧 → 颜色遮罩 */
    for(i = 0U; i < CAR_FRAME_COUNT; ++i) {
        ui->car_lane[i] = add_image(ui->car_clip, s_car_lane_paths[i],
                                    -170, -330);
    }

    for(i = 0U; i < CAR_FRAME_COUNT; ++i) {
        ui->car_body[i] = add_image(ui->car_clip, s_car_body_paths[i],
                                    -170, -330);
    }

    ui->car_mask = add_image(ui->car_clip,
                             UI_IMG("SubaruForester/navigation/lun_Color_mask.png"),
                             -170, -330);

    /*
     * 车身染色（对应 NavigationForester.qml 的 Blend multiply）：
     * lun_Color_mask 是白色车身剪影 + 半透明阴影，叠在待染色的 lun_0X 上，
     * 以 MULTIPLY 混合即得"剪影 × 颜色"，阴影与车身明暗由掩码自身灰度保留。
     * Qt 在剪影层上用 multiply 混合纯色 Rectangle；这里等价的做法是把
     * 纯色以 multiply 叠到剪影上，剪影 alpha 决定染色范围。
     */
    lv_obj_set_style_blend_mode(ui->car_mask, LV_BLEND_MODE_MULTIPLY, 0);
    /* 未收到颜色前不染色，保持素材原色（Qt 未设置时同样是素材色） */
    lv_obj_set_style_image_recolor_opa(ui->car_mask, LV_OPA_TRANSP, 0);

    /* 示宽灯与左右转向灯：同为整图叠加，靠可见性亮灭 */
    ui->car_width_lamp = add_image(ui->car_clip,
                                   UI_IMG("SubaruForester/navigation/Deng_01.png"),
                                   -170, -330);
    ui->car_turn_left = add_image(ui->car_clip,
                                  UI_IMG("SubaruForester/navigation/Deng_02.png"),
                                  -170, -330);
    ui->car_turn_right = add_image(ui->car_clip,
                                   UI_IMG("SubaruForester/navigation/Deng_03.png"),
                                   -170, -330);

    lv_obj_add_flag(ui->car_width_lamp, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->car_turn_left, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->car_turn_right, LV_OBJ_FLAG_HIDDEN);

    ui->car_frame = 0U;
    ui->car_running = 0U;
    car_view_show_frame(ui, 0U);
}

/*
 * 中心车辆视图动态部分
 *   帧轮播：车速 > 0 且非 P 挡，150ms 一帧，停车时停在当前帧（同 Qt runTimer）
 *   示宽灯：与顶部灯排示廓灯同源（0x02 码 12）
 *   转向灯：复用顶部转向灯的左右灯态，500ms 半周期、1000ms 完整周期
 */
/* 车身颜色只在取值变化时重设样式，避免每帧触发重绘 */
static void car_view_apply_color(s1_page_t *ui, const car_color_t *color)
{
    if(!color->valid) {
        if(ui->car_color_applied != 0U) {
            lv_obj_set_style_image_recolor_opa(ui->car_mask, LV_OPA_TRANSP, 0);
            ui->car_color_applied = 0U;
        }
        return;
    }
    if(ui->car_color_applied != 0U && ui->car_color_r == color->red &&
       ui->car_color_g == color->green && ui->car_color_b == color->blue) {
        return;
    }
    lv_obj_set_style_image_recolor(
        ui->car_mask,
        lv_color_make(color->red, color->green, color->blue), 0);
    lv_obj_set_style_image_recolor_opa(ui->car_mask, LV_OPA_COVER, 0);
    ui->car_color_applied = 1U;
    ui->car_color_r = color->red;
    ui->car_color_g = color->green;
    ui->car_color_b = color->blue;
}

static void car_view_apply(s1_page_t *ui, const car_state_t *st)
{
    uint32_t now = lv_tick_get();

    car_view_apply_color(ui, &st->car_color);

    if(st->speed_rpm.valid && st->speed_rpm.speed_kph > 0U &&
       st->gear.valid && st->gear.text[0] != 'P' && st->gear.text[0] != 'p') {
        if(ui->car_running == 0U) {
            ui->car_running = 1U;
            ui->car_frame = 0U;
            ui->car_frame_tick = now;
            car_view_show_frame(ui, 0U);
        }
        else if(lv_tick_elaps(ui->car_frame_tick) >= CAR_FRAME_MS) {
            ui->car_frame_tick = now;
            ui->car_frame = (uint8_t)((ui->car_frame + 1U) % CAR_FRAME_COUNT);
            car_view_show_frame(ui, ui->car_frame);
        }
    }
    else {
        ui->car_running = 0U;
    }

    /* 示宽灯：Qt 绑 iconPositionLight.show，即顶部灯排的示廓灯 */
    if(st->warnings.valid) {
        if(car_state_warning_level(st, 12U) != 0U) {
            lv_obj_remove_flag(ui->car_width_lamp, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(ui->car_width_lamp, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* 直接消费本次 ui_turn_signal_apply 的灯态，不再独立计时。 */
    if(st->turn.valid) {
        if(ui_lamp_get_state(&ui->turn_signal.left) == UI_LAMP_ON) {
            lv_obj_remove_flag(ui->car_turn_left, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(ui->car_turn_left, LV_OBJ_FLAG_HIDDEN);
        }

        if(ui_lamp_get_state(&ui->turn_signal.right) == UI_LAMP_ON) {
            lv_obj_remove_flag(ui->car_turn_right, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(ui->car_turn_right, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/* 注册一张中心卡片：记录根对象及原始区域，可见性由 card_show 统一管理 */
static void card_register(s1_page_t *ui, lv_obj_t *root)
{
    if(root == NULL || ui->card_count >= UI_CARD_MAX) {
        return;
    }

    lv_area_set(&ui->card_orig[ui->card_count], lv_obj_get_x(root), lv_obj_get_y(root),
                lv_obj_get_x(root) + lv_obj_get_width(root) - 1,
                lv_obj_get_y(root) + lv_obj_get_height(root) - 1);
    ui->cards[ui->card_count++] = root;
}

static bool card_enabled(const s1_page_t *ui, uint8_t index)
{
    return (ui->card_mask & (UINT32_C(1) << s_card_setting_bit[index])) != 0U;
}

static void card_set_mask(s1_page_t *ui, uint32_t mask);

/* 切换中心卡片：等价 Qt StackLayout 的 currentIndex，同一时刻最多显示一张。
 * 容器是 flex 布局，隐藏子对象会被 flex 跳过、后面的卡片整体前移，
 * 所以这里关掉布局、卡片全部绝对定位，隐藏只看 HIDDEN 标志，不去动尺寸。 */
static void card_show(s1_page_t *ui, uint8_t index)
{
    uint8_t i;

    if(index != UI_CARD_NONE && index >= ui->card_count) {
        return;
    }

    if(!ui->card_layout_initialized) {
        lv_obj_set_layout(ui->central_stack, LV_LAYOUT_NONE);
        for(i = 0U; i < ui->card_count; ++i) {
            lv_obj_set_pos(ui->cards[i], ui->card_orig[i].x1,
                           ui->card_orig[i].y1);
            if(i != index) {
                lv_obj_add_flag(ui->cards[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
        ui->card_layout_initialized = 1U;
    }
    else if(ui->card_index != index) {
        if(ui->card_index != UI_CARD_NONE && ui->card_index < ui->card_count) {
            lv_obj_add_flag(ui->cards[ui->card_index], LV_OBJ_FLAG_HIDDEN);
        }
        if(index != UI_CARD_NONE) {
            lv_obj_remove_flag(ui->cards[index], LV_OBJ_FLAG_HIDDEN);
        }
    }

    ui->card_index = index;
}

/*
 * Qt rebuildPageModel：按勾选重建可见集合，当前卡仍可见则保留，
 * 否则落到第一张可见卡；全部关闭时中心区域为空。
 */
static void card_set_mask(s1_page_t *ui, uint32_t mask)
{
    uint8_t i;

    ui->card_mask = mask;
    if(ui->card_index != UI_CARD_NONE && ui->card_index < ui->card_count &&
       card_enabled(ui, ui->card_index)) {
        card_show(ui, ui->card_index);
        return;
    }

    for(i = 0U; i < ui->card_count; ++i) {
        if(card_enabled(ui, i)) {
            card_show(ui, i);
            return;
        }
    }
    card_show(ui, UI_CARD_NONE);
}

/* 上一张 / 下一张：只在可见卡里循环（Qt 的 decrement/incrementCurrentIndex） */
static void card_step(s1_page_t *ui, int8_t step)
{
    uint8_t index;

    if(ui->card_count == 0U) {
        return;
    }

    /* 掩码已在车况更新时同步；按键路径只同步确有变化的设置值。 */
    {
        uint32_t mask = ui_app_center_card_mask();
        if(mask != ui->card_mask) {
            card_set_mask(ui, mask);
        }
    }

    if(ui->card_index == UI_CARD_NONE) {
        return;
    }

    index = ui->card_index;
    for(uint8_t n = 0U; n < ui->card_count; ++n) {
        int32_t next = (int32_t)index + step;
        if(next < 0) {
            next += (int32_t)ui->card_count;
        }
        else if(next >= (int32_t)ui->card_count) {
            next -= (int32_t)ui->card_count;
        }
        index = (uint8_t)next;
        if(card_enabled(ui, index)) {
            card_show(ui, index);
            return;
        }
    }
}

/* 创建整页：按 Qt 的 z 序依次摆放表盘、中央面板、车辆视图与辅助信息 */
static bool s1_create(ui_node_t *self, lv_obj_t *parent)
{
    s1_page_t *ui = (s1_page_t *)self;
    lv_obj_t *page = ui_page_create(parent);

    self->root = page;

    /* z=0：两个表盘 */
    speed_gauge_create(page, ui);
    tach_gauge_create(page, ui);

        /* 左表盘灯群：位于背景图之前，保持 Qt 的 z 序 */
    (void)ui_lamp_group_create(&s_left_lamps, page, s_left_lamp_items,
                               (uint8_t)(sizeof(s_left_lamp_items) /
                                         sizeof(s_left_lamp_items[0])));

    /* 右表盘灯群：同在 z=0，位于中央面板背景之前 */
    (void)ui_lamp_group_create(&s_right_lamps, page, s_right_lamp_items,
                               (uint8_t)(sizeof(s_right_lamp_items) /
                                         sizeof(s_right_lamp_items[0])));

    /* 底部灯排：Qt Row 无显式 z，同样压在中央面板背景之下 */
    (void)ui_lamp_group_create(&s_bottom_lamps, page, s_bottom_lamp_items,
                               (uint8_t)(sizeof(s_bottom_lamp_items) /
                                         sizeof(s_bottom_lamp_items[0])));

    /* ECO 指示：表盘原点 (166,79) + Qt 子项 (350,380)，默认隐藏 */
    ui->eco_lamp = add_image(page, UI_IMG("WarnIcon/eco.png"), 516, 459);
    lv_obj_add_flag(ui->eco_lamp, LV_OBJ_FLAG_HIDDEN);

    /*
     * Set 徽标：Qt IconSet 64×36，Row 中位于 XMode 之后、垂直居中。
     * 白底 #FFFFFF、圆角 6、加粗 "set" 28px，默认隐藏。
     */
    ui->set_badge = lv_obj_create(page);
    lv_obj_remove_style_all(ui->set_badge);
    lv_obj_remove_flag(ui->set_badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(ui->set_badge, 1066, 644);
    lv_obj_set_size(ui->set_badge, 64, 36);
    lv_obj_set_style_bg_color(ui->set_badge, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(ui->set_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ui->set_badge, 6, 0);

    lv_obj_t *set_label = lv_label_create(ui->set_badge);
    lv_label_set_text(set_label, "set");
    lv_obj_set_style_text_color(set_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(set_label, &lv_font_montserrat_28, 0);
    lv_obj_center(set_label);
    lv_obj_add_flag(ui->set_badge, LV_OBJ_FLAG_HIDDEN);

    /* 转向灯：Qt IconTurnSignals 位于 (660,98)，宽 600，左右图标各 54x48 */
    (void)ui_turn_signal_create(&ui->turn_signal, page,
                                660, 98, 1206, 98, 54, 48);

    /* z=1：中央半透明面板 */
    add_image(page, UI_IMG("SubaruForester/bg_01.png"), 568, 150);

    /* z=2：中心卡片容器（Qt StackLayout 553,140,814,509），当前显示车辆视图 */
    ui->central_stack = ui_blank_container(page);
    lv_obj_set_size(ui->central_stack, 814, 509);
    lv_obj_set_pos(ui->central_stack, 553, 140);

    /* 车辆视图容器在 stack 内的 (87,95) = 页面 (640,235) */
    central_card_create(ui->central_stack, ui);

    /* 行程数据卡片：与 Qt 一样作为第 2 页 */
    (void)itinerary_card_create(&ui->itin, ui->central_stack);
        /* 4x4-i 卡片：Qt 页签顺序第 2 张 */
    (void)page4i_card_create(&ui->card4i, ui->central_stack);
        /* 胎压卡片：Qt 页签顺序第 3 张 */
    (void)tire_card_create(&ui->tire, ui->central_stack);
        /* 底盘控制卡片：Qt 页签顺序第 5 张（排在行程数据之后） */
    (void)chassis_card_create(&ui->chassis, ui->central_stack);
          /* 短期记忆卡片：Qt 页签顺序第 6 张 */
    (void)short_term_card_create(&ui->short_term, ui->central_stack);
         /* 瞬时油耗卡片：Qt 页签顺序第 7 张 */
    (void)instant_fuel_card_create(&ui->instant_fuel, ui->central_stack);
        /* 燃油经济性卡片：Qt 页签顺序第 8 张 */
    (void)eco_card_create(&ui->eco, ui->central_stack);
           /* 多媒体卡片：Qt 页签顺序第 9 张 */
    (void)music_card_create(&ui->music, ui->central_stack);
        /* 互联信息卡片：Qt 页签顺序第 10 张 */
    (void)connect_card_create(&ui->connect, ui->central_stack);

     /* 注册中心卡片：必须与上面创建顺序一致，0 导航 / 1 行程 / 2 4x4-i / 3 胎压 / 4 底盘 … */
    card_register(ui, ui->car_clip);
    card_register(ui, ui->itin.root);
    card_register(ui, ui->card4i.root);
    card_register(ui, ui->tire.root);
    card_register(ui, ui->chassis.root);
    card_register(ui, ui->short_term.root);
    card_register(ui, ui->instant_fuel.root);
    card_register(ui, ui->eco.root);
    card_register(ui, ui->music.root);
    card_register(ui, ui->connect.root);
    ui->card_index = 0U;
    card_set_mask(ui, ui_app_center_card_mask());

        /* 顶部灯排：行高按 Qt 最高子项 64 计算，56/40 图标垂直居中 */
    (void)ui_lamp_row_create(&ui->top_lamps, page, 720, 92, 64,
                             s_top_lamps, TOP_LAMP_COUNT);

        /* 定速巡航：Qt IconConstantSpeed 位于灯排末尾 (1024,98)，行高 52 */
    (void)ui_cruise_create(&ui->cruise, page, 1024, 98,
                           &lv_font_montserrat_24, &lv_font_montserrat_14);

    /* 小车门：Qt PageSmallDoor 64×64 位于 (1152,92)，仅在故障码 22 时显示 */
    (void)ui_small_door_create(&ui->mini_door, page, 1152, 92,
                               UI_IMG("Door/Style1/small_door_bg.png"),
                               UI_IMG("Door/Style1/small_door_left_front.png"),
                               UI_IMG("Door/Style1/small_door_right_front.png"));

    /* z=2：辅助仪表与文字 */
    fuel_forester1_create(&ui->fuel, page, 1589, 295);
    water_forester1_create(&ui->water, page, 81, 295);

    /* 档位、里程、续航：控件通用，样式只决定坐标与字号 */
    ui_gear_create(&ui->gear, page, 920, 564, 90, &lv_font_montserrat_46);
    ui_mileage_create(&ui->odo, page, 620, 586, UI_MILEAGE_ODO,
                      &lv_font_montserrat_28);
    ui_mileage_create(&ui->trip, page, 1610, 656, UI_MILEAGE_TRIP,
                      &lv_font_montserrat_24);   /* 小计里程，Qt 位置 (1610,656) */
    ui_range_create(&ui->range, page, 1110, 586, &lv_font_montserrat_24);

    /* 时钟与车外温度：控件通用，样式只决定坐标与字号（Qt 位置 910,138 / 1160,145） */
    ui_clock_create(&ui->clock, page, 910, 138, &lv_font_montserrat_36);
    ui_outside_temp_create(&ui->outside_temp, page, 1160, 145,
                           &lv_font_montserrat_28);
    /* Qt SettingsMgr.outTemperature 缺省 false，温度行初始隐藏 */
    lv_obj_add_flag(ui->outside_temp.root, LV_OBJ_FLAG_HIDDEN);

    /* 覆盖层最后创建，位于所有页面元素之上（Qt z=60/66） */
    (void)ov_door_create(&ui->ov_door, page);
    (void)ov_tire_create(&ui->ov_tire, page);
    (void)ov_radar_create(&ui->ov_radar, page);
    (void)ov_cruise_create(&ui->ov_cruise, page);
    /* 提示层最后创建，始终位于其他覆盖层之上 */
    (void)ov_tips_create(&ui->ov_tips, page);

    return true;
}

/* 销毁页面根对象，子控件随之释放；指针 RAM 图保留供下次切回复用 */
static void s1_destroy(ui_node_t *self)
{
    s1_page_t *ui = (s1_page_t *)self;

    ov_door_destroy(&ui->ov_door);
    ov_tire_destroy(&ui->ov_tire);
    ov_radar_destroy(&ui->ov_radar);
    ov_cruise_destroy(&ui->ov_cruise);
    ov_tips_destroy(&ui->ov_tips);

    /* 音乐卡片的柱状动画定时器不随对象树自动释放，必须显式销毁 */
    music_card_destroy(&ui->music);

    if(self->root != NULL) {
        lv_obj_delete(self->root);
        self->root = NULL;
    }
}

/* Qt ismedia：多媒体卡片（PageMusic）当前可见，用于遮挡通用提示 */
static bool s1_media_visible(const s1_page_t *ui)
{
    return ui->card_index != UI_CARD_NONE &&
           ui->card_index < ui->card_count &&
           ui->cards[ui->card_index] == ui->music.root;
}

/* 刷新动态部分：指针角度与油量/水温刻度位置，静态元素不动 */
static void s1_apply(ui_node_t *self, const car_state_view_t *view)
{
    s1_page_t *ui = (s1_page_t *)self;
    const car_state_t *st = &view->state;
    /* 单位帧未到达时按公制处理，不能凭未初始化内容切换单位 */
    bool mile = st->units.valid && st->units.mile;
    uint32_t card_mask = ui_app_center_card_mask();

    if(card_mask != ui->card_mask) {
        card_set_mask(ui, card_mask);
    }

    if(!view->has_data) {
        return;
    }

    if(st->speed_rpm.valid) {
        /* 车速表：角度换算、刻度文本与单位都在组件内部，这里只送数值与单位 */
        speed_forester1_apply(&ui->speed, st->speed_rpm.speed_kph, mile);

        /* 转速表同样：角度换算与刻度高亮在组件内部 */
        rotate_forester1_apply(&ui->rotate, st->speed_rpm.engine_rpm);
    }

    /* Qt：y = 108 + (210 - 210 × p / 100)，p=100 时 y=108，p=0 时 y=318 */
    if(st->fuel.valid) {
        fuel_forester1_apply(&ui->fuel, st->fuel.percent);
    }

    /* 水温达到 HIGH 及以上时切高温图标，NORMAL 用普通温度计 */
    if(st->coolant.valid) {
        water_forester1_apply(&ui->water, st->coolant.percent,
                              st->coolant.state >= CAR_COOLANT_HIGH);
    }

    /* Qt：pageGear.visible = SettingsMgr.gearOnOff */
    if(st->gear_on && st->gear.valid) {
        lv_obj_remove_flag(ui->gear.root, LV_OBJ_FLAG_HIDDEN);
        ui_gear_apply(&ui->gear, st->gear.text);
    }
    else {
        lv_obj_add_flag(ui->gear.root, LV_OBJ_FLAG_HIDDEN);
    }

    if(st->mileage.valid) {
        ui_mileage_apply(&ui->odo, &st->mileage, mile);
        ui_mileage_apply(&ui->trip, &st->mileage, mile);
    }

    if(st->fuel.valid) {
        ui_range_apply(&ui->range, st->fuel.remaining_range_km, mile);
    }

    if(st->clock.valid) {
        ui_clock_apply(&ui->clock, &st->clock, st->clock.hour24);
    }

    /* Qt 用 SettingsMgr.outTemperature 控制整行显隐，缺省关闭 */
    if(st->outside_temp_on) {
        lv_obj_remove_flag(ui->outside_temp.root, LV_OBJ_FLAG_HIDDEN);
        ui_outside_temp_apply(&ui->outside_temp, &st->other_info,
                              st->units.fahrenheit);
    }
    else {
        lv_obj_add_flag(ui->outside_temp.root, LV_OBJ_FLAG_HIDDEN);
    }

    if(st->turn.valid) {
        ui_turn_signal_apply(&ui->turn_signal, &st->turn);
    }
    /* 中心车辆视图：帧轮播 + 示宽灯 + 左右转向灯 */
    car_view_apply(ui, st);
    /* 中心卡片：行程数据（0x0C） */
    itinerary_card_apply(&ui->itin, st);
        /* 4x4-i：carmw 暂无四驱扭矩帧，先用 Qt 的默认值 168 → 后轮 84%、前轮 16% */
    page4i_card_set_torque(&ui->card4i, 16U, 84U);
        /* 胎压：0x0A 数值 + 0x39 单位 */
    tire_card_apply(&ui->tire, st);
        /* 底盘控制：0x26 转向角 + 四驱指示 */
    chassis_card_apply(&ui->chassis, st);
        /* 短期记忆：0x0E 帧 */
    short_term_card_apply(&ui->short_term, st);
        /* 瞬时油耗：0x11 帧 */
    instant_fuel_card_apply(&ui->instant_fuel, st);
    /* 燃油经济性：0x0F 帧 + 0x0E 平均油耗 */ 
    eco_card_apply(&ui->eco, st);
    /* 巡航数值依赖 0x13，独立于警示灯帧，单独刷新 */
    ui_cruise_apply(&ui->cruise, st);

    /* Set 徽标：Qt visible = GlobalData.items.speedok，即 cruise_state bit1 */
    {
        bool set_shown = (st->cruise.cruise_state & 0x02U) != 0U;

        if(set_shown != ui->set_badge_shown) {
            ui->set_badge_shown = set_shown;

            if(set_shown) {
                lv_obj_remove_flag(ui->set_badge, LV_OBJ_FLAG_HIDDEN);
            }
            else {
                lv_obj_add_flag(ui->set_badge, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    /* ECO 指示：Qt show = GlobalData.items.ecoEnable && SettingsMgr.ecoIndicator */
    if(st->eco.valid && st->eco.enabled && st->eco_indicator) {
        lv_obj_remove_flag(ui->eco_lamp, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(ui->eco_lamp, LV_OBJ_FLAG_HIDDEN);
    }

        if(st->warnings.valid) {
        ui_lamp_row_apply(&ui->top_lamps, st);
        ui_lamp_group_apply(&s_left_lamps, st);
        ui_lamp_group_apply(&s_right_lamps, st);
        ui_lamp_group_apply(&s_bottom_lamps, st);
        ui_small_door_show(&ui->mini_door,
                           car_state_warning_level(st, 22U) != 0U);
    }

    /* 覆盖层优先级：车门 > 胎压 > 定速巡航 > 雷达；任一激活时隐藏中心卡片（Qt contentShow）
     * Qt：settingsPageOpen 时同时隐藏车门、胎压与雷达，并隐藏中心内容。 */
    {
        bool settings_open = ui_app_settings_is_open();

        ov_door_apply(&ui->ov_door, st, settings_open);
        ov_tire_apply(&ui->ov_tire, st,
                      ov_door_is_active(&ui->ov_door) || settings_open,
                      ui_session_tire_event());
        ov_cruise_apply(&ui->ov_cruise, st,
                        ov_door_is_active(&ui->ov_door) ||
                        ov_tire_is_active(&ui->ov_tire));
        ov_radar_apply(&ui->ov_radar, st,
                       ov_door_is_active(&ui->ov_door) ||
                       ov_tire_is_active(&ui->ov_tire) ||
                       ov_cruise_is_active(&ui->ov_cruise) ||
                       settings_open);

        /* 提示层不参与互斥，可叠加在任何覆盖层之上 */
        /* Qt：通用提示面板 visible = !settingsPageOpen && !ismedia */
        ov_tips_set_obstructed(&ui->ov_tips,
                               settings_open || s1_media_visible(ui));
        ov_tips_apply(&ui->ov_tips, st, ov_door_is_active(&ui->ov_door));

        if(settings_open ||
           ov_door_is_active(&ui->ov_door) || ov_tire_is_active(&ui->ov_tire) ||
           ov_cruise_is_active(&ui->ov_cruise) || ov_radar_is_active(&ui->ov_radar)) {
            lv_obj_add_flag(ui->central_stack, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_remove_flag(ui->central_stack, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/*
 * 方向盘按键，与 Qt PageSubaruForester1.qml 的 onKeyPressed 一致：
 *   上 / 左 -> 上一张；下 / 右 -> 下一张
 * UI_KEY_PREV / UI_KEY_NEXT 保留给 PC 演示键盘
 */
static void s1_on_key(ui_node_t *self, const ui_key_event_t *evt)
{
    s1_page_t *ui = (s1_page_t *)self;

    if(evt == NULL || evt->pressed == 0U) {
        return;
    }

    switch(evt->code) {
    case UI_KEY_NEXT:
    case UI_KEY_DOWN:
    case UI_KEY_RIGHT:
        card_step(ui, 1);
        break;
    case UI_KEY_PREV:
    case UI_KEY_UP:
    case UI_KEY_LEFT:
        card_step(ui, -1);
        break;
    default:
        break;
    }
}

/* PC 演示：媒体数据由 main.c 注入，板端不会走这里 */
static void s1_demo_media(ui_node_t *self, uint8_t mode, const char *title,
                          const char *line1, const char *line2)
{
    s1_page_t *ui = (s1_page_t *)self;

    music_card_demo_set(&ui->music, mode, title, line1, line2);
}

static void s1_demo_connect(ui_node_t *self, uint8_t state,
                            const char *mode_text, const char *bt_name)
{
    s1_page_t *ui = (s1_page_t *)self;

    connect_card_demo_set(&ui->connect, state, mode_text, bt_name);
}

const ui_node_vtbl_t page_subaru_forester1_vtbl = {
    .size = sizeof(s1_page_t),
    .create = s1_create,
    .destroy = s1_destroy,
    .apply = s1_apply,
    .on_show = NULL,
    .on_hide = NULL,
    .on_key = s1_on_key,
    .on_demo_media = s1_demo_media,
    .on_demo_connect = s1_demo_connect,
};
