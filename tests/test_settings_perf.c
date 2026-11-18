#include "test_util.h"
#include "ui/settings/ui_settings_page.h"
#include <time.h>

int g_checks;
int g_failures;
static uint32_t pixels[1920 * 180];
static unsigned renders;
static unsigned long long flushed_pixels;

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{
    (void)data;
    flushed_pixels += (unsigned long long)lv_area_get_width(area) * lv_area_get_height(area);
    lv_display_flush_ready(display);
}

static void rendered(lv_event_t *event)
{
    (void)event;
    ++renders;
}

static void settle(void)
{
    lv_obj_update_layout(lv_screen_active());
    lv_refr_now(NULL);
}

static void repeat_input(ui_settings_page_t *page)
{
    car_clock_t clock = {.year = 2026, .month = 11, .day = 17, .valid = true};
    car_units_t units = {.valid = true, .hour24 = true};
    ui_settings_page_set_date(page, &clock, &units, 2);
    ui_settings_page_set_eco_current(page, 80);
    ui_settings_page_set_speed_warning(page, true, 120);
    ui_settings_page_set_version(page, "MAIN-1", true, "MCU-1", true, "LINK-1", true);
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(1920, 720);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display, pixels, NULL, sizeof(pixels), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    lv_display_add_event_cb(display, rendered, LV_EVENT_RENDER_READY, NULL);
    lv_timer_pause(lv_display_get_refr_timer(display));
    car_settings_t settings;
    car_settings_reset(&settings);
    ui_settings_page_set_settings(&settings);
    ui_settings_page_t page;
    CHECK(ui_settings_page_create(&page, lv_screen_active()));
    ui_settings_page_set_active(&page, true);
    for(unsigned language = 0; language < 2; ++language) {
        settings.values.language = (uint8_t)language;
        page.st.view = UI_SETTINGS_VIEW_SYSTEM;
        page.st.root_index = UI_SETTINGS_ROOT_SYSTEM;
        ui_settings_page_apply(&page);
        settle();
        clock_t start = clock();
        flushed_pixels = 0;
        for(unsigned i = 0; i < 20; ++i) {
            ui_key_event_t key = {.pressed = 1, .code = i % 2 ? UI_KEY_UP : UI_KEY_DOWN};
            CHECK(ui_settings_page_key(&page, &key));
            settle();
        }
        printf("settings language=%u: 20 keys %.2fms, pixels=%llu\n", language,
               1000.0 * (clock() - start) / CLOCKS_PER_SEC, flushed_pixels);
        /* Adjacent rows within the viewport must not repaint the entire list. */
        page.st.system_index = 1;
        ui_settings_page_apply(&page);
        settle();
        flushed_pixels = 0;
        ui_key_event_t down = {.pressed = 1, .code = UI_KEY_DOWN};
        CHECK(ui_settings_page_key(&page, &down));
        settle();
        CHECK(page.st.system_index == 2);
        CHECK(flushed_pixels > 0 && flushed_pixels < 594ULL * 300);
        printf("settings adjacent row: pixels=%llu\n", flushed_pixels);
        repeat_input(&page);
        settle();
        renders = 0;
        for(unsigned i = 0; i < 20; ++i) {
            repeat_input(&page);
            settle();
        }
        printf("settings identical input: renders=%u\n", renders);
        CHECK(renders == 0);
        page.st.view = UI_SETTINGS_VIEW_EPS;
        page.st.eps_index = 0xFFU;
        ui_settings_page_apply(&page);
        settle();
        renders = 0;
        for(unsigned i = 0; i < 20; ++i) {
            ui_settings_page_apply(&page);
            settle();
        }
        CHECK(renders == 0);
        page.st.view = UI_SETTINGS_VIEW_FACTORY_PASSWORD;
        page.st.factory_index = 0U;
        ui_settings_page_apply(&page);
        settle();
        renders = 0;
        for(unsigned i = 0; i < 20; ++i) {
            ui_settings_page_apply(&page);
            settle();
        }
        CHECK(renders == 0);
        page.st.view = UI_SETTINGS_VIEW_DATE;
        ui_settings_page_apply(&page);
        repeat_input(&page);
        settle();
        renders = 0;
        repeat_input(&page);
        settle();
        CHECK(renders == 0);
    }
    ui_settings_page_destroy(&page);
    ui_settings_page_set_settings(NULL);
    lv_display_delete(display);
    lv_deinit();
    printf("Settings perf: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
