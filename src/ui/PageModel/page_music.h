/*
 * page_music.h - 中心卡片：多媒体 / 歌词（对应 CentralDisplay/PageMusic.qml）
 *
 * carmw 协议里没有媒体帧，数据由 PC 演示入口注入：
 *   main.c → ui_app_demo_media() → 样式页 → music_card_demo_set()
 * 三种子模式与 Qt media_mode 对应：0 FM / 2 来电 / 4 歌词（5 语音识别同 2）。
 */
#ifndef CARMW_UI_PAGE_MUSIC_H
#define CARMW_UI_PAGE_MUSIC_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MUSIC_BAR_COUNT 12U   /* Qt Repeater model: 12 */
#define MUSIC_TEXT_MAX  192U

typedef struct {
    lv_obj_t *root;
    lv_obj_t *title;       /* 模板标题，随模式变化 */
    lv_obj_t *fm;          /* FM 模式列 */
    lv_obj_t *fm_source;   /* 64 号信源名（LVGL 用 48 号） */
    lv_obj_t *fm_freq;     /* 频率，压在 media.bin 上 */
    lv_obj_t *phone;       /* 来电模式列 */
    lv_obj_t *phone_name;  /* 来电姓名 */
    lv_obj_t *phone_number;/* 来电号码 */
    lv_obj_t *lyric;       /* 歌词模式列 */
    lv_obj_t *song;        /* 歌曲模式列 */
    lv_obj_t *lyric_text;
    lv_obj_t *visualizer;  /* 底部节奏柱行 254×100 */
    lv_obj_t *bars[MUSIC_BAR_COUNT];
    lv_timer_t *anim;      /* 柱状动画，仅歌词有效时运行 */
    uint32_t anim_tick;
    uint8_t mode;          /* 0 / 2 / 4 / 5 */
    uint8_t active;        /* 等价 Qt isMusicActive */
    char s_title[64];
    char s_line1[128];
    char s_line2[MUSIC_TEXT_MAX];
} music_card_t;

bool music_card_create(music_card_t *card, lv_obj_t *parent);

/* PC 演示入口：mode 0 FM / 2 来电 / 4 歌词；内容与上次完全相同时直接返回 */
void music_card_demo_set(music_card_t *card, uint8_t mode, const char *title,
                         const char *line1, const char *line2);

void music_card_destroy(music_card_t *card);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_PAGE_MUSIC_H */