/*
 * page_eco.c - 中心卡片：燃油经济性（对应 CentralDisplay/EcoShow.qml）
 *
 * Qt 坐标：
 *   卡片 586×509 居中于 814×509 → (114,0)；标题区 150 高
 *   contentBody 546×300 于 (20,0)
 *   圆环模式：300×300 圆环居中于 contentBody → (123,0)
 *     - 底环 12px / 进度环 14px，半径 120，角度 150° → 390°（底部留 120° 缺口）
 *     - 末端光点 20×20，圆心落在半径 120 的弧线上
 *     - 中心列：图标 244 + "ECO SCORE" 14 号 + 数值 24 号，间距 15
 *     - 底部平均油耗 18 号，离底 30
 *   水平模式：
 *     - 平均油耗行 (260,-10)，22/42/18 号（LVGL 用 44 号）
 *     - 刻度区 420×120 居中 → (63,90)，条 420×26，量程 0~30 L/100km
 *
 * 中文标题无字形，标题用 "FUEL ECONOMY"。
 * 角度计算用 lv_trigo_sin，避免引入 libm。
 */
#include "ui/PageModel/page_eco.h"

#include <stdio.h>
#include <string.h>

#include "ui/PageModel/page_itinerary.h"   /* ui_card_template_create */
#include "ui/ui_assets.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"

#define CARD_X   114
#define CARD_Y   0
#define CARD_W   586
#define CARD_H   509
#define TITLE_H  150

#define BODY_X   20
#define BODY_Y   0
#define BODY_W   546
#define BODY_H   300

/* 圆环几何 */
#define CIRCLE_X   123
#define CIRCLE_Y   0
#define CIRCLE_SIZE 300
#define ARC_CX     150
#define ARC_CY     150
#define ARC_R      120
#define ARC_START  150
#define ARC_SWEEP  240
#define KNOB_SIZE  20
#define KNOB_R     (KNOB_SIZE / 2)

/* 水平刻度几何 */
#define GAUGE_X    63
#define GAUGE_Y    90
#define GAUGE_W    420
#define GAUGE_H    120
#define BAR_H      26
#define SCALE_MAX  30U

#define COLOR_ECO  0x00FFC3U

#define TRACK_OPA        34   /* #22FFFFFF */
#define TRACK_BORDER_OPA 68   /* #44FFFFFF */
#define AVG_TEXT_OPA     136  /* #88FFFFFF */
#define SMALL_TICK_OPA   170  /* #AAFFFFFF */

static lv_image_dsc_t s_point_dsc;   /* 指针整图驻留 RAM */

