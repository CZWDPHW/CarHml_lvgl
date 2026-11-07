#include "test_util.h"

#include "car_decode_internal.h" /* 白盒：直接测每个解码器 */
#include "carmw/car_state.h"

void test_decode_suite(void)
{
    car_state_t st;

    /* ---- 0x04 车速/转速 ---- */
    {
        const uint8_t data[4] = {0x00, 0x78, 0x13, 0x88}; /* 120 km/h, 5000 rpm */

        car_state_init(&st);
        CHECK(car_decode_speed_rpm(data, 4, &st));
        CHECK(st.speed_rpm.valid);
        CHECK(st.speed_rpm.speed_kph == 120);
        CHECK(st.speed_rpm.engine_rpm == 5000);

        /* 转速越界 -> 拒收，且不能污染已有状态 */
        const uint8_t bad[4] = {0x00, 0x78, 0x27, 0x11}; /* 10001 rpm */
        CHECK(!car_decode_speed_rpm(bad, 4, &st));
        CHECK(st.speed_rpm.engine_rpm == 5000);

        /* 超量程车速被夹到 500 */
        const uint8_t fast[4] = {0x0F, 0xA0, 0x00, 0x10}; /* 4000 km/h */
        CHECK(car_decode_speed_rpm(fast, 4, &st));
        CHECK(st.speed_rpm.speed_kph == 500);
    }

    /* ---- 0x02 故障灯：局部增量保留，全局按时间顺序覆盖 ---- */
    {
        const uint8_t list[3] = {CAR_WARN_ON, 3, 7};
        const uint8_t blink[3] = {CAR_WARN_BLINK_FAST, 9, 255};
        const uint8_t off[2] = {CAR_WARN_OFF, 3};
        const uint8_t global_on[1] = {CAR_WARN_ON};
        const uint8_t global_off[1] = {CAR_WARN_OFF};
        const uint8_t slow[2] = {CAR_WARN_BLINK_SLOW, 7};

        car_state_init(&st);
        CHECK(car_decode_warnings(list, 3, &st));
        CHECK(st.warnings.valid);
        CHECK(st.warnings.list_valid);
        CHECK(st.warnings.list_count == 2);
        CHECK(car_state_warning_level(&st, 3) == CAR_WARN_ON);
        CHECK(car_state_warning_level(&st, 7) == CAR_WARN_ON);
        CHECK(car_state_warning_level(&st, 5) == CAR_WARN_OFF);

        CHECK(car_decode_warnings(blink, 3, &st));
        CHECK(st.warnings.list_state == CAR_WARN_BLINK_FAST);
        CHECK(st.warnings.list_count == 2);
        CHECK(st.warnings.list[0] == 9);
        CHECK(st.warnings.list[1] == 255);
        CHECK(car_state_warning_level(&st, 3) == CAR_WARN_ON);
        CHECK(car_state_warning_level(&st, 7) == CAR_WARN_ON);
        CHECK(car_state_warning_level(&st, 9) == CAR_WARN_BLINK_FAST);
        CHECK(car_state_warning_level(&st, 255) == CAR_WARN_BLINK_FAST);
        CHECK(car_state_warning_level(&st, 5) == CAR_WARN_OFF);

        CHECK(car_decode_warnings(off, 2, &st));
        CHECK(st.warnings.list_state == CAR_WARN_OFF);
        CHECK(st.warnings.list_count == 1);
        CHECK(st.warnings.list[0] == 3);
        CHECK(car_state_warning_level(&st, 3) == CAR_WARN_OFF);
        CHECK(car_state_warning_level(&st, 7) == CAR_WARN_ON);
        CHECK(car_state_warning_level(&st, 9) == CAR_WARN_BLINK_FAST);
        CHECK(car_state_warning_level(&st, 255) == CAR_WARN_BLINK_FAST);

        CHECK(car_decode_warnings(global_on, 1, &st));
        CHECK(st.warnings.global_valid);
        CHECK(st.warnings.global_state == CAR_WARN_ON);
        CHECK(st.warnings.list_state == CAR_WARN_OFF); /* 保留最近局部帧诊断 */
        CHECK(st.warnings.list_count == 1);
        CHECK(st.warnings.list[0] == 3);
        for(unsigned i = 0; i < CAR_WARNING_SLOTS; ++i) {
            CHECK(car_state_warning_level(&st, (uint8_t)i) == CAR_WARN_ON);
        }

        CHECK(car_decode_warnings(slow, 2, &st));
        CHECK(car_state_warning_level(&st, 7) == CAR_WARN_BLINK_SLOW);
        CHECK(car_state_warning_level(&st, 3) == CAR_WARN_ON);
        CHECK(car_state_warning_level(&st, 200) == CAR_WARN_ON);
        CHECK(st.warnings.global_state == CAR_WARN_ON);

        CHECK(car_decode_warnings(global_off, 1, &st));
        for(unsigned i = 0; i < CAR_WARNING_SLOTS; ++i) {
            CHECK(car_state_warning_level(&st, (uint8_t)i) == CAR_WARN_OFF);
        }
        CHECK(car_decode_warnings(blink, 3, &st));
        CHECK(car_state_warning_level(&st, 9) == CAR_WARN_BLINK_FAST);
        CHECK(car_state_warning_level(&st, 255) == CAR_WARN_BLINK_FAST);
        CHECK(car_state_warning_level(&st, 7) == CAR_WARN_OFF);
        CHECK(car_state_warning_level(&st, 0) == CAR_WARN_OFF);

        /* 状态值越界，不改变已有灯态或诊断 */
        const uint8_t bad[1] = {9};
        const car_warnings_t before = st.warnings;
        CHECK(!car_decode_warnings(bad, 1, &st));
        CHECK(memcmp(&st.warnings, &before, sizeof(before)) == 0);
    }

    /* ---- 0x06 油量 ---- */
    {
        const uint8_t data[6] = {0x01, 0x2C, 0x4B, 0x81, 0x37, 0x00};

        car_state_init(&st);
        CHECK(car_decode_fuel(data, 6, &st));
        CHECK(st.fuel.remaining_range_km == 300);
        CHECK(st.fuel.percent == 75);
        CHECK(st.fuel.icon_state == 1);
        CHECK(st.fuel.port_left);
        CHECK(st.fuel.main_tank_l == 55);

        const uint8_t bad[6] = {0x01, 0x2C, 101, 0, 0, 0};
        CHECK(!car_decode_fuel(bad, 6, &st));
    }

    /* ---- 0x0A 胎压 ---- */
    {
        const uint8_t data[10] = {0x12, 0x34, 0x22, 0x35, 0x32, 0x36,
                                  0x42, 0x37, 0x52, 0x38};

        car_state_init(&st);
        CHECK(st.tires.sequence == 0);
        CHECK(car_decode_tires(data, 10, &st));
        CHECK(st.tires.valid);
        CHECK(st.tires.sequence == 1);
        CHECK(st.tires.wheels[0].kpa == 0x0234);
        CHECK(st.tires.wheels[0].state == 1);
        CHECK(st.tires.wheels[1].kpa == 0x0235);
        CHECK(st.tires.wheels[4].kpa == 0x0238);

        CHECK(car_decode_tires(data, 10, &st)); /* 重复内容也是新事件 */
        CHECK(st.tires.sequence == 2);
        for(unsigned i = 0; i < 5U; ++i) {
            CHECK(st.tires.wheels[i].valid);
            CHECK(st.tires.wheels[i].kpa == 0x0234U + i);
            CHECK(st.tires.wheels[i].state == i + 1U);
        }
        CHECK(!car_decode_tires(data, 9, &st));
        CHECK(st.tires.sequence == 2);
        CHECK(!car_decode_tires(NULL, 10, &st));
        CHECK(st.tires.sequence == 2);
        CHECK(car_decode_tires(data, 10, &st));
        CHECK(st.tires.sequence == 3);
    }

    /* ---- 0x0B 车门 ---- */
    {
        const uint8_t data[1] = {0x0F}; /* 四门全开 */

        car_state_init(&st);
        CHECK(car_decode_doors(data, 1, &st));
        CHECK(st.doors.left_front);
        CHECK(st.doors.right_front);
        CHECK(st.doors.left_rear);
        CHECK(st.doors.right_rear);
        CHECK(!st.doors.trunk);
        CHECK(!st.doors.hood);
    }

    /* ---- 0x15 水温 ---- */
    {
        const uint8_t normal[3] = {50, 0x5A, 1}; /* 50%, 40C, 偏高 */

        car_state_init(&st);
        CHECK(car_decode_coolant(normal, 3, &st));
        CHECK(st.coolant.percent == 50);
        CHECK(st.coolant.celsius == 40);
        CHECK(st.coolant.state == CAR_COOLANT_HIGH);
        CHECK(st.coolant.display == CAR_COOLANT_SHOW_VALUE);

        const uint8_t err[3] = {10, 0xFC, 0};

        car_state_init(&st);
        CHECK(car_decode_coolant(err, 3, &st));
        CHECK(st.coolant.display == CAR_COOLANT_SHOW_ERROR);

        const uint8_t hidden[3] = {10, 0xFE, 0};
        CHECK(car_decode_coolant(hidden, 3, &st));
        CHECK(st.coolant.display == CAR_COOLANT_SHOW_HIDDEN);

        const uint8_t undefined_code[3] = {10, 0xFD, 0};
        CHECK(!car_decode_coolant(undefined_code, 3, &st));

        const uint8_t over[3] = {101, 0x5A, 0};
        CHECK(!car_decode_coolant(over, 3, &st));
    }

    /* ---- 高配功能：车道状态独立于最近子帧 raw ---- */
    {
        const uint8_t lane[7] = {0x0E, 0xFF, 0xFE, 0xFF, 0xFD, 0xFE, 0xFF};
        const uint8_t drive[2] = {0x04, 3};
        const uint8_t other[7] = {0x20, 0, 0, 0, 0, 0, 0};
        const uint8_t clear[7] = {0x0E, 0, 0, 0, 0, 0, 0};

        car_state_init(&st);
        CHECK(!st.high_func.lane.valid);
        CHECK(car_decode_high_func(lane, 6, &st));
        CHECK(!st.high_func.lane.valid); /* 不完整子帧仅更新诊断 */
        CHECK(st.high_func.raw_len == 6);
        CHECK(car_decode_high_func(lane, 7, &st));
        CHECK(st.high_func.valid);
        CHECK(st.high_func.lane.valid);
        CHECK(st.high_func.lane.lane_center == 2);
        CHECK(st.high_func.lane.car_state == 5);
        CHECK(st.high_func.lane.show_area == 6);
        CHECK(st.high_func.lane.show_car == 7);
        CHECK(st.high_func.sub_id == 0x0E);
        CHECK(st.high_func.raw_len == 7);
        CHECK(memcmp(st.high_func.raw, lane, sizeof(lane)) == 0);

        CHECK(car_decode_high_func(drive, 2, &st));
        CHECK(st.high_func.drive_mode == 3);
        CHECK(st.high_func.sub_id == 0x04);
        CHECK(st.high_func.raw_len == 2);
        CHECK(memcmp(st.high_func.raw, drive, sizeof(drive)) == 0);
        CHECK(st.high_func.lane.valid);
        CHECK(st.high_func.lane.lane_center == 2);
        CHECK(st.high_func.lane.car_state == 5);
        CHECK(st.high_func.lane.show_area == 6);
        CHECK(st.high_func.lane.show_car == 7);

        CHECK(car_decode_high_func(other, 7, &st));
        CHECK(st.high_func.sub_id == 0x20);
        CHECK(st.high_func.raw_len == 7);
        CHECK(memcmp(st.high_func.raw, other, sizeof(other)) == 0);
        CHECK(st.high_func.drive_mode == 3);
        CHECK(st.high_func.lane.valid);
        CHECK(st.high_func.lane.lane_center == 2);
        CHECK(st.high_func.lane.car_state == 5);
        CHECK(st.high_func.lane.show_area == 6);
        CHECK(st.high_func.lane.show_car == 7);

        CHECK(car_decode_high_func(clear, 6, &st));
        CHECK(st.high_func.raw_len == 6);
        CHECK(memcmp(st.high_func.raw, clear, 6) == 0);
        CHECK(st.high_func.lane.valid);
        CHECK(st.high_func.lane.lane_center == 2);
        CHECK(st.high_func.lane.car_state == 5);
        CHECK(st.high_func.lane.show_area == 6);
        CHECK(st.high_func.lane.show_car == 7);

        CHECK(car_decode_high_func(clear, 7, &st));
        CHECK(st.high_func.sub_id == 0x0E);
        CHECK(st.high_func.raw_len == 7);
        CHECK(memcmp(st.high_func.raw, clear, sizeof(clear)) == 0);
        CHECK(st.high_func.lane.valid);
        CHECK(st.high_func.lane.lane_center == 0);
        CHECK(st.high_func.lane.car_state == 0);
        CHECK(st.high_func.lane.show_area == 0);
        CHECK(st.high_func.lane.show_car == 0);
        CHECK(st.high_func.drive_mode == 3);
    }

    /* ---- 0x20 方向盘按键 ---- */
    {
        const uint8_t data[2] = {0x25, 1}; /* 与 Qt 一致：data[0] 整字节透传 */

        car_state_init(&st);
        CHECK(car_decode_key(data, 2, &st));
        CHECK(st.key.code == 0x25);
        CHECK(st.key.pressed == 1);
    }

    /* ---- 0x32 时间 ---- */
    {
        const uint8_t data[6] = {26, 11, 1, 0x92, 30, 5}; /* 2026-11-01 18:30:05 24h */

        car_state_init(&st);
        CHECK(car_decode_clock(data, 6, &st));
        CHECK(st.clock.year == 2026);
        CHECK(st.clock.month == 11);
        CHECK(st.clock.day == 1);
        CHECK(st.clock.hour == 18);
        CHECK(st.clock.minute == 30);
        CHECK(st.clock.second == 5);
        CHECK(st.clock.hour24);

        const uint8_t bad_month[6] = {26, 13, 1, 0x92, 30, 5};
        CHECK(!car_decode_clock(bad_month, 6, &st));
        CHECK(st.clock.month == 11); /* 拒收时状态不变 */
    }

    /* ---- 0x33 档位 ---- */
    {
        const uint8_t data[5] = {0x03, 'P', 0, 0, 0};

        car_state_init(&st);
        CHECK(car_decode_gear(data, 5, &st));
        CHECK(st.gear.text[0] == 'P');
        CHECK(st.gear.text[1] == '\0');
        CHECK(st.gear.visible);
        CHECK(st.gear.blink);
        CHECK(!st.gear.reverse_gear);
        CHECK(car_gear_from_text(st.gear.text, CAR_GEAR_N) == CAR_GEAR_P);

        const uint8_t four[5] = {0xC0, 'D', '4', 0, 2};
        CHECK(car_decode_gear(four, 5, &st));
        CHECK(strcmp(st.gear.text, "D4") == 0);
        CHECK(st.gear.four_present);
        CHECK(st.gear.four_blink);
        CHECK(st.gear.four_state == 2);
    }

    /* ---- 0x11 瞬时油耗 ---- */
    {
        const uint8_t hidden[2] = {0xFF, 0xFE};

        car_state_init(&st);
        CHECK(car_decode_instant_fuel(hidden, 2, &st));
        CHECK(!st.instant_fuel.visible);

        const uint8_t per_hour[2] = {0x80, 0x64}; /* 100, L/h */

        car_state_init(&st);
        CHECK(car_decode_instant_fuel(per_hour, 2, &st));
        CHECK(st.instant_fuel.visible);
        CHECK(st.instant_fuel.per_hour);
        CHECK(st.instant_fuel.value_x10 == 100);
    }

    /* ---- 0x38 其他资讯 ---- */
    {
        const uint8_t data[6] = {50, 140, 60, 0x20, 40, 0x50};

        car_state_init(&st);
        CHECK(car_decode_other_info(data, 6, &st));
        CHECK(st.other_info.oil_pressure_pct == 50);
        CHECK(st.other_info.voltage_x10 == 140); /* 14.0 V */
        CHECK(st.other_info.gearbox_temp_c == 60);
        CHECK(st.other_info.oil_temp_c == 32); /* 0x50 - 0x30 */
    }

    /* ---- 0x22 雷达 ---- */
    {
        const uint8_t data[11] = {0x41, 0x42, 0x43, 0x44, 0x45,
                                  0x46, 0x47, 0x48, 0x49, 0x4A, 0x01};

        car_state_init(&st);
        CHECK(car_decode_radar(data, 11, &st));
        CHECK(st.radar.valid);
        CHECK(st.radar.display_on);
        CHECK(st.radar.front[0].color == 1);
        CHECK(st.radar.front[0].value == 1);
        CHECK(st.radar.rear[4].value == 0x0A);
        CHECK(!car_decode_radar(data, 10, &st));
    }

    /* ---- 0xEE HUD ---- */
    {
        const uint8_t data[5] = {0x14, 0x03, 0x02, 2, 5};

        car_state_init(&st);
        CHECK(car_decode_hud(data, 5, &st));
        CHECK(st.hud.show_turn);
        CHECK(st.hud.show_nav);
        CHECK(!st.hud.show_check_control);
        CHECK(st.hud.show_left_turn);
        CHECK(st.hud.show_right_turn);
        CHECK(st.hud.show_speed_unit);
        CHECK(!st.hud.speed_unit_mi);
        CHECK(st.hud.language == 2);
        CHECK(st.hud.color == 5);
    }

    /* ---- 0x14 车型列表 ---- */
    {
        const uint8_t data[12] = {1, 2, 0x01, 3, 'A', 'B', 'C',
                                  0x02, 2, 'D', 'E', 0};

        car_state_init(&st);
        CHECK(car_decode_car_model(data, 12, &st));
        CHECK(st.car_model.selected == 1);
        CHECK(st.car_model.count == 2);
        CHECK(st.car_model.parsed == 2);
        CHECK(st.car_model.entries[0].id == 1);
        CHECK(strcmp(st.car_model.entries[0].name, "ABC") == 0);
        CHECK(strcmp(st.car_model.entries[1].name, "DE") == 0);

        /* 名字长度越界：不崩，已解析的条目保留 */
        const uint8_t truncated[6] = {1, 1, 0x01, 200, 'A', 'B'};

        car_state_init(&st);
        CHECK(car_decode_car_model(truncated, 6, &st));
        CHECK(st.car_model.parsed == 0);
    }

    /* ---- 0x0E 本次行程 ---- */
    {
        const uint8_t data[8] = {0x00, 0x64, 0x00, 0x32, 0x00, 0x5A, 0x01, 0x2C};

        car_state_init(&st);
        CHECK(car_decode_trip(data, 8, &st));
        CHECK(st.trip.avg_fuel_x10 == 100);
        CHECK(st.trip.avg_speed_kph == 50);
        CHECK(st.trip.drive_minutes == 90);
        CHECK(st.trip.distance_km == 300);
    }

    /* ---- 0x00 初始化状态 ---- */
    {
        const uint8_t ok[1] = {1};
        const uint8_t bad[1] = {3};

        car_state_init(&st);
        CHECK(car_decode_init(ok, 1, &st));
        CHECK(st.init.state == 1);
        CHECK(!car_decode_init(bad, 1, &st));
    }

    /* ---- 0xFD 功能配置文本 ---- */
    {
        const uint8_t data[6] = {3, 'H', 'E', 'L', 'L', 'O'};

        car_state_init(&st);
        CHECK(car_decode_func_config(data, 6, &st));
        CHECK(st.func_config.show_time == 3);
        CHECK(strcmp(st.func_config.text, "HELLO") == 0);
    }

    /* ---- 0x1C 警告事件：textID 大端，0xFFFF 归零，进入有界事件队列 ---- */
    {
        const uint8_t warn[4] = {0x1CU, 0x00U, 23U, 5U};
        const uint8_t ff[4] = {0x1CU, 0xFFU, 0xFFU, 0U};
        const uint8_t short_frame[3] = {0x1CU, 0x00U, 23U};
        uint8_t i;

        car_state_init(&st);
        CHECK(car_decode_high_func(warn, 4, &st));
        CHECK(st.warn_events.count == 1U);
        CHECK(st.warn_events.items[0].text_id == 23U);
        CHECK(st.warn_events.items[0].seq == 1U);
        CHECK(st.warn_events.seq == 1U);

        /* 0xFFFF 是 Qt 的"无 ID"取值，统一映射为 0 */
        CHECK(car_decode_high_func(ff, 4, &st));
        CHECK(st.warn_events.count == 2U);
        CHECK(st.warn_events.items[1].text_id == 0U);
        CHECK(st.warn_events.items[1].seq == 2U);

        /* 长度不足的 0x1C 不产生事件（仍按 high_func 子帧记录） */
        CHECK(car_decode_high_func(short_frame, 3, &st));
        CHECK(st.warn_events.count == 2U);
        CHECK(st.warn_events.seq == 2U);

        /* 容量 8：继续压入 7 条，第 9 条挤掉最旧一条 */
        for(i = 0U; i < 7U; ++i) {
            const uint8_t extra[4] = {0x1CU, 0x00U, (uint8_t)(40U + i), 1U};

            CHECK(car_decode_high_func(extra, 4, &st));
        }
        CHECK(st.warn_events.seq == 9U);
        CHECK(st.warn_events.count == CAR_WARN_EVENT_QUEUE_MAX);
        CHECK(st.warn_events.dropped == 1U);
        CHECK(st.warn_events.head == 1U);
        CHECK(st.warn_events.items[1].seq == 2U); /* 最早的 seq 1 已被挤掉 */
        CHECK(st.warn_events.items[0].seq == 9U); /* 环绕写入位置 */
    }

    /* ---- 0x86 驾驶辅助三项回读：压缩编码 index 1/2/3 ---- */
    {
        /* index 1 紧急制动：status 1 = 开，0 = 关 */
        const uint8_t em_on[3] = {0x86U, 0x41U, 0x00U};
        const uint8_t em_off[3] = {0x86U, 0x01U, 0x00U};
        const uint8_t lane_on[3] = {0x86U, 0x42U, 0x00U};
        const uint8_t lane_off[3] = {0x86U, 0x02U, 0x00U};
        const uint8_t blind_on[3] = {0x86U, 0x43U, 0x00U};
        const uint8_t blind_off[3] = {0x86U, 0x03U, 0x00U};
        const uint8_t bitmap[3] = {0x86U, 0x04U, 0x01U}; /* 位图回退：盲区 */

        car_state_init(&st);
        CHECK(!st.driver_assist.valid);
        CHECK(car_decode_high_func(em_on, 3, &st));
        CHECK(st.driver_assist.valid);
        CHECK(st.driver_assist.emergency_braking);
        CHECK(st.driver_assist.lane_departure_warning);
        CHECK(st.driver_assist.blind_spot_warning);

        CHECK(car_decode_high_func(em_off, 3, &st));
        CHECK(!st.driver_assist.emergency_braking);
        CHECK(st.driver_assist.lane_departure_warning);

        CHECK(car_decode_high_func(lane_on, 3, &st));
        CHECK(st.driver_assist.lane_departure_warning);
        CHECK(car_decode_high_func(lane_off, 3, &st));
        CHECK(!st.driver_assist.lane_departure_warning);

        CHECK(car_decode_high_func(blind_on, 3, &st));
        CHECK(st.driver_assist.blind_spot_warning);
        CHECK(car_decode_high_func(blind_off, 3, &st));
        CHECK(!st.driver_assist.blind_spot_warning);

        /* data[2] 非 0 且 data[1] 非 0：按位图 d1 bit0/1/2 回读 */
        CHECK(car_decode_high_func(bitmap, 3, &st));
        CHECK(st.driver_assist.blind_spot_warning);
        CHECK(!st.driver_assist.emergency_braking);
        CHECK(!st.driver_assist.lane_departure_warning);
        CHECK(st.driver_assist.valid);
    }

    /* ---- 0x86 车辆设置回读：开关与选项 ---- */
    {
        const uint8_t welcome_on[3] = {0x86U, 0x4AU, 0x00U};   /* index 10 开 */
        const uint8_t interior_off[3] = {0x86U, 0x0BU, 0x00U}; /* index 11 关 */
        const uint8_t horn_off[3] = {0x86U, 0x0FU, 0x00U};     /* index 15 关 */
        const uint8_t unlock_p[3] = {0x86U, 0x90U, 0x00U};     /* index 16 = 2 */
        const uint8_t delay[3] = {0x86U, 0x00U, 0x51U};        /* control 1 = 5 */
        const uint8_t sensitivity[3] = {0x86U, 0x00U, 0x32U};  /* control 2 = 3 */

        car_state_init(&st);
        CHECK(!st.vehicle_setting.valid);
        CHECK(st.vehicle_setting.welcome_light); /* Qt 缺省 true */

        CHECK(car_decode_high_func(welcome_on, 3, &st));
        CHECK(st.vehicle_setting.valid);
        CHECK(st.vehicle_setting.welcome_light);

        CHECK(car_decode_high_func(interior_off, 3, &st));
        CHECK(!st.vehicle_setting.auto_interior_light);

        CHECK(car_decode_high_func(horn_off, 3, &st));
        CHECK(!st.vehicle_setting.horn_feedback);

        CHECK(car_decode_high_func(unlock_p, 3, &st));
        CHECK(st.vehicle_setting.auto_door_unlock == 2U);

        CHECK(car_decode_high_func(delay, 3, &st));
        CHECK(st.vehicle_setting.headlight_off_delay == 5U);

        CHECK(car_decode_high_func(sensitivity, 3, &st));
        CHECK(st.vehicle_setting.headlight_sensitivity == 3U);
    }

    /* ---- 长度不足一律拒收 ---- */
    {
        const uint8_t zero[16] = {0};

        car_state_init(&st);
        CHECK(!car_decode_speed_rpm(zero, 3, &st));
        CHECK(!car_decode_fuel(zero, 5, &st));
        CHECK(!car_decode_mileage(zero, 9, &st));
        CHECK(!car_decode_radar(zero, 10, &st));
        CHECK(!car_decode_hud(zero, 4, &st));
        CHECK(!car_decode_idle_stop(zero, 12, &st));
    }
}
