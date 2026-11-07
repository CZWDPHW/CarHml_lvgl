/*
 * ui_assets.c - UI 资源访问实现
 *
 * 路径拼接结果放在静态缓冲区，是为了避免每帧 malloc。
 * 同一帧内需要同时使用两条路径时，调用方需自行复制一条。
 */
#include "ui/ui_assets.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"

#include "ui/ui_common.h"
#include "../../third_party/lvgl/src/draw/lv_image_decoder_private.h"

/* 路径缓冲区长度：资源根 + LVGL 相对逻辑路径 */
#define ASSET_PATH_MAX 160

static char s_path[ASSET_PATH_MAX];

const char *ui_assets_root(void)
{
    return UI_ASSET_ROOT;
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

    (void)snprintf(s_path, sizeof(s_path), "%s%s", UI_ASSET_ROOT, rel);

    return s_path;
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
