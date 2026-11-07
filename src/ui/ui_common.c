/*
 * ui_common.c - 页面根容器实现
 */
#include "ui/ui_common.h"

lv_obj_t *ui_page_create(lv_obj_t *parent)
{
    lv_obj_t *page = lv_obj_create(parent);

    lv_obj_remove_style_all(page);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(page, UI_DESIGN_W, UI_DESIGN_H);
    lv_obj_set_pos(page, 0, 0);
    lv_obj_set_style_bg_color(page, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_transform_scale(page, UI_PAGE_SCALE, 0);
    lv_obj_set_style_transform_pivot_x(page, UI_DESIGN_W / 2, 0);
    lv_obj_set_style_transform_pivot_y(page, UI_DESIGN_H / 2, 0);

    return page;
}