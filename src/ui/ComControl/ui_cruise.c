/*
 * ui_cruise.c - 定速巡航指示实现
 *
 * 图标与数值相互独立：图标由故障码驱动（消失后保持上一次的图标），
 * 数值由 car_cruise_t.set_speed_kph 驱动，mile 单位按 Qt mileConvert 四舍五入。
 */
#include "ui/ComControl/ui_cruise.h"

#include "ui/ui_assets.h"
#include "ui/ui_theme.h"

/* Qt Row 中 IconConstantSpeed 的内容宽度（52 + 2 + 数值 + 2 + 单位） */
#define CRUISE_W 128
#define CRUISE_H UI_CRUISE_ICON_SIZE
#define CRUISE_TEXT_X (UI_CRUISE_ICON_SIZE + 2)
#define CRUISE_BLINK_FAST_MS 600U
#define CRUISE_BLINK_SLOW_MS 6000U

typedef struct {
    uint8_t code;
    uint8_t icon;
} cruise_code_icon_t;

/* 按 iconBindMap 的数字顺序处理，后者覆盖前者（与 Qt 遍历顺序一致） */
static const cruise_code_icon_t s_code_icons[] = {
    {31U,  UI_CRUISE_ICON_GREEN},
    {95U,  UI_CRUISE_ICON_GREEN},
    {99U,  UI_CRUISE_ICON_ACC_GREEN},
    {239U, UI_CRUISE_ICON_ACC_WHITE},
    {240U, UI_CRUISE_ICON_WHITE},
    {241U, UI_CRUISE_ICON_ACC_READY},
};

static const char *const s_icon_paths[UI_CRUISE_ICON_COUNT] = {
    "WarnIcon/fixed_speed_green.png",
    "WarnIcon/acc_green.png",
    "WarnIcon/acc_white.png",
    "WarnIcon/fixed_speed_white.png",
    "WarnIcon/acc_ready.png",
};

/* 切到指定图标；已加载过直接复用，加载失败保持上一张 */
static void cruise_switch_icon(ui_cruise_t *cruise, uint8_t icon)
{
    if(icon >= (uint8_t)UI_CRUISE_ICON_COUNT || cruise->icon == icon) {
        return;
    }

    if(cruise->dsc[icon].data == NULL &&
       !ui_assets_load_image(&cruise->dsc[icon], s_icon_paths[icon])) {
        return;
    }

    cruise->icon = icon;
    ui_lamp_set_icon(&cruise->lamp, &cruise->dsc[icon]);
}

/* 图标显示状态：无故障码但有设定速度时用常亮 */
static void cruise_apply_state(ui_cruise_t *cruise, uint8_t level, bool visible)
{
    if(!visible) {
        ui_lamp_set_state(&cruise->lamp, UI_LAMP_OFF);
        return;
    }

    switch(level) {
    case (uint8_t)CAR_WARN_BLINK_FAST:
        ui_lamp_set_blink_ms(&cruise->lamp, CRUISE_BLINK_FAST_MS);
        ui_lamp_set_state(&cruise->lamp, UI_LAMP_BLINK);
        break;

    case (uint8_t)CAR_WARN_BLINK_SLOW:
        ui_lamp_set_blink_ms(&cruise->lamp, CRUISE_BLINK_SLOW_MS);
        ui_lamp_set_state(&cruise->lamp, UI_LAMP_BLINK);
        break;

    default:
        ui_lamp_set_state(&cruise->lamp, UI_LAMP_ON);
        break;
    }
}

