/*
 * page_music.c - 中心卡片：多媒体 / 歌词（对应 PageMusic.qml）
 *
 * Qt 坐标：
 *   页面 847×503，Loader centerIn 814×509 → (-16,3)
 *   标题区 150 高，标题白色 36 号
 *   内容区 847×359（Qt contentHeight 默认 359）
 *   FM：列水平居中、底边距 179、间距 30；信源 64 号、频率 40 号 #FEF2A4，压在 260×80 的 media 图
 *   来电：列宽 847、底边距 151、间距 30；电话图标 120×120、姓名 42 号粗体、号码 32 号 #CCCCCC
 *   歌词：列宽 787、整体居中、间距 20；歌名 28 号粗体 #FDA23B，歌词 32 号粗体白色、自动换行
 *   节奏柱：12 根 12×100、间距 10，底边距 230；柱宽 12、圆角 6、#FDA23B 70%，上浅下深渐变
 *
 * LVGL 无 64/42/40/32 号字体，分别用 48/44/44/36。
 */
#include "ui/PageModel/page_music.h"

#include <stdio.h>
#include <string.h>

#include "ui/PageModel/page_itinerary.h"   /* ui_card_template_create */
#include "ui/ui_assets.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"

#define CARD_X   -16
#define CARD_Y   3
#define CARD_W   847
#define CARD_H   503
#define TITLE_H  150

#define BODY_W   847
#define BODY_H   359
#define LYRIC_W  787

#define COLOR_MUSIC   0xFDA23BU
#define COLOR_MUSIC_HI 0xFFC247U
#define COLOR_FREQ    0xFEF2A4U
#define COLOR_PHONE_NO 0xCCCCCCU
#define BAR_OPA       178

#define FM_BOTTOM    179
#define PHONE_BOTTOM 151
/* 电话列挂到卡片根上：底边 = 150 + 359 - 151 = 358，距卡底 145 */
#define PHONE_ROOT_OFFSET 145
#define BARS_BOTTOM  230

#define BAR_ANIM_MS  80
/* 需要缩放的图必须整图驻留 RAM，流式 bin 不支持绘制期缩放 */
static lv_image_dsc_t s_media_dsc;
static lv_image_dsc_t s_call_dsc;


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

/* 12 根柱子按正弦起伏，相位错开；Qt 用两条 SequentialAnimation，这里用定时器近似 */
static void bar_anim_cb(lv_timer_t *timer)
{
    music_card_t *card = (music_card_t *)lv_timer_get_user_data(timer);
    uint8_t i;

    if(card == NULL) {
        return;
    }

    card->anim_tick += 6U;

    for(i = 0U; i < MUSIC_BAR_COUNT; ++i) {
        int32_t angle = (int32_t)((card->anim_tick + (uint32_t)i * 30U) % 360U);
        int32_t h = 42 + (int32_t)((28 * lv_trigo_sin((int16_t)angle)) / 32768);

        if(h < 10) {
            h = 10;
        }

        lv_obj_set_height(card->bars[i], h);
    }
}

static lv_obj_t *column_create(lv_obj_t *parent, int32_t w, uint8_t spacing)
{
    lv_obj_t *col = ui_blank_container(parent);

    lv_obj_set_size(col, w, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, spacing, 0);
    return col;
}

