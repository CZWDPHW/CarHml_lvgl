/*
 * ov_cruise.c - 覆盖层：定速巡航/车道辅助（对应 CentralDisplay/PageNavigationInfo.qml）
 *
 * Qt 的 830×470 item 在页面 (440,122)，scale 0.8（绕 item 中心缩放）。
 * 每个子图还可能有自己的 scale 与 sourceClipRect，且部分素材是 1000×1000 画布、
 * 有效内容只在其中一角。这里用 PIL 把整条变换链（源裁切→子图缩放→item 0.8 缩放）
 * 全部预渲染成页面级精灵，C 侧只按固定坐标摆放。
 *
 * 图层顺序与 QML 声明顺序一致：车道 → 左盲区 → 右盲区 → 前车 → 跟车距离 1~4
 *   → 左/右偏移车道 → 左/右偏移车辆 → 跟车小车 → 居中大车。
 */
#include "ui/overlays/ov_cruise.h"

#include "ui/ui_common.h"
#include "ui/ui_theme.h"

#define OV_X 660
#define OV_Y 200
#define OV_W 600
#define OV_H 470

/* 页面坐标减去 (560,200) 后的相对坐标 */
#define POS_LANE_X        47
#define POS_LANE_Y        85
#define POS_LEFT_AREA_X   14
#define POS_LEFT_AREA_Y   147
#define POS_RIGHT_AREA_X  341
#define POS_RIGHT_AREA_Y  145
#define POS_FRONT_CAR_X   214
#define POS_FRONT_CAR_Y   66
#define POS_DIST_X        178
#define POS_MIN_CAR_X     216
#define POS_OFF_LEFT_X    184
#define POS_OFF_RIGHT_X   159
#define POS_OFF_Y         170
#define POS_MAX_CAR_X     54
#define POS_MAX_CAR_Y     115

typedef struct {
    const char *path;
    int16_t x;
    int16_t y;
} nav_sprite_t;

static const nav_sprite_t s_layers[] = {
    { UI_IMG("CruiseNav/lane.bin"),       POS_LANE_X,       POS_LANE_Y },
    { UI_IMG("CruiseNav/left_area.bin"),  POS_LEFT_AREA_X,  POS_LEFT_AREA_Y },
    { UI_IMG("CruiseNav/right_area.bin"), POS_RIGHT_AREA_X, POS_RIGHT_AREA_Y },
    { UI_IMG("CruiseNav/front_car.bin"),  POS_FRONT_CAR_X,  POS_FRONT_CAR_Y },
    { UI_IMG("CruiseNav/dist1.bin"),      POS_DIST_X,       169 },
    { UI_IMG("CruiseNav/dist2.bin"),      POS_DIST_X + 12,  156 },
    { UI_IMG("CruiseNav/dist3.bin"),      POS_DIST_X + 25,  145 },
    { UI_IMG("CruiseNav/dist4.bin"),      POS_DIST_X + 34,  135 },
    { UI_IMG("CruiseNav/lane_left.bin"),  POS_LANE_X,       POS_LANE_Y },
    { UI_IMG("CruiseNav/lane_right.bin"), POS_LANE_X,       POS_LANE_Y },
    { UI_IMG("CruiseNav/off_left.bin"),   POS_OFF_LEFT_X,   POS_OFF_Y },
    { UI_IMG("CruiseNav/off_right.bin"),  POS_OFF_RIGHT_X,  POS_OFF_Y },
    { UI_IMG("CruiseNav/min_car.bin"),    POS_MIN_CAR_X,    33 },
    { UI_IMG("CruiseNav/max_car.bin"),    POS_MAX_CAR_X,    POS_MAX_CAR_Y },
};

/* 跟车小车的 y（相对根），索引 = distanceShow 0..4；Qt Style1 分支 */
static const int16_t s_min_car_y[5] = { 33, 61, 39, 33, 17 };

static lv_obj_t *nav_image(lv_obj_t *parent, const char *path,
                           int32_t x, int32_t y)
{
    lv_obj_t *img = lv_image_create(parent);

    lv_image_set_src(img, path);
    lv_obj_set_pos(img, x, y);
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);

    return img;
}

