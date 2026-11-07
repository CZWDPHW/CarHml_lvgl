#include "carmw/car_command.h"

#include <string.h>

void car_cmd_clear(car_cmd_t *req)
{
    if(req == NULL) {
        return;
    }

    memset(req, 0, sizeof(*req));
}
/**
 * @brief 设置命令负载
 * @param req 命令指针
 * @param cmd_id 命令ID
 * @param op 操作码
 * @param payload 载荷指针
 * @param len 载荷长度
 * @return true 成功
 * @return false 失败
 */
bool car_cmd_set_payload(car_cmd_t *req,
                         uint8_t cmd_id,
                         uint8_t op,
                         const uint8_t *payload,
                         uint16_t len)
{
    if(req == NULL || len > CAR_CMD_PAYLOAD_MAX ||
       (payload == NULL && len != 0)) {
        return false;
    }

    req->cmd_id = cmd_id;
    req->op = op;
    req->len = len;

    if(len != 0) {
        memcpy(req->payload, payload, len);
    }

    return true;
}
// 请求状态
bool car_cmd_request(car_cmd_t *req, uint8_t cmd_id)
{
    return car_cmd_set_payload(req, cmd_id, (uint8_t)CAR_OP_GET, NULL, 0);
}
// 设置时间
bool car_cmd_set_time(car_cmd_t *req,
                      uint16_t year,
                      uint8_t month,
                      uint8_t day,
                      uint8_t hour,
                      uint8_t minute)
{
    if(year < 2000U || year > 2199U || month < 1U || month > 12U ||
       day < 1U || day > 31U || hour > 23U || minute > 59U) {
        return false;
    }

    uint8_t payload[6];

    payload[0] = (uint8_t)(year - 2000U);
    payload[1] = month;
    payload[2] = day;
    payload[3] = (uint8_t)(hour | 0x80U); /* bit7 = 24 小时制 */
    payload[4] = minute;
    payload[5] = 0;

    return car_cmd_set_payload(req, 0x32, (uint8_t)CAR_OP_SET_GET, payload,
                               (uint16_t)sizeof(payload));
}

bool car_cmd_set_mileage(car_cmd_t *req, uint32_t km)
{
    uint8_t payload[4];

    payload[0] = 0x02;
    payload[1] = (uint8_t)((km >> 16) & 0xFFU);
    payload[2] = (uint8_t)((km >> 8) & 0xFFU);
    payload[3] = (uint8_t)(km & 0xFFU);

    return car_cmd_set_payload(req, 0x08, (uint8_t)CAR_OP_SET_GET, payload,
                               (uint16_t)sizeof(payload));
}

bool car_cmd_select_trip(car_cmd_t *req, uint8_t trip_type)
{
    return car_cmd_set_payload(req, 0x08, (uint8_t)CAR_OP_SET_GET, &trip_type, 1);
}

bool car_cmd_set_service_km(car_cmd_t *req, uint8_t item, uint32_t km, bool set_km)
{
    uint8_t payload[4];

    payload[0] = item;

    if(set_km) {
        payload[1] = (uint8_t)((km >> 16) & 0xFFU);
        payload[2] = (uint8_t)((km >> 8) & 0xFFU);
        payload[3] = (uint8_t)(km & 0xFFU);
        return car_cmd_set_payload(req, 0x36, (uint8_t)CAR_OP_SET_GET, payload, 4);
    }

    payload[1] = (uint8_t)(km & 0xFFU);
    return car_cmd_set_payload(req, 0x36, (uint8_t)CAR_OP_SET_GET, payload, 2);
}

bool car_cmd_set_units(car_cmd_t *req,
                       bool mile,
                       bool hour24,
                       bool fahrenheit,
                       uint8_t fuel_unit,
                       uint8_t pressure_unit)
{
    uint8_t payload[3];

    payload[0] = (uint8_t)((mile ? 0x01U : 0U) | (hour24 ? 0x02U : 0U) |
                           (fahrenheit ? 0x04U : 0U));
    payload[1] = (uint8_t)((fuel_unit & 0x0FU) | ((pressure_unit & 0x0FU) << 4));
    payload[2] = 0;

    return car_cmd_set_payload(req, 0x39, (uint8_t)CAR_OP_SET_GET, payload,
                               (uint16_t)sizeof(payload));
}