static lv_obj_t *label_create(lv_obj_t *parent, const char *text,
                              const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

/* FM 模式：信源名 + (media.bin 背景 + 频率) */
static void fm_create(music_card_t *card, lv_obj_t *body)
{
    lv_obj_t *freq_box;
    lv_obj_t *freq_img;

    card->fm = column_create(body, LV_SIZE_CONTENT, 30);
    lv_obj_align(card->fm, LV_ALIGN_BOTTOM_MID, 0, -FM_BOTTOM);

    card->fm_source = label_create(card->fm, "FM RADIO",
                                   &lv_font_montserrat_48, 0xFFFFFFU);

    freq_box = ui_blank_container(card->fm);
    lv_obj_set_size(freq_box, 260, 80);

    // freq_img = lv_image_create(freq_box);
    // lv_image_set_src(freq_img, UI_IMG("Audi/media.bin"));
    // lv_obj_set_size(freq_img, 260, 80);
    // lv_image_set_inner_align(freq_img, LV_IMAGE_ALIGN_CONTAIN);
    freq_img = lv_image_create(freq_box);
    (void)ui_assets_load_image(&s_media_dsc, "Audi/media.bin");
    lv_image_set_src(freq_img, &s_media_dsc);
    lv_obj_set_size(freq_img, 260, 80);
    lv_image_set_inner_align(freq_img, LV_IMAGE_ALIGN_CONTAIN);

    card->fm_freq = label_create(freq_box, "98.5",
                                 &lv_font_montserrat_44, COLOR_FREQ);
    lv_obj_align(card->fm_freq, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_flag(card->fm, LV_OBJ_FLAG_HIDDEN);
}

/* 来电模式：图标 + 姓名 + 号码 */
static void phone_create(music_card_t *card)
{
    lv_obj_t *icon;

    card->phone = column_create(card->root, BODY_W, 30);
    lv_obj_align(card->phone, LV_ALIGN_BOTTOM_MID, 0, -PHONE_ROOT_OFFSET);

    icon = lv_image_create(card->phone);
    (void)ui_assets_load_image(&s_call_dsc, "Audi/call.bin");
    lv_image_set_src(icon, &s_call_dsc);
    lv_obj_set_size(icon, 120, 120);
    lv_image_set_inner_align(icon, LV_IMAGE_ALIGN_CONTAIN);

    card->phone_name = label_create(card->phone, "", &lv_font_montserrat_44,
                                    0xFFFFFFU);
    lv_obj_set_width(card->phone_name, BODY_W);
    lv_label_set_long_mode(card->phone_name, LV_LABEL_LONG_DOT);

    card->phone_number = label_create(card->phone, "", &lv_font_montserrat_36,
                                      COLOR_PHONE_NO);
    lv_obj_set_width(card->phone_number, BODY_W);
    lv_label_set_long_mode(card->phone_number, LV_LABEL_LONG_DOT);

    lv_obj_add_flag(card->phone, LV_OBJ_FLAG_HIDDEN);
}

/* 歌词模式：歌名 + 歌词（整体居中），节奏柱单独挂 body 底部 */
static void lyric_create(music_card_t *card, lv_obj_t *body)
{
    lv_obj_t *item;
    lv_obj_t *bar;
    uint8_t i;

    card->lyric = column_create(body, LYRIC_W, 20);
    lv_obj_align(card->lyric, LV_ALIGN_CENTER, 0, 0);

    card->song = label_create(card->lyric, "", &lv_font_montserrat_28,
                              COLOR_MUSIC);
    lv_obj_set_width(card->song, LYRIC_W);
    lv_label_set_long_mode(card->song, LV_LABEL_LONG_DOT);

    card->lyric_text = label_create(card->lyric, "", &lv_font_montserrat_36,
                                    0xFFFFFFU);
    lv_obj_set_width(card->lyric_text, LYRIC_W);
    lv_label_set_long_mode(card->lyric_text, LV_LABEL_LONG_WRAP);

    /* 节奏柱行：12×12 + 11×10 间距 = 254 宽，高 100，底边距 230 */
    card->visualizer = ui_blank_container(body);
    lv_obj_set_size(card->visualizer,
                    (int32_t)MUSIC_BAR_COUNT * 12 +
                        ((int32_t)MUSIC_BAR_COUNT - 1) * 10, 100);
    lv_obj_set_flex_flow(card->visualizer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card->visualizer, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card->visualizer, 10, 0);
    lv_obj_align(card->visualizer, LV_ALIGN_BOTTOM_MID, 0, -BARS_BOTTOM);

    for(i = 0U; i < MUSIC_BAR_COUNT; ++i) {
        item = ui_blank_container(card->visualizer);
        lv_obj_set_size(item, 12, 100);

        bar = ui_blank_container(item);
        lv_obj_set_width(bar, 12);
        lv_obj_set_height(bar, 30);
        lv_obj_set_style_radius(bar, 6, 0);
        lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_MUSIC_HI), 0);
        lv_obj_set_style_bg_grad_color(bar, lv_color_hex(COLOR_MUSIC), 0);
        lv_obj_set_style_bg_grad_dir(bar, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_bg_opa(bar, BAR_OPA, 0);
        lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);

        card->bars[i] = bar;
    }
}

