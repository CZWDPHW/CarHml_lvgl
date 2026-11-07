/*
 * page_connect.h - 中心卡片：互联信息（对应 CentralDisplay/PhoneConnect.qml）
 *
 * carmw 无手机互联数据，状态由演示入口注入：
 *   main.c → ui_app_demo_connect() → 样式页 → connect_card_demo_set()
 * state：0 互联关闭 / 1 已开启未连接 / 2 已连接 / 3 连接中
 */
#ifndef CARMW_UI_PAGE_CONNECT_H
#define CARMW_UI_PAGE_CONNECT_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;
    lv_obj_t *disabled;      /* 关闭提示文本 */
    lv_obj_t *waiting;       /* 已开启未连接：三行列 */
    lv_obj_t *wait_icon;     /* 手机互联模式图标 28×28 */
    lv_obj_t *wait_mode;     /* "CURRENT MODE: xxx" */
    lv_obj_t *wait_bt_name;  /* "BLUETOOTH: xxx" */
    lv_obj_t *connected;     /* 已连接：406×174 预览容器 */
    lv_obj_t *preview;       /* 预览图（拉伸铺满） */
    lv_obj_t *preview_name;  /* 预览图上方的模式名 */
    lv_obj_t *connecting;    /* 连接中面板 240×140 */
    uint8_t state;           /* 0~3，0xFF 为初始 */
    char s_mode[32];
    char s_bt[64];
} connect_card_t;

bool connect_card_create(connect_card_t *card, lv_obj_t *parent);

/* 演示入口：state + 模式名（CarPlay / Android Auto / HiCar）+ 蓝牙名 */
void connect_card_demo_set(connect_card_t *card, uint8_t state,
                           const char *mode_text, const char *bt_name);

void connect_card_destroy(connect_card_t *card);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_PAGE_CONNECT_H */