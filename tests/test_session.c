#include "test_util.h"

#include "carmw/car_session.h"

void test_session_suite(void)
{
    car_session_t session;
    car_session_cmd_t cmd;

    /* ---- 上电：先发 0x00 SETGET{0x02}，再发 0x14 GET ---- */
    car_session_init(&session, 0);
    CHECK(session.phase == CAR_SESSION_INIT);

    CHECK(car_session_next(&session, 0, &cmd));
    CHECK(cmd.cmd_id == 0x00);
    CHECK(cmd.op == CAR_OP_SET_GET);
    CHECK(cmd.len == 1 && cmd.payload[0] == 0x02);

    CHECK(car_session_next(&session, 0, &cmd));
    CHECK(cmd.cmd_id == 0x14);
    CHECK(cmd.op == CAR_OP_GET);
    CHECK(cmd.len == 0);

    CHECK(!car_session_next(&session, 0, &cmd));
    CHECK(session.attempts == 1);

    /* ---- 1s 没响应就重试 ---- */
    CHECK(!car_session_next(&session, 999, &cmd));
    CHECK(car_session_next(&session, 1000, &cmd));
    CHECK(cmd.cmd_id == 0x00);
    CHECK(session.attempts == 2);

    /* ---- MCU 回 STATUS{0x02} -> 进入批量拉取 ---- */
    {
        const uint8_t initializing[1] = {0x02};

        car_session_init(&session, 0);
        (void)car_session_next(&session, 0, &cmd);
        (void)car_session_next(&session, 0, &cmd);

        car_session_on_frame(&session, CAR_OP_STATUS, initializing, 1, 0);
        CHECK(session.phase == CAR_SESSION_BULK);

        CHECK(car_session_next(&session, 0, &cmd));
        CHECK(cmd.cmd_id == 0x02);
        CHECK(cmd.op == CAR_OP_GET);

        /* 40ms 内不重复发 */
        CHECK(!car_session_next(&session, 10, &cmd));
        CHECK(car_session_next(&session, 40, &cmd));
        CHECK(cmd.cmd_id == 0x08);

        /* 把 13 条拉完（前两条上面已经取走） */
        uint64_t now = 80;
        int emitted = 2;
        int guard = 0;

        while(car_session_next(&session, now, &cmd) && guard++ < 100) {
            emitted++;
            now += CAR_SESSION_BULK_INTERVAL_MS;
        }

        CHECK(emitted == 13);
        CHECK(session.bulk_done);
        CHECK(session.phase == CAR_SESSION_INIT);

        /* 拉完之后再收到 0x02 不应重跑批量 */
        car_session_on_frame(&session, CAR_OP_STATUS, initializing, 1, now);
        CHECK(session.phase != CAR_SESSION_BULK);
    }

    /* ---- MCU 回 STATUS{0x01} -> READY ---- */
    {
        const uint8_t ready[1] = {0x01};
        const uint8_t initializing[1] = {0x02};

        car_session_init(&session, 0);
        car_session_on_frame(&session, CAR_OP_STATUS, initializing, 1, 0);
        car_session_on_frame(&session, CAR_OP_STATUS, ready, 1, 100);

        CHECK(session.phase == CAR_SESSION_READY);
        CHECK(session.ready);
        CHECK(strcmp(car_session_phase_name(session.phase), "ready") == 0);

        /* READY 之后不再主动重试 */
        CHECK(!car_session_next(&session, 100000, &cmd));
    }

    /* ---- SETGET{0x01} 应答同样进入 READY 并补一次车型查询 ---- */
    {
        const uint8_t ack[1] = {0x01};

        car_session_init(&session, 0);
        (void)car_session_next(&session, 0, &cmd);
        (void)car_session_next(&session, 0, &cmd);

        car_session_on_frame(&session, CAR_OP_SET_GET, ack, 1, 10);
        CHECK(session.ready);

        CHECK(car_session_next(&session, 10, &cmd));
        CHECK(cmd.cmd_id == 0x14);
    }

    /* ---- 非法输入不得改变状态 ---- */
    {
        const uint8_t two[2] = {0x01, 0x02};

        car_session_init(&session, 0);
        car_session_on_frame(&session, CAR_OP_STATUS, two, 2, 0);
        CHECK(session.phase == CAR_SESSION_INIT);

        car_session_on_frame(&session, CAR_OP_STATUS, NULL, 0, 0);
        CHECK(session.phase == CAR_SESSION_INIT);
    }
}
