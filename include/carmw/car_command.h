/*
 * car_command.h - 下发命令构造器（纯函数，不碰串口）
 *
 * 用法：
 *   car_cmd_t req;
 *   car_cmd_set_time(&req, 2026, 11, 1, 18, 3);
 *   car_link_exec(&link, &req);
 *
 * 构造器只负责按协议摆字节，发送由 car_link_exec 入队、由 pump 线程写口，
 * UI 线程永远不会阻塞在 write() 上。
 */
#ifndef CARMW_CAR_COMMAND_H
#define CARMW_CAR_COMMAND_H

#include "carmw/car_state.h"
#include "carmw/car_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAR_CMD_PAYLOAD_MAX 16U

typedef struct {
    uint8_t cmd_id;
    uint8_t op;
    uint16_t len;
    uint8_t payload[CAR_CMD_PAYLOAD_MAX];
} car_cmd_t;

/* 灯光设置项（0x16 / 0x87 子命令） */
typedef enum {
    CAR_LIGHT_TURN = 0,
    CAR_LIGHT_LEAVING_HOME,
    CAR_LIGHT_COMING_HOME,
    CAR_LIGHT_FOOT,
    CAR_LIGHT_UNLOCK,
    CAR_LIGHT_AUTO_LOCK,
    CAR_LIGHT_WIPER,
    CAR_LIGHT_WINDOW
} car_light_setting_t;

void car_cmd_clear(car_cmd_t *req);

bool car_cmd_set_payload(car_cmd_t *req,
                         uint8_t cmd_id,
                         uint8_t op,
                         const uint8_t *payload,
                         uint16_t len);

/* 主动请求（GET，无载荷） */
bool car_cmd_request(car_cmd_t *req, uint8_t cmd_id);

bool car_cmd_set_time(car_cmd_t *req,
                      uint16_t year,
                      uint8_t month,
                      uint8_t day,
                      uint8_t hour,
                      uint8_t minute);

bool car_cmd_set_mileage(car_cmd_t *req, uint32_t km);
bool car_cmd_select_trip(car_cmd_t *req, uint8_t trip_type);
bool car_cmd_set_service_km(car_cmd_t *req, uint8_t item, uint32_t km, bool set_km);

bool car_cmd_set_units(car_cmd_t *req,
                       bool mile,
                       bool hour24,
                       bool fahrenheit,
                       uint8_t fuel_unit,
                       uint8_t pressure_unit);

bool car_cmd_set_speed_warning(car_cmd_t *req, uint8_t control, uint8_t speed_kph);
bool car_cmd_set_fuel_tank(car_cmd_t *req, uint8_t set_id, uint8_t param);

bool car_cmd_set_parking_assist(car_cmd_t *req,
                                uint8_t light_switch,
                                uint8_t is_auto,
                                uint8_t auto_frequency,
                                uint8_t light_frequency);

bool car_cmd_set_car_setting(car_cmd_t *req,
                             uint8_t indoor_auto_light,
                             uint8_t light_sensitivity,
                             uint8_t light_close_delay,
                             uint8_t speed_sensing_wipers,
                             uint8_t selecting_unlock,
                             uint8_t smart_key_lock,
                             uint8_t horn_respond,
                             uint8_t inside_auto_lock,
                             uint8_t inside_auto_unlock);

bool car_cmd_set_radar_display(car_cmd_t *req, bool on);
bool car_cmd_set_backlight(car_cmd_t *req, uint8_t mode, uint8_t day, uint8_t night);
bool car_cmd_set_car_model(car_cmd_t *req, uint8_t car_id);
bool car_cmd_set_carplay(car_cmd_t *req, bool connected);
bool car_cmd_set_hud(car_cmd_t *req, const car_hud_t *hud);
bool car_cmd_reset_trip_stats(car_cmd_t *req);
bool car_cmd_request_engine(car_cmd_t *req);
bool car_cmd_send_mcu_key(car_cmd_t *req);
bool car_cmd_set_light(car_cmd_t *req, car_light_setting_t setting, int value);

/*
 * 驾驶辅助三项（0x16 SetGet，payload [0x86, (status << 6) | index, 0]）：
 *   index 1 紧急制动 / 2 车道偏离 / 3 变道盲区。
 * 只发命令，真实状态以 MCU 0x16 Status 回读为准。
 */
bool car_cmd_set_driver_assist(car_cmd_t *req, uint8_t index, bool on);

bool car_cmd_send_debug(car_cmd_t *req, uint8_t id, uint8_t action, uint8_t num);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_CAR_COMMAND_H */
