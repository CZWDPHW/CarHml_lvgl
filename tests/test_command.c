#include "test_util.h"

#include "carmw/car_command.h"

void test_command_suite(void)
{
    car_cmd_t req;

    car_cmd_clear(&req);
    CHECK(req.cmd_id == 0 && req.op == 0 && req.len == 0);

    /* ---- 0x32 设置时间 ---- */
    CHECK(car_cmd_set_time(&req, 2026, 11, 1, 18, 30));
    CHECK(req.cmd_id == 0x32);
    CHECK(req.op == CAR_OP_SET_GET);
    CHECK(req.len == 6);
    CHECK(req.payload[0] == 26);
    CHECK(req.payload[1] == 11);
    CHECK(req.payload[2] == 1);
    CHECK(req.payload[3] == (uint8_t)(18 | 0x80)); /* 24 小时制 */
    CHECK(req.payload[4] == 30);
    CHECK(req.payload[5] == 0);

    CHECK(!car_cmd_set_time(&req, 1999, 1, 1, 0, 0));
    CHECK(!car_cmd_set_time(&req, 2026, 13, 1, 0, 0));
    CHECK(!car_cmd_set_time(&req, 2026, 1, 32, 0, 0));

    /* ---- 0x08 设置总里程 ---- */
    CHECK(car_cmd_set_mileage(&req, 0x123456));
    CHECK(req.cmd_id == 0x08);
    CHECK(req.len == 4);
    CHECK(req.payload[0] == 0x02);
    CHECK(req.payload[1] == 0x12);
    CHECK(req.payload[2] == 0x34);
    CHECK(req.payload[3] == 0x56);

    /* ---- 0x39 单位 ---- */
    CHECK(car_cmd_set_units(&req, true, true, false, 2, 1));
    CHECK(req.cmd_id == 0x39);
    CHECK(req.len == 3);
    CHECK(req.payload[0] == 0x03); /* mi | 24h */
    CHECK(req.payload[1] == 0x12); /* 油耗 2, 胎压 1 */

    /* ---- 0xEE HUD ---- */
    {
        car_hud_t hud;

        memset(&hud, 0, sizeof(hud));
        hud.show_turn = true;
        hud.show_nav = true;
        hud.show_left_turn = true;
        hud.show_speed_unit = true;
        hud.language = 2;
        hud.color = 5;

        CHECK(car_cmd_set_hud(&req, &hud));
        CHECK(req.cmd_id == 0xEE);
        CHECK(req.op == CAR_OP_SET);
        CHECK(req.len == 5);
        CHECK(req.payload[0] == 0x14); /* turn | nav */
        CHECK(req.payload[1] == 0x01); /* left */
        CHECK(req.payload[2] == 0x02); /* 速度单位显示 */
        CHECK(req.payload[3] == 2);
        CHECK(req.payload[4] == 5);

        CHECK(!car_cmd_set_hud(&req, NULL));
    }

    /* ---- 0x16 灯光设置 ---- */
    CHECK(car_cmd_set_light(&req, CAR_LIGHT_COMING_HOME, 15));
    CHECK(req.cmd_id == 0x16);
    CHECK(req.len == 2);
    CHECK(req.payload[0] == 0x87);
    CHECK(req.payload[1] == 0x37); /* (15/5)<<4 | 0x07 */

    CHECK(car_cmd_set_light(&req, CAR_LIGHT_TURN, 1));
    CHECK(req.payload[1] == 0x15);

    CHECK(car_cmd_set_light(&req, CAR_LIGHT_TURN, 0));
    CHECK(req.payload[1] == 0x05);

    /* ---- 0x3A 泊车辅助位域 ---- */
    CHECK(car_cmd_set_parking_assist(&req, 1, 2, 3, 0));
    CHECK(req.cmd_id == 0x3A);
    CHECK(req.len == 1);
    CHECK(req.payload[0] == (uint8_t)((0u << 6) | (3U << 4) | (2U << 2) | 1U));

    /* ---- 0x3B 车辆设置位域 ---- */
    CHECK(car_cmd_set_car_setting(&req, 1, 2, 3, 1, 1, 0, 1, 2, 3));
    CHECK(req.len == 2);
    CHECK(req.payload[0] == (uint8_t)((3U << 3) | (2U << 1) | 1U));
    CHECK(req.payload[1] ==
          (uint8_t)((3U << 6) | (2U << 4) | (1U << 3) | (1U << 2) | (0U << 1) |
                    1U));

    /* ---- 载荷上限 ---- */
    {
        uint8_t big[CAR_CMD_PAYLOAD_MAX + 1];

        memset(big, 0, sizeof(big));
        CHECK(!car_cmd_set_payload(&req, 0x40, CAR_OP_SET_GET, big,
                                   CAR_CMD_PAYLOAD_MAX + 1U));
        CHECK(car_cmd_set_payload(&req, 0x40, CAR_OP_SET_GET, big,
                                  CAR_CMD_PAYLOAD_MAX));
    }

    /* ---- 主动请求 ---- */
    CHECK(car_cmd_request(&req, 0x06));
    CHECK(req.cmd_id == 0x06);
    CHECK(req.op == CAR_OP_GET);
    CHECK(req.len == 0);

    CHECK(car_cmd_request_engine(&req));
    CHECK(req.cmd_id == 0x04 && req.op == CAR_OP_GET && req.len == 0);
}