static void set_text(lv_obj_t *label, const char *text)
{
    if(strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

/* 一位小数格式化：("%u.%u", x10/10, x10%10) */
static void fmt_x10(char *buf, size_t size, uint32_t x10)
{
    (void)snprintf(buf, size, "%u.%u",
                   (unsigned)(x10 / 10U), (unsigned)(x10 % 10U));
}

/* 纯色矩形：ui_blank_container 去掉了所有样式，这里按需补回 */
static lv_obj_t *rect_create(lv_obj_t *parent, int32_t x, int32_t y,
                             int32_t w, int32_t h, uint32_t color,
                             lv_opa_t opa)
{
    lv_obj_t *obj = ui_blank_container(parent);

    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, opa, 0);
    return obj;
}

/* 末端光点位置：圆心在半径 120 的弧线上，角度 150° + 240°×pct/100 */
static void knob_place(eco_card_t *card, uint32_t pct)
{
    int32_t angle = ARC_START + (int32_t)((ARC_SWEEP * pct) / 100U);
    int32_t x = ARC_CX + (ARC_R * lv_trigo_sin((int16_t)((angle + 90) % 360))) / 32768
                - KNOB_R;
    int32_t y = ARC_CY + (ARC_R * lv_trigo_sin((int16_t)(angle % 360))) / 32768
                - KNOB_R;

    lv_obj_set_pos(card->knob, x, y);
}

/* 圆环模式：300×300 圆弧 + 自绘光点 + 中心列 + 底部平均油耗 */
static void circular_create(eco_card_t *card, lv_obj_t *body)
{
    lv_obj_t *circle;
    lv_obj_t *center_col;
    lv_obj_t *row;
    lv_obj_t *icon;
    lv_obj_t *score_title;

    card->circular = ui_blank_container(body);
    lv_obj_set_size(card->circular, BODY_W, BODY_H);
    lv_obj_set_pos(card->circular, 0, 0);

    circle = ui_blank_container(card->circular);
    lv_obj_set_size(circle, CIRCLE_SIZE, CIRCLE_SIZE);
    lv_obj_set_pos(circle, CIRCLE_X, CIRCLE_Y);

    /* 圆弧：pad 30 让半径回到 120（(300-60)/2） */
    card->arc = lv_arc_create(circle);
    lv_obj_set_size(card->arc, CIRCLE_SIZE, CIRCLE_SIZE);
    lv_obj_set_pos(card->arc, 0, 0);
    lv_arc_set_bg_angles(card->arc, ARC_START, (ARC_START + ARC_SWEEP) - 360);
    lv_arc_set_angles(card->arc, ARC_START, ARC_START);

    lv_obj_set_style_pad_all(card->arc, 30, LV_PART_MAIN);
    lv_obj_set_style_arc_width(card->arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(card->arc, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(card->arc, TRACK_OPA, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(card->arc, true, LV_PART_MAIN);

    lv_obj_set_style_pad_all(card->arc, 0, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(card->arc, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(card->arc, lv_color_hex(COLOR_ECO), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(card->arc, true, LV_PART_INDICATOR);

    /* 内置 knob 关掉，用自绘圆点保证 20×20、白底 + 4px 边框 */
    lv_obj_set_style_bg_opa(card->arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_width(card->arc, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(card->arc, 0, LV_PART_KNOB);

    card->knob = ui_blank_container(circle);
    lv_obj_set_size(card->knob, KNOB_SIZE, KNOB_SIZE);
    lv_obj_set_style_radius(card->knob, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(card->knob, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card->knob, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card->knob, lv_color_hex(COLOR_ECO), 0);
    lv_obj_set_style_border_width(card->knob, 4, 0);
    knob_place(card, 0U);

    /* 中心列：图标 + (ECO SCORE + 数值)，间距 15 */
    center_col = ui_blank_container(circle);
    lv_obj_set_size(center_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(center_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(center_col, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(center_col, 15, 0);
    lv_obj_align(center_col, LV_ALIGN_CENTER, 0, 0);

    icon = lv_image_create(center_col);
    lv_image_set_src(icon, UI_IMG("WarnIcon/244.png"));

    row = ui_blank_container(center_col);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);

    score_title = lv_label_create(row);
    lv_label_set_text(score_title, "ECO SCORE");
    lv_obj_set_style_text_font(score_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(score_title, lv_color_hex(COLOR_ECO), 0);

    card->score = lv_label_create(row);
    lv_label_set_text(card->score, "0");
    lv_obj_set_style_text_font(card->score, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(card->score, lv_color_white(), 0);

    /* 底部平均油耗：离底 30 */
    card->avg = lv_label_create(card->circular);
    lv_label_set_text(card->avg, "AVG 0.0 L/100KM");
    lv_obj_set_style_text_font(card->avg, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(card->avg, lv_color_white(), 0);
    lv_obj_set_style_text_opa(card->avg, AVG_TEXT_OPA, 0);
    lv_obj_align(card->avg, LV_ALIGN_BOTTOM_MID, 0, -30);
}

/* 水平模式：平均油耗行 + 刻度条 + 指针 + 刻度数字 */
static void horizontal_create(eco_card_t *card, lv_obj_t *body)
{
    lv_obj_t *row;
    lv_obj_t *unit;
    lv_obj_t *gauge;
    lv_obj_t *track;
    lv_obj_t *scale;
    lv_obj_t *tick;
    lv_obj_t *label;
    static const uint8_t s_big[4] = {0U, 10U, 20U, 30U};
    static const uint8_t s_small[3] = {5U, 15U, 25U};
    uint8_t i;

    card->horizontal = ui_blank_container(body);
    lv_obj_set_size(card->horizontal, BODY_W, BODY_H);
    lv_obj_set_pos(card->horizontal, 0, 0);

    /* 平均油耗：AVG + 数值 + 单位，整体底对齐 */
    row = ui_blank_container(card->horizontal);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(row, 260, -10);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(row, 10, 0);

    label = lv_label_create(row);
    lv_label_set_text(label, "AVG");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_pad_bottom(label, 4, 0);

    card->h_avg = lv_label_create(row);
    lv_label_set_text(card->h_avg, "0.0");
    lv_obj_set_style_text_font(card->h_avg, &lv_font_montserrat_44, 0);
    lv_obj_set_style_text_color(card->h_avg, lv_color_white(), 0);

    unit = lv_label_create(row);
    lv_label_set_text(unit, "L/100km");
    lv_obj_set_style_text_font(unit, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(unit, lv_color_white(), 0);
    lv_obj_set_style_pad_bottom(unit, 6, 0);

    /* 刻度区 420×120 */
    gauge = ui_blank_container(card->horizontal);
    lv_obj_set_size(gauge, GAUGE_W, GAUGE_H);
    lv_obj_set_pos(gauge, GAUGE_X, GAUGE_Y);

    track = rect_create(gauge, 0, 0, GAUGE_W, BAR_H, 0xFFFFFFU, TRACK_OPA);
    lv_obj_set_style_radius(track, 4, 0);
    lv_obj_set_style_border_color(track, lv_color_white(), 0);
    lv_obj_set_style_border_opa(track, TRACK_BORDER_OPA, 0);
    lv_obj_set_style_border_width(track, 1, 0);

    /* 填充条：高度 22、内缩 2，竖直渐变 */
    card->bar = ui_blank_container(track);
    lv_obj_set_pos(card->bar, 2, 2);
    lv_obj_set_size(card->bar, 0, BAR_H - 4);
    lv_obj_set_style_radius(card->bar, 2, 0);
    lv_obj_set_style_bg_color(card->bar, lv_color_hex(0x74B2E0U), 0);
    lv_obj_set_style_bg_grad_color(card->bar, lv_color_hex(0x337AB7U), 0);
    lv_obj_set_style_bg_grad_dir(card->bar, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(card->bar, LV_OPA_COVER, 0);

    /* 指针：原图 135×154，整图入 RAM 后按 28×28 等比缩放 */
    (void)ui_assets_load_image(&s_point_dsc, "SubaruForester/point.png");
    card->pointer = lv_image_create(gauge);
    lv_image_set_src(card->pointer, &s_point_dsc);
    lv_obj_set_size(card->pointer, 28, 28);
    lv_image_set_inner_align(card->pointer, LV_IMAGE_ALIGN_CONTAIN);
    lv_obj_set_pos(card->pointer, -14 + 1, -22);

    /* 刻度：主横线 + 大刻度（0/10/20/30）+ 小刻度（5/15/25） */
    scale = ui_blank_container(gauge);
    lv_obj_set_size(scale, GAUGE_W, 40);
    lv_obj_set_pos(scale, 0, BAR_H + 10);

     (void)rect_create(scale, 0, 0, GAUGE_W, 2, 0xFFFFFFU, LV_OPA_COVER);

    for(i = 0U; i < 4U; ++i) {
        int32_t x = (int32_t)((uint32_t)s_big[i] * GAUGE_W / SCALE_MAX);

        tick = rect_create(scale, x, 0, 2, 10, 0xFFFFFFU, LV_OPA_COVER);

        label = lv_label_create(scale);
        lv_label_set_text_fmt(label, "%u", (unsigned)s_big[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_align_to(label, tick, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    }

    for(i = 0U; i < 3U; ++i) {
        int32_t x = (int32_t)((uint32_t)s_small[i] * GAUGE_W / SCALE_MAX);

        (void)rect_create(scale, x, 0, 1, 6, 0xFFFFFFU, SMALL_TICK_OPA);
    }
}

bool eco_card_create(eco_card_t *card, lv_obj_t *parent)
{
    lv_obj_t *content;
    lv_obj_t *body;

    if(card == NULL) {
        return false;
    }

    card->last_pct = 0xFFFFFFFFU;
    card->last_avg_x10 = 0xFFFFFFFFU;
    card->circular_on = 0xFFU;

    card->root = ui_blank_container(parent);
    lv_obj_add_flag(card->root, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(card->root, CARD_W, CARD_H);
    lv_obj_set_pos(card->root, CARD_X, CARD_Y);

    content = ui_card_template_create(card->root, 0, 0, CARD_W, CARD_H,
                                      TITLE_H, "FUEL ECONOMY",
                                      &lv_font_montserrat_36);

    body = ui_blank_container(content);
    lv_obj_set_size(body, BODY_W, BODY_H);
    lv_obj_set_pos(body, BODY_X, BODY_Y);

    circular_create(card, body);
    horizontal_create(card, body);

    return true;
}

void eco_card_apply(eco_card_t *card, const car_state_t *state)
{
    uint32_t pct = 0U;
    uint32_t avg_x10 = 0U;
    uint8_t circular = 0U;
    char buf[32];
    char avg[16];

    if(card == NULL || state == NULL) {
        return;
    }

    if(state->trip.valid) {
        avg_x10 = state->trip.avg_fuel_x10;
    }

    /*
     * 0x0F：value 0~255 → 百分比。
     * Qt EcoShow：showCircularMode = ecoIndicator && ecoOrfuel && ecoEnable。
     */
    if(state->eco.valid) {
        pct = ((uint32_t)state->eco.value * 100U + 127U) / 255U;
        circular = (state->eco.enabled && state->eco_indicator && state->eco_or_fuel)
                       ? 1U : 0U;
    }

    /* 模式切换：靠 HIDDEN 标志，与 Qt 两个 Item 的 visible 等价 */
    if(circular != card->circular_on) {
        card->circular_on = circular;

        if(circular != 0U) {
            lv_obj_remove_flag(card->circular, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(card->horizontal, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(card->circular, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(card->horizontal, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* ECO 百分比：圆弧进度、光点位置、数值 */
    if(pct != card->last_pct) {
        int32_t end;

        card->last_pct = pct;

        end = ARC_START + (int32_t)((ARC_SWEEP * pct) / 100U);
        if(end >= 360) {
            end -= 360;
        }
        lv_arc_set_angles(card->arc, ARC_START, end);
        knob_place(card, pct);

        (void)snprintf(buf, sizeof(buf), "%u", (unsigned)pct);
        set_text(card->score, buf);
    }

    /* 平均油耗：圆环底部文本、水平数值、进度条与指针 */
    if(avg_x10 != card->last_avg_x10) {
        uint32_t capped;
        int32_t width;

        card->last_avg_x10 = avg_x10;

        fmt_x10(avg, sizeof(avg), avg_x10);

        (void)snprintf(buf, sizeof(buf), "AVG %s L/100KM", avg);
        set_text(card->avg, buf);

        fmt_x10(buf, sizeof(buf), avg_x10);
        set_text(card->h_avg, buf);

        capped = (avg_x10 > SCALE_MAX * 10U) ? SCALE_MAX * 10U : avg_x10;
        width = (int32_t)(capped * GAUGE_W / (SCALE_MAX * 10U));

        lv_obj_set_width(card->bar, width);
        lv_obj_set_pos(card->pointer, width - 14 + 1, -22);
    }
}

void eco_card_destroy(eco_card_t *card)
{
    if(card != NULL && card->root != NULL) {
        lv_obj_delete(card->root);
        card->root = NULL;
    }
}