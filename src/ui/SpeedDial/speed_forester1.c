/*
 * speed_forester1.c - Style1 车速表实现
 *
 * 坐标、角度公式与高亮阈值全部取自 Qt SpeedDial/ForesterSpeed1.qml：
 *   公制指针角 = -195° + speed × 0.75
 *   英里制指针角 = speed > 129 ? -135.3456° + (speed-129) × 0.6214 × 1.5
 *                          : -195° + speed × 0.6214 × 0.75
 *   刻度高亮 = scaleSpeed 落在对应 Label 的 scale 条件区间内（scaleSpeed 在
 *              mileType=1 时为 speed × 0.6214，阈值仍是同一组数字）
 *
 * LVGL 旋转角度单位为 0.1° 且正值顺时针；scaleSpeed 用 1/10000 的整数表示，
 * 阈值按 Qt 的 “> x && < y” 严格不等式逐条抄录，不做近似合并。
 */
#include "ui/SpeedDial/speed_forester1.h"

#include <stddef.h>

#include "ui/ui_assets.h"
#include "ui/ui_theme.h"
#include "ui/widgets/ui_dial.h"

#define SPEED_F1_SIZE 620

/* 指针图按 alpha 包围盒裁剪：源图 620x620，(278,71)-(342,338) -> 64x267。
 * 表盘中心 (310,310)，因此裁剪图摆放 (278,71)、轴心 (32,239)。
 * 裁剪由 tools/png2bin.py 的 CROP_TO_ALPHA_BBOX 执行，两端常量必须同步。 */
#define NEEDLE_X 278
#define NEEDLE_Y 71
#define NEEDLE_PIVOT_X 32
#define NEEDLE_PIVOT_Y 239

/* 刻度文本位置与内容，取自 Qt 中各 Label 的 x/y/text */
static const ui_dial_label_t s_labels[SPEED_F1_LABEL_COUNT] = {
    {360, 510, "0"},
    {290, 520, "20"},
    {224, 510, "40"},
    {180, 484, "60"},
    {140, 450, "80"},
    {100, 400, "100"},
    {80, 350, "120"},
    {76, 294, "140"},
    {80, 236, "160"},
    {106, 180, "180"},
    {140, 140, "200"},
    {186, 100, "220"},
    {248, 76, "260"},
    {320, 76, "280"},
};

/* 英里制下刻度显示的数字，对应 Qt 中 mileType=1 时的文本 */
static const char *const s_labels_mile[SPEED_F1_LABEL_COUNT] = {
    "0", "20", "40", "60", "80", "90", "100", "110",
    "120", "130", "140", "150", "160", "170",
};

/*
 * Qt 每个 Label 的 scale 条件换算成 scaleSpeed × 10000 后的开区间 (lo, hi)。
 * 注意 Qt 不保证区间连续（例如 226~236、246~276 之间没有高亮），照抄。
 */
static const int32_t s_hit_lo[SPEED_F1_LABEL_COUNT] = {
    (-2147483647 - 1), 160000, 360000, 560000, 760000, 960000, 1160000,
    1360000, 1560000, 1760000, 1960000, 2160000, 2360000, 2760000,
};

static const int32_t s_hit_hi[SPEED_F1_LABEL_COUNT] = {
    60000, 260000, 460000, 660000, 860000, 1060000, 1260000,
    1460000, 1660000, 1860000, 2060000, 2260000, 2460000, 2147483647,
};

/* 指针需整图驻留 RAM 才能旋转绘制，跨样式切换只加载一次 */
static lv_image_dsc_t s_needle;

/* 当前车速对应的指针角度，单位 0.1°，公式见文件头 */
static int32_t needle_angle(uint32_t speed_kph, bool mile)
{
    int32_t speed = (int32_t)speed_kph;

    if(!mile) {
        return -1950 + speed * 15 / 2;
    }

    if(speed > 129) {
        return (-1353456 + (speed - 129) * 9321) / 1000;
    }

    return -1950 + speed * 46605 / 10000;
}

/* 当前车速命中的刻度序号，-1 表示没有落在任何高亮区间 */
static int32_t hit_index(uint32_t speed_kph, bool mile)
{
    int32_t scale = mile ? (int32_t)speed_kph * 6214
                         : (int32_t)speed_kph * 10000;

    for(size_t i = 0; i < SPEED_F1_LABEL_COUNT; i++) {
        if(scale > s_hit_lo[i] && scale < s_hit_hi[i]) {
            return (int32_t)i;
        }
    }

    return -1;
}

bool speed_forester1_create(speed_forester1_t *speed, lv_obj_t *parent,
                            int32_t x, int32_t y)
{
    if(speed == NULL) {
        return false;
    }

    speed->highlight = -1;
    speed->shown_mile = false;
    speed->shown_valid = false;

    if(!ui_assets_load_image(&s_needle, "SubaruForester/speed_point1.bin")) {
        return false;
    }

    ui_dial_create(&speed->dial, parent, x, y, SPEED_F1_SIZE,
                   "SubaruForester/speed_bg1.bin", &s_needle,
                   NEEDLE_X, NEEDLE_Y, NEEDLE_PIVOT_X, NEEDLE_PIVOT_Y, -1950);

    speed->unit = ui_dial_add_text(&speed->dial, "km/h", 290, 350, UI_FONT_SM);
    ui_dial_add_labels(&speed->dial, s_labels, SPEED_F1_LABEL_COUNT, UI_FONT_MD,
                       speed->labels, SPEED_F1_LABEL_COUNT);

    return true;
}

void speed_forester1_apply(speed_forester1_t *speed, uint32_t speed_kph,
                           bool mile)
{
    int32_t target;

    if(speed == NULL || speed->dial.needle == NULL) {
        return;
    }

    /* 只平滑视觉指针，刻度与单位仍按本次原始数据更新。 */
    ui_dial_set_angle(&speed->dial, needle_angle(speed_kph, mile));

    /* 刻度高亮：单位变化时即使高亮序号不变也必须重刷文本与单位 */
    target = hit_index(speed_kph, mile);

    if(speed->shown_valid && target == speed->highlight &&
       mile == speed->shown_mile) {
        return;
    }

    speed->highlight = target;
    speed->shown_mile = mile;
    speed->shown_valid = true;

    for(size_t i = 0; i < SPEED_F1_LABEL_COUNT; i++) {
        const char *text = mile ? s_labels_mile[i] : s_labels[i].text;

        lv_obj_set_style_text_font(speed->labels[i],
                                   (int32_t)i == target ? UI_FONT_LG : UI_FONT_MD,
                                   0);
        lv_label_set_text(speed->labels[i], text);
    }

    lv_label_set_text(speed->unit, mile ? "mph" : "km/h");
}
