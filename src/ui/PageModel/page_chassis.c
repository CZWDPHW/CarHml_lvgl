/*
 * page_chassis.c - 中心卡片：底盘控制（对应 ChassisControl.qml）
 *
 * Qt 坐标：
 *   卡片 586×506 居中于 814×509 的容器 → 容器内 (114,1)
 *   标题 "底盘控制" 38 号、y=48、水平居中（中文受限，用英文 CHASSIS）
 *   转向角文字 26 号粗体、#2ecc71、y=100、水平居中再左移 10，角度为 0 时隐藏
 *   车模：Image 1000×1000、scale 0.39、x=16、y=-80、sourceClipRect(230,170,550,760)
 *        → 可见区 215×296，屏幕位置 (184,152)
 *   轨迹 Canvas：起点 (250,238)/(348,238)，终点 y=120，
 *        控制点 y=167，横向弯曲量 = min(angle,128)/128 × 110，方向决定正负
 */
#include "ui/PageModel/page_chassis.h"

#include <stdio.h>

#include "ui/ui_assets.h"
#include "ui/ui_common.h"
#include "ui/ui_theme.h"

#define CARD_X   114
#define CARD_Y   1
#define CARD_W   586
#define CARD_H   506

#define TITLE_Y      48
#define ANGLE_Y      100
#define ANGLE_OFF_X  -10

#define CAR_X        184
#define CAR_Y        152
#define CAR_W        215
#define CAR_H        296

#define TRAJ_START_Y   238
#define TRAJ_END_Y     120
#define TRAJ_CP_Y      167
#define TRAJ_LEFT_X    250
#define TRAJ_RIGHT_X   348
#define TRAJ_MAX_CURVE 110
#define TRAJ_MAX_ANGLE 128
#define TRAJ_WIDTH     5
#define COL_TRAJ       0x2ECC71U

static lv_obj_t *card_image(lv_obj_t *parent, const char *path,
                            int32_t x, int32_t y)
{
    lv_obj_t *img = lv_image_create(parent);

    lv_image_set_src(img, path);
    lv_obj_set_pos(img, x, y);
    return img;
}

