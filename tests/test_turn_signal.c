#include "test_util.h"

#include "ui/ComControl/ui_turn_signal.h"
#include "ui/ui_app.h"

int g_checks;
int g_failures;

static uint32_t count_timers(void)
{
    uint32_t count = 0U;
    for(lv_timer_t *timer = lv_timer_get_next(NULL); timer != NULL;
        timer = lv_timer_get_next(timer)) {
        count++;
    }
    return count;
}

static void check_signal(const ui_turn_signal_t *signal, bool left, bool right)
{
    CHECK(ui_lamp_get_state(&signal->left) == (left ? UI_LAMP_ON : UI_LAMP_OFF));
    CHECK(ui_lamp_get_state(&signal->right) == (right ? UI_LAMP_ON : UI_LAMP_OFF));
    CHECK(signal->left.blink == NULL && signal->right.blink == NULL);
}

static void test_component(void)
{
    ui_turn_signal_t signal = {0};
    car_turn_t turn = {0};
    lv_obj_t *holder = lv_obj_create(lv_screen_active());
    uint32_t timers = count_timers();

    CHECK(ui_turn_signal_create(&signal, holder, 0, 0, 60, 0, 54, 48));
    ui_turn_signal_apply(&signal, &turn);
    check_signal(&signal, false, false);
    turn.valid = true;
    turn.left = true;
    ui_turn_signal_apply(&signal, &turn);
    check_signal(&signal, true, false);
    lv_tick_inc(500);
    ui_turn_signal_apply(&signal, &turn);
    check_signal(&signal, false, false);
    turn.right = true;
    ui_turn_signal_apply(&signal, &turn);
    check_signal(&signal, false, false);
    lv_tick_inc(500);
    ui_turn_signal_apply(&signal, &turn);
    check_signal(&signal, true, true);
    ui_turn_signal_apply(&signal, NULL);
    ui_turn_signal_apply(NULL, &turn);
    check_signal(&signal, true, true);
    CHECK(count_timers() == timers);

    lv_obj_delete(holder);
    CHECK(count_timers() == timers);
    /* 同一组件存储重建时重置相位，不继承上一页的周期。 */
    holder = lv_obj_create(lv_screen_active());
    CHECK(ui_turn_signal_create(&signal, holder, 0, 0, 60, 0, 54, 48));
    check_signal(&signal, false, false);
    ui_turn_signal_apply(&signal, &turn);
    check_signal(&signal, true, true);
    lv_obj_delete(holder);
    lv_tick_inc(500);
    (void)lv_timer_handler();
    CHECK(count_timers() == timers);
    lv_free((void *)signal.left_dsc.data);
    lv_free((void *)signal.right_dsc.data);
}

static lv_obj_t *child_at(lv_obj_t *parent, int32_t x, int32_t y)
{
    for(int32_t i = 0; i < (int32_t)lv_obj_get_child_count(parent); ++i) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        if(lv_obj_get_x(child) == x && lv_obj_get_y(child) == y) {
            return child;
        }
    }
    return NULL;
}

static lv_obj_t *image_with_path(lv_obj_t *parent, const char *path)
{
    for(int32_t i = 0; i < (int32_t)lv_obj_get_child_count(parent); ++i) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        if(lv_obj_check_type(child, &lv_image_class)) {
            const void *src = lv_image_get_src(child);
            if(lv_image_src_get_type(src) == LV_IMAGE_SRC_FILE &&
               strstr((const char *)src, path) != NULL) {
                return child;
            }
        }
    }
    return NULL;
}

static void check_page(bool left, bool right)
{
    lv_obj_t *page = ui_app_current_root();
    lv_obj_t *arrows[2];
    lv_obj_t *lamps[2];
    lv_obj_t *stack;
    lv_obj_t *car;
    const bool expected[2] = {left, right};

    CHECK(page != NULL);
    if(page == NULL) return;
    lv_obj_update_layout(page);
    arrows[0] = child_at(page, 660, 98);
    arrows[1] = child_at(page, 1206, 98);
    stack = child_at(page, 553, 140);
    CHECK(stack != NULL);
    if(stack == NULL) return;
    car = child_at(stack, 87, 95);
    CHECK(car != NULL);
    if(car == NULL) return;
    lamps[0] = image_with_path(car, "SubaruForester/navigation/Deng_02.png");
    lamps[1] = image_with_path(car, "SubaruForester/navigation/Deng_03.png");
    for(unsigned i = 0; i < 2U; ++i) {
        CHECK(arrows[i] != NULL && lamps[i] != NULL);
        if(arrows[i] == NULL || lamps[i] == NULL) continue;
        bool arrow_on = !lv_obj_has_flag(arrows[i], LV_OBJ_FLAG_HIDDEN) &&
                        lv_obj_get_style_image_opa(arrows[i], 0) == LV_OPA_COVER;
        bool car_on = !lv_obj_has_flag(lamps[i], LV_OBJ_FLAG_HIDDEN);
        CHECK(arrow_on == expected[i]);
        CHECK(car_on == expected[i]);
        CHECK(arrow_on == car_on);
    }
}

