/*
 * ui_i18n.h - Dashboard-scene localization
 *
 * Translates labels under the style page root according to the language
 * stored in settings. The settings page keeps its own translation chain.
 */
#ifndef CARMW_UI_I18N_H
#define CARMW_UI_I18N_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_i18n_set_language(bool chinese);
bool ui_i18n_is_chinese(void);

/* Map an English dashboard font to the matching CJK subset font. */
const lv_font_t *ui_i18n_font(const lv_font_t *en_font);

/* Recursively translate every label under root. */
void ui_i18n_apply(lv_obj_t *root);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_I18N_H */
