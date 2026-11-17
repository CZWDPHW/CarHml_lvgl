#include "ui/ui_display.h"

static void opaque_xrgb_flush(lv_event_t *event)
{
    lv_display_t *display = lv_event_get_target(event);
    if(lv_display_get_color_format(display) != LV_COLOR_FORMAT_XRGB8888) return;

    lv_draw_buf_t *buf = lv_display_get_buf_active(display);
    if(buf == NULL || buf->data == NULL) return;

    for(uint32_t y = 0; y < buf->header.h; ++y) {
        uint8_t *row = buf->data + y * buf->header.stride;
        for(uint32_t x = 0; x < buf->header.w; ++x) row[x * 4U + 3U] = 255U;
    }
    lv_draw_buf_flush_cache(buf, NULL);
}

/**
 * @brief 要求显示设备为透明 XRGB 格式
 * @param display 显示设备
 */
void ui_display_require_opaque_xrgb(lv_display_t *display)
{
    lv_display_add_event_cb(display, opaque_xrgb_flush, LV_EVENT_FLUSH_START, NULL);
}
