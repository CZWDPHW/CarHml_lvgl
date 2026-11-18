#include "ui_settings_language.h"
#include "lvgl.h"
#include "ui/ui_fonts.h"
#include <string.h>
#include <stdio.h>
static const lv_font_t *cjk_font_for(const lv_font_t *original)
{
    return ui_font_cjk(original);
}

/* Settings-only Latin face. Keep the dashboard fonts and Chinese sizes intact. */
static const lv_font_t *english_font_for(const lv_font_t *original)
{
    unsigned pixels = ui_font_pixels(original);
    unsigned size = pixels == 26U ? 22U : pixels == 36U ? 28U : pixels;
    const lv_font_t *font = ui_font_get(UI_TYPE_ARIAL, size, false);
    return font ? font : original;
}

/* Wording from the Qt en_US.ts settings contexts, rather than expanded caps. */
static const char *english_text_for(const char *original)
{
    static const char *const labels[][2] = {
        {"ELECTRIC POWER STEERING", "EPS"}, {"NORMAL", "Normal"}, {"SPORT", "Sport"},
        {"ACTIVE TRACE CONTROL", "Active Curve"}, {"ACTIVE ENGINE BRAKE", "Active Brake"},
        {"DRIVER ASSIST", "Driver Assist"}, {"PARKING ASSIST", "Park Assist"},
        {"CHASSIS CONTROL", "Chassis Control"}, {"MOVING OBJECT", "Moving Obj"},
        {"FRONT RADAR", "Front Radar"}, {"REAR RADAR", "Rear Radar"},
        {"REAR CROSS TRAFFIC", "Rear Cross Traffic Alert"}, {"DISPLAY", "Disp"},
        {"RADAR VOLUME", "Radar Vol"}, {"RADAR DISTANCE", "Radar Dist"},
        {"LOW", "Low"}, {"MID", "Mid"}, {"HIGH", "High"},
        {"NEAR", "Near"}, {"FAR", "Far"}, {"FRONT ONLY", "Front Only"},
    };
    size_t length = strlen(original);
    while(length && original[length - 1U] == ' ') --length;
    for(size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); ++i) {
        if(strlen(labels[i][0]) == length && strncmp(original, labels[i][0], length) == 0)
            return labels[i][1];
    }
    return original;
}

/* 旧的拼接式页面仍按英文片段识别 ID；译文与新菜单共用各语言资源表。
 * 新菜单直接绑定 ID，不依赖这里的英文匹配。 */