bool music_card_create(music_card_t *card, lv_obj_t *parent)
{
    lv_obj_t *content;
    lv_obj_t *body;

    if(card == NULL) {
        return false;
    }

    card->mode = 0xFFU;
    card->active = 0U;

    card->root = ui_blank_container(parent);
    lv_obj_add_flag(card->root, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(card->root, CARD_W, CARD_H);
    lv_obj_set_pos(card->root, CARD_X, CARD_Y);

    content = ui_card_template_create(card->root, 0, 0, CARD_W, CARD_H,
                                      TITLE_H, "MEDIA",
                                      &lv_font_montserrat_36);

    /* 模板内标题标签是首个子项，抓句柄用于随模式改标题 */
    card->title = lv_obj_get_child(lv_obj_get_parent(content), 0);

      body = ui_blank_container(content);
    lv_obj_set_size(body, BODY_W, BODY_H);
    lv_obj_set_pos(body, 0, 0);

    fm_create(card, body);
    phone_create(card);
    lyric_create(card, body);

    card->anim = lv_timer_create(bar_anim_cb, BAR_ANIM_MS, card);
    lv_timer_pause(card->anim);

    return true;
}

void music_card_demo_set(music_card_t *card, uint8_t mode, const char *title,
                         const char *line1, const char *line2)
{
    char t_title[64];
    char t_line1[128];
    char t_line2[MUSIC_TEXT_MAX];

    if(card == NULL) {
        return;
    }

    copy_text(t_title, sizeof(t_title), title);
    copy_text(t_line1, sizeof(t_line1), line1);
    copy_text(t_line2, sizeof(t_line2), line2);

    if(card->mode == mode &&
       strcmp(card->s_title, t_title) == 0 &&
       strcmp(card->s_line1, t_line1) == 0 &&
       strcmp(card->s_line2, t_line2) == 0) {
        return;
    }

    card->mode = mode;
    strcpy(card->s_title, t_title);
    strcpy(card->s_line1, t_line1);
    strcpy(card->s_line2, t_line2);

    if(card->title != NULL) {
        lv_label_set_text(card->title, t_title);
    }

    lv_obj_add_flag(card->fm, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(card->phone, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(card->lyric, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(card->visualizer, LV_OBJ_FLAG_HIDDEN);

    if(mode == 0U) {
        lv_label_set_text(card->fm_source, t_line1);
        lv_label_set_text(card->fm_freq, t_line2);
        lv_obj_remove_flag(card->fm, LV_OBJ_FLAG_HIDDEN);
        card->active = 0U;
    }
    else if(mode == 2U || mode == 5U) {
        lv_label_set_text(card->phone_name, t_line1);
        lv_label_set_text(card->phone_number, t_line2);
        lv_obj_remove_flag(card->phone, LV_OBJ_FLAG_HIDDEN);
        card->active = 0U;
    }
    else {
        lv_label_set_text(card->song, t_line1);
        lv_label_set_text(card->lyric_text, t_line2);
        lv_obj_remove_flag(card->lyric, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(card->visualizer, LV_OBJ_FLAG_HIDDEN);
        card->active = (uint8_t)(t_line2[0] != '\0');
    }

    if(card->anim != NULL) {
        if(card->active != 0U) {
            card->anim_tick = 0U;
            lv_timer_reset(card->anim);
            lv_timer_resume(card->anim);
        }
        else {
            lv_timer_pause(card->anim);
        }
    }
}

void music_card_destroy(music_card_t *card)
{
    if(card != NULL && card->root != NULL) {
        if(card->anim != NULL) {
            lv_timer_delete(card->anim);
            card->anim = NULL;
        }

        lv_obj_delete(card->root);
        card->root = NULL;
    }
}