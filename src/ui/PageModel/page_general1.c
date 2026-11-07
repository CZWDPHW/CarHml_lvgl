/*
 * page_general1.c - 占位样式（用于验证运行时样式切换）
 *
 * 后续实现 GeneralUi1 时替换本文件内容即可。
 */
#include "ui/ui_node.h"
#include "ui/ui_common.h"

typedef struct {
    ui_node_t base;
    lv_obj_t *speed_label;
    lv_obj_t *rpm_label;
} g1_page_t;

/* 创建占位页面：标题 + 车速/转速两个标签 */
static bool g1_create(ui_node_t *self, lv_obj_t *parent)
{
    g1_page_t *ui = (g1_page_t *)self;
    lv_obj_t *page = ui_page_create(parent);

    self->root = page;

    lv_obj_t *title = lv_label_create(page);

    lv_label_set_text(title, "GeneralUi1 (placeholder)");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_pos(title, 700, 300);

    ui->speed_label = lv_label_create(page);
    lv_obj_set_style_text_font(ui->speed_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(ui->speed_label, lv_color_white(), 0);
    lv_obj_set_pos(ui->speed_label, 700, 360);
    lv_label_set_text(ui->speed_label, "SPEED: 0 km/h");

    ui->rpm_label = lv_label_create(page);
    lv_obj_set_style_text_font(ui->rpm_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(ui->rpm_label, lv_color_white(), 0);
    lv_obj_set_pos(ui->rpm_label, 700, 400);
    lv_label_set_text(ui->rpm_label, "RPM: 0");

    return true;
}

/* 销毁整页对象树（子标签随父对象一起释放） */
static void g1_destroy(ui_node_t *self)
{
    if(self->root != NULL) {
        lv_obj_delete(self->root);
        self->root = NULL;
    }
}

/* 把车速/转速写进标签；尚无车况数据时保持原样 */
static void g1_apply(ui_node_t *self, const car_state_view_t *view)
{
    g1_page_t *ui = (g1_page_t *)self;

    if(!view->has_data) {
        return;
    }

    lv_label_set_text_fmt(ui->speed_label, "SPEED: %u km/h",
                          (unsigned)view->state.speed_rpm.speed_kph);
    lv_label_set_text_fmt(ui->rpm_label, "RPM: %u",
                          (unsigned)view->state.speed_rpm.engine_rpm);
}

const ui_node_vtbl_t page_general1_vtbl = {
    .size = sizeof(g1_page_t),
    .create = g1_create,
    .destroy = g1_destroy,
    .apply = g1_apply,
    .on_show = NULL,
    .on_hide = NULL,
    .on_key = NULL,
};
