/*
 * ui_dial.c - 表盘控件实现
 *
 * 角度约定与 LVGL 一致：0.1° 为单位，正值顺时针。
 * 各表的 Qt 角度公式（如 -195° + speed×0.75）留在样式层换算。
 */
#include "ui/widgets/ui_dial.h"

#include "ui/ui_assets.h"
#include "ui/ui_theme.h"

#define NEEDLE_ANIM_MS 100U

static void needle_angle_cb(void *needle, int32_t angle)
{
    lv_image_set_rotation(needle, angle);
}

void ui_dial_create(ui_dial_t *dial, lv_obj_t *parent, int32_t x, int32_t y,
                    int32_t size, const char *bg, const lv_image_dsc_t *needle,
                    int32_t needle_x, int32_t needle_y,
                    int32_t pivot_x, int32_t pivot_y, int32_t angle)
{
    lv_obj_t *img;

    if(dial == NULL) {
        return;
    }

    dial->size = size;
    dial->target_angle = angle;
    dial->root = ui_blank_container(parent);
    lv_obj_set_size(dial->root, size, size);
    lv_obj_set_pos(dial->root, x, y);

    /* 底图：普通流式绘制，不需要整图驻留 */
    img = lv_image_create(dial->root);
    lv_image_set_src(img, ui_assets_path(bg));
    lv_obj_set_pos(img, 0, 0);

    /* 指针：轴心设在表盘中心，绕中心旋转。
     * 指针图已按笔尖真实范围裁剪，因此摆放坐标和轴心由调用方按裁剪偏移给出。 */
    dial->needle = lv_image_create(dial->root);
    lv_image_set_src(dial->needle, needle);
    lv_obj_set_pos(dial->needle, needle_x, needle_y);
    lv_image_set_pivot(dial->needle, pivot_x, pivot_y);
    lv_image_set_rotation(dial->needle, angle);
}

void ui_dial_set_angle(ui_dial_t *dial, int32_t angle)
{
    lv_anim_t anim;
    lv_anim_t *running;
    int32_t current;

    if(dial == NULL || dial->needle == NULL || dial->target_angle == angle) {
        return;
    }

    running = lv_anim_get(dial->needle, needle_angle_cb);
    /* 保留有符号角度，不能用 image 的 0..3599 回读值跨零点插值。 */
    current = running != NULL ? running->current_value : dial->target_angle;
    dial->target_angle = angle;
    lv_anim_delete(dial->needle, needle_angle_cb);
    if(current == angle) {
        return;
    }

    lv_anim_init(&anim);
    /* 绑定图片而非调用方结构；LVGL 对象析构会自动删除该动画。 */
    lv_anim_set_var(&anim, dial->needle);
    lv_anim_set_exec_cb(&anim, needle_angle_cb);
    lv_anim_set_values(&anim, current, angle);
    lv_anim_set_duration(&anim, NEEDLE_ANIM_MS);
    lv_anim_set_path_cb(&anim, lv_anim_path_linear);
    lv_anim_start(&anim);
}

void ui_dial_add_labels(ui_dial_t *dial, const ui_dial_label_t *items,
                        size_t count, const lv_font_t *font,
                        lv_obj_t **out, size_t out_count)
{
    if(dial == NULL || items == NULL) {
        return;
    }

    for(size_t i = 0; i < count; i++) {
        lv_obj_t *label = ui_dial_add_text(dial, items[i].text, items[i].x,
                                           items[i].y, font);

        /* 需要句柄的调用方按顺序取走，超出容量的部分忽略 */
        if(out != NULL && i < out_count) {
            out[i] = label;
        }
    }
}

lv_obj_t *ui_dial_add_text(ui_dial_t *dial, const char *text, int32_t x,
                           int32_t y, const lv_font_t *font)
{
    lv_obj_t *label;

    if(dial == NULL || dial->root == NULL) {
        return NULL;
    }

    label = lv_label_create(dial->root);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(UI_COLOR_WHITE), 0);
    lv_obj_set_pos(label, x, y);

    return label;
}
