#ifndef CARMW_UI_DISPLAY_H
#define CARMW_UI_DISPLAY_H

#include "lvgl.h"

/* XRGB has no transparency; some framebuffer planes still read its X byte as alpha. */
void ui_display_require_opaque_xrgb(lv_display_t *display);

#endif
