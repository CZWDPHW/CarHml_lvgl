/*
 * ui_node.h - UI 组件契约
 *
 * 对应 QML 的 Item 树：每个可复用界面组件实现同一组生命周期函数，
 * 由 ui_app 负责创建、销毁与状态分发。
 */
#ifndef CARMW_UI_NODE_H
#define CARMW_UI_NODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "carmw/car_state.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ui_node ui_node_t;

/* 按键事件：与 carmw 解耦，由 main.c 转换成该结构 */
typedef struct {
    uint8_t code;
    uint8_t pressed;
} ui_key_event_t;

/* 按键语义码：由输入层（PC 键盘 / 板端方向盘驱动）映射，样式层只认这几个 */
typedef enum {
    UI_KEY_PREV = 1,   /* 上一张中心卡片：方向盘左 / 上（PC 演示用） */
    UI_KEY_NEXT = 2,   /* 下一张中心卡片：方向盘右 / 下（PC 演示用） */
    UI_KEY_OK   = 3,   /* 确认 */
    UI_KEY_UP   = 4,   /* 方向盘上 */
    UI_KEY_DOWN = 5,   /* 方向盘下 */
    UI_KEY_LEFT = 6,   /* 方向盘左 */
    UI_KEY_RIGHT = 7,  /* 方向盘右 */
} ui_key_code_t;

typedef struct {
    size_t size;                                                     /* 组件私有结构大小 */
    bool (*create)(ui_node_t *self, lv_obj_t *parent);               /* 创建 LVGL 对象 */
    void (*destroy)(ui_node_t *self);                                /* 销毁 LVGL 对象 */
    void (*apply)(ui_node_t *self, const car_state_view_t *view);    /* 应用车状态快照 */
    void (*on_show)(ui_node_t *self);                                /* 页面显示 */
    void (*on_hide)(ui_node_t *self);                                /* 页面隐藏 */
    void (*on_key)(ui_node_t *self, const ui_key_event_t *evt);      /* 按键分发 */
        /* PC 演示可选项：注入媒体数据（样式不实现时为 NULL） */
    void (*on_demo_media)(ui_node_t *self, uint8_t mode, const char *title,
                          const char *line1, const char *line2);
    /* PC 演示可选项：注入互联状态 */
    void (*on_demo_connect)(ui_node_t *self, uint8_t state,
                            const char *mode_text, const char *bt_name);
} ui_node_vtbl_t;

struct ui_node {
    const ui_node_vtbl_t *vt;// 虚表指针
    lv_obj_t *root;// 根容器
};

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_NODE_H */
