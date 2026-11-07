/*
 * page_connect.c - 中心卡片：互联信息（对应 PhoneConnect.qml）
 *
 * Qt 坐标：
 *   卡片 586×503 居中于 814×509 → (114,3)；标题区 150
 *   内容区 546×264 于 (20,0)
 *   关闭提示：topMargin 50，宽 506，24 号居中，两行
 *   已连接：406×174 居中 → (70,45)，图拉伸铺满 + 3px #00C800 边框，
 *           模式名 16 号贴在图上方左侧（LVGL 用 18 号）
 *   待连接：列居中、间距 14，行间距 10：
 *           模式图标 28×28 + "CURRENT MODE: xxx" 32 号（用 36）
 *           蓝牙图标 28×28 + 提示 24 号
 *           26×26 占位 + "BLUETOOTH: xxx" 24 号 #00C6FF
 *   连接中：240×140 居中、圆角 12、#E0000000 + 1px #40FFFFFF，
 *           内部 40×40 圆环 4px #00C89F + "CONNECTING..." 20 号
 *
 * 需要缩放的图一律整图驻留 RAM；中文文案受限，全部英文。
 */
#include "ui/PageModel/page_connect.h"

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

#define PREVIEW_W 406
#define PREVIEW_H 174
#define PREVIEW_X ((BODY_W - PREVIEW_W) / 2)
#define PREVIEW_Y ((BODY_H - PREVIEW_H) / 2)

#define PANEL_W 240
#define PANEL_H 140
#define PANEL_X ((BODY_W - PANEL_W) / 2)
#define PANEL_Y ((BODY_H - PANEL_H) / 2)

#define COLOR_GREEN   0x00C800U
#define COLOR_SPINNER 0x00C89FU
#define COLOR_BT_NAME 0x00C6FFU

#define MODE_COUNT 3U

/* 三种模式：0 Android Auto / 1 CarPlay / 2 HiCar */
static const char *const s_icon_path[MODE_COUNT] = {
    "WarnIcon/android-auto.bin",
    "WarnIcon/carplay_fill.bin",
    "WarnIcon/HiCar.bin",
};
static const char *const s_preview_path[MODE_COUNT] = {
    "WarnIcon/AA.bin",
    "WarnIcon/carplay.bin",
    "WarnIcon/HC.bin",
};
static const char *const s_mode_name[MODE_COUNT] = {
    "Android Auto",
    "CarPlay",
    "HiCar",
};

static lv_image_dsc_t s_icon_dsc[MODE_COUNT];
static lv_image_dsc_t s_preview_dsc[MODE_COUNT];
static lv_image_dsc_t s_bt_dsc;

static void copy_text(char *dst, size_t size, const char *src)
{
    size_t len;

    if(src == NULL) {
        src = "";
    }

    len = strlen(src);
    if(len >= size) {
        len = size - 1U;
    }

    memcpy(dst, src, len);
    dst[len] = '\0';
}

static uint8_t mode_index(const char *mode_text)
{
    uint8_t i;

    for(i = 0U; i < MODE_COUNT; ++i) {
        if(mode_text != NULL && strcmp(mode_text, s_mode_name[i]) == 0) {
            return i;
        }
    }

    return 0U;
}

