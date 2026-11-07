#include "carmw/car_session.h"

#include <string.h>

#define INIT_CMD_ID 0x00U
#define CAR_MODEL_CMD_ID 0x14U
#define SPEED_WARNING_DEFAULT_KPH 120U

/* 初始化期间主动拉取的 13 条命令 */
static const car_session_cmd_t s_bulk_commands[] = {
    {0x02, (uint8_t)CAR_OP_GET, 0, {0}},
    {0x08, (uint8_t)CAR_OP_GET, 0, {0}},
    {0x30, (uint8_t)CAR_OP_GET, 0, {0}},
    {0x06, (uint8_t)CAR_OP_GET, 0, {0}},
    {0x15, (uint8_t)CAR_OP_GET, 0, {0}},
    {0x18, (uint8_t)CAR_OP_GET, 0, {0}},
    {0x05, (uint8_t)CAR_OP_GET, 0, {0}},
    {0x36, (uint8_t)CAR_OP_GET, 0, {0}},
    {0x33, (uint8_t)CAR_OP_GET, 0, {0}},
    {0x16, (uint8_t)CAR_OP_GET, 1, {0x1A}},
    {0x16, (uint8_t)CAR_OP_GET, 2, {0x86, 0x10}},
    {0x37, (uint8_t)CAR_OP_SET_GET, 2,
     {0x01, (uint8_t)SPEED_WARNING_DEFAULT_KPH}},
    {0x00, (uint8_t)CAR_OP_SET_GET, 1, {0x01}}
};

#define BULK_COUNT (sizeof(s_bulk_commands) / sizeof(s_bulk_commands[0]))

static bool session_enqueue(car_session_t *session,
                            uint8_t cmd_id,
                            uint8_t op,
                            const uint8_t *payload,
                            uint8_t len)
{
    if(len > CAR_SESSION_PAYLOAD_MAX ||
       session->pending_count >= CAR_SESSION_MAX_PENDING ||
       (payload == NULL && len != 0)) {
        return false;
    }

    car_session_cmd_t *slot = &session->pending[session->pending_count++];

    slot->cmd_id = cmd_id;
    slot->op = op;
    slot->len = len;

    if(len != 0) {
        memcpy(slot->payload, payload, len);
    }

    return true;
}

static void session_enqueue_start(car_session_t *session)
{
    static const uint8_t enter_init = 0x02;

    (void)session_enqueue(session, INIT_CMD_ID, (uint8_t)CAR_OP_SET_GET,
                          &enter_init, 1);
    (void)session_enqueue(session, CAR_MODEL_CMD_ID, (uint8_t)CAR_OP_GET, NULL, 0);
}

static bool session_pop(car_session_t *session, car_session_cmd_t *out)
{
    if(session->pending_index >= session->pending_count) {
        session->pending_index = 0;
        session->pending_count = 0;
        return false;
    }

    *out = session->pending[session->pending_index++];

    if(session->pending_index >= session->pending_count) {
        session->pending_index = 0;
        session->pending_count = 0;
    }

    return true;
}

void car_session_init(car_session_t *session, uint64_t now_ms)
{
    if(session == NULL) {
        return;
    }

    memset(session, 0, sizeof(*session));
    session->started = true;
    session->phase = CAR_SESSION_INIT;
    session->retry_at_ms = now_ms; /* 首次 next() 立刻发 */
}

void car_session_on_frame(car_session_t *session,
                          uint8_t op,
                          const uint8_t *data,
                          uint16_t len,
                          uint64_t now_ms)
{
    if(session == NULL || !session->started || data == NULL || len != 1) {
        return;
    }

    if(op == (uint8_t)CAR_OP_STATUS) {
        switch(data[0]) {
            case 0x00:
                /*
                 * MCU 说"我还没初始化"：可能是刚上电，也可能是运行中重启了。
                 * 复位到 INIT 并允许重跑批量拉取，否则板子重启后仪表会一直空着。
                 */
                session->ready = false;
                session->bulk_done = false;
                session->phase = CAR_SESSION_INIT;
                session->retry_at_ms = now_ms + CAR_SESSION_RETRY_MS;
                break;

            case 0x02: /* 正在初始化：开始拉取初始状态 */
                if(!session->bulk_done && session->phase != CAR_SESSION_READY) {
                    session->phase = CAR_SESSION_BULK;
                    session->bulk_index = 0;
                    session->bulk_at_ms = now_ms;
                }
                break;

            case 0x01: /* 初始化完成 */
                session->phase = CAR_SESSION_READY;
                session->ready = true;
                break;

            default:
                break;
        }

        return;
    }

    if(op != (uint8_t)CAR_OP_SET_GET) {
        return;
    }

    if(data[0] == 0x01U) {
        /* 我方 SETGET 的应答：初始化成功，补一条车型查询 */
        session->phase = CAR_SESSION_READY;
        session->ready = true;
        (void)session_enqueue(session, CAR_MODEL_CMD_ID, (uint8_t)CAR_OP_GET,
                              NULL, 0);
    }
    else if(data[0] == 0x02U) {
        /* MCU 回显"初始化中"：重发请求 */
        session_enqueue_start(session);
    }
}

bool car_session_next(car_session_t *session,
                      uint64_t now_ms,
                      car_session_cmd_t *out)
{
    if(session == NULL || out == NULL || !session->started) {
        return false;
    }

    if(session_pop(session, out)) {
        return true;
    }

    if(session->phase == CAR_SESSION_BULK && now_ms >= session->bulk_at_ms) {
        if(session->bulk_index < BULK_COUNT) {
            *out = s_bulk_commands[session->bulk_index++];
            session->bulk_at_ms = now_ms + CAR_SESSION_BULK_INTERVAL_MS;

            if(session->bulk_index >= BULK_COUNT) {
                session->bulk_done = true;
                session->phase = CAR_SESSION_INIT;
                session->retry_at_ms = now_ms + CAR_SESSION_RETRY_MS;
            }

            return true;
        }
    }

    if(session->phase == CAR_SESSION_INIT && now_ms >= session->retry_at_ms) {
        session->attempts++;
        session->retry_at_ms = now_ms + CAR_SESSION_RETRY_MS;
        session_enqueue_start(session);
        return session_pop(session, out);
    }

    return false;
}

const char *car_session_phase_name(car_session_phase_t phase)
{
    switch(phase) {
        case CAR_SESSION_IDLE:
            return "idle";
        case CAR_SESSION_INIT:
            return "init";
        case CAR_SESSION_BULK:
            return "bulk";
        case CAR_SESSION_READY:
            return "ready";
        default:
            return "?";
    }
}
