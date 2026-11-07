/*
 * ov_tips.c - 覆盖层：提示条与通用提示面板
 *
 * 三个独立部件共用一层全页容器，坐标与 Qt 一致：
 *   燃油低横幅 230×60，黑底 + 1px 黄边，42px 图标 + 24px 黄字
 *   P 挡提示条 宽随文字，居中，y=30，样式同上
 *   通用提示面板 260×280 @ (834,253)，背景 tips_bg + 30% 黑遮罩
 *
 * 所有图片都使用预渲染 bin，运行期不做缩放。
 */
#include "ui/overlays/ov_tips.h"

#include "ui/ui_common.h"
#include "ui/ui_theme.h"

#define TIPS_W      1920
#define TIPS_H      720
#define TIPS_YELLOW 0xFFF59A
#define TIPS_GRAY   0xCCCCCC

#define FUEL_X  845
#define FUEL_Y  30
#define FUEL_W  230
#define FUEL_H  60

#define GEAR_H  60

#define GEN_X   834
#define GEN_Y   253
#define GEN_W   260
#define GEN_H   280

/* 通用提示面板内部坐标（Qt ColumnLayout margins 16 / spacing 12） */
#define GEN_PAD       16
#define GEN_TITLE_Y   16
#define GEN_SEP_Y     56
#define GEN_MID_Y     69
#define GEN_MID_H     167
#define GEN_ICON_Y    30
#define GEN_TEXT_Y    101
#define GEN_BOTTOM_Y  248

/* 0x03 弹窗没有 duration_s 时的默认显示时长，等价 Qt 的 2000ms */
#define POPUP_DEFAULT_MS 2000U
/* 相同内容的 0x03 帧间隔超过该值才视为一次新事件，避免连续帧反复重播 */
#define POPUP_FRAME_GAP_MS 1000U
/* Qt showBriefly 未传 durationS，因此所有 0x1C 警告都按默认 2s 播放 */
#define WARN_EVENT_MS 2000U
/* Qt main.qml onMessageNotify：msgId 32 仅在 SettingsMgr.show4h 时显示 */
#define TIP_ID_SHOW_4H 32U

/* Qt 的辅助提示行：Text 24px 粗体 #FF4444 居中，位于正文区下方 */
#define GEN_AUX_Y 209

typedef struct {
    uint32_t id;
    const char *icon;
} tip_icon_t;

/* 图标映射与 Qt GeneralTipsWindow 的 switch 一致，未列出的 id 用默认红三角 */
static const tip_icon_t s_icons[] = {
    {1U,  UI_IMG("WarnIcon/tips/icon_1.bin")},
    {2U,  UI_IMG("WarnIcon/tips/icon_2.bin")},
    {3U,  UI_IMG("WarnIcon/tips/icon_3.bin")},
    {4U,  UI_IMG("WarnIcon/tips/icon_4.bin")},
    {5U,  UI_IMG("WarnIcon/tips/icon_5.bin")},
    {7U,  UI_IMG("WarnIcon/tips/icon_2.bin")},
    {8U,  UI_IMG("WarnIcon/tips/icon_2.bin")},
    {9U,  UI_IMG("WarnIcon/tips/icon_9.bin")},
    {10U, UI_IMG("WarnIcon/tips/icon_2.bin")},
    {12U, UI_IMG("WarnIcon/tips/icon_4.bin")},
    {13U, UI_IMG("WarnIcon/tips/icon_13.bin")},
    {14U, UI_IMG("WarnIcon/tips/icon_13.bin")},
    {15U, UI_IMG("WarnIcon/tips/icon_15.bin")},
    {16U, UI_IMG("WarnIcon/tips/icon_15.bin")},
    {17U, UI_IMG("WarnIcon/tips/icon_15.bin")},
    {18U, UI_IMG("WarnIcon/tips/icon_15.bin")},
    {19U, UI_IMG("WarnIcon/tips/icon_15.bin")},
    {20U, UI_IMG("WarnIcon/tips/icon_15.bin")},
    {21U, UI_IMG("WarnIcon/tips/icon_15.bin")},
    {22U, UI_IMG("WarnIcon/tips/icon_15.bin")},
    {23U, UI_IMG("WarnIcon/tips/icon_23.bin")},
    {24U, UI_IMG("WarnIcon/tips/icon_24.bin")},
    {25U, UI_IMG("WarnIcon/tips/icon_25.bin")},
    {26U, UI_IMG("WarnIcon/tips/icon_26.bin")},
    {27U, UI_IMG("WarnIcon/tips/icon_27.bin")},
    {28U, UI_IMG("WarnIcon/tips/icon_28.bin")},
    {29U, UI_IMG("WarnIcon/tips/icon_29.bin")},
    {30U, UI_IMG("WarnIcon/tips/icon_3.bin")},
    {39U, UI_IMG("WarnIcon/tips/icon_39.bin")},
};

