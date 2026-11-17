/*
 * fuel_forester1.c - Style1 油量表实现
 *
 * 坐标取自 Qt FuelTank/ForesterFuelTank1.qml：
 *   容器 (0,0) 内：底图 0,0；刻度 x=100，y=108+(210-210×p/100)，130x210
 *   加油口图标 (66,316)，F (198,86) 白色，E (108,320) 红色
 *
 * 加油口图标是 Style1 之外的通用叠加件（见下方 fuel_port_create 注释），
 * 不改变 ForesterFuelTank1.qml 本身的布局。
 */
#include "ui/FuelTank/fuel_forester1.h"

#include "ui/ui_assets.h"
#include "ui/ui_theme.h"

#define FUEL_F1_W 250
#define FUEL_F1_H 368

/* 刻度条参数，与 Qt 中 sourceClipRect 一致 */
#define FUEL_F1_TICK_X 100

/* 加油口方向图标 64×64，与原 44×44 油枪图标同中心：66+(44-64)/2 = 56，316+(-10) = 306 */
#define FUEL_PORT_X 56
#define FUEL_PORT_Y 306

/*
 * 加油口方向图标（Qt ResourceLib.warnlight 的 left/right_fuel_*.png）。
 * 全套通用仪表（如 GeneralUi2.qml、GeneralSpeed4.qml）按加油口在左/右
 * （SettingsMgr.fuelFillerLeft 或 0x02 帧 isLeftTank）与低油量状态换图；
 * Forester 车型的 ForesterFuelTank1.qml 自身不渲染该图标，但森林人页
 * 复用通用油量图标，故按通用规则叠加，避免私有状态丢失。
 */
static void fuel_port_create(fuel_forester1_t *fuel, lv_obj_t *root)
{
    fuel->port_img = lv_image_create(root);
    lv_image_set_src(fuel->port_img, &fuel->port_dsc[0]);
    lv_obj_set_pos(fuel->port_img, FUEL_PORT_X, FUEL_PORT_Y);
}

/* 按加油口方向与低油量状态换图；Qt: fuelStateIcon==2 为黄图，其余白图 */
static void fuel_port_apply(fuel_forester1_t *fuel, bool left, bool low)
{
    int8_t variant;

    if(fuel->port_img == NULL) {
        return;
    }

    variant = left ? (low ? 1 : 0) : (low ? 3 : 2);
    if(fuel->port_variant == variant) {
        return;
    }

    fuel->port_variant = variant;
    lv_image_set_src(fuel->port_img, &fuel->port_dsc[variant]);
}

/* 载入四张加油口图标；失败时整块图标不创建，不影响原有油量表 */
static bool fuel_port_load(fuel_forester1_t *fuel)
{
    static const char *const src[4] = {
        "WarnIcon/left_fuel_white.png", "WarnIcon/left_fuel_hig.png",
        "WarnIcon/right_fuel_white.png", "WarnIcon/right_fuel_hig.png"
    };

    for(uint8_t i = 0; i < 4U; ++i) {
        if(!ui_assets_load_image(&fuel->port_dsc[i], src[i])) {
            return false;
        }
    }

    /* 缺省左 + 白 */
    fuel->port_variant = 0;

    return true;
}

/* 创建底色、油枪图标与 F/E 标记等静态元素 */
static void fuel_static_create(lv_obj_t *root)
{
    lv_obj_t *img = lv_image_create(root);
    lv_obj_t *label;

    lv_image_set_src(img, ui_assets_path("SubaruForester/fuel1_bg.png"));
    lv_obj_set_pos(img, 0, 0);

    img = lv_image_create(root);
    lv_image_set_src(img, ui_assets_path("SubaruForester/fuel_44.png"));
    lv_obj_set_pos(img, 66, 316);

    /* F：满油端，白色 */
    label = lv_label_create(root);
    lv_label_set_text(label, "F");
    lv_obj_set_style_text_font(label, UI_FONT_MD, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(UI_COLOR_WHITE), 0);
    lv_obj_set_pos(label, 198, 86);

    /* E：空油端，红色 */
    label = lv_label_create(root);
    lv_label_set_text(label, "E");
    lv_obj_set_style_text_font(label, UI_FONT_MD, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(UI_COLOR_RED), 0);
    lv_obj_set_pos(label, 108, 320);
}

bool fuel_forester1_create(fuel_forester1_t *fuel, lv_obj_t *parent,
                           int32_t x, int32_t y)
{
    if(fuel == NULL) {
        return false;
    }

    fuel->low = false;
    fuel->port_variant = -1;

    fuel->root = ui_blank_container(parent);
    lv_obj_set_size(fuel->root, FUEL_F1_W, FUEL_F1_H);
    lv_obj_set_pos(fuel->root, x, y);

    fuel_static_create(fuel->root);
    /* 加油口图标压在油枪图标之上，与通用仪表的层级一致；素材缺失则不创建 */
    if(fuel_port_load(fuel)) {
        fuel_port_create(fuel, fuel->root);
    }

    /* 初始用白色刻度图，0% 时液面在底部 */
    if(!ui_tick_create(&fuel->tick, fuel->root,
                       "SubaruForester/fuel1_white_tick.png",
                       FUEL_F1_TICK_X, 318, 108, 130, 210)) {
        return false;
    }

    return true;
}

void fuel_forester1_apply(fuel_forester1_t *fuel, uint8_t percent)
{
    bool low;

    if(fuel == NULL || fuel->root == NULL) {
        return;
    }

    /* Qt 为 fuelPercentage > 25 ? 白 : 红，即 25% 本身已经是红色 */
    low = percent <= FUEL_F1_LOW_PERCENT;

    /* 跨过 25% 阈值时换图，Qt 为 fuelPercentage > 25 ? 白 : 红 */
    if(low != fuel->low) {
        fuel->low = low;
        lv_image_set_src(fuel->tick.img,
                         ui_assets_path(low ? "SubaruForester/fuel1_tick.png"
                                            : "SubaruForester/fuel1_white_tick.png"));
    }

    ui_tick_set_percent(&fuel->tick, percent);
}

void fuel_forester1_apply_port(fuel_forester1_t *fuel, bool port_left,
                               uint8_t icon_state)
{
    if(fuel == NULL || fuel->port_img == NULL) {
        return;
    }

    /* Qt 通用油量图标：fuelStateIcon === 2 为低油量黄图，其余白图 */
    fuel_port_apply(fuel, port_left, icon_state == 2U);
}