static void step(car_state_view_t *view, uint32_t ms, bool left, bool right)
{
    lv_tick_inc(ms);
    ui_app_apply(view);
    check_page(left, right);
    /* apply 之后即使处理其它定时器，箭头也不能自行翻转。 */
    (void)lv_timer_handler();
    check_page(left, right);
}

static void test_page(void)
{
    car_state_view_t view = {0};
    uint32_t timers;
    uint32_t idle_timers = count_timers();

    CHECK(ui_app_init());
    CHECK(ui_app_set_style("forester1"));
    timers = count_timers();
    step(&view, 1000, false, false); /* 尚无数据 */
    view.has_data = true;
    view.state.turn.left = true;
    step(&view, 1000, false, false); /* 无有效转向输入不能点亮 */
    view.state.turn.valid = true;
    step(&view, 0, true, false);
    step(&view, 499, true, false);
    step(&view, 1, false, false);
    for(unsigned i = 0; i < 10U; ++i) step(&view, 0, false, false);

    /* 灭相位左切右、加入双闪，均不能重启某一侧。 */
    view.state.turn.left = false;
    view.state.turn.right = true;
    step(&view, 100, false, false);
    view.state.turn.left = true;
    step(&view, 100, false, false);
    step(&view, 300, true, true);
    /* 亮相位右切左再切右，仍沿用原周期边界。 */
    view.state.turn.right = false;
    step(&view, 100, true, false);
    view.state.turn.left = false;
    view.state.turn.right = true;
    step(&view, 100, false, true);
    step(&view, 300, false, false);
    view.state.turn.left = true;
    step(&view, 0, false, false);

    /* 跨过多个半周期，按绝对余量恢复，而不是仅翻转一次。 */
    step(&view, 2250, false, false); /* t=3750 */
    step(&view, 250, true, true);    /* t=4000 */
    for(uint32_t elapsed = 33U; elapsed <= 6600U; elapsed += 33U) {
        bool on = elapsed % 1000U < 500U;
        step(&view, 33, on, on);
    }
    step(&view, 10400, true, true);  /* t=21000，长延迟仍恢复亮相位 */

    /* 无效输入/无数据保持已有显示；有效输入恢复后继续原时钟。 */
    view.state.turn.valid = false;
    view.state.turn.left = false;
    view.state.turn.right = false;
    step(&view, 500, true, true);
    view.state.turn.valid = true;
    view.has_data = false;
    step(&view, 1000, true, true);
    view.has_data = true;
    view.state.turn.left = true;
    view.state.turn.right = true;
    step(&view, 0, false, false);

    view.state.turn.left = false;
    view.state.turn.right = false;
    step(&view, 0, false, false);
    view.state.turn.right = true;
    step(&view, 1, false, true); /* 关后重开立即亮 */
    step(&view, 500, false, false);
    view.state.turn.right = false;
    step(&view, 0, false, false);

    /* tick 在亮相位中回绕，500/1000ms 边界仍正确。 */
    lv_tick_inc(UINT32_MAX - lv_tick_get() - 249U);
    view.state.turn.left = true;
    view.state.turn.right = true;
    step(&view, 0, true, true);
    step(&view, 499, true, true);
    step(&view, 1, false, false);
    step(&view, 500, true, true);
    CHECK(count_timers() == timers);

    for(unsigned i = 0; i < 3U; ++i) {
        step(&view, 500, false, false);
        CHECK(ui_app_set_style("general1"));
        lv_tick_inc(750);
        (void)lv_timer_handler();
        CHECK(ui_app_set_style("forester1"));
        check_page(true, true); /* 重建后补发有效快照，从共同亮相位开始 */
        CHECK(count_timers() == timers);
    }
    ui_app_deinit();
    lv_tick_inc(1000);
    (void)lv_timer_handler();
    CHECK(count_timers() == idle_timers);
    CHECK(ui_app_init());
    CHECK(ui_app_set_style("forester1"));
    check_page(false, false);
    step(&view, 0, true, true);
    ui_app_deinit();
    CHECK(count_timers() == idle_timers);
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(1920, 720);
    if(display == NULL) return 1;
    lv_display_delete_refr_timer(display);
    test_component();
    test_page();
    lv_display_delete(display);
    lv_deinit();
    printf("转向灯同步回归：%d 项检查，%d 项失败\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
