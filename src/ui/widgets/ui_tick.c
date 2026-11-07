/*
 * ui_tick.c - 竖向刻度条控件实现
 *
 * 裁剪窗移动 dy 时，窗内图片反向移动 -dy，
 * 使得图片在父坐标系中的位置保持不变，只有可见区域在变。
 * 这正是 Qt 侧 sourceClipRect 的效果。
 */
#include "ui/widgets/ui_tick.h"
#include "ui/ui_assets.h"

bool ui_tick_create(ui_tick_t *tick, lv_obj_t *parent, const char *src,
                    int32_t x, int32_t y_bottom, int32_t y_top,
                    int32_t w, int32_t h)
{
    if(tick == NULL || src == NULL) {
        return false;
    }

    tick->x = x;
    tick->y_bottom = y_bottom;
    tick->y_top = y_top;
    tick->percent = -1;

    tick->clip = lv_obj_create(parent);
    lv_obj_remove_style_all(tick->clip);
    lv_obj_remove_flag(tick->clip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(tick->clip, w, h);
    lv_obj_set_pos(tick->clip, x, y_bottom);

    tick->img = lv_image_create(tick->clip);
    lv_image_set_src(tick->img, ui_assets_path(src));
    lv_obj_set_pos(tick->img, -x, -y_bottom);

    ui_tick_set_percent(tick, 0);

    return true;
}

void ui_tick_set_percent(ui_tick_t *tick, int32_t percent)
{
    int32_t y;

    if(tick == NULL || tick->clip == NULL) {
        return;
    }

    if(percent < 0) {
        percent = 0;
    }
    if(percent > 100) {
        percent = 100;
    }

    if(percent == tick->percent) {
        return;
    }

    tick->percent = percent;

    /* 线性插值：0% 取 y_bottom，100% 取 y_top */
    y = tick->y_bottom + (tick->y_top - tick->y_bottom) * percent / 100;

    lv_obj_set_pos(tick->clip, tick->x, y);
    lv_obj_set_pos(tick->img, -tick->x, -y);
}