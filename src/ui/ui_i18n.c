/*
 * ui_i18n.c - Dashboard-scene localization engine
 *
 * The table below is the single source for Chinese dashboard text. The font
 * generation script extracts every non-ASCII character from this file, so
 * keep Chinese text inside the table only.
 */
#include "ui/ui_i18n.h"
#include "ui/ui_fonts.h"

#include <stdio.h>
#include <string.h>


static bool s_chinese;

static const char *const words[][2] = {
    /* Card titles */
    {"SHORT TERM", "短期行程"},
    {"TIRE PRESSURE", "胎压信息"},
    {"FUEL ECONOMY", "燃油经济性"},
    {"ECO SCORE", "节能评分"},
    {"INSTANT FUEL", "瞬时油耗"},
    {"CHASSIS", "底盘控制"},
    {"ITINERARY", "行程资讯"},
    {"CONNECT", "互联"},
    {"MEDIA", "多媒体"},
    {"FM RADIO", "FM 收音机"},
    {"GeneralUi1 (placeholder)", "通用界面 1（占位）"},

    /* Short term rows and eco average */
    {"TRIP DIST ", "行驶距离 "},
    {"AVG SPEED ", "平均车速 "},
    {"DRIVE TIME ", "行驶时间 "},
    {"AVG FUEL ", "平均油耗 "},
    {"AVG ", "平均 "},
    {"AVG", "平均"},

    /* Mileage and range prefixes on the main dial */
    {"RANGE: ", "续航: "},
    {"RANGE:", "续航:"},

    /* Connect card */
    {"CONNECT IS OFF\nPLEASE ENABLE AND SELECT A MODE", "互联已关闭\n请开启并选择模式"},
    {"CONNECT BLUETOOTH FOR WIRELESS", "连接蓝牙以使用无线互联"},
    {"CONNECTING...", "连接中..."},
    {"CURRENT MODE: ", "当前模式: "},
    {"BLUETOOTH: ", "蓝牙: "},

    /* Placeholder page */
    {"SPEED: ", "车速: "},
    {"RPM: ", "转速: "},

    /* Overlay banners and general tip panel */
    {"LOW FUEL!", "燃油低!"},
    {"SHIFT TO P, STAY SAFE!", "请挂入 P 挡，注意安全!"},
    {"WARNING", "警告"},
    {"PARK/START", "驻车/启动"},
    {"FAULT", "故障"},
    {"SEE USER MANUAL", "请参阅用户手册"},

    /* Warning text table (matches ov_tips s_texts) */
    {"Headlamp system fault", "前大灯系统故障"},
    {"Key system fault", "钥匙系统故障"},
    {"Battery low, please charge", "电池电量低，请充电"},
    {"Press brake and start button", "请踩下制动踏板并按启动按钮"},
    {"Only start button left", "仅剩启动按钮"},
    {"ACC: turn wheel and press start", "ACC：转动方向盘并按启动"},
    {"Key not detected", "未检测到钥匙"},
    {"Key registration complete", "钥匙注册完成"},
    {"Key ID mismatch", "钥匙 ID 不匹配"},
    {"Chassis control system fault", "底盘控制系统故障"},
    {"Press brake pedal to operate switch", "请踩下制动踏板操作开关"},
    {"Caution: steep slope", "注意：陡坡"},
    {"Steep slope, press brake to operate", "陡坡，请踩制动踏板操作"},
    {"Rear left tire leaking", "左后轮胎漏气"},
    {"Rear right tire leaking", "右后轮胎漏气"},
    {"Front left tire leaking", "左前轮胎漏气"},
    {"Front right tire leaking", "右前轮胎漏气"},
    {"Rear left tire low, inflate", "左后轮胎气压低，请充气"},
    {"Rear right tire low, inflate", "右后轮胎气压低，请充气"},
    {"Front left tire low, inflate", "左前轮胎气压低，请充气"},
    {"Front right tire low, inflate", "右前轮胎气压低，请充气"},
    {"Low fuel", "燃油不足"},
    {"Start", "启动"},
    {"Start-stop system fault", "启停系统故障"},
    {"Off", "关闭"},
    {"Battery shutting down, power save", "电池即将关闭，进入省电模式"},
    {"Key battery low", "钥匙电池电量低"},
    {"System off, shift to reverse", "系统关闭，请挂入倒挡"},
    {"4WD fault", "四驱故障"},
    {"Lane blind spot fault", "车道盲区故障"},
    {"Please shift to P", "请挂入 P 挡"},
    {"Please turn off ignition", "请关闭点火"},
    {"Move key near start button", "请将钥匙靠近启动按钮"},
    {"Press brake and START to drive", "请踩制动并按 START 行驶"},
    {"Unavailable, select driver assist", "不可用，请选择驾驶辅助"},
    {"Release parking system", "解除驻车系统"},
    {"Tire burst", "轮胎爆裂"},
    {"TPMS fault, see user manual", "胎压监测故障，请参阅用户手册"},
    {"Unavailable, clean rear camera", "不可用，请清洁后摄像头"},
    {"BCI unavailable, throttle temp high", "BCI 不可用，节气门温度过高"},
    {"DCA unavailable, throttle temp high", "DCA 不可用，节气门温度过高"},
    {"BCI unavailable, side radar blocked", "BCI 不可用，侧雷达被遮挡"},
    {"BSW unavailable, side radar blocked", "BSW 不可用，侧雷达被遮挡"},
    {"BSI unavailable, side radar blocked", "BSI 不可用，侧雷达被遮挡"},
    {"BSI unavailable, cabin temp high", "BSI 不可用，车内温度过高"},
    {"LDW unavailable, cabin temp high", "LDW 不可用，车内温度过高"},
    {"LDP unavailable, cabin temp high", "LDP 不可用，车内温度过高"},
    {"Release parking brake", "解除驻车制动"},
};