bool car_cmd_set_speed_warning(car_cmd_t *req, uint8_t control, uint8_t speed_kph)
{
    uint8_t payload[2];

    payload[0] = control;
    payload[1] = speed_kph;

    return car_cmd_set_payload(req, 0x37, (uint8_t)CAR_OP_SET_GET, payload, 2);
}

bool car_cmd_set_fuel_tank(car_cmd_t *req, uint8_t set_id, uint8_t param)
{
    uint8_t payload[2];

    payload[0] = set_id;
    payload[1] = param;

    return car_cmd_set_payload(req, 0x06, (uint8_t)CAR_OP_SET_GET, payload, 2);
}

bool car_cmd_set_parking_assist(car_cmd_t *req,
                                uint8_t light_switch,
                                uint8_t is_auto,
                                uint8_t auto_frequency,
                                uint8_t light_frequency)
{
    uint8_t payload = (uint8_t)(((light_frequency & 0x03U) << 6) |
                                ((auto_frequency & 0x03U) << 4) |
                                ((is_auto & 0x03U) << 2) |
                                (light_switch & 0x03U));

    return car_cmd_set_payload(req, 0x3A, (uint8_t)CAR_OP_SET_GET, &payload, 1);
}

bool car_cmd_set_car_setting(car_cmd_t *req,
                             uint8_t indoor_auto_light,
                             uint8_t light_sensitivity,
                             uint8_t light_close_delay,
                             uint8_t speed_sensing_wipers,
                             uint8_t selecting_unlock,
                             uint8_t smart_key_lock,
                             uint8_t horn_respond,
                             uint8_t inside_auto_lock,
                             uint8_t inside_auto_unlock)
{
    uint8_t payload[2];

    payload[0] = (uint8_t)(((light_close_delay & 0x07U) << 3) |
                           ((light_sensitivity & 0x03U) << 1) |
                           (indoor_auto_light & 0x01U));
    payload[1] = (uint8_t)(((inside_auto_unlock & 0x03U) << 6) |
                           ((inside_auto_lock & 0x03U) << 4) |
                           ((horn_respond & 0x01U) << 3) |
                           ((selecting_unlock & 0x01U) << 2) |
                           ((smart_key_lock & 0x01U) << 1) |
                           (speed_sensing_wipers & 0x01U));

    return car_cmd_set_payload(req, 0x3B, (uint8_t)CAR_OP_SET_GET, payload, 2);
}

bool car_cmd_set_radar_display(car_cmd_t *req, bool on)
{
    uint8_t payload = on ? 1U : 0U;

    return car_cmd_set_payload(req, 0x22, (uint8_t)CAR_OP_SET_GET, &payload, 1);
}

bool car_cmd_set_backlight(car_cmd_t *req, uint8_t mode, uint8_t day, uint8_t night)
{
    uint8_t payload[3];

    payload[0] = mode;
    payload[1] = day;
    payload[2] = night;

    return car_cmd_set_payload(req, 0x18, (uint8_t)CAR_OP_SET_GET, payload, 3);
}

bool car_cmd_set_car_model(car_cmd_t *req, uint8_t car_id)
{
    return car_cmd_set_payload(req, 0x14, (uint8_t)CAR_OP_SET_GET, &car_id, 1);
}

bool car_cmd_set_carplay(car_cmd_t *req, bool connected)
{
    uint8_t payload = connected ? 1U : 0U;

    return car_cmd_set_payload(req, 0x3D, (uint8_t)CAR_OP_SET_GET, &payload, 1);
}

bool car_cmd_set_hud(car_cmd_t *req, const car_hud_t *hud)
{
    if(hud == NULL) {
        return false;
    }

    uint8_t payload[5];

    payload[0] = (uint8_t)((hud->show_turn ? 0x10U : 0U) |
                           (hud->show_check_control ? 0x08U : 0U) |
                           (hud->show_nav ? 0x04U : 0U) |
                           (hud->show_driver_assist ? 0x02U : 0U) |
                           (hud->show_ma_view ? 0x01U : 0U));
    payload[1] = (uint8_t)((hud->show_left_turn ? 0x01U : 0U) |
                           (hud->show_right_turn ? 0x02U : 0U));
    payload[2] = (uint8_t)((hud->show_speed_unit ? 0x02U : 0U) |
                           (hud->speed_unit_mi ? 0x01U : 0U));
    payload[3] = hud->language;
    payload[4] = hud->color;

    return car_cmd_set_payload(req, 0xEE, (uint8_t)CAR_OP_SET, payload,
                               (uint16_t)sizeof(payload));
}

