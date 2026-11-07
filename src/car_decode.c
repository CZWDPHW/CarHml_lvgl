#include "car_decode_internal.h"

#include <string.h>

/* ---------------- 小工具 ---------------- */

static uint16_t rd_u16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);// 读取16位无符号整数
}

static uint32_t rd_u24(const uint8_t *data)
{
    return ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[2];// 读取24位无符号整数
}

static void copy_text(char *dst, size_t capacity, const uint8_t *src, uint16_t len)
{
    size_t used = 0;

    if(capacity == 0) {
        return;
    }

    while(used + 1U < capacity && used < (size_t)len) {
        dst[used] = (char)src[used];
        used++;
    }

    dst[used] = '\0';
}

/* ---------------- 逐命令解码 ---------------- */

bool car_decode_init(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len != 1 || data[0] > 2) {
        return false;
    }

    state->init.state = data[0];
    state->init.valid = true;
    return true;
}

bool car_decode_warnings(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len == 0 || data[0] > (uint8_t)CAR_WARN_BLINK_SLOW) {
        return false;
    }

    if(len == 1) {
        memset(state->warnings.levels, data[0], sizeof(state->warnings.levels));
        state->warnings.global_valid = true;
        state->warnings.global_state = data[0];
        state->warnings.valid = true;
        return true;
    }

    uint16_t count = (uint16_t)(len - 1U);
    if(count > CAR_WARNING_SLOTS) {
        count = CAR_WARNING_SLOTS;
    }

    state->warnings.list_valid = true;
    state->warnings.list_state = data[0];
    state->warnings.list_count = count;

    for(uint16_t i = 0; i < count; ++i) {
        state->warnings.list[i] = data[1U + i];
    }
    for(uint16_t i = 1; i < len; ++i) {
        state->warnings.levels[data[i]] = data[0];
    }

    state->warnings.valid = true;
    return true;
}

bool car_decode_popup(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len != 4) {
        return false;
    }

    state->popup.text_id = rd_u16(data);
    state->popup.icon = data[2];
    state->popup.duration_s = data[3];
    state->popup.valid = true;
    return true;
}
/**
 * @brief 解码速度和转速数据
 * @param data 数据指针
 * @param len 数据长度
 * @param state 状态指针
 * @return bool 解码是否成功
 */
bool car_decode_speed_rpm(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len != 4) {
        return false;
    }

    uint16_t rpm = rd_u16(data + 2);
    if(rpm > 10000U) {
        return false; /* 发动机转速不可能超过 10000，判为无效帧 */
    }

    uint16_t speed = rd_u16(data);
    if(speed > 500U) {
        speed = 500U; /* 仪表量程上限，防止指针爆表 */
    }

    state->speed_rpm.speed_kph = speed;
    state->speed_rpm.engine_rpm = rpm;
    state->speed_rpm.valid = true;
    return true;
}

bool car_decode_sys_setting(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 3) {
        return false;
    }

    state->sys_setting.reverse_sound = ((data[0] >> 1) & 0x01U) != 0;
    state->sys_setting.reverse_trackline = (data[0] & 0x01U) != 0;
    state->sys_setting.current_car = data[1];
    state->sys_setting.key = data[2];
    state->sys_setting.reserved = (len >= 4) ? data[3] : 0;
    state->sys_setting.time_source = (len >= 5) ? data[4] : 0;
    state->sys_setting.valid = true;
    return true;
}

bool car_decode_fuel(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 6 || data[2] > 100U) {
        return false;
    }

    state->fuel.remaining_range_km = rd_u16(data);
    state->fuel.percent = data[2];
    state->fuel.icon_state = (uint8_t)(data[3] & 0x0FU);
    state->fuel.port_left = (data[3] & 0x80U) != 0;
    state->fuel.main_tank_l = data[4];
    state->fuel.sub_tank_l = data[5];
    state->fuel.valid = true;
    return true;
}