static const struct { const char *source; ui_settings_text_id_t text; } words[] = {
    {"Normal", UI_TEXT_NORMAL},
    {"Sport", UI_TEXT_SPORT},
    {"Active Curve", UI_TEXT_CHASSIS_TRACE},
    {"Active Brake", UI_TEXT_CHASSIS_BRAKE},
    {"Emergency Braking", UI_TEXT_EMERGENCY_BRAKING},
    {"Lane Departure", UI_TEXT_LANE_DEPARTURE},
    {"Blind Spot", UI_TEXT_BLIND_SPOT},
    {"Moving Obj", UI_TEXT_PARK_MOVING_OBJECT},
    {"Front Radar", UI_TEXT_PARK_FRONT_RADAR},
    {"Rear Radar", UI_TEXT_PARK_REAR_RADAR},
    {"Rear Cross Traffic Alert", UI_TEXT_PARK_REAR_CROSS},
    {"Disp", UI_TEXT_PARK_DISPLAY},
    {"Radar Vol", UI_TEXT_PARK_VOLUME},
    {"Radar Dist", UI_TEXT_PARK_DISTANCE},
    {"Parking Sensor", UI_TEXT_PARK_SENSOR},
    {"Low", UI_TEXT_LOW},
    {"Mid", UI_TEXT_MID},
    {"High", UI_TEXT_HIGH},
    {"Near", UI_TEXT_NEAR},
    {"Far", UI_TEXT_FAR},
    {"Front Only", UI_TEXT_FRONT_ONLY},
    {"Chinese", UI_TEXT_CHINESE},
    {"English", UI_TEXT_ENGLISH},
    {"Traditional Chinese", UI_TEXT_TRADITIONAL_CHINESE},
    {"Russian", UI_TEXT_RUSSIAN},
    {"Spanish", UI_TEXT_SPANISH},
    {"German", UI_TEXT_GERMAN},
    {"Turkish", UI_TEXT_TURKISH},
    {"Korean", UI_TEXT_KOREAN},
    {"French", UI_TEXT_FRENCH},
    {"Italian", UI_TEXT_ITALIAN},
    {"Portuguese", UI_TEXT_PORTUGUESE},
    {"Bulgarian", UI_TEXT_BULGARIAN},
    {"Polish", UI_TEXT_POLISH},
    {"Romanian", UI_TEXT_ROMANIAN},
    {"Dutch", UI_TEXT_DUTCH},
    {"Czech", UI_TEXT_CZECH},
    {"Japanese", UI_TEXT_JAPANESE},
    {"Greek", UI_TEXT_GREEK},
    {"Arabic", UI_TEXT_ARABIC},
    {"YEAR", UI_TEXT_YEAR},
    {"MONTH", UI_TEXT_MONTH},
    {"DAY", UI_TEXT_DAY},
    {"HOUR", UI_TEXT_HOUR},
    {"MINUTE", UI_TEXT_MINUTE},
    {"SEND DATE & TIME", UI_TEXT_SEND_DATE_TIME},
    {"FORMAT", UI_TEXT_FORMAT},
    {"SOURCE", UI_TEXT_SOURCE},
    {"INSTRUMENT", UI_TEXT_INSTRUMENT},
    {"CENTRAL", UI_TEXT_CENTRAL},
    {"CONFIG ONLY", UI_TEXT_CONFIG_ONLY},
    {"DEFAULT", UI_TEXT_DEFAULT},
    {"ANDROID AUTO", UI_TEXT_ANDROID_AUTO},
    {"ANIMATION 1", UI_TEXT_ANIMATION_1},
    {"PLAYER UNAVAILABLE", UI_TEXT_PLAYER_UNAVAILABLE},
    {"SDK UNAVAILABLE", UI_TEXT_SDK_UNAVAILABLE},
    {"ENABLE", UI_TEXT_ENABLE},
    {"UNAVAILABLE", UI_TEXT_UNAVAILABLE},
    {"NO MODEL DATA", UI_TEXT_NO_MODEL_DATA},
    {"FAILED / PARTIAL CLEANUP", UI_TEXT_FAILED_PARTIAL_CLEANUP},
    {"LOG DIRECTORY CONTENTS ONLY", UI_TEXT_LOG_DIRECTORY_CONTENTS_ONLY},
    {"CONFIG / RESOURCES / MD5 KEPT", UI_TEXT_CONFIG_RESOURCES_MD5_KEPT},
    {"MILEAGE", UI_TEXT_MILEAGE},
    {"TEMP", UI_TEXT_TEMP},
    {"MILE", UI_TEXT_MILE},
    {"THROTTLE", UI_TEXT_THROTTLE},
    {"CUR", UI_TEXT_CUR},
    {"BEST", UI_TEXT_BEST},
    {"ECO", UI_TEXT_ECO},
    {"TIME SETTINGS", UI_TEXT_TIME_SETTINGS},
    {"TIME SOURCE: CENTRAL", UI_TEXT_TIME_SOURCE_CENTRAL},
    {"TIME SOURCE: INSTRUMENT", UI_TEXT_TIME_SOURCE_INSTRUMENT},
    {"TIME SOURCE: UNKNOWN", UI_TEXT_TIME_SOURCE_UNKNOWN},
    {"TIME FORMAT: 24h", UI_TEXT_TIME_FORMAT_24H},
    {"TIME FORMAT: 12h", UI_TEXT_TIME_FORMAT_12H},
    {"TIME FORMAT: ---", UI_TEXT_TIME_FORMAT},
    {"OPERATE ON AUDIO UNIT", UI_TEXT_OPERATE_ON_AUDIO_UNIT},
    {"WELCOME LIGHT ", UI_TEXT_WELCOME_LIGHT},
    {"AUTO INTERIOR LIGHT ", UI_TEXT_AUTO_INTERIOR_LIGHT},
    {"LIGHT OFF DELAY", UI_TEXT_LIGHT_OFF_DELAY},
    {"HORN FEEDBACK ", UI_TEXT_HORN_FEEDBACK},
    {"AUTO UNLOCK ", UI_TEXT_AUTO_UNLOCK},
    {"EXTERNAL DOORS ", UI_TEXT_EXTERNAL_DOORS},
    {"SMART KEY LOCK ", UI_TEXT_SMART_KEY_LOCK},
    {"AUTO DOOR LOCK", UI_TEXT_AUTO_DOOR_LOCK},
    {"IGN OFF", UI_TEXT_IGN_OFF},
    {"P SHIFT", UI_TEXT_P_SHIFT},
    {"SLOW", UI_TEXT_SLOW},
    {"STANDARD", UI_TEXT_STANDARD},
    {"VERY FAST", UI_TEXT_VERY_FAST},
    {"FAST", UI_TEXT_FAST},
    {"SPEED", UI_TEXT_SPEED},
    {"NORMAL", UI_TEXT_NORMAL_LABEL},
    {"SPORT", UI_TEXT_SPORT_LABEL},
    {"LANE CHANGE FLASH ", UI_TEXT_LANE_CHANGE_FLASH},
    {"SPEED SENSING WIPER ", UI_TEXT_SPEED_SENSING_WIPER},
    {"REAR WIPER LINKAGE ", UI_TEXT_REAR_WIPER_LINKAGE},
    {"MIST MODE ", UI_TEXT_MIST_MODE},
    {"OIL CHANGE", UI_TEXT_OIL_CHANGE},
    {"TIRE CHANGE", UI_TEXT_TIRE_CHANGE},
    {"CUSTOM ITEM", UI_TEXT_CUSTOM_ITEM},
    {"SET SERVICE KM", UI_TEXT_SET_SERVICE_KM},
    {"REMAINING KM", UI_TEXT_REMAINING_KM},
    {"RESET", UI_TEXT_RESET},
    {"GENERAL SETTINGS", UI_TEXT_GENERAL_SETTINGS},
    {"ECO SETTINGS", UI_TEXT_ECO_SETTINGS},
    {"DRIVER ASSIST", UI_TEXT_DRIVER_ASSIST},
    {"SPEED WARNING", UI_TEXT_SPEED_WARNING},
    {"VEHICLE SETTINGS", UI_TEXT_VEHICLE_SETTINGS},
    {"DOOR SETTINGS", UI_TEXT_DOOR_SETTINGS},
    {"MAINTENANCE", UI_TEXT_MAINTENANCE},
    {"LIGHT SENSITIVITY", UI_TEXT_LIGHT_SENSITIVITY},
    {"APP VERSION", UI_TEXT_APP_VERSION},
    {"MAIN FIRMWARE", UI_TEXT_MAIN_FIRMWARE},
    {"SMALL CHIP", UI_TEXT_SMALL_CHIP},
    {"WIRED", UI_TEXT_WIRED},
    {"SYSTEM VERSION", UI_TEXT_SYSTEM_VERSION},
    {"READ FAILED", UI_TEXT_READ_FAILED},
    {"UNKNOWN", UI_TEXT_UNKNOWN},
    {"CHANGES SAVED ON ACC OFF", UI_TEXT_CHANGES_SAVED_ON_ACC_OFF},
    {"FACTORY SETTINGS", UI_TEXT_FACTORY_SETTINGS},
    {"SYSTEM SETTINGS", UI_TEXT_SYSTEM_SETTINGS},
    {"LANGUAGE", UI_TEXT_LANGUAGE},
    {"DATE & TIME", UI_TEXT_DATE_TIME},
    {"VERSION INFO", UI_TEXT_VERSION_INFO},
    {"CLEAR CACHE", UI_TEXT_CLEAR_CACHE},
    {"BOOT ANIMATION", UI_TEXT_BOOT_ANIMATION},
    {"ITINERARY", UI_TEXT_IT_TRIP},
    {"INFORMATION", UI_TEXT_INFORMATION},
    {"MY CAR", UI_TEXT_MY_CAR},
    {"SYSTEM", UI_TEXT_SYSTEM},
    {"CONNECT", UI_TEXT_CONNECT},
    {"DIAL STYLE", UI_TEXT_DIAL_STYLE},
    {"UNITS", UI_TEXT_UNITS},
    {"CENTER CARDS", UI_TEXT_CENTER_CARDS},
    {"GENERAL", UI_TEXT_GENERAL},
    {"ECO HISTORY", UI_TEXT_ECO_HISTORY},
    {"DRIVING INFO", UI_TEXT_DRIVING_INFO},
    {"OUTSIDE TEMP ", UI_TEXT_OUTSIDE_TEMP},
    {"GEAR ", UI_TEXT_GEAR},
    {"AVG FUEL ", UI_TEXT_AVG_FUEL},
    {"DAY LIGHT ", UI_TEXT_DAY_LIGHT},
    {"NIGHT LIGHT ", UI_TEXT_NIGHT_LIGHT},
    {"SPEED LIMIT ", UI_TEXT_SPEED_LIMIT},
    {"SPEED WARN ", UI_TEXT_SPEED_WARN},
    {"ECO DISPLAY ", UI_TEXT_ECO_DISPLAY_LABEL},
    {"ECO INDICATOR ", UI_TEXT_ECO_INDICATOR_LABEL},
    {"DRIVING ASSIST", UI_TEXT_ASSIST_DRIVING},
    {"PARKING ASSIST", UI_TEXT_PARKING_ASSIST},
    {"CHASSIS CONTROL", UI_TEXT_CHASSIS_CONTROL},
    {"EMERGENCY BRAKING", UI_TEXT_DA_EMERGENCY},
    {"LANE DEPARTURE ", UI_TEXT_LANE_DEPARTURE_LABEL},
    {"BLIND SPOT ", UI_TEXT_BLIND_SPOT_LABEL},
    {"LIGHT SETTINGS", UI_TEXT_LIGHT_SETTINGS},
    {"TURN LIGHT", UI_TEXT_TURN_LIGHT},
    {"DOORS", UI_TEXT_DOORS},
    {"WIPER", UI_TEXT_WIPER},
    {"SERVICE", UI_TEXT_SERVICE},
    {"VEHICLE", UI_TEXT_IT_VEHICLE},
    {"REMOTE ENGINE START", UI_TEXT_REMOTE_ENGINE_START},
    {"SMART DOOR LOCK", UI_TEXT_SMART_DOOR_LOCK},
    {"SELECT UNLOCK", UI_TEXT_SELECT_UNLOCK},
    {"EXTERNAL DOOR", UI_TEXT_EXTERNAL_DOOR},
    {"SINGLE DOOR UNLOCK", UI_TEXT_SINGLE_DOOR_UNLOCK},
    {"HEADLIGHT SENSITIVITY", UI_TEXT_HEADLIGHT_SENSITIVITY},
    {"HEADLIGHT OFF DELAY", UI_TEXT_HEADLIGHT_OFF_DELAY},
    {"ELECTRIC POWER STEERING", UI_TEXT_ELECTRIC_POWER_STEERING},
    {"REAR CROSS TRAFFIC ALERT", UI_TEXT_REAR_CROSS_TRAFFIC_ALERT},
    {"AUTO STOP", UI_TEXT_AUTO_STOP},
    {"TOTAL MILEAGE", UI_TEXT_TOTAL_MILEAGE},
    {"FUEL TANK", UI_TEXT_FUEL_TANK},
    {"DATA DEBUG: UNAVAILABLE", UI_TEXT_DATA_DEBUG_UNAVAILABLE},
    {"TIRE DISPLAY", UI_TEXT_TIRE_DISPLAY},
    {"CAR MODEL", UI_TEXT_CAR_MODEL},
    {"FUNCTION SWITCHES", UI_TEXT_FUNCTION_SWITCHES},
    {"DOOR DISPLAY", UI_TEXT_DOOR_DISPLAY},
    {"RADAR", UI_TEXT_RADAR},
    {"4WD DISPLAY", UI_TEXT_4WD_DISPLAY},
    {"FUEL FILLER RIGHT", UI_TEXT_FUEL_FILLER_RIGHT},
    {"RIGHT HAND DRIVE", UI_TEXT_RIGHT_HAND_DRIVE},
    {"LOGO: UNAVAILABLE", UI_TEXT_LOGO_UNAVAILABLE},
    {"PASSWORD", UI_TEXT_PASSWORD},
    {"WRONG PASSWORD", UI_TEXT_WRONG_PASSWORD},
    {"PLATFORM UNAVAILABLE", UI_TEXT_PLATFORM_UNAVAILABLE},
    {"CONFIRM / BACK", UI_TEXT_CONFIRM_BACK},
    {"CANCEL / BACK", UI_TEXT_CANCEL_BACK},
    {"CONFIRM", UI_TEXT_CONFIRM},
    {"CANCEL", UI_TEXT_CANCEL},
    {"BACK", UI_TEXT_BACK},
    {"UNSUPPORTED", UI_TEXT_UNSUPPORTED},
    {"ON", UI_TEXT_ON},
    {"OFF", UI_TEXT_OFF},
    {"CLEAR LOGS", UI_TEXT_CLEAR_LOGS},
    {"CONFIRM CLEAR LOGS", UI_TEXT_CONFIRM_CLEAR_LOGS},
    {"LOGS CLEARED", UI_TEXT_LOGS_CLEARED},
    {"CLEANING LOGS...", UI_TEXT_CLEANING_LOGS},
    {"NAV", UI_TEXT_NAV},
    {"TIRE", UI_TEXT_TIRE},
    {"CHASSIS", UI_TEXT_CHASSIS},
    {"SHORT TERM", UI_TEXT_SHORT_TERM},
    {"FUEL", UI_TEXT_FUEL},
    {"MUSIC", UI_TEXT_MUSIC},
};
typedef struct {
    char original[512];
    char translated[512];
    char displayed[512];
    bool ready;
    uint8_t language;
    ui_settings_text_id_t text_id;
    const lv_font_t *font;
} label_translation_t;

