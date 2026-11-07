/*
 * ui_assets.c - UI 资源访问实现
 *
 * 路径拼接结果放在静态缓冲区，是为了避免每帧 malloc。
 * 同一帧内需要同时使用两条路径时，调用方需自行复制一条。
 */
#include "ui/ui_assets.h"

#include <stdio.h>
// #include <string.h>

#include "ui/ui_common.h"

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

/*
 * 文件格式为 lv_image_header_t 头 + 像素数据。
 * LV_BIN_DECODER_RAM_LOAD=0 时 LVGL 是逐行流式解码，
 * 变换绘制拿不到整图，因此这里把数据整体读入并交给 lv_image_dsc_t。
 */
bool ui_assets_load_image(lv_image_dsc_t *dsc, const char *rel)
{
    lv_fs_file_t f;
    uint32_t size = 0;
    uint32_t rn = 0;
    uint32_t data_size;
    uint8_t *data;
    const char *path;

    if(dsc == NULL) {
        return false;
    }

    /* 已加载过（跨样式切换复用），直接返回 */
    if(dsc->data != NULL) {
        return true;
    }

    path = ui_assets_path(rel);
    if(path == NULL) {
        return false;
    }

    if(lv_fs_open(&f, path, LV_FS_MODE_RD) != LV_FS_RES_OK) {
        return false;
    }

    if(lv_fs_seek(&f, 0, LV_FS_SEEK_END) != LV_FS_RES_OK ||
       lv_fs_tell(&f, &size) != LV_FS_RES_OK ||
       size <= sizeof(lv_image_header_t)) {
        lv_fs_close(&f);
        return false;
    }

    if(lv_fs_seek(&f, 0, LV_FS_SEEK_SET) != LV_FS_RES_OK) {
        lv_fs_close(&f);
        return false;
    }

    lv_fs_read(&f, &dsc->header, sizeof(lv_image_header_t), &rn);

    data_size = size - (uint32_t)sizeof(lv_image_header_t);
    data = lv_malloc(data_size);

    if(data == NULL) {
        lv_fs_close(&f);
        return false;
    }

    lv_fs_read(&f, data, data_size, &rn);
    lv_fs_close(&f);

    if(rn != data_size) {
        lv_free(data);
        return false;
    }

    dsc->data_size = data_size;
    dsc->data = data;

    return true;
}