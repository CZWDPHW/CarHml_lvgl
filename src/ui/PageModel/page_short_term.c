/*
 * page_short_term.c - 中心卡片：短期记忆（对应 PageShortTerm.qml）
 *
 * Qt 坐标（CentralTemplate 分支）：
 *   卡片 586×503 居中于 814×509 → (114,3)；标题区 150 高
 *   contentBody 546×264 水平居中 → (20,0)
 *   图标 68×122：(89,95)
 *   Column 四行：(182,40)，行高 38、间距 10，字号 27 粗体
 *
 * 中文标签在 Montserrat 里无字形，标签用英文缩写，数值格式与 Qt 一致。
 */
#include "ui/PageModel/page_short_term.h"

#include <stdio.h>
#include <string.h>

#include "ui/PageModel/page_itinerary.h"   /* ui_card_template_create */
#include "ui/ui_assets.h"
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

#define ICON_X   89
#define ICON_Y   95

#define COL_X    182
#define COL_Y    40
#define ROW_H    38
#define ROW_GAP  10

/* 与上次文本不同才写，避免每帧触发重排 */
static void set_text(lv_obj_t *label, const char *text)
{
    if(strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

/* 一行：名称 + 数值，返回数值 label（可选再挂单位） */
static lv_obj_t *row_create(lv_obj_t *column, const char *name)
{
    lv_obj_t *row = ui_blank_container(column);
    lv_obj_t *name_label;
    lv_obj_t *value;

    lv_obj_set_size(row, LV_SIZE_CONTENT, ROW_H);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(row, 0, 0);

    name_label = lv_label_create(row);
    lv_label_set_text(name_label, name);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(name_label, lv_color_white(), 0);
    lv_obj_set_style_pad_bottom(name_label, 4, 0);   /* 视觉居中 */

    value = lv_label_create(row);
    lv_label_set_text(value, "---");
    lv_obj_set_style_text_font(value, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(value, lv_color_white(), 0);
    lv_obj_set_style_pad_bottom(value, 2, 0);        /* bottomMargin 2 */

    return value;
}

bool short_term_card_create(short_term_card_t *card, lv_obj_t *parent)
{
    lv_obj_t *content;
    lv_obj_t *body;
    lv_obj_t *column;
    lv_obj_t *icon;

    if(card == NULL) {
        return false;
    }

    card->root = ui_blank_container(parent);
    lv_obj_add_flag(card->root, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(card->root, CARD_W, CARD_H);
    lv_obj_set_pos(card->root, CARD_X, CARD_Y);

    /* 标题区 150 高，标题居中（中文受限，用英文） */
    content = ui_card_template_create(card->root, 0, 0, CARD_W, CARD_H,
                                      TITLE_H, "SHORT TERM",
                                      &lv_font_montserrat_36);

    body = ui_blank_container(content);
    lv_obj_set_size(body, BODY_W, BODY_H);
    lv_obj_set_pos(body, BODY_X, BODY_Y);

    icon = lv_image_create(body);
    lv_image_set_src(icon, UI_IMG("material2/MenuItem/short_term.bin"));
    lv_obj_set_pos(icon, ICON_X, ICON_Y);

    column = ui_blank_container(body);
    lv_obj_set_size(column, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(column, COL_X, COL_Y);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(column, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(column, ROW_GAP, 0);

    card->value[0] = row_create(column, "TRIP DIST ");
    card->value[1] = row_create(column, "AVG SPEED ");
    card->value[2] = row_create(column, "DRIVE TIME ");
    card->value[3] = row_create(column, "AVG FUEL ");

    /* 油耗单位：18 号，紧跟在数值后、下边距 4 */
    card->fuel_unit = lv_label_create(lv_obj_get_parent(card->value[3]));
    lv_label_set_text(card->fuel_unit, "L/100km");
    lv_obj_set_style_text_font(card->fuel_unit, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(card->fuel_unit, lv_color_white(), 0);
    lv_obj_set_style_pad_left(card->fuel_unit, 6, 0);    /* leftMargin 6 */
    lv_obj_set_style_pad_bottom(card->fuel_unit, 4, 0);  /* bottomMargin 4 */

    return true;
}

/* 一位小数的整数格式化：("%u.%u", x10/10, x10%10) */
static void fmt_x10(char *buf, size_t size, uint32_t x10)
{
    (void)snprintf(buf, size, "%u.%u",
                   (unsigned)(x10 / 10U), (unsigned)(x10 % 10U));
}

void short_term_card_apply(short_term_card_t *card, const car_state_t *state)
{
    const car_trip_t *trip;
    const car_units_t *units;
    char buf[24];
    bool mile;
    uint8_t fuel_unit;
    uint32_t avg = 0U;

    if(card == NULL || state == NULL || !state->trip.valid) {
        return;
    }

    trip = &state->trip;
    units = &state->units;
    avg = trip->avg_fuel_x10;

    /* 单位未有效时按公制显示，不能凭未初始化内容切换 */
    mile = units->valid && units->mile;
    fuel_unit = units->valid ? units->fuel_unit : 0U;

    /* 小计里程：km 直接显示整数，mile 换英制保留一位 */
    if(mile) {
        uint32_t x10 = ((uint32_t)trip->distance_km * 10000U + 804U) / 1609U;

        fmt_x10(buf, sizeof(buf), x10);
        strcat(buf, " mi");
    }
    else {
        (void)snprintf(buf, sizeof(buf), "%u km", (unsigned)trip->distance_km);
    }
    set_text(card->value[0], buf);

    /* 平均时速 */
    if(mile) {
        uint32_t x10 = ((uint32_t)trip->avg_speed_kph * 10000U + 804U) / 1609U;

        fmt_x10(buf, sizeof(buf), x10);
        strcat(buf, " mph");
    }
    else {
        (void)snprintf(buf, sizeof(buf), "%u km/h",
                       (unsigned)trip->avg_speed_kph);
    }
    set_text(card->value[1], buf);

    /* 行驶时长 */
    (void)snprintf(buf, sizeof(buf), "%uh %um",
                   (unsigned)(trip->drive_minutes / 60U),
                   (unsigned)(trip->drive_minutes % 60U));
    set_text(card->value[2], buf);

    /* 单位标签只由单位状态决定，与数值是否有效无关（Qt 的 fuelType 分支） */
    switch(fuel_unit) {
    case 1U:
        set_text(card->fuel_unit, "km/L");
        break;
    case 2U:
        set_text(card->fuel_unit, "mpg");
        break;
    default:
        set_text(card->fuel_unit, "L/100km");
        break;
    }

    /* 平均油耗行：Qt 用 SettingsMgr.avgFuelOnOff 控制整行显隐 */
    if(state->avg_fuel_on) {
        lv_obj_remove_flag(card->value[3], LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(card->fuel_unit, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(card->value[3], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(card->fuel_unit, LV_OBJ_FLAG_HIDDEN);
    }

    /* 平均油耗：<=0 → 0，>1200（120 L/100km）→ --.-，特殊值不参与倒数换算 */
    if(avg == 0U) {
        (void)snprintf(buf, sizeof(buf), "0");
    }
    else if(avg > 1200U) {
        (void)snprintf(buf, sizeof(buf), "--.-");
    }
    else {
        switch(fuel_unit) {
        case 1U:   /* km/L = 1000 / (avg/10) */
            fmt_x10(buf, sizeof(buf), (10000U + avg / 2U) / avg);
            break;
        case 2U:   /* mpg = 2824.81 / (avg/10) */
            fmt_x10(buf, sizeof(buf), (28248U + avg / 2U) / avg);
            break;
        default:   /* L/100km：avg 本身就是 x10 */
            fmt_x10(buf, sizeof(buf), avg);
            break;
        }
    }
    set_text(card->value[3], buf);
}

void short_term_card_destroy(short_term_card_t *card)
{
    if(card != NULL && card->root != NULL) {
        lv_obj_delete(card->root);
        card->root = NULL;
    }
}