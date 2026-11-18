/*
 * test_i18n.c - dashboard-scene localization regression
 */
#include "test_util.h"

#include "ui/ui_i18n.h"
#include "ui/ui_theme.h"
#include "ui/settings/ui_settings_language.h"

int g_checks;
int g_failures;

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    return label;
}

static const char *text_of(lv_obj_t *label)
{
    return lv_label_get_text(label);
}

static const lv_font_t *font_of(lv_obj_t *label)
{
    return lv_obj_get_style_text_font(label, 0);
}

static void test_translate(lv_obj_t *parent)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_t *title;
    lv_obj_t *connect;
    lv_obj_t *fuel;
    lv_obj_t *number;
    lv_obj_t *brand;
    lv_obj_t *eco_avg;
    lv_obj_t *fm;
    lv_obj_t *trip;
    lv_obj_t *placeholder;
    const lv_font_t *cjk36 = ui_i18n_font(&lv_font_montserrat_36);

    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, 0, 0);
    lv_obj_set_size(box, 800, 400);

    title = make_label(box, "SHORT TERM", &lv_font_montserrat_36);
    connect = make_label(box, "CURRENT MODE: CarPlay", &lv_font_montserrat_36);
    fuel = make_label(box, "LOW FUEL!", &lv_font_montserrat_24);
    number = make_label(box, "12 km", &lv_font_montserrat_28);
    brand = make_label(box, "4x4-i", &lv_font_montserrat_28);
    eco_avg = make_label(box, "AVG 12.3 L/100KM", &lv_font_montserrat_18);
    fm = make_label(box, "FM RADIO", &lv_font_montserrat_48);
    trip = make_label(box, "TRIP DIST 25 km", &lv_font_montserrat_28);
    placeholder = make_label(box, "GeneralUi1 (placeholder)",
                             &lv_font_montserrat_36);

    CHECK(cjk36 != NULL);
    CHECK(ui_font_pixels(ui_i18n_font(&lv_font_montserrat_44)) == 44U);
    CHECK(cjk36 != ui_i18n_font(&lv_font_montserrat_44));

    ui_i18n_set_language(true);
    CHECK(ui_i18n_is_chinese());
    ui_i18n_apply(box);

    CHECK(strcmp(text_of(title), "短期行程") == 0);
    CHECK(font_of(title) == cjk36);
    CHECK(strcmp(text_of(connect), "当前模式: CarPlay") == 0);
    CHECK(strcmp(text_of(fuel), "燃油低!") == 0);
    CHECK(strcmp(text_of(number), "12 km") == 0);
    CHECK(font_of(number) == &lv_font_montserrat_28);
    CHECK(strcmp(text_of(brand), "4x4-i") == 0);
    CHECK(strcmp(text_of(eco_avg), "平均 12.3 L/100KM") == 0);
    CHECK(strcmp(text_of(fm), "FM 收音机") == 0);
    CHECK(ui_font_pixels(font_of(fm)) == 48U);
    CHECK(strcmp(text_of(trip), "行驶距离 25 km") == 0);
    CHECK(strcmp(text_of(placeholder), "通用界面 1（占位）") == 0);

    /* Repeated frame passes must keep the translated text stable. */
    ui_i18n_apply(box);
    CHECK(strcmp(text_of(title), "短期行程") == 0);
    CHECK(strcmp(text_of(eco_avg), "平均 12.3 L/100KM") == 0);

    /* Pages rewrite dynamic labels with English every frame. */
    lv_label_set_text(title, "SHORT TERM");
    lv_label_set_text(eco_avg, "AVG 8.4 L/100KM");
    ui_i18n_apply(box);
    CHECK(strcmp(text_of(title), "短期行程") == 0);
    CHECK(strcmp(text_of(eco_avg), "平均 8.4 L/100KM") == 0);

    /* Switching back restores the original text and font. */
    ui_i18n_set_language(false);
    CHECK(!ui_i18n_is_chinese());
    ui_i18n_apply(box);
    CHECK(strcmp(text_of(title), "SHORT TERM") == 0);
    CHECK(font_of(title) == &lv_font_montserrat_36);
    CHECK(strcmp(text_of(connect), "CURRENT MODE: CarPlay") == 0);
    CHECK(strcmp(text_of(fuel), "LOW FUEL!") == 0);
    CHECK(font_of(fuel) == &lv_font_montserrat_24);
    CHECK(strcmp(text_of(eco_avg), "AVG 8.4 L/100KM") == 0);
    CHECK(font_of(eco_avg) == &lv_font_montserrat_18);
    CHECK(strcmp(text_of(fm), "FM RADIO") == 0);
    CHECK(font_of(fm) == &lv_font_montserrat_48);
    CHECK(strcmp(text_of(trip), "TRIP DIST 25 km") == 0);

    /* Deleting the tree frees per-label caches through LV_EVENT_DELETE. */
    lv_obj_delete(box);
}

static void count_invalidations(lv_event_t *event)
{
    unsigned *count = lv_event_get_user_data(event);
    ++*count;
}