bool car_decode_mileage(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len != 10) {
        return false;
    }

    state->mileage.trip_type = data[0];
    state->mileage.odometer_raw = rd_u24(data + 1);
    state->mileage.trip_a_raw = rd_u24(data + 4);
    state->mileage.trip_b_raw = rd_u24(data + 7);
    state->mileage.valid = true;
    return true;
}

bool car_decode_tires(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len != 10) {
        return false;
    }

    for(unsigned i = 0; i < 5U; ++i) {
        const uint8_t *item = data + i * 2U;
        state->tires.wheels[i].kpa = (uint16_t)(rd_u16(item) & 0x0FFFU);
        state->tires.wheels[i].state = (uint8_t)((item[0] >> 4) & 0x0FU);
        state->tires.wheels[i].valid = true;
    }

    state->tires.sequence++;
    state->tires.valid = true;
    return true;
}

bool car_decode_doors(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 1) {
        return false;
    }

    state->doors.left_front = (data[0] & 0x01U) != 0;
    state->doors.right_front = (data[0] & 0x02U) != 0;
    state->doors.left_rear = (data[0] & 0x04U) != 0;
    state->doors.right_rear = (data[0] & 0x08U) != 0;
    state->doors.trunk = (data[0] & 0x10U) != 0;
    state->doors.hood = (data[0] & 0x20U) != 0;
    state->doors.valid = true;
    return true;
}

bool car_decode_trip_stats(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 10) {
        return false;
    }

    state->trip_stats.avg_fuel_x10 = rd_u16(data);
    state->trip_stats.avg_speed_kph = rd_u16(data + 2);
    state->trip_stats.drive_minutes = rd_u24(data + 4);
    state->trip_stats.distance_km = rd_u24(data + 7);
    state->trip_stats.valid = true;
    return true;
}

bool car_decode_refuel(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 6) {
        return false;
    }

    state->refuel.remaining_range_km = rd_u16(data);
    state->refuel.avg_fuel_x10 = rd_u16(data + 2);
    state->refuel.avg_speed_kph = rd_u16(data + 4);
    state->refuel.valid = true;
    return true;
}

bool car_decode_trip(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 8) {
        return false;
    }

    state->trip.avg_fuel_x10 = rd_u16(data);
    state->trip.avg_speed_kph = rd_u16(data + 2);
    state->trip.drive_minutes = rd_u16(data + 4);
    state->trip.distance_km = rd_u16(data + 6);
    state->trip.valid = true;
    return true;
}

bool car_decode_eco(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 2) {
        return false;
    }

    state->eco.enabled = ((data[0] >> 4) & 0x01U) != 0;
    state->eco.energy_save = ((data[0] >> 5) & 0x01U) != 0;
    state->eco.flags = data[0];
    state->eco.value = data[1];
    state->eco.valid = true;
    return true;
}

bool car_decode_instant_fuel(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len != 2) {
        return false;
    }

    uint16_t raw = rd_u16(data);

    state->instant_fuel.raw = raw;
    state->instant_fuel.per_hour = (raw & 0x8000U) != 0;
    state->instant_fuel.value_x10 = (uint16_t)(raw & 0x7FFFU);
    state->instant_fuel.visible = raw != 0xFFFEU;
    state->instant_fuel.valid = true;
    return true;
}

bool car_decode_idle_stop(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 13) {
        return false;
    }

    state->idle_stop.time_mode = data[0];
    state->idle_stop.used_s = rd_u16(data + 1);
    state->idle_stop.start_saved_fuel = rd_u16(data + 3);
    state->idle_stop.reset_saved_fuel = rd_u16(data + 5);
    state->idle_stop.start_total_s = rd_u24(data + 7);
    state->idle_stop.reset_total_s = rd_u24(data + 10);
    state->idle_stop.valid = true;
    return true;
}

bool car_decode_cruise(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 5) {
        return false;
    }

    state->cruise.text_id = data[0];
    state->cruise.cruise_state = data[1];
    state->cruise.set_speed_kph = data[2];
    state->cruise.acc_state = data[3];
    state->cruise.limit_speed_kph = data[4];
    state->cruise.valid = true;
    return true;
}

