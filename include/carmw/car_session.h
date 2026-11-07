/*
 * car_session.h - 上电握手 / 初始化数据拉取状态机
 *
 * 现场时序（与 Qt 版一致）：
 *   1. 屏发 0x00 SETGET {0x02}  进入初始化
 *   2. MCU 回 0x00 STATUS {0x00} 表示还没就绪，屏每 1s 重试
 *   3. MCU 回 0x00 STATUS {0x02} 表示正在初始化 -> 屏按 40ms 间隔把
 *      13 条关键状态逐条 GET 一遍，让仪表在等待期间就有数据可显示
 *   4. MCU 回 0x00 STATUS {0x01} 表示初始化完成 -> 进入 READY
 *
 * 状态机不直接写串口：每次 car_session_next() 吐出一条待发命令，
 * 由 car_link 决定怎么发（同步写或入队）。
 */
#ifndef CARMW_CAR_SESSION_H
#define CARMW_CAR_SESSION_H

#include "carmw/car_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAR_SESSION_PAYLOAD_MAX 4U
#define CAR_SESSION_RETRY_MS 1000U
#define CAR_SESSION_BULK_INTERVAL_MS 40U
#define CAR_SESSION_MAX_PENDING 4U
#define CAR_SESSION_READY_TIMEOUT_MS (CAR_SESSION_RETRY_MS * 4U)

typedef enum {
    CAR_SESSION_IDLE = 0, /* 未启动 */
    CAR_SESSION_INIT,     /* 已发初始化请求，等 MCU 应答 */
    CAR_SESSION_BULK,     /* 正在逐条拉取初始状态 */
    CAR_SESSION_READY      /* 初始化完成 */
} car_session_phase_t;

typedef struct {
    uint8_t cmd_id;
    uint8_t op;
    uint8_t len;
    uint8_t payload[CAR_SESSION_PAYLOAD_MAX];
} car_session_cmd_t;//会话命令结构体

typedef struct {
    car_session_phase_t phase;
    bool started;
    bool ready;
    bool bulk_done;
    uint8_t bulk_index;
    uint64_t retry_at_ms;
    uint64_t bulk_at_ms;
    uint32_t attempts; /* 初始化请求发送次数 */
    car_session_cmd_t pending[CAR_SESSION_MAX_PENDING];
    uint8_t pending_count;
    uint8_t pending_index;
} car_session_t;

void car_session_init(car_session_t *session, uint64_t now_ms);

/* 由 car_link 在收到 0x00 帧时调用。 */
void car_session_on_frame(car_session_t *session,
                          uint8_t op,
                          const uint8_t *data,
                          uint16_t len,
                          uint64_t now_ms);

/* 取下一条待发命令；没有则返回 false。 */
bool car_session_next(car_session_t *session,
                      uint64_t now_ms,
                      car_session_cmd_t *out);

const char *car_session_phase_name(car_session_phase_t phase);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_CAR_SESSION_H */
