/*
 * ui_assets.c - UI 资源访问实现
 *
 * 路径拼接结果放在静态缓冲区，是为了避免每帧 malloc。
 * 同一帧内需要同时使用两条路径时，调用方需自行复制一条。
 */
#include "ui/ui_assets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"

#include "ui/ui_common.h"
#include "../../third_party/lvgl/src/draw/lv_image_decoder_private.h"

/* 路径缓冲区长度：资源根 + LVGL 相对逻辑路径 */
#define ASSET_PATH_MAX 160

static char s_path[ASSET_PATH_MAX];
/* 资源根：缺省为编译期 UI_ASSET_ROOT，始终是 LVGL 逻辑路径（A:/...） */
static char s_root[ASSET_PATH_MAX] = UI_ASSET_ROOT;

const char *ui_assets_root(void)
{
    return s_root;
}

const char *ui_assets_path(const char *rel)
{
    if(rel == NULL) {
        return NULL;
    }

    /* 相对路径允许带或不带开头斜杠 */
    if(rel[0] == '/') {
        rel++;
    }

    if(strlen(s_root) + strlen(rel) + 1U > sizeof(s_path)) {
        return NULL;
    }

    (void)snprintf(s_path, sizeof(s_path), "%s%s", s_root, rel);

    return s_path;
}

typedef struct {
    char *source;
    lv_image_dsc_t image;
} mirror_image_t;

static void mirror_image_delete(lv_event_t *event)
{
    mirror_image_t *cache = lv_event_get_user_data(event);
    lv_image_cache_drop(&cache->image);
    lv_free((void *)cache->image.data);
    lv_free(cache->source);
    lv_free(cache);
}

bool ui_assets_mirror_image(lv_obj_t *image, bool mirrored,
                            int32_t crop_x, int32_t crop_y,
                            int32_t width, int32_t height)
{
    mirror_image_t *cache = NULL;
    for(uint32_t i = 0; i < lv_obj_get_event_count(image); ++i) {
        lv_event_dsc_t *event = lv_obj_get_event_dsc(image, i);
        if(lv_event_dsc_get_cb(event) == mirror_image_delete) {
            cache = lv_event_dsc_get_user_data(event);
            break;
        }
    }
    if(!mirrored) {
        if(cache != NULL) {
            lv_image_set_src(image, cache->source);
            lv_image_cache_drop(&cache->image);
            lv_free((void *)cache->image.data);
            cache->image.data = NULL;
        }
        lv_obj_set_pos(image, -crop_x, -crop_y);
        return true;
    }
    if(cache == NULL || cache->image.data == NULL) {
        bool new_cache = cache == NULL;
        const char *source = lv_image_get_src(image);
        lv_image_decoder_dsc_t decoder = {0};
        if(lv_image_src_get_type(source) != LV_IMAGE_SRC_FILE ||
           lv_image_decoder_open(&decoder, source, NULL) != LV_RESULT_OK) return false;
        /* PNG decoder yields packed RGB/ARGB pixels. Never use unsigned scale_x=-1. */
        const lv_draw_buf_t *decoded = decoder.decoded;
        if(decoded == NULL || (decoded->header.cf != LV_COLOR_FORMAT_ARGB8888 &&
                              decoded->header.cf != LV_COLOR_FORMAT_RGB888) ||
           crop_x < 0 || crop_y < 0 || width <= 0 || height <= 0 ||
           crop_x + width > (int32_t)decoded->header.w ||
           crop_y + height > (int32_t)decoded->header.h) {
            lv_image_decoder_close(&decoder);
            return false;
        }
        uint32_t pixel_size = lv_color_format_get_size(decoded->header.cf);
        uint32_t stride = (uint32_t)width * pixel_size;
        if(new_cache) cache = lv_malloc_zeroed(sizeof(*cache));
        uint8_t *pixels = lv_malloc(stride * (uint32_t)height);
        char *path = new_cache ? lv_malloc(strlen(source) + 1U) : cache->source;
        if(cache == NULL || pixels == NULL || path == NULL) {
            if(new_cache) {
                lv_free(cache);
                lv_free(path);
            }
            lv_free(pixels);
            lv_image_decoder_close(&decoder);
            return false;
        }
        if(new_cache) strcpy(path, source);
        for(int32_t y = 0; y < height; ++y) {
            for(int32_t x = 0; x < width; ++x) {
                memcpy(pixels + (uint32_t)y * stride + (uint32_t)x * pixel_size,
                       decoded->data + (uint32_t)(crop_y + y) * decoded->header.stride +
                       (uint32_t)(crop_x + width - 1 - x) * pixel_size, pixel_size);
            }
        }
        cache->source = path;
        cache->image.header = decoded->header;
        cache->image.header.magic = LV_IMAGE_HEADER_MAGIC;
        cache->image.header.flags &= LV_IMAGE_FLAGS_PREMULTIPLIED;
        cache->image.header.w = width;
        cache->image.header.h = height;
        cache->image.header.stride = stride;
        cache->image.data_size = stride * (uint32_t)height;
        cache->image.data = pixels;
        lv_image_decoder_close(&decoder);
        if(new_cache) lv_obj_add_event_cb(image, mirror_image_delete, LV_EVENT_DELETE, cache);
    }
    if(lv_image_get_src(image) != &cache->image) lv_image_set_src(image, &cache->image);
    lv_obj_set_pos(image, 0, 0);
    return true;
}

bool ui_assets_load_image(lv_image_dsc_t *dsc, const char *rel)
{
    lv_image_decoder_dsc_t decoder = {0};
    uint8_t *data;
    lv_image_header_t header;
    uint32_t data_size;
    const char *path;

    if(dsc == NULL) {
        return false;
    }
    if(dsc->data != NULL) {
        return true;
    }

    path = ui_assets_path(rel);
    if(path == NULL || lv_image_decoder_open(&decoder, path, NULL) != LV_RESULT_OK) {
        return false;
    }

    header = decoder.decoded->header;
    data_size = decoder.decoded->data_size;
    data = lv_malloc(data_size);
    if(data != NULL) memcpy(data, decoder.decoded->data, data_size);
    lv_image_decoder_close(&decoder);
    if(data == NULL) return false;

    dsc->header = header;
    dsc->data_size = data_size;
    dsc->data = data;
    dsc->reserved = NULL;
    dsc->reserved_2 = NULL;
    dsc->header.flags = 0;
    dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    return true;
}
