/* Included by the settings page to share its command and row helpers. */
static const char *const factory_names[] = {
    "TOTAL MILEAGE", "FUEL TANK", "DATA DEBUG: UNAVAILABLE", "TIRE DISPLAY",
    "CAR MODEL", "FUNCTION SWITCHES", "DOOR DISPLAY", "RADAR", "4WD DISPLAY",
    "FUEL FILLER RIGHT", "RIGHT HAND DRIVE", "LOGO: UNAVAILABLE", "CONFIRM / BACK"
};
static const char *const feature_names[] = {
    "ELECTRIC POWER STEERING", "REMOTE ENGINE START", "SMART DOOR LOCK",
    "SELECT UNLOCK", "EXTERNAL DOOR", "SINGLE DOOR UNLOCK", "HEADLIGHT SENSITIVITY",
    "HEADLIGHT OFF DELAY", "EMERGENCY BRAKING", "REAR CROSS TRAFFIC ALERT", "AUTO STOP", "BACK"
};
static const char *const language_names[] = {
    "Chinese", "English", "Traditional Chinese", "Russian", "Spanish", "German",
    "Turkish", "Korean", "French", "Italian", "Portuguese", "Bulgarian", "Polish",
    "Romanian", "Dutch", "Czech", "Japanese", "Greek", "Arabic", "BACK"
};
static unsigned factory_count(ui_settings_page_t *p)
{
    switch(p->st.view) {
    case UI_SETTINGS_VIEW_LANGUAGE: return 20;
    case UI_SETTINGS_VIEW_FACTORY_PASSWORD: return 6;
    case UI_SETTINGS_VIEW_FACTORY_ODO: return 8;
    case UI_SETTINGS_VIEW_FACTORY_TANK: return 3;
    case UI_SETTINGS_VIEW_FACTORY_FEATURES: return 12;
    case UI_SETTINGS_VIEW_FACTORY_MODEL: return p->factory_models.valid && p->factory_models.parsed ?
        (p->factory_models.parsed > CAR_MODEL_MAX ? CAR_MODEL_MAX : p->factory_models.parsed) + 1U : 2U;
    default: return 13;
    }
}
static void factory_render(ui_settings_page_t *p)
{
    unsigned count = factory_count(p);
    car_settings_values_t *v = s_settings ? &s_settings->values : NULL;
    lv_obj_remove_flag(p->factory_list, LV_OBJ_FLAG_HIDDEN);
    for(unsigned i = 0; i < 20; ++i) {
        lv_obj_t *r = p->factory_rows[i];
        if(i >= count) { lv_obj_add_flag(r, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_remove_flag(r, LV_OBJ_FLAG_HIDDEN);
        char text[160];
        bool enabled = true;
        switch(p->st.view) {
        case UI_SETTINGS_VIEW_LANGUAGE:
            enabled = i < 2 || i == 19;
            snprintf(text, sizeof(text), "%s%s%s", language_names[i],
                !enabled ? " : UNSUPPORTED" : "", v && v->language == i ? " *" : "");
            break;
        case UI_SETTINGS_VIEW_FACTORY_PASSWORD:
        case UI_SETTINGS_VIEW_FACTORY_ODO: {
            unsigned digits = p->st.view == UI_SETTINGS_VIEW_FACTORY_PASSWORD ? 4 : 6;
            if(i < digits) snprintf(text, sizeof(text), "%u: %u", i + 1, p->st.factory_digits[i]);
            else snprintf(text, sizeof(text), "%s", i == digits ?
                (p->st.factory_error ? "WRONG PASSWORD" : "CONFIRM") : "CANCEL / BACK");
            break;
        }
        case UI_SETTINGS_VIEW_FACTORY_FEATURES:
            snprintf(text, sizeof(text), "%s%s%s", feature_names[i], i < 11 && v ?
                ((v->factory_features & (1U << i)) ? " : ON" : " : OFF") : "",
                i == 10 ? " (CONFIG ONLY)" : "");
            break;
        case UI_SETTINGS_VIEW_FACTORY_TANK:
            snprintf(text, sizeof(text), "%s", i == 0 ? "57 L" : i == 1 ? "69 L" : "BACK");
            enabled = i == 2 || s_cmd_cb;
            break;
        case UI_SETTINGS_VIEW_FACTORY_MODEL:
            snprintf(text, sizeof(text), "%s", i == count - 1 ? "BACK" :
                !p->factory_models.valid || !p->factory_models.parsed ? "NO MODEL DATA" : p->factory_models.entries[i].name);
            enabled = i == count - 1 || (s_cmd_cb && p->factory_models.valid && p->factory_models.parsed);
            break;
        default: {
            int on = -1;
            if(v) {
                if(i == 3) on = v->tire_on;
                if(i == 6) on = v->door_on;
                if(i == 7) on = v->radar_on;
                if(i == 8) on = v->show_4h;
                if(i == 9) on = v->fuel_filler_right;
                if(i == 10) on = v->right_hand_drive;
            }
            enabled = i != 2 && i != 11 && (s_cmd_cb || (i != 7 && i != 9));
            snprintf(text, sizeof(text), "%s%s", factory_names[i],
                !enabled && i != 2 && i != 11 ? " : UNAVAILABLE" : on < 0 ? "" : on ? " : ON" : " : OFF");
            break;
        }
        }
        ui_settings_label_set_text(lv_obj_get_child(r, 0), text);
        row_style(r, p->st.factory_index == i, enabled);
    }
    list_center(p->factory_list, p->st.factory_index, ROW_H);
}
static bool factory_key(ui_settings_page_t *p, uint8_t code)
{
    ui_settings_state_t *st = &p->st;
    unsigned i = st->factory_index, count = factory_count(p);
    bool password = st->view == UI_SETTINGS_VIEW_FACTORY_PASSWORD;
    bool digits = password || st->view == UI_SETTINGS_VIEW_FACTORY_ODO;
    unsigned n = password ? 4 : 6;
    if(st->view != UI_SETTINGS_VIEW_LANGUAGE && !password && !st->factory_unlocked) {
        st->view = UI_SETTINGS_VIEW_FACTORY_PASSWORD;
        st->factory_index = 0;
        return true;
    }
    if(code == UI_KEY_UP || code == UI_KEY_DOWN) {
        if(digits && i < n) st->factory_digits[i] = (st->factory_digits[i] + (code == UI_KEY_UP ? 1 : 9)) % 10;
        else st->factory_index = (i + count + (code == UI_KEY_UP ? -1 : 1)) % count;
        return true;
    }
    if(code == UI_KEY_LEFT || code == UI_KEY_RIGHT) {
        st->factory_index = (i + count + (code == UI_KEY_LEFT ? -1 : 1)) % count;
        return true;
    }
    if(code != UI_KEY_OK) return false;
    car_settings_values_t *v = s_settings ? &s_settings->values : NULL;
    car_cmd_t cmd;
    if(st->view == UI_SETTINGS_VIEW_LANGUAGE) {
        if(i == 19) st->view = UI_SETTINGS_VIEW_SYSTEM;
        else if(i < 2 && v) car_settings_set_language(s_settings, i);
    }
    else if(digits) {
        if(i < n) st->factory_index++;
        else if(i == n + 1) {
            st->view = password ? UI_SETTINGS_VIEW_SYSTEM : UI_SETTINGS_VIEW_FACTORY;
            st->factory_index = 0;
        }
        else {
            uint32_t value = 0;
            for(unsigned d = 0; d < n; ++d) value = value * 10 + st->factory_digits[d];
            if(password) {
                st->factory_error = value != 1688;
                if(!st->factory_error) { st->factory_unlocked = true; st->view = UI_SETTINGS_VIEW_FACTORY; st->factory_index = 0; }
            }
            else if(s_cmd_cb && car_cmd_set_mileage(&cmd, value)) {
                s_cmd_cb(&cmd);
                st->view = UI_SETTINGS_VIEW_FACTORY;
                st->factory_index = 0;
            }
        }
    }
    else if(st->view == UI_SETTINGS_VIEW_FACTORY_FEATURES) {
        if(i == 11) { st->view = UI_SETTINGS_VIEW_FACTORY; st->factory_index = 5; }
        else if(v) car_settings_set_factory(s_settings, v->factory_features ^ (1U << i),
            v->tire_on, v->door_on, v->fuel_filler_right, v->fuel_tank);
    }
    else if(st->view == UI_SETTINGS_VIEW_FACTORY_TANK) {
        if(i < 2 && s_cmd_cb && v && car_cmd_set_fuel_tank(&cmd, 0, i ? 69 : 57)) {
            s_cmd_cb(&cmd);
            car_settings_set_factory(s_settings, v->factory_features, v->tire_on, v->door_on, v->fuel_filler_right, i ? 69 : 57);
        }
        if(i == 2 || s_cmd_cb) { st->view = UI_SETTINGS_VIEW_FACTORY; st->factory_index = 1; }
    }
    else if(st->view == UI_SETTINGS_VIEW_FACTORY_MODEL) {
        if(i == count - 1) { st->view = UI_SETTINGS_VIEW_FACTORY; st->factory_index = 4; }
        else if(s_cmd_cb && p->factory_models.valid && i < p->factory_models.parsed &&
                i < CAR_MODEL_MAX && car_cmd_set_car_model(&cmd, p->factory_models.entries[i].id)) s_cmd_cb(&cmd);
    }
    else if(v) {
        if(i == 12) { st->view = UI_SETTINGS_VIEW_SYSTEM; st->factory_unlocked = false; }
        else if(i == 0 || i == 1 || i == 4 || i == 5) {
            st->view = i == 0 ? UI_SETTINGS_VIEW_FACTORY_ODO : i == 1 ? UI_SETTINGS_VIEW_FACTORY_TANK :
                i == 4 ? UI_SETTINGS_VIEW_FACTORY_MODEL : UI_SETTINGS_VIEW_FACTORY_FEATURES;
            st->factory_index = 0;
            memset(st->factory_digits, 0, sizeof(st->factory_digits));
        }
        else if(i == 3 || i == 6 || i == 9) {
            if(i == 9 && !s_cmd_cb) return true;
            if(i == 9 && car_cmd_set_fuel_tank(&cmd, 2, !v->fuel_filler_right)) s_cmd_cb(&cmd);
            car_settings_set_factory(s_settings, v->factory_features, i == 3 ? !v->tire_on : v->tire_on,
                i == 6 ? !v->door_on : v->door_on, i == 9 ? !v->fuel_filler_right : v->fuel_filler_right, v->fuel_tank);
        }
        else if(i == 7 && s_cmd_cb) {
            if(car_cmd_set_radar_display(&cmd, !v->radar_on)) s_cmd_cb(&cmd);
            car_settings_set_radar_on(s_settings, !v->radar_on);
        }
        else if(i == 8) car_settings_set_show_4h(s_settings, !v->show_4h);
        else if(i == 10) car_settings_set_appearance(s_settings, v->car_color_rgb, !v->right_hand_drive);
    }
    return true;
}
