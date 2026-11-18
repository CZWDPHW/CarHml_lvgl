/* 功能绑定：普通 C 结构体，不决定行顺序，也不保存任何语言文案。
 * 作为 ui_settings_page.c 的内部实现包含，引用该文件的 MCU 回读缓存。
 * 新增同类 MCU 设置时在这里绑定缓存字段和协议 index。 */
static const ui_settings_text_id_t steering_modes[] = {
    UI_TEXT_NORMAL, UI_TEXT_SPORT
};
static const ui_settings_text_id_t radar_volume[] = {
    UI_TEXT_LOW, UI_TEXT_MID, UI_TEXT_HIGH
};
static const ui_settings_text_id_t radar_distance[] = {
    UI_TEXT_NEAR, UI_TEXT_MID, UI_TEXT_FAR
};
static const ui_settings_text_id_t parking_sensor[] = {
    UI_TEXT_OFF, UI_TEXT_ON, UI_TEXT_FRONT_ONLY
};
static const ui_settings_text_id_t eco_display[] = {
    UI_TEXT_FUEL, UI_TEXT_THROTTLE
};

static const model_item_t model_bindings[UI_SETTINGS_ITEM_COUNT] = {
    [UI_SETTINGS_ITEM_BACK] = {.text_id = UI_TEXT_BACK, .kind = MODEL_BACK},
    [UI_SETTINGS_ITEM_ROOT_BACK] = {.text_id = UI_TEXT_BACK, .kind = MODEL_ROOT_BACK},
    [UI_SETTINGS_IT_TRIP] = {
        .text_id = UI_TEXT_IT_TRIP,
        .kind = MODEL_LOCKED,
    },
    [UI_SETTINGS_IT_ASSIST] = {
        .text_id = UI_TEXT_IT_ASSIST,
        .kind = MODEL_NAV,
        .target_view = UI_SETTINGS_VIEW_ITINERARY_ASSIST,
        .feature_bit = 0,
    },
    [UI_SETTINGS_IT_VEHICLE] = {
        .text_id = UI_TEXT_IT_VEHICLE,
        .kind = MODEL_NAV,
        .target_view = UI_SETTINGS_VIEW_VEHICLE,
        .feature_bit = 0,
    },
    [UI_SETTINGS_ASSIST_DRIVING] = {
        .text_id = UI_TEXT_ASSIST_DRIVING,
        .kind = MODEL_NAV,
        .target_view = UI_SETTINGS_VIEW_DRIVING_ASSIST,
        .feature_bit = 0,
    },
    [UI_SETTINGS_ASSIST_PARKING] = {
        .text_id = UI_TEXT_ASSIST_PARKING,
        .kind = MODEL_NAV,
        .target_view = UI_SETTINGS_VIEW_PARKING,
        .feature_bit = 0,
    },
    [UI_SETTINGS_ASSIST_CHASSIS] = {
        .text_id = UI_TEXT_ASSIST_CHASSIS,
        .kind = MODEL_NAV,
        .target_view = UI_SETTINGS_VIEW_CHASSIS,
        .feature_bit = 0,
    },
    [UI_SETTINGS_ASSIST_EPS] = {
        .text_id = UI_TEXT_ASSIST_EPS,
        .kind = MODEL_NAV,
        .target_view = UI_SETTINGS_VIEW_EPS,
        .feature_bit = 6,
    },
    [UI_SETTINGS_EPS_MODE] = {
        .text_id = UI_TEXT_EPS_MODE,
        .kind = MODEL_MCU_CHOICE,
        .number_value = &s_eps_mode,
        .command_index = 9,
        .feature_bit = 0,
        .options = steering_modes,
        .option_count = MODEL_COUNT(steering_modes),
    },
    [UI_SETTINGS_PARK_MOVING_OBJECT] = {
        .text_id = UI_TEXT_PARK_MOVING_OBJECT,
        .kind = MODEL_MCU_SWITCH,
        .bool_value = &s_pc_moving_object,
        .command_index = 4,
        .feature_bit = 0,
    },
    [UI_SETTINGS_PARK_FRONT_RADAR] = {
        .text_id = UI_TEXT_PARK_FRONT_RADAR,
        .kind = MODEL_MCU_SWITCH,
        .bool_value = &s_pc_front_radar,
        .command_index = 5,
        .feature_bit = 0,
    },
    [UI_SETTINGS_PARK_REAR_RADAR] = {
        .text_id = UI_TEXT_PARK_REAR_RADAR,
        .kind = MODEL_MCU_SWITCH,
        .bool_value = &s_pc_rear_radar,
        .command_index = 6,
        .feature_bit = 0,
    },
    [UI_SETTINGS_PARK_DISPLAY] = {
        .text_id = UI_TEXT_PARK_DISPLAY,
        .kind = MODEL_MCU_SWITCH,
        .bool_value = &s_pc_display,
        .command_index = 7,
        .feature_bit = 0,
    },
    [UI_SETTINGS_PARK_REAR_CROSS] = {
        .text_id = UI_TEXT_PARK_REAR_CROSS,
        .kind = MODEL_MCU_SWITCH,
        .bool_value = &s_pc_rear_cross,
        .command_index = 26,
        .feature_bit = 9,
    },
    [UI_SETTINGS_PARK_VOLUME] = {
        .text_id = UI_TEXT_PARK_VOLUME,
        .kind = MODEL_MCU_CHOICE,
        .number_value = &s_pc_radar_volume,
        .command_index = 17,
        .feature_bit = 0,
        .options = radar_volume,
        .option_count = MODEL_COUNT(radar_volume),
    },
    [UI_SETTINGS_PARK_DISTANCE] = {
        .text_id = UI_TEXT_PARK_DISTANCE,
        .kind = MODEL_MCU_CHOICE,
        .number_value = &s_pc_radar_distance,
        .command_index = 18,
        .feature_bit = 0,
        .options = radar_distance,
        .option_count = MODEL_COUNT(radar_distance),
    },
    [UI_SETTINGS_PARK_SENSOR] = {
        .text_id = UI_TEXT_PARK_SENSOR,
        .kind = MODEL_MCU_CHOICE,
        .number_value = &s_pc_parking_sensor,
        .command_index = 27,
        .feature_bit = 0,
        .options = parking_sensor,
        .option_count = MODEL_COUNT(parking_sensor),
    },
    [UI_SETTINGS_CHASSIS_TRACE] = {
        .text_id = UI_TEXT_CHASSIS_TRACE,
        .kind = MODEL_MCU_SWITCH,
        .bool_value = &s_pc_trace,
        .command_index = 8,
        .feature_bit = 0,
    },
    [UI_SETTINGS_CHASSIS_BRAKE] = {
        .text_id = UI_TEXT_CHASSIS_BRAKE,
        .kind = MODEL_MCU_SWITCH,
        .bool_value = &s_pc_engine_brake,
        .command_index = 19,
        .feature_bit = 0,
    },
    [UI_SETTINGS_DA_EMERGENCY] = {
        .text_id = UI_TEXT_DA_EMERGENCY,
        .kind = MODEL_MCU_SWITCH,
        .bool_value = &s_da_emergency,
        .command_index = 1,
        .feature_bit = 8,
    },
    [UI_SETTINGS_DA_LANE] = {
        .text_id = UI_TEXT_DA_LANE,
        .kind = MODEL_MCU_SWITCH,
        .bool_value = &s_da_lane,
        .command_index = 2,
        .feature_bit = 0,
    },
    [UI_SETTINGS_DA_BLIND] = {
        .text_id = UI_TEXT_DA_BLIND,
        .kind = MODEL_MCU_SWITCH,
        .bool_value = &s_da_blind,
        .command_index = 3,
        .feature_bit = 0,
    },
    [UI_SETTINGS_DRV_OUT_TEMP] = {
        .text_id = UI_TEXT_DRV_OUT_TEMP,
        .kind = MODEL_LOCAL,
        .local_offset = offsetof(car_settings_values_t, out_temperature),
        .set_local = car_settings_set_out_temperature,
        .local_default = 0,
    },
    [UI_SETTINGS_DRV_GEAR] = {
        .text_id = UI_TEXT_DRV_GEAR,
        .kind = MODEL_LOCAL,
        .local_offset = offsetof(car_settings_values_t, gear_on),
        .set_local = car_settings_set_gear_on,
        .local_default = 1,
    },
    [UI_SETTINGS_DRV_AVG_FUEL] = {
        .text_id = UI_TEXT_DRV_AVG_FUEL,
        .kind = MODEL_LOCAL,
        .local_offset = offsetof(car_settings_values_t, avg_fuel_on),
        .set_local = car_settings_set_avg_fuel_on,
        .local_default = 0,
    },
    [UI_SETTINGS_ECO_DISPLAY] = {
        .text_id = UI_TEXT_ECO_DISPLAY,
        .kind = MODEL_LOCAL,
        .local_offset = offsetof(car_settings_values_t, eco_or_fuel),
        .set_local = car_settings_set_eco_or_fuel,
        .local_default = 1,
        .options = eco_display,
        .option_count = MODEL_COUNT(eco_display),
    },
    [UI_SETTINGS_ECO_INDICATOR] = {
        .text_id = UI_TEXT_ECO_INDICATOR,
        .kind = MODEL_LOCAL,
        .local_offset = offsetof(car_settings_values_t, eco_indicator),
        .set_local = car_settings_set_eco_indicator,
        .local_default = 0,
    },
    [UI_SETTINGS_TURN_LANE_CHANGE] = {
        .text_id = UI_TEXT_TURN_LANE_CHANGE,
        .kind = MODEL_MCU_SWITCH,
        .bool_value = &s_vs_lane_change,
        .command_index = 12,
        .feature_bit = 0,
    },
    [UI_SETTINGS_WIPER_SPEED_SENSE] = {
        .text_id = UI_TEXT_WIPER_SPEED_SENSE,
        .kind = MODEL_MCU_SWITCH,
        .bool_value = &s_vs_wiper_speed,
        .command_index = 20,
        .feature_bit = 0,
    },
    [UI_SETTINGS_WIPER_REAR_LINKAGE] = {
        .text_id = UI_TEXT_WIPER_REAR_LINKAGE,
        .kind = MODEL_MCU_SWITCH,
        .bool_value = &s_vs_wiper_rear,
        .command_index = 21,
        .feature_bit = 0,
    },
    [UI_SETTINGS_WIPER_MIST] = {
        .text_id = UI_TEXT_WIPER_MIST,
        .kind = MODEL_MCU_SWITCH,
        .bool_value = &s_vs_wiper_mist,
        .command_index = 22,
        .feature_bit = 0,
    },
};
