/*
 * ov_radar.c - 覆盖层：倒车/泊车雷达（对应 CentralDisplay/PageRadar.qml）
 *
 * 所有弧线贴图（8 个传感器 × 绿/黄/红）已按 Qt 的父子旋转/位移链用 PIL 预渲染，
 * 每个精灵裁到自身包围盒，运行时只做定位与显隐，零旋转零缩放。
 *
 * Qt 图层顺序：背景 → 前雷达波 → 前弧 → 小车 → 后雷达波 → 后弧。
 */
#include "ui/overlays/ov_radar.h"

#include "ui/ui_common.h"
#include "ui/ui_theme.h"

#define RADAR_X 815
#define RADAR_Y 160
#define RADAR_W 290
#define RADAR_H 430

#define WAVE_W  191
#define WAVE_H  90
#define WAVE_FRONT_X 50
#define WAVE_FRONT_Y 10
#define WAVE_REAR_X  50
#define WAVE_REAR_Y  330

#define CAR_W 114
#define CAR_H 246
#define CAR_X ((RADAR_W - CAR_W) / 2)
#define CAR_Y ((RADAR_H - CAR_H) / 2)

#define CODE_HIDDEN 0U
#define CODE_G      1U
#define CODE_Y      2U
#define CODE_R      3U

typedef struct {
    int16_t x;
    int16_t y;
} arc_pos_t;

/* 预渲染精灵在 290×430 页面内的左上角坐标（PIL 计算后写入） */
static const arc_pos_t s_front_pos[4][3] = {
    { { 50, 28 }, { 68, 45 }, { 86, 61 } },
    { { 88,  9 }, { 100, 30 }, { 110, 50 } },
    { { 142, 10 }, { 142, 30 }, { 144, 50 } },
    { { 189, 25 }, { 180, 43 }, { 169, 60 } },
};

static const arc_pos_t s_rear_pos[4][3] = {
    { { 50, 353 }, { 67, 343 }, { 84, 332 } },
    { { 89, 386 }, { 99, 368 }, { 109, 349 } },
    { { 144, 385 }, { 144, 368 }, { 143, 349 } },
    { { 190, 350 }, { 179, 341 }, { 168, 331 } },
};

static const char *const s_front_path[4][3] = {
    { UI_IMG("Radar/front_0_g.png"), UI_IMG("Radar/front_0_y.png"),
      UI_IMG("Radar/front_0_r.png") },
    { UI_IMG("Radar/front_1_g.png"), UI_IMG("Radar/front_1_y.png"),
      UI_IMG("Radar/front_1_r.png") },
    { UI_IMG("Radar/front_2_g.png"), UI_IMG("Radar/front_2_y.png"),
      UI_IMG("Radar/front_2_r.png") },
    { UI_IMG("Radar/front_3_g.png"), UI_IMG("Radar/front_3_y.png"),
      UI_IMG("Radar/front_3_r.png") },
};

static const char *const s_rear_path[4][3] = {
    { UI_IMG("Radar/rear_0_g.png"), UI_IMG("Radar/rear_0_y.png"),
      UI_IMG("Radar/rear_0_r.png") },
    { UI_IMG("Radar/rear_1_g.png"), UI_IMG("Radar/rear_1_y.png"),
      UI_IMG("Radar/rear_1_r.png") },
    { UI_IMG("Radar/rear_2_g.png"), UI_IMG("Radar/rear_2_y.png"),
      UI_IMG("Radar/rear_2_r.png") },
    { UI_IMG("Radar/rear_3_g.png"), UI_IMG("Radar/rear_3_y.png"),
      UI_IMG("Radar/rear_3_r.png") },
};

static lv_obj_t *radar_image(lv_obj_t *parent, const char *path,
                             int32_t x, int32_t y)
{
    lv_obj_t *img = lv_image_create(parent);

    lv_image_set_src(img, path);
    lv_obj_set_pos(img, x, y);
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);

    return img;
}

bool ov_radar_create(ov_radar_t *ov, lv_obj_t *parent)
{
    lv_obj_t *bg;
    lv_obj_t *car;
    uint8_t i;
    uint8_t c;

    if(ov == NULL) {
        return false;
    }

    ov->root = ui_blank_container(parent);
    lv_obj_set_size(ov->root, RADAR_W, RADAR_H);
    lv_obj_set_pos(ov->root, RADAR_X, RADAR_Y);

    bg = lv_image_create(ov->root);
    lv_image_set_src(bg, UI_IMG("Radar/radar_bg.png"));
    lv_obj_set_pos(bg, 0, 0);

    /* 前雷达波 + 前弧 */
    ov->wave_front = radar_image(ov->root, UI_IMG("Radar/wave_front.png"),
                                 WAVE_FRONT_X, WAVE_FRONT_Y);
    for(i = 0U; i < 4U; ++i) {
        for(c = 0U; c < 3U; ++c) {
            ov->arc_front[i][c] = radar_image(ov->root, s_front_path[i][c],
                                              s_front_pos[i][c].x,
                                              s_front_pos[i][c].y);
        }
    }

    /* 小车 */
    car = lv_image_create(ov->root);
    lv_image_set_src(car, UI_IMG("Radar/car.png"));
    lv_obj_set_pos(car, CAR_X, CAR_Y);

    /* 后雷达波 + 后弧（在 Qt 中后弧叠在小车之上） */
    ov->wave_rear = radar_image(ov->root, UI_IMG("Radar/wave_rear.png"),
                                WAVE_REAR_X, WAVE_REAR_Y);
    for(i = 0U; i < 4U; ++i) {
        for(c = 0U; c < 3U; ++c) {
            ov->arc_rear[i][c] = radar_image(ov->root, s_rear_path[i][c],
                                             s_rear_pos[i][c].x,
                                             s_rear_pos[i][c].y);
        }
    }

    lv_obj_add_flag(ov->root, LV_OBJ_FLAG_HIDDEN);

    ov->active = 0U;
    ov->last_sig = 0U;

    return true;
}

