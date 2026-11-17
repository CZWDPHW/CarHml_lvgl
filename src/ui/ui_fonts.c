#include "ui/ui_fonts.h"

#define UI_QT_FONT(family, pixels, bold, name) LV_FONT_DECLARE(name)
#include "ui/fonts/qt_fonts.inc"
#undef UI_QT_FONT

typedef struct {
    ui_font_family_t family;
    unsigned pixels;
    bool bold;
    const lv_font_t *font;
} font_entry_t;

static const font_entry_t s_fonts[] = {
#define UI_QT_FONT(family, pixels, bold, name) {UI_TYPE_##family, pixels, bold, &name},
#include "ui/fonts/qt_fonts.inc"
#undef UI_QT_FONT
};

const lv_font_t *ui_font_get(ui_font_family_t family, unsigned pixels, bool bold)
{
    for(size_t i = 0; i < sizeof(s_fonts) / sizeof(s_fonts[0]); ++i) {
        if(s_fonts[i].family == family && s_fonts[i].pixels == pixels && s_fonts[i].bold == bold)
            return s_fonts[i].font;
    }
    return NULL;
}

unsigned ui_font_pixels(const lv_font_t *font)
{
    for(size_t i = 0; i < sizeof(s_fonts) / sizeof(s_fonts[0]); ++i) {
        if(s_fonts[i].font == font) return s_fonts[i].pixels;
    }
    /* Legacy callers still use built-in fonts. Nominal px != line_height. */
#define LEGACY(px) if(font == &lv_font_montserrat_##px) return px
    LEGACY(14); LEGACY(18); LEGACY(20); LEGACY(22); LEGACY(24);
    LEGACY(28); LEGACY(36); LEGACY(44); LEGACY(46); LEGACY(48);
#undef LEGACY
    return 0;
}

const lv_font_t *ui_font_cjk(const lv_font_t *font)
{
    for(size_t i = 0; i < sizeof(s_fonts) / sizeof(s_fonts[0]); ++i) {
        if(s_fonts[i].font == font) {
            if(s_fonts[i].family == UI_TYPE_TEXT || s_fonts[i].family == UI_TYPE_GEELY ||
               s_fonts[i].family == UI_TYPE_ARIAL) return font;
            const lv_font_t *text = UI_TEXT(s_fonts[i].pixels);
            return text ? text : UI_TEXT(24);
        }
    }
    unsigned pixels = ui_font_pixels(font);
    const lv_font_t *text = pixels ? UI_TEXT(pixels) : NULL;
    return text ? text : UI_TEXT(24);
}
