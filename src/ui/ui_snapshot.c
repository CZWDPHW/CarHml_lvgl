/*
 * ui_snapshot.c - PC 截图实现（仅 CARMW_PLATFORM_PC）
 *
 * 前提：PC 分支使用 lv_sdl_window_create + LV_DISPLAY_RENDER_MODE_DIRECT，
 * 显示缓冲本身就是整屏 ARGB8888。因此截图只需：
 *   1. lv_timer_handler 刷新一帧（由调用方保证）；
 *   2. lv_display_get_buf_active() 取当前整屏缓冲；
 *   3. ui_png_write 写出 PNG。
 * 不改动 lv_conf、不影响板端（本文件只在 PC 编译）。
 */
#include "ui/ui_snapshot.h"
#include "ui/ui_png.h"

#if defined(CARMW_PLATFORM_PC)

#include <stdio.h>

bool ui_snapshot_save(lv_obj_t *obj, const char *path)
{
    lv_display_t *disp;
    lv_draw_buf_t *buf;
    int32_t w;
    int32_t h;

    (void)obj;

    disp = lv_display_get_default();
    if(disp == NULL || path == NULL) {
        return false;
    }

    buf = lv_display_get_buf_active(disp);
    if(buf == NULL || buf->data == NULL) {
        fprintf(stderr, "snapshot: no active draw buf\n");
        return false;
    }

    w = buf->header.w;
    h = buf->header.h;
    return ui_png_write(path, (const uint32_t *)buf->data, w, h);
}

#else /* !CARMW_PLATFORM_PC */

bool ui_snapshot_save(lv_obj_t *obj, const char *path)
{
    (void)obj;
    (void)path;
    return false;
}

#endif /* CARMW_PLATFORM_PC */