typedef struct {
    uint32_t id;
    const char *text;
} tip_text_t;

/* 文案与 Qt GeneralTipsWindow 的 switch 一致，中文按项目约定改英文 */
static const tip_text_t s_texts[] = {
    {1U,  "Headlamp system fault"},
    {2U,  "Key system fault"},
    {3U,  "Battery low, please charge"},
    {4U,  "Press brake and start button"},
    {5U,  "Only start button left"},
    {6U,  "ACC: turn wheel and press start"},
    {7U,  "Key not detected"},
    {8U,  "Key system fault"},
    {9U,  "Key registration complete"},
    {10U, "Key ID mismatch"},
    {11U, "Chassis control system fault"},
    {12U, "Press brake pedal to operate switch"},
    {13U, "Caution: steep slope"},
    {14U, "Steep slope, press brake to operate"},
    {15U, "Rear left tire leaking"},
    {16U, "Rear right tire leaking"},
    {17U, "Front left tire leaking"},
    {18U, "Front right tire leaking"},
    {19U, "Rear left tire low, inflate"},
    {20U, "Rear right tire low, inflate"},
    {21U, "Front left tire low, inflate"},
    {22U, "Front right tire low, inflate"},
    {23U, "Low fuel"},
    {27U, "Start"},
    {28U, "Start-stop system fault"},
    {29U, "Off"},
    {30U, "Battery shutting down, power save"},
    {31U, "Key battery low"},
    {32U, "System off, shift to reverse"},
    {33U, "4WD fault"},
    {34U, "Lane blind spot fault"},
    {35U, "Please shift to P"},
    {36U, "Please turn off ignition"},
    {37U, "Move key near start button"},
    {38U, "Press brake and START to drive"},
    {39U, "Unavailable, select driver assist"},
    {41U, "Release parking system"},
    {42U, "Tire burst"},
    {43U, "TPMS fault, see user manual"},
    {44U, "Unavailable, clean rear camera"},
    {45U, "BCI unavailable, throttle temp high"},
    {46U, "DCA unavailable, throttle temp high"},
    {47U, "BCI unavailable, side radar blocked"},
    {48U, "BSW unavailable, side radar blocked"},
    {49U, "BSI unavailable, side radar blocked"},
    {50U, "BSI unavailable, cabin temp high"},
    {51U, "LDW unavailable, cabin temp high"},
    {52U, "LDP unavailable, cabin temp high"},
    {53U, "Release parking brake"},
    {54U, "Tire burst"},
    {55U, "Tire burst"},
    {56U, "Tire burst"},
};

static lv_obj_t *tips_box(lv_obj_t *parent, int32_t x, int32_t y,
                          int32_t w, int32_t h, uint8_t border)
{
    lv_obj_t *box = lv_obj_create(parent);

    lv_obj_remove_style_all(box);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(box, w, h);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_style_bg_color(box, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);

    if(border != 0U) {
        lv_obj_set_style_border_color(box, lv_color_hex(TIPS_YELLOW), 0);
        lv_obj_set_style_border_width(box, 1, 0);
    }

    return box;
}