/* 逐字节跳过 UTF-8 序列，返回码点数 */
static size_t utf8_skip(const char *text)
{
    unsigned char lead = (unsigned char)*text;

    if(lead < 0x80U) return 1U;
    if((lead & 0xE0U) == 0xC0U) return 2U;
    if((lead & 0xF0U) == 0xE0U) return 3U;
    return 4U;
}

/* 子集字库缺字检查：缺字时该字符会显示成占位方块，等同布局溢出 */
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
        if(cp >= 0x80U &&
           !lv_font_get_glyph_dsc(font, &dsc, cp, 0U)) {
            return false;
        }
        text += len;
    }
    return true;
}

static void translation_delete(lv_event_t *event)
{
    lv_free(lv_event_get_user_data(event));
}

static label_translation_t *label_translation(lv_obj_t *label, bool create)
{
    for(uint32_t i = 0; i < lv_obj_get_event_count(label); ++i) {
        lv_event_dsc_t *dsc = lv_obj_get_event_dsc(label, i);
        if(lv_event_dsc_get_cb(dsc) == translation_delete)
            return lv_event_dsc_get_user_data(dsc);
    }
    if(!create) return NULL;
    label_translation_t *saved = lv_malloc_zeroed(sizeof(*saved));
    if(saved) {
        saved->font = lv_obj_get_style_text_font(label, 0);
        lv_obj_add_event_cb(label, translation_delete, LV_EVENT_DELETE, saved);
    }
    return saved;
}