typedef struct {
    char original[256];
    char translated[256];
    const lv_font_t *font;
    const lv_font_t *cjk_font;
    bool translated_shown;
} label_i18n_t;

static size_t utf8_skip(const char *text)
{
    unsigned char lead = (unsigned char)*text;

    if(lead < 0x80U) {
        return 1U;
    }
    if((lead & 0xE0U) == 0xC0U) {
        return 2U;
    }
    if((lead & 0xF0U) == 0xE0U) {
        return 3U;
    }
    return 4U;
}

/* A missing glyph in a subset font renders as a placeholder box. */
static bool cjk_covers(const char *text, const lv_font_t *font)
{
    while(*text) {
        size_t len = utf8_skip(text);
        uint32_t cp;
        lv_font_glyph_dsc_t dsc;

        if(len == 1U) {
            cp = (unsigned char)text[0];
        }
        else {
            static const uint32_t mask[] = {0U, 0U, 0x1FU, 0x0FU, 0x07U};
            cp = (uint32_t)((unsigned char)text[0] & mask[len]);
            for(size_t i = 1U; i < len; ++i) {
                cp = (cp << 6) | ((unsigned char)text[i] & 0x3FU);
            }
        }
        if(cp >= 0x80U && !lv_font_get_glyph_dsc(font, &dsc, cp, 0U)) {
            return false;
        }
        text += len;
    }
    return true;
}

static bool has_non_ascii(const char *text)
{
    while(*text) {
        if((unsigned char)*text >= 0x80U) {
            return true;
        }
        ++text;
    }
    return false;
}

/* Longest-match replacement, same boundary rules as the settings table. */
static void translate_text(const char *source, char *out, size_t out_size)
{
    size_t used = 0U;

    while(*source && used + 1U < out_size) {
        const char *replacement = NULL;
        size_t longest = 0U;

        for(size_t i = 0U; i < sizeof(words) / sizeof(words[0]); ++i) {
            size_t n = strlen(words[i][0]);

            if(n > longest && strncmp(source, words[i][0], n) == 0 &&
               (source[n] == '\0' || source[n] == ' ' || source[n] == ':' ||
                source[n] == ')' || words[i][0][n - 1U] == ' ')) {
                longest = n;
                replacement = words[i][1];
            }
        }

        if(replacement != NULL) {
            size_t n = strlen(replacement);

            if(used + n >= out_size) {
                break;
            }
            memcpy(out + used, replacement, n);
            used += n;
            source += longest;
        }
        else {
            out[used++] = *source++;
        }
    }

    out[used] = '\0';
}

