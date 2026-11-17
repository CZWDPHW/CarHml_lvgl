#ifndef CARMW_UI_FONTS_H
#define CARMW_UI_FONTS_H

#include "lvgl.h"

typedef enum {
    UI_TYPE_TEXT,       /* Unspecified Qt Latin family: retain Montserrat. */
    UI_TYPE_GEELY,
    UI_TYPE_KONEXY,
    UI_TYPE_FORGOTTEN,
    UI_TYPE_BEBAS,
    UI_TYPE_ARIAL
} ui_font_family_t;

/* Exact generated pixel size; unsupported combinations return NULL. */
const lv_font_t *ui_font_get(ui_font_family_t family, unsigned pixels, bool bold);
unsigned ui_font_pixels(const lv_font_t *font);
/* Same-size text/CJK font for legacy or specialized Latin-only fonts. */
const lv_font_t *ui_font_cjk(const lv_font_t *font);

#define UI_TEXT(px) ui_font_get(UI_TYPE_TEXT, (px), false)
#define UI_TEXT_BOLD(px) ui_font_get(UI_TYPE_TEXT, (px), true)
#define UI_GEELY(px) ui_font_get(UI_TYPE_GEELY, (px), true)
#define UI_KONEXY(px) ui_font_get(UI_TYPE_KONEXY, (px), false)

#endif
