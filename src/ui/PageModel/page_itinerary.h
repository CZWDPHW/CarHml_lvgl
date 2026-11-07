/*
 * page_itinerary.h - 中心卡片：行程数据（对应 CentralDisplay/PageDrivingInfo.qml）
 *
 * 中心卡片是页面内的子容器，不是独立样式节点：由样式页创建、显隐与销毁。
 * 页面模板（标题 150 高 + 内容区）也放在这里，后续其余中心卡片复用。
 */
#ifndef CARMW_UI_PAGE_ITINERARY_H
#define CARMW_UI_PAGE_ITINERARY_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "carmw/car_state.h"
#include "ui/ComControl/ui_info_row.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 卡片通用模板：新建一页，左上角位于 parent 内 (x,y)，尺寸 w×h，
 * 顶部标题区高 title_h，标题文字居中（字号 title_font），
 * 返回内容区对象（尺寸 w×(h-title_h)，左上角在标题区下方）。
 */
lv_obj_t *ui_card_template_create(lv_obj_t *parent, int32_t x, int32_t y,
                                  int32_t w, int32_t h, int32_t title_h,
                                  const char *title, const lv_font_t *title_font);

typedef struct {
    lv_obj_t *root;
    ui_info_row_t row_time;
    ui_info_row_t row_distance;
    ui_info_row_t row_speed;
} itinerary_card_t;

/* 在中心卡片容器内创建行程数据卡片（586×509，居中于 814×509） */
bool itinerary_card_create(itinerary_card_t *card, lv_obj_t *parent);

/* 刷新三行文字：时长 / 里程 / 平均车速（数据来自 0x0C 帧） */
void itinerary_card_apply(itinerary_card_t *card, const car_state_t *state);

void itinerary_card_destroy(itinerary_card_t *card);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_PAGE_ITINERARY_H */