static void set_visible(lv_obj_t *obj, uint8_t on)
{
    if(on != 0U) {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

bool ov_cruise_create(ov_cruise_t *ov, lv_obj_t *parent)
{
    if(ov == NULL) {
        return false;
    }

    ov->root = ui_blank_container(parent);
    lv_obj_set_size(ov->root, OV_W, OV_H);
    lv_obj_set_pos(ov->root, OV_X, OV_Y);

    ov->lane = nav_image(ov->root, s_layers[0].path, s_layers[0].x, s_layers[0].y);
    ov->left_area = nav_image(ov->root, s_layers[1].path, s_layers[1].x, s_layers[1].y);
    ov->right_area = nav_image(ov->root, s_layers[2].path, s_layers[2].x, s_layers[2].y);
    ov->front_car = nav_image(ov->root, s_layers[3].path, s_layers[3].x, s_layers[3].y);

    for(uint8_t i = 0U; i < 4U; ++i) {
        ov->dist[i] = nav_image(ov->root, s_layers[4U + i].path,
                                s_layers[4U + i].x, s_layers[4U + i].y);
    }

    ov->lane_left = nav_image(ov->root, s_layers[8].path, s_layers[8].x, s_layers[8].y);
    ov->lane_right = nav_image(ov->root, s_layers[9].path, s_layers[9].x, s_layers[9].y);
    ov->off_left = nav_image(ov->root, s_layers[10].path, s_layers[10].x, s_layers[10].y);
    ov->off_right = nav_image(ov->root, s_layers[11].path, s_layers[11].x, s_layers[11].y);
    ov->min_car = nav_image(ov->root, s_layers[12].path, s_layers[12].x, s_layers[12].y);
    ov->max_car = nav_image(ov->root, s_layers[13].path, s_layers[13].x, s_layers[13].y);

    lv_obj_add_flag(ov->root, LV_OBJ_FLAG_HIDDEN);

    ov->active = 0U;
    ov->last_sig = 0xFFFFFFFFU;

    return true;
}

void ov_cruise_apply(ov_cruise_t *ov, const car_state_t *state, bool suppressed)
{
    uint8_t lane_center = 0U;
    uint8_t car_state = 0U;
    uint8_t show_area = 0U;
    uint8_t show_car = 0U;
    uint8_t control_led = 0U;
    uint8_t distance_show = 0U;
    uint8_t show_front_car = 0U;
    uint8_t emergency;
    uint8_t lane_dep;
    uint8_t blind;
    uint8_t assist_on;
    uint8_t display;
    uint32_t sig;
    uint8_t i;

    if(ov == NULL || state == NULL) {
        return;
    }

    /* 读取独立保存的 0x0E 状态，不受其他高配子帧覆盖影响。 */
    if(state->high_func.lane.valid) {
        lane_center = state->high_func.lane.lane_center;
        car_state = state->high_func.lane.car_state;
        show_area = state->high_func.lane.show_area;
        show_car = state->high_func.lane.show_car;
    }

    if(state->cruise.valid) {
        control_led = (uint8_t)(state->cruise.cruise_state & 0x01U);
        distance_show = (uint8_t)(state->cruise.acc_state & 0x07U);
        show_front_car = (uint8_t)((state->cruise.acc_state & 0x08U) ? 1U : 0U);
    }

    /* 三项驾驶辅助的 MCU 真实状态（0x16 回读），不是本地偏好 */
    emergency = state->driver_assist.emergency_braking ? 1U : 0U;
    lane_dep = state->driver_assist.lane_departure_warning ? 1U : 0U;
    blind = state->driver_assist.blind_spot_warning ? 1U : 0U;
    assist_on = (emergency || lane_dep || blind) ? 1U : 0U;

    /*
     * Qt main.qml：数据条件 && (emergencyBraking || laneDepartureWarning ||
     * blindSpotWarning)，任一辅助开关关闭不会让整页显示。
     */
    display = (!suppressed && assist_on &&
               (control_led != 0U || lane_center != 0U ||
                car_state == 2U || car_state == 3U ||
                show_area != 0U || show_car != 0U)) ? 1U : 0U;

    sig = display;
    sig |= (uint32_t)car_state << 1;
    sig |= (uint32_t)show_area << 4;
    sig |= (uint32_t)show_car << 7;
    sig |= (uint32_t)lane_center << 10;
    sig |= (uint32_t)distance_show << 12;
    sig |= (uint32_t)show_front_car << 15;
    sig |= (uint32_t)emergency << 16;
    sig |= (uint32_t)lane_dep << 17;
    sig |= (uint32_t)blind << 18;

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

    set_visible(ov->lane, (car_state <= 1U) ? 1U : 0U);
    set_visible(ov->left_area,
                ((show_area == 2U || show_area == 3U) && blind) ? 1U : 0U);
    set_visible(ov->right_area,
                ((show_area == 1U || show_area == 3U) && blind) ? 1U : 0U);
    set_visible(ov->front_car, (show_car == 1U && emergency) ? 1U : 0U);

    for(i = 0U; i < 4U; ++i) {
        uint8_t on = (car_state <= 1U && distance_show >= (uint8_t)(i + 1U)) ? 1U : 0U;

        set_visible(ov->dist[i], on);
    }

    set_visible(ov->lane_left,
                (car_state == 2U && lane_dep) ? 1U : 0U);
    set_visible(ov->lane_right,
                (car_state == 3U && lane_dep) ? 1U : 0U);
    set_visible(ov->off_left,
                (car_state == 2U && lane_dep) ? 1U : 0U);
    set_visible(ov->off_right,
                (car_state == 3U && lane_dep) ? 1U : 0U);

    {
        uint8_t on = (car_state != 2U && car_state != 3U && show_front_car != 0U)
                     ? 1U : 0U;
        uint8_t idx = (distance_show <= 4U) ? distance_show : 0U;

        set_visible(ov->min_car, on);
        lv_obj_set_y(ov->min_car, s_min_car_y[idx]);
    }

    /*
     * 居中大车：Qt 条件 (carState===1 || showcar>0 || showarea>0) &&
     * !rightCar.visible && !leftCar.visible；左右偏移车受车道偏离开关门控。
     */
    {
        uint8_t left_visible = (car_state == 2U && lane_dep) ? 1U : 0U;
        uint8_t right_visible = (car_state == 3U && lane_dep) ? 1U : 0U;

        set_visible(ov->max_car,
                    ((car_state == 1U || show_car > 0U || show_area > 0U) &&
                     !left_visible && !right_visible) ? 1U : 0U);
    }
}

bool ov_cruise_is_active(const ov_cruise_t *ov)
{
    return (ov != NULL && ov->active != 0U);
}

void ov_cruise_destroy(ov_cruise_t *ov)
{
    if(ov != NULL && ov->root != NULL) {
        lv_obj_delete(ov->root);
        ov->root = NULL;
        ov->active = 0U;
        ov->last_sig = 0xFFFFFFFFU;
    }
}
