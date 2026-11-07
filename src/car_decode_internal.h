/*
 * car_decode_internal.h - 解码器内部声明（不对外安装）
 *
 * 每个函数对应一个 cmd_id 的载荷布局；返回 false 表示"长度/取值不合法"，
 * 上层据此回 NACK。约定：
 *   - 必须先校验再写 state，失败时不得留下半截数据
 *   - 只写自己负责的字段组
 */
#ifndef CARMW_CAR_DECODE_INTERNAL_H
#define CARMW_CAR_DECODE_INTERNAL_H

#include "carmw/car_state.h"

bool car_decode_init(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_warnings(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_popup(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_speed_rpm(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_sys_setting(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_fuel(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_mileage(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_tires(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_doors(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_trip_stats(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_refuel(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_trip(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_eco(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_instant_fuel(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_idle_stop(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_cruise(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_car_model(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_coolant(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_high_func(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_warn_tone(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_backlight(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_drive_mode(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_key(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_radar(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_trajectory(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_version_wired(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_version_main(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_version_chip(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_version_debug(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_clock(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_gear(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_acc(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_turn(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_maintenance(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_speed_warning(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_other_info(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_units(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_hud(const uint8_t *data, uint16_t len, car_state_t *state);
bool car_decode_func_config(const uint8_t *data, uint16_t len, car_state_t *state);

#endif /* CARMW_CAR_DECODE_INTERNAL_H */