bool car_decode_car_model(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 2) {
        return false;
    }

    car_car_model_t next;
    memset(&next, 0, sizeof(next));

    next.selected = data[0];
    next.count = data[1];

    uint16_t offset = 2;
    uint8_t parsed = 0;

    while(parsed < next.count && parsed < CAR_MODEL_MAX) {
        if((uint32_t)offset + 1U >= (uint32_t)len) {
            break; /* 条目头不完整 */
        }

        uint8_t id = data[offset];
        uint8_t name_len = data[offset + 1U];
        offset = (uint16_t)(offset + 2U);

        if((uint32_t)name_len > (uint32_t)(len - offset)) {
            break; /* 名字长度越界 */
        }

        next.entries[parsed].id = id;
        copy_text(next.entries[parsed].name, sizeof(next.entries[parsed].name),
                  data + offset, name_len);
        offset = (uint16_t)(offset + name_len);
        parsed++;
    }

    next.parsed = parsed;
    next.valid = true;
    state->car_model = next;
    return true;
}

bool car_decode_coolant(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len != 3 || data[0] > 100U ||
       data[2] > (uint8_t)CAR_COOLANT_OVERHEAT) {
        return false;
    }

    car_coolant_t next;
    memset(&next, 0, sizeof(next));

    next.percent = data[0];
    next.state = data[2];
    next.display = CAR_COOLANT_SHOW_VALUE;

    if(data[1] <= 0xFBU) {
        next.celsius = (int16_t)((int)data[1] - 50);
    }
    else {
        switch(data[1]) {
            case 0xFCU:
                next.display = CAR_COOLANT_SHOW_ERROR;
                break;
            case 0xFEU:
                next.display = CAR_COOLANT_SHOW_HIDDEN;
                break;
            case 0xFFU:
                next.display = CAR_COOLANT_SHOW_UNKNOWN;
                break;
            default:
                return false; /* 0xFD 协议未定义 */
        }
    }

    next.valid = true;
    state->coolant = next;
    return true;
}

