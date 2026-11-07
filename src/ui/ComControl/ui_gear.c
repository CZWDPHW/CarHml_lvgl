/*
 * ui_gear.c - 档位显示实现
 */
#include "ui/ComControl/ui_gear.h"

#include <stddef.h>
#include <string.h>

#include "ui/ui_theme.h"

bool ui_gear_create(ui_gear_t *gear, lv_obj_t *parent, int32_t x, int32_t y,
                    int32_t size, const lv_font_t *font)
{
    if(gear == NULL) {
        return false;
    }

    gear->root = ui_blank_container(parent);
    lv_obj_set_size(gear->root, size, size);
    lv_obj_set_pos(gear->root, x, y);

    gear->label = lv_label_create(gear->root);
    lv_label_set_text(gear->label, "");
    lv_obj_set_style_text_font(gear->label, font, 0);
    lv_obj_set_style_text_color(gear->label, lv_color_white(), 0);
    lv_obj_center(gear->label);

    gear->shown[0] = '\0';

    return true;
}

void ui_gear_apply(ui_gear_t *gear, const char *text)
{
    size_t len;

    if(gear == NULL || gear->label == NULL || text == NULL) {
        return;
    }

    /* 档位没变就不重设文本，避免每帧触发标签重排 */
    if(strcmp(gear->shown, text) == 0) {
        return;
    }

    /* 截断到 UI_GEAR_TEXT_MAX-1，与 car_gear_t.text 的宽度一致 */
    len = strlen(text);
    if(len >= UI_GEAR_TEXT_MAX) {
        len = UI_GEAR_TEXT_MAX - 1;
    }

    memcpy(gear->shown, text, len);
    gear->shown[len] = '\0';

    lv_label_set_text(gear->label, gear->shown);
}