bool car_cmd_reset_trip_stats(car_cmd_t *req)
{
    uint8_t payload = 0;

    return car_cmd_set_payload(req, 0x0C, (uint8_t)CAR_OP_SET_GET, &payload, 1);
}

bool car_cmd_request_engine(car_cmd_t *req)
{
    return car_cmd_request(req, 0x04);
}

bool car_cmd_send_mcu_key(car_cmd_t *req)
{
    uint8_t payload[2] = {5, 1};

    return car_cmd_set_payload(req, 0x05, (uint8_t)CAR_OP_SET_GET, payload, 2);
}

bool car_cmd_set_light(car_cmd_t *req, car_light_setting_t setting, int value)
{
    uint8_t num = 0;
    uint8_t payload[2];

    switch(setting) {
        case CAR_LIGHT_TURN:
            num = value ? 0x15U : 0x05U;
            break;
        case CAR_LIGHT_LEAVING_HOME:
            num = value ? 0x16U : 0x06U;
            break;
        case CAR_LIGHT_COMING_HOME:
            num = (uint8_t)((((value / 5) & 0x0F) << 4) | 0x07);
            break;
        case CAR_LIGHT_FOOT:
            num = (uint8_t)((((value / 10) & 0x0F) << 4) | 0x08);
            break;
        case CAR_LIGHT_UNLOCK:
            num = value ? 0x12U : 0x02U;
            break;
        case CAR_LIGHT_AUTO_LOCK:
            num = value ? 0x13U : 0x03U;
            break;
        case CAR_LIGHT_WIPER:
            num = value ? 0x14U : 0x04U;
            break;
        case CAR_LIGHT_WINDOW:
            num = value ? 0x11U : 0x01U;
            break;
        default:
            return false;
    }

    payload[0] = 0x87;
    payload[1] = num;

    return car_cmd_set_payload(req, 0x16, (uint8_t)CAR_OP_SET_GET, payload, 2);
}

bool car_cmd_set_driver_assist(car_cmd_t *req, uint8_t index, bool on)
{
    uint8_t payload[3];

    if(index < 1U || index > 3U) {
        return false;
    }

    payload[0] = 0x86U;
    payload[1] = (uint8_t)(((on ? 1U : 0U) << 6) | index);
    payload[2] = 0U;

    return car_cmd_set_payload(req, 0x16, (uint8_t)CAR_OP_SET_GET, payload,
                               (uint16_t)sizeof(payload));
}

bool car_cmd_set_vehicle_setting(car_cmd_t *req, uint8_t index, uint8_t status)
{
    uint8_t payload[3];

    if(index < 1U || index > 31U) {
        return false;
    }

    payload[0] = 0x86U;
    payload[1] = (uint8_t)(((status & 0x03U) << 6) | (index & 0x3FU));
    payload[2] = 0U;

    return car_cmd_set_payload(req, 0x16, (uint8_t)CAR_OP_SET_GET, payload,
                               (uint16_t)sizeof(payload));
}

bool car_cmd_set_vehicle_setting_ext(car_cmd_t *req, uint8_t control,
                                     uint8_t status)
{
    uint8_t payload[3];

    if(control < 1U || control > 2U) {
        return false;
    }

    payload[0] = 0x86U;
    payload[1] = 0U;
    payload[2] = (uint8_t)(((status & 0x0FU) << 4) | (control & 0x0FU));

    return car_cmd_set_payload(req, 0x16, (uint8_t)CAR_OP_SET_GET, payload,
                               (uint16_t)sizeof(payload));
}

bool car_cmd_send_debug(car_cmd_t *req, uint8_t id, uint8_t action, uint8_t num)
{
    uint8_t payload[3];

    payload[0] = action;
    payload[1] = id;
    payload[2] = num;

    return car_cmd_set_payload(req, 0x40, (uint8_t)CAR_OP_SET_GET, payload,
                               action == 4U ? 3U : 2U);
}