bool car_decode_high_func(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 1) {
        return false;
    }

    uint8_t raw_len = (uint8_t)(len > CAR_HIGH_FUNC_RAW_MAX ? CAR_HIGH_FUNC_RAW_MAX : len);

    state->high_func.sub_id = data[0];
    state->high_func.raw_len = raw_len;
    memcpy(state->high_func.raw, data, raw_len);

    if(data[0] == 0x04U && len >= 2) {
        state->high_func.drive_mode = data[1];
    }
    else if(data[0] == 0x0EU && len >= 7) {
        state->high_func.lane.lane_center = (uint8_t)(data[2] & 0x03U);
        state->high_func.lane.car_state = (uint8_t)(data[4] & 0x07U);
        state->high_func.lane.show_area = (uint8_t)(data[5] & 0x07U);
        state->high_func.lane.show_car = (uint8_t)(data[6] & 0x07U);
        state->high_func.lane.valid = true;
    }
    else if(data[0] == 0x3FU && len >= 11) {
        state->high_func.pitch =
            (data[7] & 0x80U) ? (int16_t)(-(int)rd_u16(data + 7))
                              : (int16_t)rd_u16(data + 7);
        state->high_func.roll =
            (data[9] & 0x80U) ? (int16_t)(-(int)rd_u16(data + 9))
                              : (int16_t)rd_u16(data + 9);
    }
    else if(data[0] == 0x86U && len >= 2) {
        /*
         * Qt QashqaiSet 设置项回读：
         *   len == 3 且 data[2] == 0 -> 压缩编码，index = data[1] & 0x3F，
         *   status = (data[1] >> 6) & 0x03，status == 1 表示开。
         *   本工程保存 index 1~3 驾驶辅助，10/11/13/14/15/16/24/25/28 车辆设置。
         *   len == 3 且 data[1] == 0 -> 扩展 ack，control 1 = 灯光关闭延时（32），
         *   control 2 = 点灯灵敏度（33）。
         *   其余按位图：data[1] bit0 紧急制动 / bit1 车道偏离 / bit2 变道盲区。
         */
        if(len == 3U && data[2] == 0U) {
            uint8_t index = (uint8_t)(data[1] & 0x3FU);
            uint8_t status = (uint8_t)((data[1] >> 6) & 0x03U);
            car_vehicle_setting_t *vs = &state->vehicle_setting;

            if(index >= 1U && index <= 31U) {
                bool on = (status == 1U);

                switch(index) {
                case 1U:
                    state->driver_assist.emergency_braking = on;
                    state->driver_assist.valid = true;
                    break;
                case 2U:
                    state->driver_assist.lane_departure_warning = on;
                    state->driver_assist.valid = true;
                    break;
                case 3U:
                    state->driver_assist.blind_spot_warning = on;
                    state->driver_assist.valid = true;
                    break;
                case 10U:
                    vs->welcome_light = on;
                    vs->valid = true;
                    break;
                case 11U:
                    vs->auto_interior_light = on;
                    vs->valid = true;
                    break;
                case 13U:
                    vs->external_doors = on;
                    vs->valid = true;
                    break;
                case 14U:
                    vs->single_door_unlock = on;
                    vs->valid = true;
                    break;
                case 15U:
                    vs->horn_feedback = on;
                    vs->valid = true;
                    break;
                case 16U:
                    vs->auto_door_unlock = (uint8_t)(status & 0x03U);
                    vs->valid = true;
                    break;
                case 24U:
                    vs->smart_door_unlock = on;
                    vs->valid = true;
                    break;
                case 25U:
                    vs->select_unlock = on;
                    vs->valid = true;
                    break;
                case 28U:
                    vs->auto_door_lock = (uint8_t)(status & 0x03U);
                    vs->valid = true;
                    break;
                default:
                    break;
                }
            }
        }
        else if(len == 3U && data[1] == 0U) {
            /*
             * 扩展 ack：control 1 灯光关闭延时（index 32），
             * control 2 点灯灵敏度（index 33）。
             */
            uint8_t control = (uint8_t)(data[2] & 0x0FU);
            uint8_t status = (uint8_t)((data[2] >> 4) & 0x0FU);
            car_vehicle_setting_t *vs = &state->vehicle_setting;

            if(control == 1U) {
                vs->headlight_off_delay = (uint8_t)(status & 0x07U);
                vs->valid = true;
            }
            else if(control == 2U) {
                vs->headlight_sensitivity = (uint8_t)(status & 0x03U);
                vs->valid = true;
            }
        }
        else {
            state->driver_assist.emergency_braking = (data[1] & 0x01U) != 0U;
            state->driver_assist.lane_departure_warning = (data[1] & 0x02U) != 0U;
            state->driver_assist.blind_spot_warning = (data[1] & 0x04U) != 0U;
            state->driver_assist.valid = true;
        }
    }
    else if(data[0] == 0x1CU && len >= 4) {
        /* Qt advance_parse case 0x1C：textID = data[1..2] 大端，0xFFFF 表示 0 */
        uint16_t text_id = (uint16_t)(((uint16_t)data[1] << 8) | data[2]);

        if(text_id == 0xFFFFU) {
            text_id = 0U;
        }
        car_state_warn_event_push(state, text_id);
    }

    state->high_func.valid = true;
    return true;
}

bool car_decode_warn_tone(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 1) {
        return false;
    }

    state->warn_tone.door_unlocked = ((data[0] >> 7) & 0x01U) != 0;
    state->warn_tone.horn = ((data[0] >> 6) & 0x01U) != 0;
    state->warn_tone.handbrake = ((data[0] >> 5) & 0x01U) != 0;
    state->warn_tone.seatbelt_fast = ((data[0] >> 4) & 0x01U) != 0;
    state->warn_tone.seatbelt_slow = ((data[0] >> 3) & 0x01U) != 0;
    state->warn_tone.valid = true;
    return true;
}

bool car_decode_backlight(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len != 6) {
        return false;
    }

    state->backlight.brightness = data[0];
    state->backlight.current_mode = data[1];
    state->backlight.set_mode = data[2];
    state->backlight.day_value = data[3];
    state->backlight.night_value = data[4];
    state->backlight.tft_led = data[5] != 0;
    state->backlight.valid = true;
    return true;
}

