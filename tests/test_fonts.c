#include "test_util.h"
#include "ui/ui_fonts.h"
#include "ui/ui_i18n.h"
#include "ui/ui_app.h"
#include "ui/ui_theme.h"
#include "ui/SpeedDial/speed_forester1.h"
#include "ui/RotationDial/rotate_forester1.h"
#include "ui/settings/ui_settings_page.h"
#include "ui/PageModel/page_connect.h"
#include "ui/ui_common.h"
#include "ui/ComControl/ui_outside_temp.h"

#include <stdlib.h>

int g_checks;
int g_failures;

static void glyph_check(const lv_font_t *font, uint32_t cp)
{
    lv_font_glyph_dsc_t glyph = {0};
    CHECK(lv_font_get_glyph_dsc(font, &glyph, cp, 0));
    CHECK(!glyph.is_placeholder);
    CHECK(glyph.adv_w > 0);
}

static void test_fonts(void)
{
#define UI_QT_FONT(family, pixels, bold, name) \
    CHECK(ui_font_get(UI_TYPE_##family, pixels, bold) != NULL); \
    CHECK(ui_font_pixels(ui_font_get(UI_TYPE_##family, pixels, bold)) == pixels); \
    glyph_check(ui_font_get(UI_TYPE_##family, pixels, bold), '8');
#include "ui/fonts/qt_fonts.inc"
#undef UI_QT_FONT
    CHECK(ui_font_get(UI_TYPE_KONEXY, 26, false) == NULL);
    CHECK(ui_font_pixels(ui_font_cjk(&lv_font_montserrat_48)) == 48);
    CHECK(ui_font_pixels(ui_font_cjk(&lv_font_montserrat_44)) == 44);
    const lv_font_t *gear = ui_font_get(UI_TYPE_FORGOTTEN, 62, false);
    const char *const gears[] = {"P", "R", "N", "D", "S", "L", "M1", "M5"};
    for(size_t i = 0; i < sizeof(gears) / sizeof(gears[0]); ++i) {
        lv_point_t size;
        lv_text_get_size(&size, gears[i], gear, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        CHECK(size.x <= 90 && size.y <= 90);
        for(const char *c = gears[i]; *c; ++c) glyph_check(gear, (uint8_t)*c);
    }
    for(unsigned px = 20; px <= 48; px += 2) {
        const lv_font_t *font = UI_TEXT(px);
        if(font) { glyph_check(font, 0x8F66); glyph_check(font, 0x2103); }
    }
}

static void test_temperature_symbols(void)
{
    const lv_font_t *font = ui_font_get(UI_TYPE_BEBAS, 28, false);
    glyph_check(font, 0xB0);
    lv_font_glyph_dsc_t degree = {0};
    CHECK(lv_font_get_glyph_dsc(font, &degree, 0xB0, 'C'));
    CHECK(degree.box_w > 0 && degree.box_h > 0);
    for(unsigned fahrenheit = 0; fahrenheit < 2; ++fahrenheit) {
        ui_outside_temp_t temp = {0};
        car_other_info_t info = {.valid = true, .outside_temp_raw = 174};
        CHECK(ui_outside_temp_create(&temp, lv_screen_active(), 50, 50, font));
        ui_outside_temp_apply(&temp, &info, fahrenheit != 0);
        CHECK(strcmp(lv_label_get_text(temp.value), fahrenheit ? "77.0" : "25.0") == 0);
        for(unsigned mode = 0; mode < 3; ++mode) {
            ui_i18n_set_language(mode == 1);
            ui_i18n_apply(temp.root);
            CHECK(strcmp(lv_label_get_text(temp.unit), fahrenheit ? "°F" : "°C") == 0);
            const lv_font_t *shown_font = lv_obj_get_style_text_font(temp.unit, 0);
            glyph_check(shown_font, 0xB0);
            CHECK(ui_font_pixels(shown_font) == 28);
            if(mode != 1) CHECK(shown_font == font);
        }
        lv_obj_delete(temp.root);
    }
}

static void test_frame_font_changes(void)
{
    rotate_forester1_t rpm = {0};
    speed_forester1_t speed = {0};
    CHECK(rotate_forester1_create(&rpm, lv_screen_active(), 0, 0));
    CHECK(speed_forester1_create(&speed, lv_screen_active(), 650, 0));
    ui_i18n_set_language(true);
    ui_i18n_apply(rpm.dial.root);
    rotate_forester1_apply(&rpm, 1000);
    ui_i18n_apply(rpm.dial.root);
    CHECK(lv_obj_get_style_text_font(rpm.zero, 0) == UI_KONEXY(44));
    CHECK(lv_obj_get_style_text_font(rpm.labels[0], 0) == UI_KONEXY(48));
    rotate_forester1_apply(&rpm, 0);
    ui_i18n_apply(rpm.dial.root);
    CHECK(lv_obj_get_style_text_font(rpm.zero, 0) == UI_KONEXY(48));
    CHECK(lv_obj_get_style_text_font(rpm.labels[0], 0) == UI_KONEXY(44));
    speed_forester1_apply(&speed, 100, false);
    CHECK(speed.highlight == 5);
    CHECK(speed.dial.target_angle == -1200); /* Existing speed rule is preserved. */
    CHECK(lv_obj_get_style_text_font(speed.labels[5], 0) == UI_KONEXY(24));
    CHECK(lv_obj_get_style_transform_scale_x(speed.labels[5], 0) == 307);
    speed_forester1_apply(&speed, 100, true);
    CHECK(strcmp(lv_label_get_text(speed.labels[5]), "90") == 0);
    CHECK(speed.dial.target_angle == -1484);
    lv_obj_delete(rpm.dial.root);
    lv_obj_delete(speed.dial.root);

    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "LOW FUEL!");
    lv_obj_set_style_text_font(label, UI_TEXT_BOLD(42), 0);
    ui_i18n_apply(label);
    CHECK(strcmp(lv_label_get_text(label), "燃油低!") == 0);
    CHECK(lv_obj_get_style_text_font(label, 0) == UI_TEXT_BOLD(42));
    ui_i18n_set_language(false);
    ui_i18n_apply(label);
    CHECK(strcmp(lv_label_get_text(label), "LOW FUEL!") == 0);
    CHECK(lv_obj_get_style_text_font(label, 0) == UI_TEXT_BOLD(42));
    lv_obj_delete(label);
}

static void check_unclipped(lv_obj_t *obj)
{
    lv_area_t bounds;
    lv_obj_get_coords(obj, &bounds);
    for(lv_obj_t *parent = lv_obj_get_parent(obj); parent; parent = lv_obj_get_parent(parent)) {
        lv_area_t clip;
        lv_obj_get_coords(parent, &clip);
        if(lv_obj_has_flag(parent, LV_OBJ_FLAG_OVERFLOW_VISIBLE)) {
            int32_t extra = 0;
            lv_obj_send_event(parent, LV_EVENT_REFR_EXT_DRAW_SIZE, &extra);
            lv_area_increase(&clip, extra, extra);
        }
        CHECK(bounds.x1 >= clip.x1 && bounds.x2 <= clip.x2);
        CHECK(bounds.y1 >= clip.y1 && bounds.y2 <= clip.y2);
    }
}

static void test_connect_text_bounds(void)
{
    connect_card_t card = {0};
    lv_obj_t *stack = ui_blank_container(lv_screen_active());
    lv_obj_set_size(stack, 814, 509);
    lv_obj_set_pos(stack, 553, 140);
    CHECK(connect_card_create(&card, stack));
    const char *const modes[] = {"Android Auto", "CarPlay", "HiCar"};
    for(unsigned language = 0; language < 2; ++language) {
        ui_i18n_set_language(language == 0);
        for(unsigned mode = 0; mode < 3; ++mode) {
            connect_card_demo_set(&card, 1, modes[mode], "DASHBOARD");
            ui_i18n_apply(stack);
            lv_obj_update_layout(stack);
            for(uint32_t row = 0; row < lv_obj_get_child_count(card.waiting); ++row) {
                lv_obj_t *item = lv_obj_get_child(card.waiting, (int32_t)row);
                for(uint32_t child = 0; child < lv_obj_get_child_count(item); ++child)
                    check_unclipped(lv_obj_get_child(item, (int32_t)child));
            }
        }
    }
    connect_card_destroy(&card);
    lv_obj_delete(stack);
    ui_i18n_set_language(false);
}

static uint32_t s_frame[1920 * 720];

static void flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area;
    (void)pixels;
    lv_display_flush_ready(display);
}

static void save_frame(const char *path)
{
    lv_refr_now(NULL);
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    if(!file) return;
    fprintf(file, "P6\n1920 720\n255\n");
    for(size_t i = 0; i < sizeof(s_frame) / sizeof(s_frame[0]); ++i) {
        uint8_t rgb[] = {(uint8_t)(s_frame[i] >> 16), (uint8_t)(s_frame[i] >> 8), (uint8_t)s_frame[i]};
        fwrite(rgb, 1, 3, file);
    }
    fclose(file);
}

static void save_variant(const char *path, const char *suffix)
{
    char variant[1024];
    snprintf(variant, sizeof(variant), "%s-%s.ppm", path, suffix);
    save_frame(variant);
}

static void render_main(const char *path)
{
    car_settings_t settings;
    car_settings_reset(&settings);
    settings.values.center_card_mask = 0x3ffU;
    car_state_view_t view = {0};
    car_state_init(&view.state);
    view.has_data = true;
    view.state.speed_rpm.valid = true;
    view.state.speed_rpm.speed_kph = 100;
    view.state.speed_rpm.engine_rpm = 2000;
    view.state.gear_on = true;
    view.state.gear.valid = true;
    snprintf(view.state.gear.text, sizeof(view.state.gear.text), "M1");
    view.state.fuel.valid = true;
    view.state.fuel.percent = 65;
    view.state.fuel.remaining_range_km = 358;
    view.state.fuel.port_left = true;
    view.state.coolant.valid = true;
    view.state.coolant.percent = 50;
    view.state.mileage.valid = true;
    view.state.mileage.odometer_raw = 12345;
    view.state.mileage.trip_a_raw = 1234;
    view.state.mileage.trip_type = 1;
    view.state.clock.valid = true;
    view.state.clock.hour = 12;
    view.state.clock.minute = 34;
    view.state.clock.hour24 = true;
    CHECK(ui_app_init());
    ui_app_settings_set_settings(&settings);
    CHECK(ui_app_set_style("forester1"));
    ui_app_apply(&view);
    lv_tick_inc(150);
    lv_anim_refr_now();
    ui_app_apply(&view);
    save_frame(path);
    /* Actual dashboard placement, including both English temperature units. */
    uint8_t saved_temperature_unit = settings.values.unit_temperature;
    settings.values.out_temperature = 1;
    settings.values.unit_temperature = CAR_UNIT_METRIC;
    view.state.other_info.valid = true;
    view.state.other_info.outside_temp_raw = 174;
    ui_app_apply(&view);
    save_variant(path, "temperature-c-en");
    settings.values.unit_temperature = CAR_UNIT_IMPERIAL;
    ui_app_apply(&view);
    save_variant(path, "temperature-f-en");
    settings.values.unit_temperature = saved_temperature_unit;
    settings.values.out_temperature = 0;
    settings.values.language = 0;
    ui_app_apply(&view);
    save_variant(path, "zh");
    const ui_key_event_t next = {.code = UI_KEY_NEXT, .pressed = 1};
    for(unsigned i = 0; i < 8; ++i) ui_app_handle_key(&next);
    ui_app_demo_media(0, "MEDIA", "FM RADIO", "98.5");
    ui_app_apply(&view);
    save_variant(path, "fm-zh");
    settings.values.language = 1;
    ui_app_apply(&view);
    save_variant(path, "fm-en");
    ui_app_demo_media(2, "INCOMING CALL", "DRIVER", "+86 123 4567 8901");
    ui_app_apply(&view);
    save_variant(path, "phone-en");
    settings.values.language = 0;
    ui_app_demo_media(2, "INCOMING CALL", "驾驶员", "+86 123 4567 8901");
    ui_app_apply(&view);
    save_variant(path, "phone-zh");
    ui_app_handle_key(&next);
    ui_app_demo_connect(1, "CarPlay", "DASHBOARD");
    ui_app_apply(&view);
    save_variant(path, "connect-zh");
    settings.values.language = 1;
    ui_app_apply(&view);
    save_variant(path, "connect-en");

    /* 使用实际仪表底图验证设置覆盖层，速度为零以满足设置页门控。 */
    view.state.speed_rpm.speed_kph = 0;
    ui_app_settings_open();
    ui_app_apply(&view);
    save_variant(path, "settings-root");
    const ui_key_event_t down = {.code = UI_KEY_DOWN, .pressed = 1};
    const ui_key_event_t ok = {.code = UI_KEY_OK, .pressed = 1};
    for(unsigned i = 0; i < 4; ++i) ui_app_handle_key(&down);
    ui_app_handle_key(&ok);
    ui_app_apply(&view);
    save_variant(path, "settings-system-integrated-en");
    settings.values.language = 0;
    ui_app_settings_refresh();
    save_variant(path, "settings-system-integrated-zh");
    settings.values.language = 1;
    ui_app_settings_close();
    ui_app_settings_set_settings(NULL);
    ui_app_deinit();

    ui_settings_page_t menu = {0};
    ui_settings_page_set_settings(&settings);
    CHECK(ui_settings_page_create(&menu, lv_screen_active()));
    menu.st.view = UI_SETTINGS_VIEW_SYSTEM;
    menu.st.root_index = UI_SETTINGS_ROOT_SYSTEM;
    menu.st.system_index = 0;
    ui_settings_page_set_active(&menu, true);
    save_variant(path, "settings-en");
    settings.values.language = 0;
    ui_settings_page_apply(&menu);
    save_variant(path, "settings-zh");
    menu.st.view = UI_SETTINGS_VIEW_MY_CAR;
    menu.st.root_index = UI_SETTINGS_ROOT_MY_CAR;
    menu.st.mycar_locked = true;
    menu.st.mycar_index = 0;
    ui_settings_page_apply(&menu);
    save_variant(path, "settings-mycar-zh");
    menu.st.view = UI_SETTINGS_VIEW_CENTER_CARDS;
    menu.st.card_index = 0;
    ui_settings_page_apply(&menu);
    save_variant(path, "settings-cards-zh");
    ui_settings_page_destroy(&menu);
    ui_settings_page_set_settings(NULL);
}

int main(int argc, char **argv)
{
    lv_init();
    lv_display_t *display = lv_display_create(1920, 720);
    if(!display) return 1;
    lv_timer_pause(lv_display_get_refr_timer(display));
    lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_flush_cb(display, flush_cb);
    lv_display_set_buffers(display, s_frame, NULL, sizeof(s_frame), LV_DISPLAY_RENDER_MODE_DIRECT);
    test_fonts();
    test_temperature_symbols();
    test_frame_font_changes();
    test_connect_text_bounds();
    if(argc > 1) render_main(argv[1]);
    lv_display_delete(display);
    lv_deinit();
    printf("Fonts: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
