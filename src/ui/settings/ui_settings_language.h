#ifndef UI_SETTINGS_LANGUAGE_H
#define UI_SETTINGS_LANGUAGE_H
#include "lvgl.h"
void ui_settings_translate(lv_obj_t *root, bool chinese);
/* Accept English source text without resetting an unchanged translated label. */
void ui_settings_label_set_text(lv_obj_t *label, const char *text);
#endif
