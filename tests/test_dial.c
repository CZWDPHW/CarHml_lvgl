#include "test_util.h"

#include "ui/SpeedDial/speed_forester1.h"
#include "ui/RotationDial/rotate_forester1.h"
#include "ui/ui_app.h"

int g_checks;
int g_failures;

static void advance(uint32_t ms)
{
    lv_tick_inc(ms);
    lv_anim_refr_now();
}

static void check_angle(const ui_dial_t *dial, int32_t angle)
{
    int32_t normalized = (angle % 3600 + 3600) % 3600;
    CHECK(lv_image_get_rotation(dial->needle) == normalized);
}

static void create_dial(ui_dial_t *dial, int32_t angle)
{
    static const uint8_t pixel[4] = {255, 255, 255, 255};
    static const lv_image_dsc_t needle = {
        .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_ARGB8888,
                   .w = 1, .h = 1, .stride = 4},
        .data_size = sizeof(pixel),
        .data = pixel,
    };
    ui_dial_create(dial, lv_screen_active(), 0, 0, 620,
                   "SubaruForester/speed_bg1.bin", &needle, 0, 0, 0, 0, angle);
}

static void test_same_target(void)
{
    ui_dial_t dial = {0};
    uint16_t baseline = lv_anim_count_running();
    create_dial(&dial, -1950);
    check_angle(&dial, -1950);
    CHECK(lv_anim_count_running() == baseline);
    ui_dial_set_angle(&dial, -1950);
    CHECK(lv_anim_count_running() == baseline);
    ui_dial_set_angle(&dial, -950);
    check_angle(&dial, -1950);
    for(unsigned elapsed = 20; elapsed <= 100; elapsed += 20) {
        advance(20);
        lv_anim_t *anim = lv_anim_get(dial.needle, NULL);
        int32_t time = anim != NULL ? anim->act_time : 0;
        for(unsigned i = 0; i < 5; ++i) ui_dial_set_angle(&dial, -950);
        CHECK(lv_anim_get(dial.needle, NULL) == anim);
        if(anim != NULL) CHECK(anim->act_time == time);
        int32_t angle = -1950 + (int32_t)elapsed * 10;
        int32_t actual = lv_image_get_rotation(dial.needle);
        CHECK(actual >= angle + 3600 - 2 && actual <= angle + 3600 + 2);
    }
    check_angle(&dial, -950);
    CHECK(lv_anim_count_running() == baseline);
    lv_obj_delete(dial.root);
}

static void test_redirect(void)
{
    static const uint32_t times[] = {0, 1, 20, 50, 99, 100, 150};
    for(size_t i = 0; i < sizeof(times) / sizeof(times[0]); ++i) {
        ui_dial_t dial = {0};
        create_dial(&dial, -500);
        ui_dial_set_angle(&dial, 500);
        advance(times[i]);
        lv_anim_t *anim = lv_anim_get(dial.needle, NULL);
        int32_t current = anim != NULL ? anim->current_value : 500;
        int32_t displayed = lv_image_get_rotation(dial.needle);
        /* 两次动画处理之间到来的新目标也必须接续最近显示值。 */
        lv_tick_inc(7);
        ui_dial_set_angle(&dial, -1000);
        CHECK(lv_image_get_rotation(dial.needle) == displayed);
        anim = lv_anim_get(dial.needle, NULL);
        CHECK(anim != NULL);
        if(anim != NULL) {
            CHECK(anim->start_value == current);
            CHECK(anim->end_value == -1000);
            CHECK(anim->duration == 100);
            CHECK(anim->path_cb == lv_anim_path_linear);
        }
        advance(50);
        anim = lv_anim_get(dial.needle, NULL);
        CHECK(anim != NULL);
        if(anim != NULL) {
            int32_t midpoint = (current - 1000) / 2;
            CHECK(anim->current_value >= midpoint - 1 && anim->current_value <= midpoint + 1);
        }
        advance(50);
        check_angle(&dial, -1000);
        CHECK(lv_anim_get(dial.needle, NULL) == NULL);
        lv_obj_delete(dial.root);
    }

    ui_dial_t dial = {0};
    create_dial(&dial, -500);
    ui_dial_set_angle(&dial, 500);
    advance(50);
    check_angle(&dial, 0);
    ui_dial_set_angle(&dial, 0); /* 改向当前显示角度：取消旧目标，不再回跳。 */
    CHECK(lv_anim_get(dial.needle, NULL) == NULL);
    advance(100);
    check_angle(&dial, 0);
    lv_obj_delete(dial.root);
}

