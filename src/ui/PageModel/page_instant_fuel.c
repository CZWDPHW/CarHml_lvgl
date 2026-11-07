/*
 * page_instant_fuel.c - 中心卡片：瞬时油耗（对应 PageInstantFuel.qml）
 *
 * Qt 坐标（CentralTemplate 分支）：
 *   卡片 586×503 居中于 814×509 → (114,3)；标题区 150 高
 *   contentBody 546×264 水平居中 → (20,0)
 *   图标 84×123 水平居中，topMargin = (264-123)/2-20 = 50
 *   文本 34 号水平居中，topMargin = (264-h)/2+40 ≈ 150
 *
 * 中文标题在 Montserrat 里无字形，改用英文 "INSTANT FUEL"。
 * 数值来自 0x11（car_instant_fuel_t），单位随 0x39 的 units.fuel_unit。
 */
#include "ui/PageModel/page_instant_fuel.h"

#include <stdio.h>
#include <string.h>

#include "ui/PageModel/page_itinerary.h"   /* ui_card_template_create */
#include "ui/ui_common.h"
#include "ui/ui_theme.h"

#define CARD_X   114
#define CARD_Y   3
#define CARD_W   586
#define CARD_H   503
#define TITLE_H  150

#define BODY_X   20
#define BODY_Y   0
#define BODY_W   546
#define BODY_H   264

#define ICON_X   231
#define ICON_Y   50
#define TEXT_Y   150

/* 与上次文本不同才写，避免每帧触发重排 */
static void set_text(lv_obj_t *label, const char *text)
{
    if(strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

bool instant_fuel_card_create(instant_fuel_card_t *card, lv_obj_t *parent)
{
    lv_obj_t *content;
    lv_obj_t *body;

    if(card == NULL) {
        return false;
    }

    card->root = ui_blank_container(parent);
    lv_obj_add_flag(card->root, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(card->root, CARD_W, CARD_H);
    lv_obj_set_pos(card->root, CARD_X, CARD_Y);

    /* 标题区 150 高，标题居中（中文受限，用英文） */
    content = ui_card_template_create(card->root, 0, 0, CARD_W, CARD_H,
                                      TITLE_H, "INSTANT FUEL",
                                      &lv_font_montserrat_36);

    body = ui_blank_container(content);
    lv_obj_set_size(body, BODY_W, BODY_H);
    lv_obj_set_pos(body, BODY_X, BODY_Y);

    /* 图标先建、文本后建：与 Qt 声明顺序一致，文本压在图上 */
    card->icon = lv_image_create(body);
    lv_image_set_src(card->icon, UI_IMG("material2/MenuItem/instant_fuel.bin"));
    lv_obj_set_pos(card->icon, ICON_X, ICON_Y);

    card->value = lv_label_create(body);
    lv_label_set_text(card->value, "0.0 L / 100km");
    lv_obj_set_style_text_font(card->value, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(card->value, lv_color_white(), 0);
    lv_obj_align(card->value, LV_ALIGN_TOP_MID, 0, TEXT_Y);

    return true;
}

/* 一位小数的整数格式化：("%u.%u", x10/10, x10%10) */
static void fmt_x10(char *buf, size_t size, uint32_t x10)
{
    (void)snprintf(buf, size, "%u.%u",
                   (unsigned)(x10 / 10U), (unsigned)(x10 % 10U));
}

void instant_fuel_card_apply(instant_fuel_card_t *card, const car_state_t *state)
{
    const car_instant_fuel_t *fuel;
    const car_units_t *units;
    char buf[24];
    uint16_t x10;
    const char *unit_text;

    if(card == NULL || state == NULL || !state->instant_fuel.valid) {
        return;
    }

    fuel = &state->instant_fuel;
    units = &state->units;

    /* 0xFFFE：Qt 侧按“隐藏”处理，这里保留上一次显示值 */
    if(!fuel->visible) {
        return;
    }

    x10 = fuel->value_x10;

    /* 0x7FFF：协议约定的 “---.-” */
    if(x10 == 0x7FFFU) {
        (void)snprintf(buf, sizeof(buf), "---.- %s",
                       fuel->per_hour ? "L/h"
                                      : (units->fuel_unit == 1U ? "km / L"
                                                                : (units->fuel_unit == 2U
                                                                       ? "mpg"
                                                                       : "L / 100km")));
        set_text(card->value, buf);
        return;
    }

    /* 按小时油耗：0x8000 置位时 value/10 直接是 L/h */
    if(fuel->per_hour) {
        if(x10 == 0U) {
            (void)snprintf(buf, sizeof(buf), "0.0 L/h");
        }
        else {
            fmt_x10(buf, sizeof(buf), x10);
            strcat(buf, " L/h");
        }
        set_text(card->value, buf);
        return;
    }

    /* 百公里油耗：0 → "0.0"；其余按 units.fuel_unit 换算，与短期记忆卡片一致 */
    if(x10 == 0U) {
        unit_text = (units->fuel_unit == 1U) ? "km / L"
                                               : (units->fuel_unit == 2U ? "mpg"
                                                                         : "L / 100km");
        (void)snprintf(buf, sizeof(buf), "0.0 %s", unit_text);
    }
    else {
        switch(units->fuel_unit) {
        case 1U:   /* km/L = 1000 / (x10/10) */
            fmt_x10(buf, sizeof(buf), (10000U + x10 / 2U) / x10);
            strcat(buf, " km / L");
            break;
        case 2U:   /* mpg = 2824.81 / (x10/10) */
            fmt_x10(buf, sizeof(buf), (28248U + x10 / 2U) / x10);
            strcat(buf, " mpg");
            break;
        default:   /* L/100km：x10 本身就是 x10 */
            fmt_x10(buf, sizeof(buf), x10);
            strcat(buf, " L / 100km");
            break;
        }
    }

    set_text(card->value, buf);
}

void instant_fuel_card_destroy(instant_fuel_card_t *card)
{
    if(card != NULL && card->root != NULL) {
        lv_obj_delete(card->root);
        card->root = NULL;
    }
}