static void test_unchanged_labels(lv_obj_t *parent)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_t *number = make_label(box, "120", &lv_font_montserrat_28);
    lv_display_t *display = lv_obj_get_display(box);
    unsigned invalidations = 0U;

    ui_i18n_set_language(true);
    ui_i18n_apply(box);
    lv_obj_update_layout(box);
    lv_display_add_event_cb(display, count_invalidations,
                            LV_EVENT_INVALIDATE_AREA, &invalidations);

    /* 先验证计数器，再检查静态中文界面不产生重复失效区域。 */
    lv_obj_invalidate(number);
    CHECK(invalidations > 0U);
    invalidations = 0U;
    ui_i18n_apply(box);
    CHECK(invalidations == 0U);

    lv_label_set_text(number, "121");
    lv_obj_update_layout(box);
    invalidations = 0U;
    ui_i18n_apply(box);
    CHECK(invalidations == 0U);
    CHECK(strcmp(text_of(number), "121") == 0);
    CHECK(font_of(number) == &lv_font_montserrat_28);

    lv_display_remove_event_cb_with_user_data(display, count_invalidations,
                                             &invalidations);
    lv_obj_delete(box);
}

static void test_warning_texts(lv_obj_t *parent)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_t *a;
    lv_obj_t *b;
    lv_obj_t *c;

    lv_obj_remove_style_all(box);

    a = make_label(box, "Key system fault", &lv_font_montserrat_18);
    b = make_label(box, "Press brake and START to drive",
                   &lv_font_montserrat_18);
    c = make_label(box, "CONNECT IS OFF\nPLEASE ENABLE AND SELECT A MODE",
                   &lv_font_montserrat_24);

    ui_i18n_set_language(true);
    ui_i18n_apply(box);

    CHECK(strcmp(text_of(a), "钥匙系统故障") == 0);
    CHECK(strcmp(text_of(b), "请踩制动并按 START 行驶") == 0);
    CHECK(strcmp(text_of(c), "互联已关闭\n请开启并选择模式") == 0);

    ui_i18n_set_language(false);
    ui_i18n_apply(box);
    CHECK(strcmp(text_of(a), "Key system fault") == 0);
    CHECK(strcmp(text_of(c), "CONNECT IS OFF\nPLEASE ENABLE AND SELECT A MODE") == 0);

    lv_obj_delete(box);
}

static void test_settings_catalogs(lv_obj_t *parent)
{
    for(ui_settings_text_id_t id = UI_TEXT_BACK; id < UI_TEXT_COUNT; ++id) {
        CHECK(ui_settings_text_get(id, 1U)[0] != '\0');
        CHECK(ui_settings_text_get(id, 0U)[0] != '\0');
        /* No French catalog yet: its stable language ID must fall back. */
        CHECK(strcmp(ui_settings_text_get(id, 8U), ui_settings_text_get(id, 1U)) == 0);
    }
    CHECK(strcmp(ui_settings_text_get(UI_TEXT_COUNT, 0U), "") == 0);
    CHECK(strcmp(ui_settings_text_get((ui_settings_text_id_t)-1, 0U), "") == 0);
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_t *label = make_label(box, "", ui_font_get(UI_TYPE_TEXT, 26U, false));
    const uint8_t languages[] = {1U, 0U, 8U, 255U, 0U, 1U};
    for(size_t i = 0; i < sizeof(languages); ++i) {
        uint8_t language = languages[i];
        ui_settings_label_set_id(label, UI_TEXT_PARK_VOLUME, language);
        CHECK(strcmp(text_of(label), language == 0U ? "雷达音量" : "Radar Vol") == 0);
        CHECK(ui_font_pixels(font_of(label)) == (language == 0U ? 26U : 22U));
        ui_settings_translate(box, language);
        CHECK(strcmp(text_of(label), language == 0U ? "雷达音量" : "Radar Vol") == 0);
    }
    /* A source-text page can reuse a previously keyed label (notably titles). */
    ui_settings_label_set_text(label, "VERSION INFO");
    ui_settings_translate(box, 0U);
    CHECK(strcmp(text_of(label), "版本信息") == 0);
    ui_settings_label_set_text(label, "");
    ui_settings_translate(box, 1U);
    CHECK(strcmp(text_of(label), "") == 0);
    ui_settings_label_set_id(label, UI_TEXT_EPS_MODE, 0U);
    lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
    ui_settings_translate(box, 1U);
    CHECK(strcmp(text_of(label), "电动助力转向") == 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_HIDDEN);
    ui_settings_translate(box, 1U);
    CHECK(strcmp(text_of(label), "EPS") == 0);
    lv_obj_delete(box);
}

int main(void)
{
    lv_init();
    /* Object-state test without window or rendering. */
    lv_display_t *display = lv_display_create(1920, 720);

    if(display == NULL) {
        return 1;
    }
    lv_display_delete_refr_timer(display);

    test_translate(lv_screen_active());
    test_warning_texts(lv_screen_active());
    test_unchanged_labels(lv_screen_active());
    test_settings_catalogs(lv_screen_active());

    lv_display_delete(display);
    lv_deinit();

    printf("中文化回归：%d 项检查，%d 项失败\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
