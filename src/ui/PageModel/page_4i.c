/*
 * page_4i.c - 中心卡片：4x4-i 驱动力分配（对应 Page4I.qml）
 *
 * Qt 坐标链（root 400×450，居中于 814×509 的 StackLayout）：
 *   标题 "4x4-i" 28 号加粗，水平居中，topMargin 20
 *   内容区：左右各 20、底 20，top = 标题底 + 20 → 360×356
 *   左侧 carContainer 140×0.85H，垂直居中；车体 inset 10、圆角 35、边框 1.5
 *   右侧 chartContainer 距左侧 15：中间刻度 0/50/100%，上=前轮条、下=后轮条
 */
#include "ui/PageModel/page_4i.h"

#include "ui/ui_common.h"
#include "ui/ui_theme.h"

#define CARD_X   207      /* (814-400)/2 */
#define CARD_Y   29       /* (509-450)/2 */
#define CARD_W   400
#define CARD_H   450

#define CONTENT_X   20
#define CONTENT_Y   74    /* 20 + 标题行高 34 + 20 */
#define CONTENT_W   360
#define CONTENT_H   356

#define CAR_W       140
#define CAR_H       302   /* 0.85 × 356 */
#define CAR_Y       27    /* (356-302)/2 */

#define CHART_X     155   /* 140 + 15 */
#define CHART_W     185
#define SCALE_Y     158   /* (356-40)/2 */
#define SCALE_H     40

#define BAR_H       30
#define BAR_INSET   2
#define BLOCK_H     45

/* 颜色与尺寸，全部取自 Qt */
#define COL_LINE      0x66A3FFU
#define COL_BAR_BORD  0x7DA7D9U
#define COL_BAR_TOP   0xA1CAFFU
#define COL_BAR_BOT   0x4372B6U
#define COL_WHEEL     0x1A2438U

/* 纯色矩形 */
static lv_obj_t *rect(lv_obj_t *parent, int32_t x, int32_t y,
                      int32_t w, int32_t h, uint32_t color, lv_opa_t opa)
{
    lv_obj_t *obj = ui_blank_container(parent);

    lv_obj_set_size(obj, w, h);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, opa, 0);

    return obj;
}

/* 空心描边矩形（Qt 的 Rectangle{border}） */
static lv_obj_t *outline(lv_obj_t *parent, int32_t x, int32_t y,
                         int32_t w, int32_t h, int32_t radius,
                         uint32_t color, int32_t border, lv_opa_t opa)
{
    lv_obj_t *obj = rect(parent, x, y, w, h, color, LV_OPA_TRANSP);

    lv_obj_set_style_border_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(obj, border, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_opa(obj, opa, 0);

    return obj;
}

/* 文字标签：box 内按 align 对齐 */
static lv_obj_t *label(lv_obj_t *parent, int32_t x, int32_t y, int32_t w,
                       const char *text, const lv_font_t *font,
                       lv_text_align_t align)
{
    lv_obj_t *obj = lv_label_create(parent);

    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, w);
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_white(), 0);
    lv_obj_set_style_text_align(obj, align, 0);

    return obj;
}

/* 车辆俯视示意：外廓 + 前脸弧线 + 传动轴 + 四个车轮 */
static void car_create(lv_obj_t *content)
{
    lv_obj_t *car = ui_blank_container(content);
    lv_obj_t *body;
    lv_obj_t *wheel;
    lv_obj_t *glow;

    lv_obj_set_size(car, CAR_W, CAR_H);
    lv_obj_set_pos(car, 0, CAR_Y);

    /* 外廓：inset 10、圆角 35、边框 1.5、整体 40% 透明 */
    body = outline(car, 10, 10, CAR_W - 20, CAR_H - 20, 35,
                   COL_LINE, 2, LV_OPA_40);

    /* 模拟车头圆弧的内部线条 */
    (void)outline(body, 10, 10, CAR_W - 40, 40, 20, COL_LINE, 1, LV_OPA_COVER);

    /* 传动轴三根线：前轴 / 后轴 / 纵轴（2px，50% 透明） */
    (void)rect(car, 20, 60, 100, 2, COL_LINE, LV_OPA_50);
    (void)rect(car, 25, CAR_H - 62, 90, 2, COL_LINE, LV_OPA_50);
    (void)rect(car, 69, 60, 2, CAR_H - 120, COL_LINE, LV_OPA_50);

    /* 四个车轮 24×45：深色底 + 白色边框 + 外圈发光 */
    {
        static const int32_t s_wheel_pos[4][2] = {
            {2, 38}, {114, 38}, {8, CAR_H - 83}, {108, CAR_H - 83},
        };
        uint8_t i;

        for(i = 0U; i < 4U; ++i) {
            wheel = rect(car, s_wheel_pos[i][0], s_wheel_pos[i][1],
                         24, 45, COL_WHEEL, LV_OPA_COVER);
            lv_obj_set_style_radius(wheel, 6, 0);
            lv_obj_set_style_border_color(wheel, lv_color_white(), 0);
            lv_obj_set_style_border_width(wheel, 2, 0);

            glow = outline(wheel, -4, -4, 32, 53, 9, 0xFFFFFFU, 1, LV_OPA_30);
            (void)glow;
        }
    }
}