void ui_settings_label_set_id(lv_obj_t *label, ui_settings_text_id_t text, uint8_t language)
{
    label_translation_t *saved = label_translation(label, true);
    if(!saved) return;
    if(saved->ready && saved->text_id == text && saved->language == language &&
       strcmp(lv_label_get_text(label), saved->displayed) == 0) return;
    const char *resolved = ui_settings_text_get(text, language);
    const lv_font_t *font = language == 0U ? cjk_font_for(saved->font)
                                         : english_font_for(saved->font);
    /* Choose by actual text, including any future CJK catalog. The resource
     * remains independent of the menu and of the original English wording. */
    bool non_ascii = false;
    for(const unsigned char *p = (const unsigned char *)resolved; *p; ++p)
        if(*p >= 0x80U) { non_ascii = true; break; }
    if(non_ascii && !cjk_covers(resolved, font)) font = cjk_font_for(saved->font);
    if(!cjk_covers(resolved, font)) {
        resolved = ui_settings_text_get(text, 1U);
        font = english_font_for(saved->font);
    }
    lv_label_set_text(label, resolved);
    if(lv_obj_get_style_text_font(label, 0) != font)
        lv_obj_set_style_text_font(label, font, 0);
    saved->text_id = text;
    saved->language = language;
    saved->ready = true;
    snprintf(saved->displayed, sizeof(saved->displayed), "%s", lv_label_get_text(label));
}