const lv_font_t *ui_i18n_font(const lv_font_t *en_font)
{
    return ui_font_cjk(en_font);
}

void ui_i18n_set_language(bool chinese)
{
    s_chinese = chinese;
}

bool ui_i18n_is_chinese(void)
{
    return s_chinese;
}

static void state_delete(lv_event_t *event)
{
    lv_free(lv_event_get_user_data(event));
}

static label_i18n_t *label_state(lv_obj_t *label)
{
    label_i18n_t *saved = NULL;

    for(uint32_t i = 0U; i < lv_obj_get_event_count(label); ++i) {
        lv_event_dsc_t *dsc = lv_obj_get_event_dsc(label, i);

        if(lv_event_dsc_get_cb(dsc) == state_delete) {
            saved = lv_event_dsc_get_user_data(dsc);
        }
    }

    if(saved == NULL) {
        const char *text = lv_label_get_text(label);

        saved = lv_malloc_zeroed(sizeof(*saved));
        if(saved == NULL) {
            return NULL;
        }
        snprintf(saved->original, sizeof(saved->original), "%s", text);
        snprintf(saved->translated, sizeof(saved->translated), "%s", text);
        saved->font = lv_obj_get_style_text_font(label, 0);
        saved->cjk_font = ui_i18n_font(saved->font);
        lv_obj_add_event_cb(label, state_delete, LV_EVENT_DELETE, saved);
    }

    return saved;
}

static void apply_label(lv_obj_t *label)
{
    label_i18n_t *saved = label_state(label);
    const char *text;

    if(saved == NULL) {
        return;
    }

    text = lv_label_get_text(label);
    /* Components can change font size on each frame (e.g. RPM highlight).
     * Capture that explicit update before restoring a cached translation font. */
    const lv_font_t *current = lv_obj_get_style_text_font(label, 0);
    if(current != saved->font && current != saved->cjk_font) {
        saved->font = current;
        saved->cjk_font = ui_i18n_font(current);
    }

    if(s_chinese) {
        /* Already translated (or nothing to translate) this cycle. */
        if(saved->translated_shown && strcmp(text, saved->translated) == 0) {
            return;
        }

        snprintf(saved->original, sizeof(saved->original), "%s", text);
        translate_text(saved->original, saved->translated,
                       sizeof(saved->translated));

        if(strcmp(saved->translated, saved->original) != 0 ||
           has_non_ascii(saved->original)) {
            if(cjk_covers(saved->translated, saved->cjk_font)) {
                lv_label_set_text(label, saved->translated);
                if(lv_obj_get_style_text_font(label, 0) != saved->cjk_font) {
                    lv_obj_set_style_text_font(label, saved->cjk_font, 0);
                }
                saved->translated_shown = true;
                return;
            }
        }

        /* Subset font cannot draw the result: keep the English original. */
        if(strcmp(text, saved->original) != 0) {
            lv_label_set_text(label, saved->original);
        }
        /* 相同字体也会触发 LVGL 样式刷新，避免每帧让数字标签重新布局。 */
        if(lv_obj_get_style_text_font(label, 0) != saved->font) {
            lv_obj_set_style_text_font(label, saved->font, 0);
        }
        snprintf(saved->translated, sizeof(saved->translated), "%s",
                 saved->original);
        saved->translated_shown = false;
        return;
    }

    if(!saved->translated_shown && strcmp(text, saved->original) == 0) {
        return;
    }

    lv_label_set_text(label, saved->original);
    lv_obj_set_style_text_font(label, saved->font, 0);
    snprintf(saved->translated, sizeof(saved->translated), "%s",
             saved->original);
    saved->translated_shown = false;
}

void ui_i18n_apply(lv_obj_t *root)
{
    if(root == NULL) {
        return;
    }

    if(lv_obj_check_type(root, &lv_label_class)) {
        apply_label(root);
    }

    for(uint32_t i = 0U; i < lv_obj_get_child_count(root); ++i) {
        ui_i18n_apply(lv_obj_get_child(root, i));
    }
}