static uint8_t point_code(const car_radar_point_t *pt)
{
    /* value 0x3F 表示该传感器无目标；color 0/1/2 → 绿/黄/红 */
    if(pt->value == 0x3FU) {
        return CODE_HIDDEN;
    }

    switch(pt->color) {
    case 0U:
        return CODE_G;
    case 1U:
        return CODE_Y;
    case 2U:
        return CODE_R;
    default:
        return CODE_HIDDEN;
    }
}

/* 0 无方向 / 1 前 / 2 后 */
static uint8_t gear_dir(const car_state_t *state)
{
    char g;

    if(!state->gear.valid) {
        return 0U;
    }

    g = state->gear.text[0];
    if(g >= 'a' && g <= 'z') {
        g = (char)(g - 'a' + 'A');
    }

    if(g == 'D' || g == 'M') {
        return 1U;
    }
    if(g == 'R') {
        return 2U;
    }
    return 0U;
}

void ov_radar_apply(ov_radar_t *ov, const car_state_t *state, bool suppressed)
{
    uint8_t codes_front[4] = { CODE_HIDDEN, CODE_HIDDEN, CODE_HIDDEN, CODE_HIDDEN };
    uint8_t codes_rear[4] = { CODE_HIDDEN, CODE_HIDDEN, CODE_HIDDEN, CODE_HIDDEN };
    uint8_t display;
    uint8_t dir = 0U;
    uint32_t sig = 0U;
    uint8_t i;
    uint8_t c;

    if(ov == NULL || state == NULL) {
        return;
    }

    /* Qt main.qml：互联会话中（carPlayModel）或雷达关闭（!radaronoff）都不显示雷达 */
    display = (state->radar.valid && state->radar.display_on &&
               state->radar_on && !state->carplay_active &&
               !suppressed) ? 1U : 0U;

    if(display != 0U) {
        dir = gear_dir(state);

        if(dir == 1U) {
            for(i = 0U; i < 4U; ++i) {
                codes_front[i] = point_code(&state->radar.front[i]);
            }
        }
        else if(dir == 2U) {
            for(i = 0U; i < 4U; ++i) {
                codes_rear[i] = point_code(&state->radar.rear[i]);
            }
        }
    }

    /* 签名：active(1) + dir(2) + 前 4 × 2bit + 后 4 × 2bit */
    sig = display | ((uint32_t)dir << 1);
    for(i = 0U; i < 4U; ++i) {
        sig |= (uint32_t)codes_front[i] << (3U + i * 2U);
        sig |= (uint32_t)codes_rear[i] << (11U + i * 2U);
    }

    if(sig == ov->last_sig) {
        return;
    }
    ov->last_sig = sig;
    ov->active = display;

    if(display == 0U) {
        lv_obj_add_flag(ov->root, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_remove_flag(ov->root, LV_OBJ_FLAG_HIDDEN);

    if(dir == 1U) {
        lv_obj_remove_flag(ov->wave_front, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(ov->wave_front, LV_OBJ_FLAG_HIDDEN);
    }

    if(dir == 2U) {
        lv_obj_remove_flag(ov->wave_rear, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(ov->wave_rear, LV_OBJ_FLAG_HIDDEN);
    }

    for(i = 0U; i < 4U; ++i) {
        for(c = 0U; c < 3U; ++c) {
            uint8_t want = (uint8_t)(c + 1U);

            if(codes_front[i] == want) {
                lv_obj_remove_flag(ov->arc_front[i][c], LV_OBJ_FLAG_HIDDEN);
            }
            else {
                lv_obj_add_flag(ov->arc_front[i][c], LV_OBJ_FLAG_HIDDEN);
            }

            if(codes_rear[i] == want) {
                lv_obj_remove_flag(ov->arc_rear[i][c], LV_OBJ_FLAG_HIDDEN);
            }
            else {
                lv_obj_add_flag(ov->arc_rear[i][c], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

bool ov_radar_is_active(const ov_radar_t *ov)
{
    return (ov != NULL && ov->active != 0U);
}

void ov_radar_destroy(ov_radar_t *ov)
{
    if(ov != NULL && ov->root != NULL) {
        lv_obj_delete(ov->root);
        ov->root = NULL;
        ov->wave_front = NULL;
        ov->wave_rear = NULL;
        ov->active = 0U;
        ov->last_sig = 0U;
    }
}