static lv_obj_t *tips_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);

    return label;
}

static const char *tip_icon_path(uint32_t id)
{
    size_t i;

    for(i = 0U; i < sizeof(s_icons) / sizeof(s_icons[0]); ++i) {
        if(s_icons[i].id == id) {
            return s_icons[i].icon;
        }
    }

    return UI_IMG("WarnIcon/tips/icon_1.bin");
}

static const char *tip_text(uint32_t id)
{
    size_t i;

    for(i = 0U; i < sizeof(s_texts) / sizeof(s_texts[0]); ++i) {
        if(s_texts[i].id == id) {
            return s_texts[i].text;
        }
    }

    return "";
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

/* 通用提示面板的显示内容：标题、图标、正文，与 Qt 的 showid 分支一致 */
static void general_set_id(ov_tips_t *ov, uint32_t id)
{
    static const char *const s_title_normal = "WARNING";
    static const char *const s_title_park = "PARK/START";
    const char *text = tip_text(id);
    const char *title;
    int32_t need;
    int32_t row_x;

    if(id == 27U || id == 28U || id == 29U) {
        title = s_title_park;
    }
    else {
        title = s_title_normal;
    }

    lv_label_set_text(ov->gen_title, title);
    lv_image_set_src(ov->gen_icon, tip_icon_path(id));
    lv_label_set_text(ov->gen_text, text);

    /* Qt main.qml：24/25/26 隐藏正文并显示红色"故障"（改英文 FAULT） */
    if(id == 24U || id == 25U || id == 26U) {
        set_visible(ov->gen_text, 0U);
        set_visible(ov->gen_aux, 1U);
    }
    else {
        set_visible(ov->gen_text, 1U);
        set_visible(ov->gen_aux, 0U);
    }

    /* 标题行按 Qt 的 RowLayout 居中：28px 图标 + 8px 间距 + 22px 文本 */
    lv_obj_update_layout(ov->gen_title);
    need = 28 + 8 + lv_obj_get_width(ov->gen_title);
    row_x = GEN_PAD + (GEN_W - GEN_PAD * 2 - need) / 2;
    lv_obj_set_pos(ov->gen_title_icon, row_x, GEN_TITLE_Y);
    lv_obj_set_pos(ov->gen_title, row_x + 36, GEN_TITLE_Y + 2);

    /* Qt 中 id 39 的图标居中偏左（x=48），其余 x=80 */
    lv_obj_set_pos(ov->gen_icon, (id == 39U) ? 48 : 80, GEN_MID_Y + GEN_ICON_Y);
}

bool ov_tips_create(ov_tips_t *ov, lv_obj_t *parent)
{
    if(ov == NULL) {
        return false;
    }

    ov->obstructed = false;
    ov->root = ui_blank_container(parent);
    lv_obj_set_size(ov->root, TIPS_W, TIPS_H);
    lv_obj_set_pos(ov->root, 0, 0);

    /* 燃油低横幅：黑底 1px 黄边，Row 从 10 起，图标 42、间距 12、文字 24 */
    ov->fuel_banner = tips_box(ov->root, FUEL_X, FUEL_Y, FUEL_W, FUEL_H, 1U);
    {
        lv_obj_t *icon = lv_image_create(ov->fuel_banner);
        lv_image_set_src(icon, UI_IMG("WarnIcon/tips/fuel_low_42.bin"));
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 10, 0);
    }
    {
        lv_obj_t *label = tips_label(ov->fuel_banner, "LOW FUEL!",
                                     &lv_font_montserrat_24, TIPS_YELLOW);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 64, 0);
    }

    /*
     * P 挡提示条：宽度 = 10 + 42 + 12 + 文字宽，x = (1920 - 宽) / 2 - 20，y = 30。
     * 高度固定 60，图标与文字垂直居中。
     */
    {
        static const char *const s_gear_text = "SHIFT TO P, STAY SAFE!";
        int32_t row_w;

        ov->gear_banner = tips_box(ov->root, 0, FUEL_Y, 400, GEAR_H, 1U);
        {
            lv_obj_t *icon = lv_image_create(ov->gear_banner);
            lv_image_set_src(icon, UI_IMG("WarnIcon/tips/red_warning_42.bin"));
            lv_obj_align(icon, LV_ALIGN_LEFT_MID, 10, 0);
        }
        {
            lv_obj_t *label = tips_label(ov->gear_banner, s_gear_text,
                                         &lv_font_montserrat_24, TIPS_YELLOW);
            lv_obj_align(label, LV_ALIGN_LEFT_MID, 64, 0);

            /* 宽度 = 10 + 42 + 12 + 文字宽，x = (1920 - 宽) / 2 - 20 */
            lv_obj_update_layout(label);
            row_w = 10 + 42 + 12 + lv_obj_get_width(label);
            lv_obj_set_size(ov->gear_banner, row_w, GEAR_H);
            lv_obj_set_pos(ov->gear_banner, (TIPS_W - row_w) / 2 - 20, FUEL_Y);
        }
    }

    /* 通用提示面板：tips_bg 拉伸图 + 30% 黑遮罩 + 圆角 8 */
    ov->general = lv_obj_create(ov->root);
    lv_obj_remove_style_all(ov->general);
    lv_obj_remove_flag(ov->general, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(ov->general, GEN_W, GEN_H);
    lv_obj_set_pos(ov->general, GEN_X, GEN_Y);

    {
        lv_obj_t *bg = lv_image_create(ov->general);
        lv_image_set_src(bg, UI_IMG("WarnIcon/tips/tips_bg_260.bin"));
        lv_obj_set_pos(bg, 0, 0);

        lv_obj_t *mask = lv_obj_create(ov->general);
        lv_obj_remove_style_all(mask);
        lv_obj_remove_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(mask, GEN_W, GEN_H);
        lv_obj_set_pos(mask, 0, 0);
        lv_obj_set_style_bg_color(mask, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(mask, LV_OPA_30, 0);
        lv_obj_set_style_radius(mask, 8, 0);
    }

    ov->gen_title_icon = lv_image_create(ov->general);
    lv_image_set_src(ov->gen_title_icon, UI_IMG("WarnIcon/tips/red_warning_28.bin"));
    lv_obj_set_pos(ov->gen_title_icon, GEN_PAD + 20, GEN_TITLE_Y);

    ov->gen_title = tips_label(ov->general, "WARNING",
                               &lv_font_montserrat_22, UI_COLOR_WHITE);
    lv_obj_set_pos(ov->gen_title, GEN_PAD + 60, GEN_TITLE_Y + 2);

    /* 1px 白色 20% 分隔线 */
    {
        lv_obj_t *sep = lv_obj_create(ov->general);
        lv_obj_remove_style_all(sep);
        lv_obj_remove_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(sep, GEN_W - GEN_PAD * 2, 1);
        lv_obj_set_pos(sep, GEN_PAD, GEN_SEP_Y);
        lv_obj_set_style_bg_color(sep, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_20, 0);
    }

    ov->gen_icon = lv_image_create(ov->general);
    lv_obj_set_pos(ov->gen_icon, 80, GEN_MID_Y + GEN_ICON_Y);

    ov->gen_text = tips_label(ov->general, "", &lv_font_montserrat_18,
                              UI_COLOR_WHITE);
    lv_obj_set_pos(ov->gen_text, GEN_PAD + 10, GEN_MID_Y + GEN_TEXT_Y);
    lv_obj_set_width(ov->gen_text, GEN_W - GEN_PAD * 2 - 20);
    lv_label_set_long_mode(ov->gen_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(ov->gen_text, LV_TEXT_ALIGN_CENTER, 0);

    /* 正文隐藏时显示的红字，Qt 为 24px 粗体 #FF4444 居中 */
    ov->gen_aux = tips_label(ov->general, "FAULT", &lv_font_montserrat_24,
                             0xFF4444);
    lv_obj_set_width(ov->gen_aux, GEN_W - GEN_PAD * 2);
    lv_obj_set_pos(ov->gen_aux, GEN_PAD, GEN_AUX_Y);
    lv_obj_set_style_text_align(ov->gen_aux, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(ov->gen_aux, LV_OBJ_FLAG_HIDDEN);

    {
        lv_obj_t *bottom = tips_label(ov->general, "SEE USER MANUAL",
                                      &lv_font_montserrat_14, TIPS_GRAY);
        lv_obj_set_width(bottom, GEN_W - GEN_PAD * 2);
        lv_obj_set_pos(bottom, GEN_PAD, GEN_BOTTOM_Y);
        lv_obj_set_style_text_align(bottom, LV_TEXT_ALIGN_CENTER, 0);
    }

    lv_obj_add_flag(ov->fuel_banner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ov->gear_banner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ov->general, LV_OBJ_FLAG_HIDDEN);

    ov->popup_sig = 0xFFFFFFFFU;
    ov->popup_frame_tick = 0U;
    ov->warn_seen_init = false;
    ov->warn_seen_seq = 0U;
    ov->queue_head = 0U;
    ov->queue_count = 0U;
    ov->queue_dropped = 0U;
    ov->active = false;
    ov->shown_tick = 0U;
    ov->shown_duration = POPUP_DEFAULT_MS;

    return true;
}

static void queue_push(ov_tips_t *ov, uint16_t id, uint32_t duration_ms)
{
    uint8_t slot;

    /* Qt 的 msgQueue 无上界且不去重；这里取有界实现，满时挤掉最旧一条 */
    if(ov->queue_count >= OV_TIPS_QUEUE_MAX) {
        ov->queue_head = (uint8_t)((ov->queue_head + 1U) % OV_TIPS_QUEUE_MAX);
        ov->queue_count = (uint8_t)(ov->queue_count - 1U);
        ov->queue_dropped++;
    }

    slot = (uint8_t)((ov->queue_head + ov->queue_count) % OV_TIPS_QUEUE_MAX);
    ov->queue[slot].id = id;
    ov->queue[slot].duration_ms = duration_ms;
    ov->queue_count = (uint8_t)(ov->queue_count + 1U);
}

static bool queue_pop(ov_tips_t *ov, ov_tips_item_t *out)
{
    if(ov->queue_count == 0U) {
        return false;
    }

    *out = ov->queue[ov->queue_head];
    ov->queue_head = (uint8_t)((ov->queue_head + 1U) % OV_TIPS_QUEUE_MAX);
    ov->queue_count = (uint8_t)(ov->queue_count - 1U);
    return true;
}

/* Qt：id 32（四驱不可用）受 SettingsMgr.show4h 门控，其余提示不受影响 */
static bool tip_allowed(const car_state_t *state, uint16_t id)
{
    return !(id == TIP_ID_SHOW_4H && !state->show_4h);
}

/*
 * 把快照里尚未消费的 0x1C 事件追加进播放队列。
 * 首次 apply 只记录进度，不重放界面创建前已经发生的事件；
 * 之后按 seq 升序补上同一刷新周期内到达的所有事件，不丢中间条。
 */
static void consume_warn_events(ov_tips_t *ov, const car_state_t *state)
{
    const car_warn_events_t *queue = &state->warn_events;
    uint8_t i;

    if(!ov->warn_seen_init) {
        ov->warn_seen_init = true;
        ov->warn_seen_seq = queue->seq;
        return;
    }

    for(i = 0U; i < queue->count; ++i) {
        const car_warn_event_t *ev =
            &queue->items[(queue->head + i) % CAR_WARN_EVENT_QUEUE_MAX];

        if(ev->seq > ov->warn_seen_seq) {
            /* 设置门控丢弃时仍推进进度，避免后续 apply 重复判定 */
            if(tip_allowed(state, ev->text_id)) {
                /* Qt 的 showBriefly 没有传 durationS，警告统一按默认 2s 播放 */
                queue_push(ov, ev->text_id, WARN_EVENT_MS);
            }
            ov->warn_seen_seq = ev->seq;
        }
    }
}

void ov_tips_apply(ov_tips_t *ov, const car_state_t *state, bool door_active)
{
    uint8_t fuel_low;
    uint8_t gear_show;
    uint32_t now;
    ov_tips_item_t item;

    if(ov == NULL || state == NULL) {
        return;
    }

    now = lv_tick_get();

    /* 燃油低横幅：Qt 的 fuelStateIcon === 2 */
    fuel_low = (state->fuel.valid && state->fuel.icon_state == 2U) ? 1U : 0U;
    set_visible(ov->fuel_banner, fuel_low);

    /* P 挡提示：车门覆盖层打开且档位不在 P（Qt 的 doorPageOpen && geartext[0] !== "P"） */
    gear_show = 0U;
    if(door_active && state->gear.valid &&
       state->gear.text[0] != 'P' && state->gear.text[0] != 'p') {
        gear_show = 1U;
    }
    set_visible(ov->gear_banner, gear_show);

    /*
     * 两条入口都进同一条 FIFO 队列。同一份快照无法还原 0x1C 与 0x03 的真实
     * 到达先后，统一先补 0x1C 事件、再判定 0x03 新帧。
     */
    consume_warn_events(ov, state);

    /* 0x03 弹窗：内容变化或同一内容的帧间隔超过 1s 视为一次新事件 */
    if(state->popup.valid) {
        uint32_t sig = (state->popup.text_id & 0xFFFFU) |
                       ((uint32_t)state->popup.icon << 16) |
                       ((uint32_t)state->popup.duration_s << 24);

        if(sig != ov->popup_sig ||
           lv_tick_elaps(ov->popup_frame_tick) >= POPUP_FRAME_GAP_MS) {
            ov->popup_sig = sig;
            if(tip_allowed(state, state->popup.text_id)) {
                queue_push(ov, state->popup.text_id,
                           (state->popup.duration_s != 0U)
                               ? (uint32_t)state->popup.duration_s * 1000U
                               : POPUP_DEFAULT_MS);
            }
        }
        ov->popup_frame_tick = now;
    }

    /* 当前条目到期后先隐藏，再尝试取下一条，保证队列连续播放 */
    if(ov->active && lv_tick_elaps(ov->shown_tick) >= ov->shown_duration) {
        ov->active = false;
        lv_obj_add_flag(ov->general, LV_OBJ_FLAG_HIDDEN);
    }

    if(!ov->active && queue_pop(ov, &item)) {
        general_set_id(ov, item.id);
        ov->active = true;
        ov->shown_tick = now;
        ov->shown_duration = item.duration_ms;
        lv_obj_remove_flag(ov->general, LV_OBJ_FLAG_HIDDEN);
    }

    /* 设置页/媒体页遮挡：只隐藏通用提示面板，播放队列与计时照常推进 */
    if(ov->active && ov->obstructed) {
        lv_obj_add_flag(ov->general, LV_OBJ_FLAG_HIDDEN);
    }
    else if(ov->active) {
        lv_obj_remove_flag(ov->general, LV_OBJ_FLAG_HIDDEN);
    }
}

void ov_tips_set_obstructed(ov_tips_t *ov, bool obstructed)
{
    if(ov != NULL) {
        ov->obstructed = obstructed;
    }
}

void ov_tips_destroy(ov_tips_t *ov)
{
    if(ov != NULL) {
        if(ov->root != NULL) {
            lv_obj_delete(ov->root);
            ov->root = NULL;
        }
        ov->fuel_banner = NULL;
        ov->gear_banner = NULL;
        ov->general = NULL;
        ov->gen_title_icon = NULL;
        ov->gen_title = NULL;
        ov->gen_icon = NULL;
        ov->gen_text = NULL;
        ov->gen_aux = NULL;
        ov->active = false;
        ov->queue_head = 0U;
        ov->queue_count = 0U;
    }
}
