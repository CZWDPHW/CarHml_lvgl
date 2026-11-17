#include <stdio.h>
#include <string.h>

#include "ui/ui_display.h"

static int failures;
static unsigned flushes;
static uint8_t pixels[64 * 16 * 4];
static uint8_t expected[sizeof(pixels)];

/* Reproduce an XRGB renderer that leaves the unused byte zero or partly set. */
static void varying_x_byte(lv_event_t *event)
{
    lv_draw_buf_t *buf = lv_display_get_buf_active(lv_event_get_target(event));
    for(uint32_t y = 0; y < buf->header.h; ++y) {
        uint8_t *row = buf->data + y * buf->header.stride;
        for(uint32_t x = 0; x < buf->header.w; ++x) row[x * 4U + 3U] = (uint8_t)x;
    }
    memcpy(expected, buf->data, buf->header.stride * buf->header.h);
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *map)
{
    lv_draw_buf_t *buf = lv_display_get_buf_active(display);
    const bool opaque = lv_display_get_color_format(display) == LV_COLOR_FORMAT_XRGB8888;
    (void)area;
    ++flushes;
    for(uint32_t y = 0; y < buf->header.h; ++y) {
        const uint8_t *row = map + y * buf->header.stride;
        for(uint32_t x = 0; x < buf->header.w; ++x) {
            const uint8_t expected_alpha = opaque ? 255U : (uint8_t)x;
            const uint8_t *rgb = expected + y * buf->header.stride + 4U * x;
            if(row[4U * x + 3U] != expected_alpha ||
               memcmp(row + 4U * x, rgb, 3) != 0) ++failures;
        }
    }
    lv_display_flush_ready(display);
}

static void check_format(lv_color_format_t format, lv_display_render_mode_t mode)
{
    const unsigned before = flushes;
    lv_display_t *display = lv_display_create(64, 16);
    lv_display_set_color_format(display, format);
    /* Force multiple flushes in partial mode, including narrow invalidations. */
    uint32_t size = mode == LV_DISPLAY_RENDER_MODE_PARTIAL ? 64U * 3U * 4U : sizeof(pixels);
    lv_display_set_buffers(display, pixels, NULL, size, mode);
    lv_display_set_flush_cb(display, flush);
    lv_display_add_event_cb(display, varying_x_byte, LV_EVENT_FLUSH_START, NULL);
    ui_display_require_opaque_xrgb(display);
    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x123456), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_refr_now(display);
    lv_area_t narrow = {3, 1, 11, 9};
    lv_obj_invalidate_area(screen, &narrow);
    lv_refr_now(display);
    if(flushes == before) ++failures;
    lv_display_delete(display);
}

int main(void)
{
    lv_init();
    check_format(LV_COLOR_FORMAT_XRGB8888, LV_DISPLAY_RENDER_MODE_PARTIAL);
    check_format(LV_COLOR_FORMAT_XRGB8888, LV_DISPLAY_RENDER_MODE_DIRECT);
    check_format(LV_COLOR_FORMAT_XRGB8888, LV_DISPLAY_RENDER_MODE_FULL);
    check_format(LV_COLOR_FORMAT_ARGB8888, LV_DISPLAY_RENDER_MODE_PARTIAL);
    printf("display: %u flushes, %d failures\n", flushes, failures);
    return failures ? 1 : 0;
}