bool ui_cruise_create(ui_cruise_t *cruise, lv_obj_t *parent, int32_t x, int32_t y,
                      const lv_font_t *value_font, const lv_font_t *unit_font)
{
    if(cruise == NULL || parent == NULL) {
        return false;
    }

    if(!ui_assets_load_image(&cruise->dsc[UI_CRUISE_ICON_GREEN],
                             s_icon_paths[UI_CRUISE_ICON_GREEN])) {
        return false;
    }

    cruise->root = ui_blank_container(parent);
    lv_obj_set_size(cruise->root, CRUISE_W, CRUISE_H);
    lv_obj_set_pos(cruise->root, x, y);
    /* 数值可能比预留宽度略宽，别裁掉 */
    lv_obj_add_flag(cruise->root, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    cruise->icon = (uint8_t)UI_CRUISE_ICON_GREEN;
    ui_lamp_create_sized(&cruise->lamp, cruise->root,
                         &cruise->dsc[UI_CRUISE_ICON_GREEN],
                         0, 0, UI_CRUISE_ICON_SIZE, UI_CRUISE_ICON_SIZE);

    /* 数值贴着行底，Qt bottomMargin 4 */
    cruise->value = lv_label_create(cruise->root);
    lv_label_set_text(cruise->value, "");
    lv_obj_set_style_text_font(cruise->value, value_font, 0);
    lv_obj_set_style_text_color(cruise->value, lv_color_white(), 0);
    lv_obj_align(cruise->value, LV_ALIGN_BOTTOM_LEFT, CRUISE_TEXT_X, -4);
    lv_obj_add_flag(cruise->value, LV_OBJ_FLAG_HIDDEN);

    /* 单位贴在数值右侧，Qt bottomMargin 6，故上移 2 */
    cruise->unit = lv_label_create(cruise->root);
    lv_label_set_text(cruise->unit, "km/h");
    lv_obj_set_style_text_font(cruise->unit, unit_font, 0);
    lv_obj_set_style_text_color(cruise->unit, lv_color_white(), 0);
    lv_obj_align_to(cruise->unit, cruise->value,
                    LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -2);
    lv_obj_add_flag(cruise->unit, LV_OBJ_FLAG_HIDDEN);

    return true;
}

void ui_cruise_apply(ui_cruise_t *cruise, const car_state_t *state)
{
    uint8_t level = 0;
    uint8_t icon = (uint8_t)UI_CRUISE_ICON_GREEN;
    uint16_t speed;
    uint16_t shown;
    bool mile;
    bool text_visible;

    if(cruise == NULL || cruise->root == NULL || state == NULL) {
        return;
    }

    for(size_t i = 0; i < sizeof(s_code_icons) / sizeof(s_code_icons[0]); ++i) {
        uint8_t lv = car_state_warning_level(state, s_code_icons[i].code);

        if(lv != 0U) {
            level = lv;
            icon = s_code_icons[i].icon;
        }
    }

    /* Qt 只在故障码到来时切图，码消失后保留上一次图标 */
    if(level != 0U) {
        cruise_switch_icon(cruise, icon);
    }

    speed = state->cruise.valid ? state->cruise.set_speed_kph : 0U;
    /* 单位帧未到达时按公制处理，不能凭未初始化内容切换单位 */
    mile = state->units.valid && state->units.mile;
    /* Qt mileConvert：Math.round(kph × 0.621371) */
    shown = mile ? (uint16_t)(((uint32_t)speed * 621U + 500U) / 1000U) : speed;
    text_visible = speed != 0U;

    cruise_apply_state(cruise, level, level != 0U || text_visible);

    if(cruise->shown_valid && cruise->shown_speed == shown &&
       cruise->shown_mile == mile && cruise->shown_text == text_visible) {
        return;
    }

    cruise->shown_speed = shown;
    cruise->shown_mile = mile;
    cruise->shown_text = text_visible;
    cruise->shown_valid = true;

    if(text_visible) {
        lv_label_set_text_fmt(cruise->value, "%u", (unsigned)shown);
        lv_label_set_text(cruise->unit, mile ? "mile/h" : "km/h");
        lv_obj_align_to(cruise->unit, cruise->value,
                        LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -2);
        lv_obj_remove_flag(cruise->value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(cruise->unit, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(cruise->value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(cruise->unit, LV_OBJ_FLAG_HIDDEN);
    }
}