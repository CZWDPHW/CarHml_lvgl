/*
 * ui_outside_temp.c - 车外温度实现
 *
 * Qt 换算：outsideTempC = (raw - 0x7C) × 0.5
 * 特殊值：0xFF 隐藏 / 0xFE 显示 --.- / 0xFC 显示 E
 *
 * 中文字库未就绪，Qt 的 ℃/℉ 用 "°C"/"°F" 占位（montserrat 已含 0xB0）。
 */
#include "ui/ComControl/ui_outside_temp.h"

#include <stdio.h>

#include "ui/ui_theme.h"

#define TEMP_RAW_HIDDEN 0xFFU
#define TEMP_RAW_DASH 0xFEU
#define TEMP_RAW_ERROR 0xFCU
#define TEMP_RAW_ZERO 0x7CU       /* 换算后正好 0 ℃ */
#define TEMP_C_OFFSET 0x7CU

/* 行内创建一个白色标签 */
static lv_obj_t *temp_label(lv_obj_t *row, const char *text,
                            const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(row);

    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);

    return label;
}

/* 0.1 单位的整数格式化成 "22.0"/"-5.5"，避免依赖浮点 printf */
static void format_x10(char *buf, size_t size, int16_t value)
{
    int32_t v = value;
    const char *sign = "";

    if(v < 0) {
        sign = "-";
        v = -v;
    }

    (void)snprintf(buf, size, "%s%ld.%ld", sign, (long)(v / 10), (long)(v % 10));
}

bool ui_outside_temp_create(ui_outside_temp_t *temp, lv_obj_t *parent,
                            int32_t x, int32_t y, const lv_font_t *font)
{
    if(temp == NULL) {
        return false;
    }

    temp->root = ui_blank_container(parent);
    lv_obj_set_size(temp->root, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(temp->root, x, y);
    lv_obj_set_flex_flow(temp->root, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(temp->root, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    temp->value = temp_label(temp->root, "--", font);
    temp->unit = temp_label(temp->root, "", font);

    temp->shown_x10 = 0;
    temp->shown_mode = (uint8_t)UI_TEMP_PLACEHOLDER;
    temp->shown_fahren = false;
    temp->shown_valid = false;

    return true;
}

void ui_outside_temp_apply(ui_outside_temp_t *temp,
                           const car_other_info_t *info, bool fahrenheit)
{
    char buf[16];
    uint8_t mode;
    int16_t x10 = 0;

    if(temp == NULL || temp->value == NULL)
        return;

    if(info == NULL || !info->valid) {
        mode = (uint8_t)UI_TEMP_PLACEHOLDER;
    }
    else {
        uint8_t raw = info->outside_temp_raw;

        if(raw == TEMP_RAW_HIDDEN) {
            mode = (uint8_t)UI_TEMP_HIDDEN;
        }
        else if(raw == TEMP_RAW_DASH) {
            mode = (uint8_t)UI_TEMP_DASH;
        }
        else if(raw == TEMP_RAW_ERROR) {
            mode = (uint8_t)UI_TEMP_ERROR;
        }
        else if(raw == TEMP_RAW_ZERO) {
            mode = (uint8_t)UI_TEMP_PLACEHOLDER;
        }
        else {
            /* 每 0.5 ℃ 对应 5 个 0.1 单位 */
            x10 = (int16_t)(((int32_t)raw - (int32_t)TEMP_C_OFFSET) * 5);

            if(fahrenheit) {
                x10 = (int16_t)(((int32_t)x10 * 18 + 3200) / 10);
            }

            mode = (uint8_t)UI_TEMP_NORMAL;
        }
    }

    if(temp->shown_valid && temp->shown_mode == mode &&
       (mode != (uint8_t)UI_TEMP_NORMAL ||
        (temp->shown_x10 == x10 && temp->shown_fahren == fahrenheit))) {
        return;
    }

    temp->shown_mode = mode;
    temp->shown_x10 = x10;
    temp->shown_fahren = fahrenheit;
    temp->shown_valid = true;

    switch(mode) {
    case UI_TEMP_NORMAL:
    case UI_TEMP_DASH:
        format_x10(buf, sizeof(buf), x10);
        lv_label_set_text(temp->value, mode == (uint8_t)UI_TEMP_NORMAL ? buf : "--.-");
        lv_label_set_text(temp->unit, fahrenheit ? "°F" : "°C");
        break;
    case UI_TEMP_HIDDEN:
        lv_label_set_text(temp->value, "");
        lv_label_set_text(temp->unit, "");
        break;
    case UI_TEMP_ERROR:
        lv_label_set_text(temp->value, "E");
        lv_label_set_text(temp->unit, "");
        break;
    default:
        lv_label_set_text(temp->value, "--");
        lv_label_set_text(temp->unit, "");
        break;
    }
}
