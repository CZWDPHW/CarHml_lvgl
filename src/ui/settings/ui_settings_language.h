#ifndef UI_SETTINGS_LANGUAGE_H
#define UI_SETTINGS_LANGUAGE_H
#include "lvgl.h"
#include "ui_settings_text.h"
/* 使用设置中原有的语言编号；缺失译文由资源层回退英文。 */
void ui_settings_translate(lv_obj_t *root, uint8_t language);
/* ID 标签直接查资源表，保留字体和缓存，MCU 回读后也可立即更新。 */
void ui_settings_label_set_id(lv_obj_t *label, ui_settings_text_id_t text, uint8_t language);
/* Accept English source text without resetting an unchanged translated label. */
void ui_settings_label_set_text(lv_obj_t *label, const char *text);
#endif