static void test_components(void)
{
    speed_forester1_t speed = {0};
    rotate_forester1_t rotate = {0};
    uint16_t baseline = lv_anim_count_running();
    for(unsigned cycle = 0; cycle < 3; ++cycle) {
        lv_obj_t *holder = lv_obj_create(lv_screen_active());
        CHECK(speed_forester1_create(&speed, holder, 0, 0));
        CHECK(rotate_forester1_create(&rotate, holder, 0, 0));
        check_angle(&speed.dial, -1950);
        check_angle(&rotate.dial, 1800);
        CHECK(lv_anim_count_running() == baseline);
        speed_forester1_apply(&speed, 100, false);
        rotate_forester1_apply(&rotate, 2000);
        CHECK(speed.highlight == 5 && rotate.highlight == 1);
        check_angle(&speed.dial, -1950);
        check_angle(&rotate.dial, 1800);
        for(unsigned i = 0; i < 5; ++i) {
            advance(20);
            speed_forester1_apply(&speed, 100, false);
            rotate_forester1_apply(&rotate, 2000);
        }
        check_angle(&speed.dial, -1200);
        check_angle(&rotate.dial, 1320);
        CHECK(lv_anim_count_running() == baseline);
        speed_forester1_apply(&speed, 100, true);
        CHECK(strcmp(lv_label_get_text(speed.unit), "mph") == 0);
        CHECK(strcmp(lv_label_get_text(speed.labels[5]), "90") == 0);
        advance(100);
        check_angle(&speed.dial, -1484);
        speed_forester1_apply(&speed, 200, true);
        rotate_forester1_apply(&rotate, 8000);
        advance(40);
        CHECK(lv_anim_count_running() == baseline + 2);
        lv_obj_delete(holder); /* 父树销毁时两根指针仍在运动。 */
        CHECK(lv_anim_count_running() == baseline);
        advance(200);
        CHECK(lv_anim_count_running() == baseline);
    }
}

static bool has_text(lv_obj_t *parent, const char *text)
{
    for(int32_t i = 0; i < (int32_t)lv_obj_get_child_count(parent); ++i) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        if(lv_obj_check_type(child, &lv_label_class) &&
           strcmp(lv_label_get_text(child), text) == 0) return true;
    }
    return false;
}

static void test_page_lifecycle(void)
{
    car_state_view_t view = {0};
    uint16_t baseline = lv_anim_count_running();
    view.has_data = true;
    view.state.speed_rpm.valid = true;
    view.state.speed_rpm.speed_kph = 100;
    view.state.speed_rpm.engine_rpm = 2000;
    CHECK(ui_app_init());
    for(unsigned i = 0; i < 3; ++i) {
        CHECK(ui_app_set_style("forester1"));
        ui_app_apply(&view);
        advance(40);
        CHECK(lv_anim_count_running() == baseline + 2);
        CHECK(ui_app_set_style("general1"));
        CHECK(lv_anim_count_running() == baseline);
        CHECK(has_text(ui_app_current_root(), "SPEED: 100 km/h"));
        CHECK(has_text(ui_app_current_root(), "RPM: 2000"));
        view.state.speed_rpm.speed_kph = 120;
        view.state.speed_rpm.engine_rpm = 3000;
        ui_app_apply(&view);
        CHECK(has_text(ui_app_current_root(), "SPEED: 120 km/h"));
        CHECK(has_text(ui_app_current_root(), "RPM: 3000"));
        view.state.speed_rpm.speed_kph = 100;
        view.state.speed_rpm.engine_rpm = 2000;
        ui_app_apply(&view);
    }
    CHECK(ui_app_set_style("forester1"));
    CHECK(lv_anim_count_running() == baseline + 2);
    ui_app_deinit();
    CHECK(lv_anim_count_running() == baseline);
    advance(200);
    CHECK(lv_anim_count_running() == baseline);
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(1920, 720);
    if(display == NULL) return 1;
    lv_display_delete_refr_timer(display);
    test_same_target();
    test_redirect();
    test_components();
    test_page_lifecycle();
    lv_display_delete(display);
    lv_deinit();
    printf("指针插值回归：%d 项检查，%d 项失败\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
