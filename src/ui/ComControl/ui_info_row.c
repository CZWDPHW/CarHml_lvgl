/*
 * ui_info_row.c - 图标 + 文字信息行实现
 */
#include "ui/ComControl/ui_info_row.h"

#include <string.h>

#include "ui/ui_theme.h"

#define INFO_ROW_TEXT_MAX 32

bool ui_info_row_create(ui_info_row_t *row, lv_obj_t *parent,
                        const void *icon, int32_t icon_w, int32_t icon_h,
                        int32_t gap, const lv_font_t *font)
{
    if(row == NULL) {
        return false;
    }

    row->root = ui_blank_container(parent);
    lv_obj_set_size(row->root, LV_SIZE_CONTENT, icon_h);
    lv_obj_set_flex_flow(row->root, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row->root, gap, 0);
    lv_obj_set_flex_align(row->root, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* 图片容器：固定图标矩形，图在内部等比缩放居中 */
    {
        lv_obj_t *box = ui_blank_container(row->root);

        lv_obj_set_size(box, icon_w, icon_h);
        row->icon = lv_image_create(box);
        lv_image_set_src(row->icon, icon);
        lv_obj_set_size(row->icon, icon_w, icon_h);
        lv_image_set_inner_align(row->icon, LV_IMAGE_ALIGN_CONTAIN);
        lv_obj_center(row->icon);
    }

    row->label = lv_label_create(row->root);
    lv_label_set_text(row->label, "");
    lv_obj_set_style_text_font(row->label, font, 0);
    lv_obj_set_style_text_color(row->label, lv_color_white(), 0);
    lv_obj_set_style_text_align(row->label, LV_TEXT_ALIGN_LEFT, 0);

    return true;
}

void ui_info_row_set_text(ui_info_row_t *row, const char *text)
{
    if(row == NULL || row->label == NULL || text == NULL) {
        return;
    }

    if(strncmp(lv_label_get_text(row->label), text, INFO_ROW_TEXT_MAX) == 0) {
        return;
    }

    lv_label_set_text(row->label, text);
}