bool car_decode_drive_mode(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 1) {
        return false;
    }

    state->drive_mode = data[0];
    state->drive_mode_valid = true;
    return true;
}

bool car_decode_key(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 2) {
        return false;
    }

    /* 与 Qt 端一致：键值取整个字节（Qt 用 data[0] & 0xFF） */
    state->key.code = data[0];
    state->key.pressed = data[1];
    state->key.valid = true;
    return true;
}

bool car_decode_radar(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len != 11) {
        return false;
    }

    for(unsigned i = 0; i < 5U; ++i) {
        state->radar.front[i].color = (uint8_t)((data[i] >> 6) & 0x03U);
        state->radar.front[i].value = (uint8_t)(data[i] & 0x3FU);
        state->radar.rear[i].color = (uint8_t)((data[i + 5U] >> 6) & 0x03U);
        state->radar.rear[i].value = (uint8_t)(data[i + 5U] & 0x3FU);
    }

    state->radar.display_on = (data[10] & 0x01U) != 0;
    state->radar.valid = true;
    return true;
}

bool car_decode_trajectory(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 2) {
        return false;
    }

    state->trajectory.right = (data[0] & 0x80U) != 0;
    state->trajectory.angle = (uint8_t)(data[0] & 0x7FU);
    state->trajectory.chassis_lift = ((data[1] >> 4) & 0x01U) != 0;
    state->trajectory.wheel_lf = (data[1] & 0x01U) != 0;
    state->trajectory.wheel_rf = ((data[1] >> 1) & 0x01U) != 0;
    state->trajectory.wheel_lr = ((data[1] >> 2) & 0x01U) != 0;
    state->trajectory.wheel_rr = ((data[1] >> 3) & 0x01U) != 0;
    state->trajectory.valid = true;
    return true;
}

bool car_decode_version_wired(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 1) {
        return false;
    }

    copy_text(state->version.wired, sizeof(state->version.wired), data, len);
    state->version.wired_valid = true;
    return true;
}

bool car_decode_version_main(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 1) {
        return false;
    }

    copy_text(state->version.main, sizeof(state->version.main), data, len);
    state->version.main_valid = true;
    return true;
}

bool car_decode_version_chip(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 1) {
        return false;
    }

    copy_text(state->version.chip, sizeof(state->version.chip), data, len);
    state->version.chip_valid = true;
    return true;
}

bool car_decode_version_debug(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 1) {
        return false;
    }

    copy_text(state->version.debug, sizeof(state->version.debug), data, len);
    state->version.debug_valid = true;
    return true;
}

bool car_decode_clock(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 6) {
        return false;
    }

    uint8_t month = data[1];
    uint8_t day = data[2];
    uint8_t hour = (uint8_t)(data[3] & 0x7FU);
    uint8_t minute = data[4];
    uint8_t second = data[5];

    if(month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 ||
       minute > 59 || second > 59) {
        return false; /* 时间字段越界：多半是错帧，宁可丢掉也不要把仪表时间调乱 */
    }

    state->clock.year = (uint16_t)(2000U + data[0]);
    state->clock.month = month;
    state->clock.day = day;
    state->clock.hour = hour;
    state->clock.minute = minute;
    state->clock.second = second;
    state->clock.hour24 = (data[3] & 0x80U) != 0;
    state->clock.valid = true;
    return true;
}

bool car_decode_gear(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 5) {
        return false;
    }

    uint8_t flags = data[0];

    state->gear.text[0] = (data[1] >= 0x20U && data[1] <= 0x7EU) ? (char)data[1] : '\0';
    state->gear.text[1] = (data[2] >= 0x20U && data[2] <= 0x7EU) ? (char)data[2] : '\0';
    state->gear.text[2] = (data[3] >= 0x20U && data[3] <= 0x7EU) ? (char)data[3] : '\0';
    state->gear.text[3] = '\0';

    state->gear.visible = (flags & 0x01U) != 0;
    state->gear.blink = (flags & 0x02U) != 0;
    state->gear.reverse_gear = (flags & 0x04U) != 0;
    state->gear.reverse_beep = (flags & 0x08U) != 0;
    state->gear.four_present = (flags & 0x40U) != 0;
    state->gear.four_blink = (flags & 0x80U) != 0;
    state->gear.four_state = data[4];
    state->gear.valid = true;
    return true;
}

