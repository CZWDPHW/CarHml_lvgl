#include "carmw/car_registry.h"

#include "car_decode_internal.h"

/*
 * 表必须按 cmd_id 严格升序（car_registry_find 用二分查找）。
 * 单元测试 test_registry.c 会校验这个不变量，改动后跑一次测试即可。
 */
static const car_cmd_desc_t s_commands[] = {
    {0x00, 1, 1, CAR_CH_INIT, car_decode_init, "Initialization state"},//cmdid, minlen, maxlen, channel, decoder, name
    {0x01, 1, 4095, 0, NULL, "Data by ID"},
    {0x02, 1, 4095, CAR_CH_WARNINGS, car_decode_warnings, "Warning lamps"},
    {0x03, 4, 4, CAR_CH_POPUP, car_decode_popup, "Popup"},
    {0x04, 4, 4, CAR_CH_SPEED_RPM, car_decode_speed_rpm, "Speed and RPM"},
    {0x05, 3, 64, CAR_CH_SYS_SETTING, car_decode_sys_setting, "System settings"},
    {0x06, 6, 64, CAR_CH_FUEL, car_decode_fuel, "Fuel"},
    {0x07, 1, 4095, 0, NULL, "UI and popup IDs"},
    {0x08, 10, 10, CAR_CH_MILEAGE, car_decode_mileage, "Mileage"},
    {0x0A, 10, 10, CAR_CH_TIRE, car_decode_tires, "Tire pressure"},
    {0x0B, 1, 1, CAR_CH_DOORS, car_decode_doors, "Doors"},
    {0x0C, 10, 4095, CAR_CH_TRIP_STATS, car_decode_trip_stats, "After reset"},
    {0x0D, 6, 64, CAR_CH_REFUEL, car_decode_refuel, "After refuel"},
    {0x0E, 8, 64, CAR_CH_TRIP, car_decode_trip, "After start"},
    {0x0F, 2, 64, CAR_CH_ECO, car_decode_eco, "ECO"},
    {0x11, 2, 2, CAR_CH_INSTANT_FUEL, car_decode_instant_fuel, "Instant fuel"},
    {0x12, 13, 13, CAR_CH_IDLE_STOP, car_decode_idle_stop, "Intelligent idle"},
    {0x13, 5, 64, CAR_CH_CRUISE, car_decode_cruise, "Cruise control"},
    {0x14, 2, 4095, CAR_CH_CAR_MODEL, car_decode_car_model, "Vehicle model"},
    {0x15, 3, 3, CAR_CH_COOLANT, car_decode_coolant, "Coolant"},
    {0x16, 1, 64, CAR_CH_HIGH_FUNC, car_decode_high_func, "OEM feature"},
    {0x17, 1, 1, CAR_CH_WARN_TONE, car_decode_warn_tone, "Warning tone"},
    {0x18, 6, 6, CAR_CH_BACKLIGHT, car_decode_backlight, "Backlight"},
    {0x19, 1, 4, CAR_CH_DRIVE_MODE, car_decode_drive_mode, "Drive mode"},
    {0x20, 2, 64, CAR_CH_KEY, car_decode_key, "Steering key"},
    {0x22, 11, 11, CAR_CH_RADAR, car_decode_radar, "Parking radar"},
    {0x26, 2, 64, CAR_CH_TRAJECTORY, car_decode_trajectory, "Trajectory"},
    {0x29, 1, 64, CAR_CH_VERSION, car_decode_version_wired, "Wired version"},
    {0x30, 1, 64, CAR_CH_VERSION, car_decode_version_main, "Software version"},
    {0x31, 1, 64, CAR_CH_VERSION, car_decode_version_chip, "Small chip version"},
    {0x32, 6, 64, CAR_CH_CLOCK, car_decode_clock, "Clock"},
    {0x33, 5, 64, CAR_CH_GEAR, car_decode_gear, "Gear"},
    {0x34, 1, 1, CAR_CH_ACC, car_decode_acc, "ACC"},
    {0x35, 1, 1, CAR_CH_TURN, car_decode_turn, "Turn signals"},
    {0x36, 8, 8, CAR_CH_MAINTENANCE, car_decode_maintenance, "Maintenance"},
    {0x37, 2, 2, CAR_CH_SPEED_WARN, car_decode_speed_warning, "Speed warning"},
    {0x38, 6, 6, CAR_CH_OTHER_INFO, car_decode_other_info, "Other vehicle info"},
    {0x39, 3, 64, CAR_CH_UNITS, car_decode_units, "Units"},
    {0x3A, 1, 16, 0, NULL, "Parking assist"},
    {0x3B, 2, 16, 0, NULL, "Car settings"},
    {0x3D, 1, 1, 0, NULL, "Connection state"},
    {0x40, 1, 4095, 0, NULL, "MCU CAN debug"},
    {0x41, 1, 4095, 0, NULL, "MCU debug"},
    {0xEE, 5, 64, CAR_CH_HUD, car_decode_hud, "HUD"},
    {0xFD, 1, 64, CAR_CH_FUNC_CONFIG, car_decode_func_config, "MCU function config"},
    {0xFE, 1, 64, CAR_CH_VERSION, car_decode_version_debug, "MCU debug port"},
    {0xFF, 1, 1, 0, NULL, "Protocol upgrade"}
};

#define CAR_REGISTRY_SIZE (sizeof(s_commands) / sizeof(s_commands[0]))

size_t car_registry_count(void)
{
    return CAR_REGISTRY_SIZE;
}

const car_cmd_desc_t *car_registry_at(size_t index)
{
    if(index >= CAR_REGISTRY_SIZE) {
        return NULL;
    }

    return &s_commands[index];
}

const car_cmd_desc_t *car_registry_find(uint8_t cmd_id)
{
    size_t low = 0;
    size_t high = CAR_REGISTRY_SIZE;

    while(low < high) {
        size_t mid = low + (high - low) / 2U;

        if(s_commands[mid].cmd_id == cmd_id) {
            return &s_commands[mid];
        }

        if(s_commands[mid].cmd_id < cmd_id) {
            low = mid + 1U;
        }
        else {
            high = mid;
        }
    }

    return NULL;
}

const char *car_cmd_name(uint8_t cmd_id)
{
    const car_cmd_desc_t *desc = car_registry_find(cmd_id);

    return desc != NULL ? desc->name : "Unknown";
}

bool car_registry_accept(uint8_t cmd_id, uint16_t len, void *user)
{
    (void)user;

    const car_cmd_desc_t *desc = car_registry_find(cmd_id);

    if(desc != NULL) {
        return len >= desc->min_len && len <= desc->max_len;
    }

    return len <= CAR_UNKNOWN_MAX_PAYLOAD;
}