static void show_only(connect_card_t *card, lv_obj_t *target)
{
    lv_obj_add_flag(card->disabled, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(card->waiting, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(card->connected, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(card->connecting, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(card->preview_name, LV_OBJ_FLAG_HIDDEN);

    if(target != NULL) {
        lv_obj_remove_flag(target, LV_OBJ_FLAG_HIDDEN);
    }
}

/* 关闭提示：两行居中，topMargin 50 */
static void disabled_create(connect_card_t *card, lv_obj_t *body)
{
    card->disabled = lv_label_create(body);
    lv_label_set_text(card->disabled,
                      "CONNECT IS OFF\nPLEASE ENABLE AND SELECT A MODE");
    lv_obj_set_width(card->disabled, BODY_W - 40);
    lv_obj_set_style_text_font(card->disabled, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(card->disabled, lv_color_white(), 0);
    lv_obj_set_style_text_align(card->disabled, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(card->disabled, LV_ALIGN_TOP_MID, 0, 50);
}

/* 已连接：预览图 + 3px 绿框；模式名挂在 body 上（避免被预览容器裁掉） */
static void connected_create(connect_card_t *card, lv_obj_t *body)
{
    lv_obj_t *frame;

    card->connected = ui_blank_container(body);
    lv_obj_set_size(card->connected, PREVIEW_W, PREVIEW_H);
    lv_obj_set_pos(card->connected, PREVIEW_X, PREVIEW_Y);

    card->preview = lv_image_create(card->connected);
    lv_obj_set_size(card->preview, PREVIEW_W, PREVIEW_H);
    lv_image_set_inner_align(card->preview, LV_IMAGE_ALIGN_STRETCH);
    lv_obj_set_pos(card->preview, 0, 0);

    /* 边框在图上，与 Qt 声明顺序一致 */
    frame = ui_blank_container(card->connected);
    lv_obj_set_size(frame, PREVIEW_W, PREVIEW_H);
    lv_obj_set_pos(frame, 0, 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(frame, lv_color_hex(COLOR_GREEN), 0);
    lv_obj_set_style_border_width(frame, 3, 0);

    card->preview_name = lv_label_create(body);
    lv_label_set_text(card->preview_name, "");
    lv_obj_set_style_text_font(card->preview_name, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(card->preview_name, lv_color_white(), 0);
    lv_obj_align_to(card->preview_name, card->connected,
                    LV_ALIGN_OUT_TOP_LEFT, 0, 0);

    lv_obj_add_flag(card->connected, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(card->preview_name, LV_OBJ_FLAG_HIDDEN);
}

/* 待连接：列居中，间距 14；行内间距 10 */
static void waiting_create(connect_card_t *card, lv_obj_t *body)
{
    lv_obj_t *row;
    lv_obj_t *label;
    lv_obj_t *spacer;

    card->waiting = ui_blank_container(body);
    lv_obj_set_size(card->waiting, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card->waiting, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card->waiting, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card->waiting, 14, 0);
    lv_obj_align(card->waiting, LV_ALIGN_CENTER, 0, 0);

    /* 第一行：模式图标 + 当前模式 */
    row = ui_blank_container(card->waiting);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 10, 0);

    card->wait_icon = lv_image_create(row);
    lv_obj_set_size(card->wait_icon, 28, 28);
    lv_image_set_inner_align(card->wait_icon, LV_IMAGE_ALIGN_CONTAIN);

    card->wait_mode = lv_label_create(row);
    lv_label_set_text(card->wait_mode, "");
    lv_obj_set_style_text_font(card->wait_mode, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(card->wait_mode, lv_color_white(), 0);

    /* 第二行：蓝牙图标 + 提示 */
    row = ui_blank_container(card->waiting);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 10, 0);

    (void)ui_assets_load_image(&s_bt_dsc, "WarnIcon/bluetooth.bin");

    {
        lv_obj_t *bt_icon = lv_image_create(row);

        lv_image_set_src(bt_icon, &s_bt_dsc);
        lv_obj_set_size(bt_icon, 28, 28);
        lv_image_set_inner_align(bt_icon, LV_IMAGE_ALIGN_CONTAIN);
    }

    label = lv_label_create(row);
    lv_label_set_text(label, "CONNECT BLUETOOTH FOR WIRELESS");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);

    /* 第三行：26×26 占位 + 蓝牙名称 */
    row = ui_blank_container(card->waiting);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 10, 0);

    spacer = ui_blank_container(row);
    lv_obj_set_size(spacer, 26, 26);

    card->wait_bt_name = lv_label_create(row);
    lv_label_set_text(card->wait_bt_name, "");
    lv_obj_set_style_text_font(card->wait_bt_name, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(card->wait_bt_name,
                                lv_color_hex(COLOR_BT_NAME), 0);

    lv_obj_add_flag(card->waiting, LV_OBJ_FLAG_HIDDEN);
}

/* 连接中：240×140 面板 + 圆环 + 文案 */
static void connecting_create(connect_card_t *card, lv_obj_t *body)
{
    lv_obj_t *col;
    lv_obj_t *ring;
    lv_obj_t *label;

    card->connecting = ui_blank_container(body);
    lv_obj_set_size(card->connecting, PANEL_W, PANEL_H);
    lv_obj_set_pos(card->connecting, PANEL_X, PANEL_Y);
    lv_obj_set_style_radius(card->connecting, 12, 0);
    lv_obj_set_style_bg_color(card->connecting, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(card->connecting, 224, 0);
    lv_obj_set_style_border_color(card->connecting, lv_color_white(), 0);
    lv_obj_set_style_border_opa(card->connecting, 64, 0);
    lv_obj_set_style_border_width(card->connecting, 1, 0);

    col = ui_blank_container(card->connecting);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 16, 0);
    lv_obj_align(col, LV_ALIGN_CENTER, 0, 0);

    ring = ui_blank_container(col);
    lv_obj_set_size(ring, 40, 40);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(COLOR_SPINNER), 0);
    lv_obj_set_style_border_width(ring, 4, 0);

    label = lv_label_create(col);
    lv_label_set_text(label, "CONNECTING...");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);

    lv_obj_add_flag(card->connecting, LV_OBJ_FLAG_HIDDEN);
}

bool connect_card_create(connect_card_t *card, lv_obj_t *parent)
{
    lv_obj_t *content;
    lv_obj_t *body;

    if(card == NULL) {
        return false;
    }

    card->state = 0xFFU;

    card->root = ui_blank_container(parent);
    lv_obj_add_flag(card->root, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(card->root, CARD_W, CARD_H);
    lv_obj_set_pos(card->root, CARD_X, CARD_Y);

    content = ui_card_template_create(card->root, 0, 0, CARD_W, CARD_H,
                                      TITLE_H, "CONNECT",
                                      &lv_font_montserrat_36);

    body = ui_blank_container(content);
    lv_obj_set_size(body, BODY_W, BODY_H);
    lv_obj_set_pos(body, BODY_X, BODY_Y);

    disabled_create(card, body);
    connected_create(card, body);
    waiting_create(card, body);
    connecting_create(card, body);

    return true;
}

void connect_card_demo_set(connect_card_t *card, uint8_t state,
                           const char *mode_text, const char *bt_name)
{
    char t_mode[32];
    char t_bt[64];
    char buf[96];
    uint8_t idx;

    if(card == NULL) {
        return;
    }

    copy_text(t_mode, sizeof(t_mode), mode_text);
    copy_text(t_bt, sizeof(t_bt), bt_name);

    if(card->state == state &&
       strcmp(card->s_mode, t_mode) == 0 &&
       strcmp(card->s_bt, t_bt) == 0) {
        return;
    }

    card->state = state;
    strcpy(card->s_mode, t_mode);
    strcpy(card->s_bt, t_bt);

    idx = mode_index(t_mode);

    if(state == 0U) {
        show_only(card, card->disabled);
    }
    else if(state == 2U) {
        (void)ui_assets_load_image(&s_preview_dsc[idx], s_preview_path[idx]);
        lv_image_set_src(card->preview, &s_preview_dsc[idx]);

        (void)snprintf(buf, sizeof(buf), "%s", s_mode_name[idx]);
        lv_label_set_text(card->preview_name, buf);

        show_only(card, card->connected);
        lv_obj_remove_flag(card->preview_name, LV_OBJ_FLAG_HIDDEN);
    }
    else if(state == 3U) {
        show_only(card, card->connecting);
    }
    else {
        (void)ui_assets_load_image(&s_icon_dsc[idx], s_icon_path[idx]);
        lv_image_set_src(card->wait_icon, &s_icon_dsc[idx]);

        (void)snprintf(buf, sizeof(buf), "CURRENT MODE: %s", s_mode_name[idx]);
        lv_label_set_text(card->wait_mode, buf);

        (void)snprintf(buf, sizeof(buf), "BLUETOOTH: %s", t_bt);
        lv_label_set_text(card->wait_bt_name, buf);

        show_only(card, card->waiting);
    }
}

void connect_card_destroy(connect_card_t *card)
{
    if(card != NULL && card->root != NULL) {
        lv_obj_delete(card->root);
        card->root = NULL;
    }
}