bool car_decode_acc(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 1) {
        return false;
    }

    state->acc.on = (data[0] & 0x01U) != 0;
    state->acc.valid = true;
    return true;
}

bool car_decode_turn(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 1) {
        return false;
    }

    state->turn.left = (data[0] & 0x01U) != 0;
    state->turn.right = (data[0] & 0x02U) != 0;
    state->turn.high_beam_key = (data[0] & 0x10U) != 0;
    state->turn.valid = true;
    return true;
}

bool car_decode_maintenance(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len != 8) {
        return false;
    }

    state->maintenance.state = (uint8_t)((data[0] >> 4) & 0x0FU);
    state->maintenance.item = (uint8_t)(data[0] & 0x0FU);
    state->maintenance.set_km = rd_u24(data + 1);
    state->maintenance.now_km = (int32_t)rd_u24(data + 4);
    state->maintenance.show_time = data[7];
    state->maintenance.valid = true;
    return true;
}

bool car_decode_speed_warning(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len != 2) {
        return false;
    }

    state->speed_warning.remind_on = (data[0] & 0x01U) != 0;
    state->speed_warning.blink = ((data[0] >> 1) & 0x01U) != 0;
    state->speed_warning.speed_kph = data[1];
    state->speed_warning.valid = true;
    return true;
}

bool car_decode_other_info(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len != 6) {
        return false;
    }

    state->other_info.oil_pressure_pct = data[0];
    state->other_info.voltage_x10 = (uint16_t)data[1];
    state->other_info.gearbox_temp_c = data[2];
    state->other_info.outside_temp_raw = data[3];
    state->other_info.oil_temp_pct = data[4];
    state->other_info.oil_temp_c = (int16_t)((int)data[5] - 0x30);
    state->other_info.valid = true;
    return true;
}

bool car_decode_units(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 3) {
        return false;
    }

    state->units.mile = (data[0] & 0x01U) != 0;
    state->units.hour24 = ((data[0] >> 1) & 0x01U) != 0;
    state->units.fahrenheit = ((data[0] >> 2) & 0x01U) != 0;
    state->units.fuel_unit = (uint8_t)(data[1] & 0x0FU);
    state->units.pressure_unit = (uint8_t)((data[1] >> 4) & 0x0FU);
    state->units.valid = true;
    return true;
}

bool car_decode_hud(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 5) {
        return false;
    }

    state->hud.show_turn = ((data[0] >> 4) & 0x01U) != 0;
    state->hud.show_check_control = ((data[0] >> 3) & 0x01U) != 0;
    state->hud.show_nav = ((data[0] >> 2) & 0x01U) != 0;
    state->hud.show_driver_assist = ((data[0] >> 1) & 0x01U) != 0;
    state->hud.show_ma_view = (data[0] & 0x01U) != 0;
    state->hud.show_left_turn = (data[1] & 0x01U) != 0;
    state->hud.show_right_turn = ((data[1] >> 1) & 0x01U) != 0;
    state->hud.show_speed_unit = ((data[2] >> 1) & 0x01U) != 0;
    state->hud.speed_unit_mi = (data[2] & 0x01U) != 0;
    state->hud.language = data[3];
    state->hud.color = data[4];
    state->hud.valid = true;
    return true;
}

bool car_decode_func_config(const uint8_t *data, uint16_t len, car_state_t *state)
{
    if(data == NULL || len < 1) {
        return false;
    }

    state->func_config.show_time = data[0];

    if(len > 1) {
        copy_text(state->func_config.text, sizeof(state->func_config.text),
                  data + 1, (uint16_t)(len - 1U));
    }
    else {
        state->func_config.text[0] = '\0';
    }

    state->func_config.valid = true;
    return true;
}