void ui_settings_label_set_text(lv_obj_t *label, const char *text)
{
    if(text) {
        label_translation_t *saved = label_translation(label, false);
        if(saved) {
            if(saved->ready && saved->text_id == UI_TEXT_NONE &&
               strcmp(text, saved->original) == 0) return;
            saved->text_id = UI_TEXT_NONE;
            saved->ready = false;
        }
        if(strcmp(lv_label_get_text(label), text) == 0) return;
    }
    lv_label_set_text(label, text);
}

void ui_settings_translate(lv_obj_t *root, uint8_t language)
{
    if(!root) return;
    /* Hidden branches are translated when they are next shown. */
    if(lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return;
#if LV_USE_ROLLER
    /* Roller owns a repeated numeric option label; translating that internal
     * buffer would truncate it and replace the wheel's fixed bold digit font. */
    if(lv_obj_check_type(root, &lv_roller_class)) return;
#endif
    if(lv_obj_check_type(root, &lv_label_class)) {
        label_translation_t *saved = label_translation(root, true);
        if(!saved) return;
        if(saved->text_id != UI_TEXT_NONE) {
            ui_settings_label_set_id(root, saved->text_id, language);
            return;
        }
        if(saved->ready && saved->language == language &&
           strcmp(lv_label_get_text(root), saved->displayed) == 0) return;
        /* LVGL DOTS 临时改写字符串；恢复原文后再缓存，避免把省略号当译文源。 */
        if(lv_label_get_long_mode(root) == LV_LABEL_LONG_MODE_DOTS)
            lv_label_set_text(root, NULL);
        const char *text = lv_label_get_text(root);
        /* An explicit text update invalidates the cache, including clearing a
         * label to the same empty string as the previous translation buffer. */
        if(!saved->ready || strcmp(text, saved->translated) != 0)
            snprintf(saved->original, sizeof(saved->original), "%s", text);
        /*
         * 标签文本是运行时拼出来的（如 "时间源: 中控"）。词库缺词时回退原文，
         * 中文子集字库缺字时回退英文，避免出现方块字撑破行宽。
         */
        if(language != 1U) {
            const char *source = saved->original;
            size_t used = 0;
            while(*source && used + 1 < sizeof(saved->translated)) {
                const char *replacement = NULL;
                size_t longest = 0;
                for(unsigned i = 0; i < sizeof(words)/sizeof(words[0]); ++i) {
                    size_t n = strlen(words[i].source);
                    if(n > longest && strncmp(source, words[i].source, n) == 0 &&
                       (source[n] == '\0' || source[n] == ' ' || source[n] == ':' || source[n] == ')' ||
                        words[i].source[n - 1] == ' ')) {
                        longest = n;
                        replacement = ui_settings_text_get(words[i].text, language);
                    }
                }
                if(replacement) {
                    size_t n = strlen(replacement);
                    if(used + n >= sizeof(saved->translated)) break;
                    memcpy(saved->translated + used, replacement, n);
                    used += n;
                    source += longest;
                }
                else saved->translated[used++] = *source++;
            }
            saved->translated[used] = '\0';
            const lv_font_t *font = language == 0U ? cjk_font_for(saved->font)
                                                    : english_font_for(saved->font);
            if(!cjk_covers(saved->translated, font)) font = cjk_font_for(saved->font);
            if(cjk_covers(saved->translated, font)) {
                lv_label_set_text(root, saved->translated);
                /* The selected subset contains Latin, symbols and CJK at the same pixel size. */
                lv_obj_set_style_text_font(root, font, 0);
            }
            else {
                lv_label_set_text(root, saved->original);
                lv_obj_set_style_text_font(root, saved->font, 0);
            }
        }
        else {
            snprintf(saved->translated, sizeof(saved->translated), "%s", english_text_for(saved->original));
            lv_label_set_text(root, saved->translated);
            lv_obj_set_style_text_font(root, english_font_for(saved->font), 0);
        }
        saved->ready = true;
        saved->language = language;
        snprintf(saved->displayed, sizeof(saved->displayed), "%s", lv_label_get_text(root));
    }
    for(uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        ui_settings_translate(lv_obj_get_child(root, i), language);
}