/* 只在可见性变化时改标志，避免每帧重复写 */
static void set_shown(lv_obj_t *obj, uint8_t *last, uint8_t show)
{
    if(show != *last) {
        *last = show;

        if(show != 0U) {
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/* 按二次贝塞尔采样，写入左右各一组点（点坐标为卡片内绝对坐标） */
static void traj_rebuild(chassis_card_t *card, uint8_t angle, uint8_t right)
{
    int32_t a = (angle > TRAJ_MAX_ANGLE) ? TRAJ_MAX_ANGLE : (int32_t)angle;
    int32_t curve = (a * TRAJ_MAX_CURVE) / TRAJ_MAX_ANGLE;
    int32_t den = CHASSIS_TRAJ_POINTS - 1;
    int32_t i;

    if(right == 0U) {
        curve = -curve;
    }

    for(i = 0; i < CHASSIS_TRAJ_POINTS; ++i) {
        int32_t u = den - i;
        /* y = (238·u² + 2·167 u·i + 120·i²) / den² */
        int32_t y = (TRAJ_START_Y * u * u + 2 * TRAJ_CP_Y * u * i +
                     TRAJ_END_Y * i * i) / (den * den);

        card->pts_left[i].x = TRAJ_LEFT_X + (curve * i * i) / (den * den);
        card->pts_left[i].y = y;
        card->pts_right[i].x = TRAJ_RIGHT_X + (curve * i * i) / (den * den);
        card->pts_right[i].y = y;
    }

    lv_line_set_points(card->traj_left, card->pts_left, CHASSIS_TRAJ_POINTS);
    lv_line_set_points(card->traj_right, card->pts_right, CHASSIS_TRAJ_POINTS);
}

static lv_obj_t *traj_create(lv_obj_t *parent)
{
    lv_obj_t *line = lv_line_create(parent);

    lv_obj_set_size(line, CARD_W, CARD_H);
    lv_obj_set_pos(line, 0, 0);
    lv_obj_set_style_line_width(line, TRAJ_WIDTH, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(COL_TRAJ), 0);
    lv_obj_set_style_line_opa(line, LV_OPA_90, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);

    return line;
}

bool chassis_card_create(chassis_card_t *card, lv_obj_t *parent)
{
    lv_obj_t *title;
    uint8_t i;

    if(card == NULL) {
        return false;
    }

    card->root = ui_blank_container(parent);
    lv_obj_add_flag(card->root, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(card->root, CARD_W, CARD_H);
    lv_obj_set_pos(card->root, CARD_X, CARD_Y);

    /* 标题：Qt 38 号，LVGL 只有 36，中文受限用英文 */
    title = lv_label_create(card->root);
    lv_label_set_text(title, "CHASSIS");
    lv_obj_set_width(title, CARD_W);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 0, TITLE_Y);

    /* 转向角文字：绿字、居中左移 10、角度 0 时隐藏 */
    card->angle_label = lv_label_create(card->root);
    lv_label_set_text(card->angle_label, "");
    lv_obj_set_width(card->angle_label, CARD_W);
    lv_obj_set_style_text_font(card->angle_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(card->angle_label, lv_color_hex(COL_TRAJ), 0);
    lv_obj_set_style_text_align(card->angle_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(card->angle_label, ANGLE_OFF_X, ANGLE_Y);
    lv_obj_add_flag(card->angle_label, LV_OBJ_FLAG_HIDDEN);

    /* 图层顺序与 Qt 一致：车模 → 升降背景 → 四轮驱动指示 */
    lv_obj_t *img_car = card_image(card->root, UI_IMG("OffRoad/off_road_car.png"),
                                   CAR_X, CAR_Y);
    (void)img_car;

    card->img_bg = card_image(card->root, UI_IMG("OffRoad/drive_bg_blue.png"),
                              CAR_X, CAR_Y);

    card->img_wheel[0] = card_image(card->root, UI_IMG("OffRoad/2h_z.png"),
                                    CAR_X, CAR_Y);
    card->img_wheel[1] = card_image(card->root, UI_IMG("OffRoad/2h_y.png"),
                                    CAR_X, CAR_Y);
    card->img_wheel[2] = card_image(card->root, UI_IMG("OffRoad/after_2h_z.png"),
                                    CAR_X, CAR_Y);
    card->img_wheel[3] = card_image(card->root, UI_IMG("OffRoad/after_2h_y.png"),
                                    CAR_X, CAR_Y);

    card->traj_left = traj_create(card->root);
    card->traj_right = traj_create(card->root);

    card->last_lift = 0xFFU;
    card->last_show = 0xFFU;
    card->last_traj = 0xFFU;
    card->last_traj_r = 0xFFU;
    card->last_angle = 0xFFU;
    card->last_right = 0xFFU;
    for(i = 0U; i < 4U; ++i) {
        card->last_wheel[i] = 0xFFU;
    }

    return true;
}

void chassis_card_apply(chassis_card_t *card, const car_state_t *state)
{
    const car_trajectory_t *t;
    uint8_t show;

    if(card == NULL || state == NULL || !state->trajectory.valid) {
        return;
    }

    t = &state->trajectory;

    set_shown(card->img_bg, &card->last_lift, t->chassis_lift ? 1U : 0U);
    set_shown(card->img_wheel[0], &card->last_wheel[0], t->wheel_lf ? 1U : 0U);
    set_shown(card->img_wheel[1], &card->last_wheel[1], t->wheel_rf ? 1U : 0U);
    set_shown(card->img_wheel[2], &card->last_wheel[2], t->wheel_lr ? 1U : 0U);
    set_shown(card->img_wheel[3], &card->last_wheel[3], t->wheel_rr ? 1U : 0U);

    /* 角度为 0 时隐藏角度文字与轨迹（Qt: visible: wheelAngle !== 0） */
    show = (t->angle != 0U) ? 1U : 0U;
    set_shown(card->angle_label, &card->last_show, show);

    if(show != 0U) {
        if(t->angle != card->last_angle || t->right != card->last_right ||
           card->last_show != show) {
            char buf[10];

            (void)snprintf(buf, sizeof(buf), "%u°", (unsigned)t->angle);
            lv_label_set_text(card->angle_label, buf);
            traj_rebuild(card, t->angle, t->right ? 1U : 0U);
        }
    }

    set_shown(card->traj_left, &card->last_traj, show);
    set_shown(card->traj_right, &card->last_traj_r, show);

    card->last_angle = t->angle;
    card->last_right = t->right ? 1U : 0U;
}

void chassis_card_destroy(chassis_card_t *card)
{
    if(card != NULL && card->root != NULL) {
        lv_obj_delete(card->root);
        card->root = NULL;
    }
}