/* 扭矩条：外框 + 内部渐变填充（返回填充对象，供动态改宽度） */
static lv_obj_t *bar_create(lv_obj_t *chart, int32_t y_block, bool top_ticks)
{
    lv_obj_t *block = ui_blank_container(chart);
    lv_obj_t *fill;
    int32_t frame_y = top_ticks ? 8 : (BLOCK_H - BAR_H - 8);
    int32_t tick_y = top_ticks ? 0 : (BLOCK_H - 6);

    lv_obj_set_size(block, CHART_W, BLOCK_H);
    lv_obj_set_pos(block, 0, y_block);

    /* 刻度短线：左 / 中 / 右各 2×6 */
    (void)rect(block, 0, tick_y, 2, 6, 0xFFFFFFU, LV_OPA_COVER);
    (void)rect(block, (CHART_W - 2) / 2, tick_y, 2, 6, 0xFFFFFFU, LV_OPA_COVER);
    (void)rect(block, CHART_W - 2, tick_y, 2, 6, 0xFFFFFFU, LV_OPA_COVER);

    /* 外框 */
    {
        lv_obj_t *frame = outline(block, 0, frame_y, CHART_W, BAR_H, 0,
                                  COL_BAR_BORD, 2, LV_OPA_COVER);

        /* 渐变能量条：#a1caff → #4372b6，横向 */
        fill = rect(frame, BAR_INSET, BAR_INSET,
                    BAR_INSET, BAR_H - 2 * BAR_INSET, COL_BAR_TOP, LV_OPA_COVER);
        lv_obj_set_style_bg_grad_color(fill, lv_color_hex(COL_BAR_BOT), 0);
        lv_obj_set_style_bg_grad_dir(fill, LV_GRAD_DIR_HOR, 0);
    }

    return fill;
}

/* 右侧：中间刻度文字 0 / 50 / 100 %，上下各一条扭矩条 */
static void chart_create(page4i_card_t *card, lv_obj_t *content)
{
    lv_obj_t *chart = ui_blank_container(content);
    lv_obj_t *scale;

    lv_obj_set_size(chart, CHART_W, CONTENT_H);
    lv_obj_set_pos(chart, CHART_X, 0);

    scale = ui_blank_container(chart);
    lv_obj_set_size(scale, CHART_W, SCALE_H);
    lv_obj_set_pos(scale, 0, SCALE_Y);

    (void)label(scale, 0, 0, 60, "0", &lv_font_montserrat_22, LV_TEXT_ALIGN_LEFT);
    (void)label(scale, 0, 0, CHART_W, "50", &lv_font_montserrat_22, LV_TEXT_ALIGN_CENTER);
    (void)label(scale, CHART_W - 70, 0, 70, "100 %", &lv_font_montserrat_22, LV_TEXT_ALIGN_RIGHT);

    card->fill_front = bar_create(chart, SCALE_Y - 10 - BLOCK_H, true);
    card->fill_rear = bar_create(chart, SCALE_Y + SCALE_H + 10, false);
}

bool page4i_card_create(page4i_card_t *card, lv_obj_t *parent)
{
    lv_obj_t *content;

    if(card == NULL) {
        return false;
    }

    card->root = ui_blank_container(parent);
    lv_obj_add_flag(card->root, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(card->root, CARD_W, CARD_H);
    lv_obj_set_pos(card->root, CARD_X, CARD_Y);

    /* 标题 "4x4-i"：28 号，水平居中，距顶 20 */
    (void)label(card->root, 0, 20, CARD_W, "4x4-i",
                &lv_font_montserrat_28, LV_TEXT_ALIGN_CENTER);

    content = ui_blank_container(card->root);
    lv_obj_set_size(content, CONTENT_W, CONTENT_H);
    lv_obj_set_pos(content, CONTENT_X, CONTENT_Y);

    car_create(content);
    chart_create(card, content);

    card->front_pct = 0xFFU;
    card->rear_pct = 0xFFU;

    return true;
}

void page4i_card_set_torque(page4i_card_t *card, uint8_t front_pct, uint8_t rear_pct)
{
    int32_t inner_w = CHART_W - 2 * BAR_INSET;

    if(card == NULL || card->fill_front == NULL || card->fill_rear == NULL) {
        return;
    }

    if(front_pct > 100U) {
        front_pct = 100U;
    }
    if(rear_pct > 100U) {
        rear_pct = 100U;
    }

    if(front_pct == card->front_pct && rear_pct == card->rear_pct) {
        return;
    }

    card->front_pct = front_pct;
    card->rear_pct = rear_pct;

    lv_obj_set_width(card->fill_front, (int32_t)(inner_w * front_pct) / 100);
    lv_obj_set_width(card->fill_rear, (int32_t)(inner_w * rear_pct) / 100);
}

void page4i_card_destroy(page4i_card_t *card)
{
    if(card != NULL && card->root != NULL) {
        lv_obj_delete(card->root);
        card->root = NULL;
    }
}