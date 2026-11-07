/*
 * page_itinerary.c - 中心卡片：行程数据（对应 PageDrivingInfo.qml）
 *
 * Qt 坐标链：
 *   StackLayout(553,140,814,509) → Loader 居中 586×509
 *   → 卡片在容器内 (114,0)，尺寸 586×509
 *   → CentralTemplate 标题区高 150，内容区 586×359
 *   → 内容区里 Item 546×300 居中 → 卡片内 (20,180)
 *   → Column 居中后 x 偏移 +50、y 偏移 -70，行间距 35
 *
 * 每行：图标 58×46 + 间距 25 + 文字宽 180、字号 27。
 */
#include "ui/PageModel/page_itinerary.h"

#include <stdio.h>

#include "ui/ui_assets.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"

#define CARD_X        114
#define CARD_Y        0
#define CARD_W        586
#define CARD_H        509
#define CARD_TITLE_H  150

/* Column 居中后偏移 (+50,-70)：三行间距 35 */
#define ROW_GAP       35
#define ROW_ICON_W    58
#define ROW_ICON_H    46
#define ROW_ICON_GAP  25

lv_obj_t *ui_card_template_create(lv_obj_t *parent, int32_t x, int32_t y,
                                  int32_t w, int32_t h, int32_t title_h,
                                  const char *title, const lv_font_t *title_font)
{
    lv_obj_t *card = ui_blank_container(parent);
    lv_obj_t *label;

    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);

    /* 标题：居中，与 Qt 一样落在标题区垂直中心 */
    label = lv_label_create(card);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, title_font, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_width(label, w - 100);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, (title_h - title_font->line_height) / 2);

    /* 内容区：标题区下方，剩余高度 */
    {
        lv_obj_t *content = ui_blank_container(card);

        lv_obj_set_size(content, w, h - title_h);
        lv_obj_set_pos(content, 0, title_h);
        return content;
    }
}

/* 创建三行：时间 / 里程 / 平均车速，整个竖排块在内容区居中 */
static void rows_create(itinerary_card_t *card, lv_obj_t *content)
{
    /*
     * Qt 的 Item 546×300 只是尺寸参考，不裁剪子项；这里也不套中间容器，
     * 否则 -70 的偏移会让首行被容器上边界裁掉。
     */
    lv_obj_t *column = ui_blank_container(content);

    (void)ui_info_row_create(&card->row_time,
                             column, UI_IMG("material2/MenuItem/clock.png"),
                             ROW_ICON_W, ROW_ICON_H, ROW_ICON_GAP,
                             &lv_font_montserrat_28);
    (void)ui_info_row_create(&card->row_distance,
                             column, UI_IMG("material2/MenuItem/kilometer.png"),
                             ROW_ICON_W, ROW_ICON_H, ROW_ICON_GAP,
                             &lv_font_montserrat_28);
    (void)ui_info_row_create(&card->row_speed,
                             column, UI_IMG("material2/MenuItem/gauge.png"),
                             ROW_ICON_W, ROW_ICON_H, ROW_ICON_GAP,
                             &lv_font_montserrat_28);

    /* 三行竖排，行间距 ROW_GAP */
    lv_obj_set_size(column, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(column, ROW_GAP, 0);
    lv_obj_set_flex_align(column, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Column 在内容区居中后 x 偏移 +50、y 偏移 -70 */
    lv_obj_center(column);
    lv_obj_set_style_translate_x(column, 50, 0);
    lv_obj_set_style_translate_y(column, -70, 0);
}

bool itinerary_card_create(itinerary_card_t *card, lv_obj_t *parent)
{
    lv_obj_t *content;

    if(card == NULL) {
        return false;
    }

    card->root = ui_blank_container(parent);
    lv_obj_set_size(card->root, CARD_W, CARD_H);
    lv_obj_set_pos(card->root, CARD_X, CARD_Y);
    /* 居中卡片外层自带一层同尺寸容器：卡片根对象仍由调用方定位，不参与其布局 */
    lv_obj_add_flag(card->root, LV_OBJ_FLAG_IGNORE_LAYOUT);

    /* 标题 + 内容区直接建在 root 内，不再套一层卡片 */
    content = ui_card_template_create(card->root, 0, 0, CARD_W, CARD_H,
                                      CARD_TITLE_H, "ITINERARY",
                                      &lv_font_montserrat_36);
    rows_create(card, content);

    return true;
}

void itinerary_card_destroy(itinerary_card_t *card)
{
    if(card != NULL && card->root != NULL) {
        lv_obj_delete(card->root);
        card->root = NULL;
    }
}

/* 刷新三行文字：时长 / 里程 / 平均车速（数据来自 0x0C 帧） */
void itinerary_card_apply(itinerary_card_t *card, const car_state_t *state)
{
    const car_trip_stats_t *ts = &state->trip_stats;
    bool mile = state->units.valid && state->units.mile;
    char buf[16];

    if(card == NULL || !ts->valid) {
        return;
    }

    (void)snprintf(buf, sizeof(buf), "%uh %um",
                   (unsigned)(ts->drive_minutes / 60U),
                   (unsigned)(ts->drive_minutes % 60U));
    ui_info_row_set_text(&card->row_time, buf);

    if(mile) {
        (void)snprintf(buf, sizeof(buf), "%u mi",
                       (unsigned)(((uint64_t)ts->distance_km * 10U + 8U) / 16U));
    }
    else {
        (void)snprintf(buf, sizeof(buf), "%u km", (unsigned)ts->distance_km);
    }
    ui_info_row_set_text(&card->row_distance, buf);

    if(mile) {
        (void)snprintf(buf, sizeof(buf), "%u mph",
                       (unsigned)(((uint32_t)ts->avg_speed_kph * 10U + 8U) / 16U));
    }
    else {
        (void)snprintf(buf, sizeof(buf), "%u km/h",
                       (unsigned)ts->avg_speed_kph);
    }
    ui_info_row_set_text(&card->row_speed, buf);
}
