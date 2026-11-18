/* PagePasswordSettings.qml: 466x190 原图，458x100 横排，四列 90px + OK。
 * 数字由真正的无限滚轮绘制和滚动；界面文字翻译不处理滚轮内部数字标签。 */
#define PASSWORD_W 466
#define PASSWORD_H 190
#define PASSWORD_X ((SETTINGS_W - PASSWORD_W) / 2)
#define PASSWORD_COLOR_FOCUS 0xFF0000

static void password_color(lv_obj_t *object, uint32_t color, lv_part_t part)
{
    if(lv_color_to_u32(lv_obj_get_style_text_color(object, part)) !=
       lv_color_to_u32(lv_color_hex(color)))
        lv_obj_set_style_text_color(object, lv_color_hex(color), part);
}

static void password_focus_next(ui_settings_state_t *state)
{
    if(state->factory_index > 5U) state->factory_index = 0U;
    else if(state->factory_child_back && state->factory_index > 0U) --state->factory_index;
    else ++state->factory_index;
    if(state->factory_index == 0U) state->factory_child_back = false;
    if(state->factory_index == 5U) state->factory_child_back = true;
}

static void password_enter(ui_settings_page_t *page)
{
    page->st.view = UI_SETTINGS_VIEW_FACTORY_PASSWORD;
    page->st.factory_index = 0U;
    page->st.factory_child_back = false;
    page->st.factory_error = false;
    page->st.factory_unlocked = false;
    memset(page->st.factory_digits, 0, sizeof(page->st.factory_digits));
    for(unsigned col = 0; col < 4U; ++col)
        lv_roller_set_selected(page->password_rollers[col], 0U, LV_ANIM_OFF);
}

static void password_render(ui_settings_page_t *page)
{
    lv_obj_remove_flag(page->password_list, LV_OBJ_FLAG_HIDDEN);
    for(unsigned col = 0; col < 4U; ++col) {
        lv_obj_t *roller = page->password_rollers[col];
        password_color(roller, page->st.factory_index == col ? PASSWORD_COLOR_FOCUS : UI_COLOR_WHITE,
                       LV_PART_SELECTED);
        uint8_t digit = page->st.factory_digits[col] % 10U;
        if(lv_roller_get_selected(roller) != digit)
            lv_roller_set_selected(roller, digit, LV_ANIM_ON);
    }
    lv_obj_t *ok = lv_obj_get_child(page->password_ok, 0);
    lv_obj_t *back = lv_obj_get_child(page->password_back, 0);
    ui_settings_label_set_id(ok, UI_TEXT_PASSWORD_OK, model_language());
    ui_settings_label_set_id(back, UI_TEXT_PASSWORD_BACK, model_language());
    password_color(ok, page->st.factory_index == 4U ? PASSWORD_COLOR_FOCUS : UI_COLOR_WHITE, LV_PART_MAIN);
    password_color(back, page->st.factory_index == 5U ? PASSWORD_COLOR_FOCUS : UI_COLOR_WHITE, LV_PART_MAIN);
}

static bool password_key(ui_settings_page_t *page, uint8_t code)
{
    ui_settings_state_t *state = &page->st;
    if(state->factory_index > 5U) state->factory_index = 0U;
    uint8_t focus = state->factory_index;
    bool next = code == UI_KEY_UP || code == UI_KEY_RIGHT;
    bool previous = code == UI_KEY_DOWN || code == UI_KEY_LEFT;
    if(next || previous) {
        if(focus < 4U) {
            state->factory_digits[focus] = (state->factory_digits[focus] + (next ? 1U : 9U)) % 10U;
            state->factory_error = false;
        }
        else password_focus_next(state);
        return true;
    }
    if(code != UI_KEY_OK) return false;
    if(focus < 4U) password_focus_next(state);
    else if(focus == 4U) {
        state->factory_error = state->factory_digits[0] != 1U || state->factory_digits[1] != 6U ||
                               state->factory_digits[2] != 8U || state->factory_digits[3] != 8U;
        if(!state->factory_error) {
            state->factory_unlocked = true;
            state->view = UI_SETTINGS_VIEW_FACTORY;
            state->factory_index = 0U;
        }
    }
    else {
        state->view = UI_SETTINGS_VIEW_SYSTEM;
        state->system_index = 3U;
        state->factory_unlocked = false;
    }
    return true;
}

static void password_roller_changed(lv_event_t *event)
{
    ui_settings_page_t *page = lv_event_get_user_data(event);
    if(page->st.view != UI_SETTINGS_VIEW_FACTORY_PASSWORD) return;
    lv_obj_t *roller = lv_event_get_current_target(event);
    for(unsigned col = 0; col < 4U; ++col) {
        if(roller != page->password_rollers[col]) continue;
        page->st.factory_digits[col] = (uint8_t)lv_roller_get_selected(roller);
        page->st.factory_index = (uint8_t)col;
        page->st.factory_error = false;
        ui_settings_page_apply(page);
        return;
    }
}

static void password_create(ui_settings_page_t *page)
{
    page->password_list = make_list(page->body);
    lv_obj_set_height(page->password_list, SETTINGS_H - SUBPAGE_BODY_Y);
    page->password_bg = lv_image_create(page->password_list);
    lv_image_set_src(page->password_bg, UI_IMG("settings/tumbler_pwd.png"));
    lv_obj_set_pos(page->password_bg, PASSWORD_X, 0);
    lv_obj_set_size(page->password_bg, PASSWORD_W, PASSWORD_H);
    for(unsigned col = 0; col < 4U; ++col) {
        lv_obj_t *roller = lv_roller_create(page->password_bg);
        lv_obj_remove_style_all(roller);
        lv_obj_set_style_text_font(roller, UI_TEXT_BOLD(28), LV_PART_MAIN);
        lv_obj_set_style_text_font(roller, UI_TEXT_BOLD(28), LV_PART_SELECTED);
        lv_obj_set_style_text_color(roller, lv_color_hex(UI_COLOR_WHITE), LV_PART_MAIN);
        lv_obj_set_style_text_align(roller, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_line_space(roller, 33 - lv_font_get_line_height(UI_TEXT_BOLD(28)), LV_PART_MAIN);
        lv_obj_set_style_anim_duration(roller, 150, LV_PART_MAIN);
        lv_roller_set_options(roller, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9", LV_ROLLER_MODE_INFINITE);
        lv_roller_set_visible_row_count(roller, 3U);
        lv_obj_set_size(roller, 90, 100);
        lv_obj_set_pos(roller, 4 + 92 * (int32_t)col, 45);
        lv_obj_add_event_cb(roller, password_roller_changed, LV_EVENT_VALUE_CHANGED, page);
        page->password_rollers[col] = roller;
    }
    page->password_ok = make_row(page->password_bg, 372, 65, 90, 60, "OK");
    page->password_back = make_row(page->password_list, PASSWORD_X, PASSWORD_H,
                                   PASSWORD_W, ROW_H, "BACK");
}
