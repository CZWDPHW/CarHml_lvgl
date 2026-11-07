/*
 * car_link.h - 把各层缝在一起的中枢
 *
 * 一个 car_link_t 就是"一条车载串口链路"：
 *      port -> parser -> registry/decode -> pending state -> store
 *                       ^                                  |
 *                       +------ ACK/NACK <-----------------+
 *      session(握手) -> TX 队列 -> port
 *
 * 两种驱动方式，同一套代码路径：
 *
 *   A. 协作式（推荐给 LVGL）：把 car_link_pump(&link, 5) 放进一个
 *      lv_timer，每帧跑一次。不创建任何线程，UI 和数据天生同线程。
 *
 *   B. 独立线程：car_link_start(&link)，pump 在后台线程里跑。
 *      此时 UI 通过 car_link_view() 拿快照，锁只保护队列和快照拷贝。
 */
#ifndef CARMW_CAR_LINK_H
#define CARMW_CAR_LINK_H

#include "carmw/car_command.h"
#include "carmw/car_lock.h"
#include "carmw/car_parser.h"
#include "carmw/car_port.h"
#include "carmw/car_session.h"
#include "carmw/car_state.h"

#include <stdatomic.h>

#ifndef CARMW_NO_THREADS
#include <pthread.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define CAR_LINK_TX_DEPTH 16U
#define CAR_LINK_KEY_DEPTH 8U
#define CAR_LINK_TRACE_DEPTH 16U
#define CAR_LINK_TRACE_PAYLOAD 16U
#define CAR_LINK_NACK_MIN_INTERVAL_MS 20U

typedef struct {
    uint64_t frames_rx;
    uint64_t frames_decoded;
    uint64_t frames_rejected; /* 长度对但内容非法（已回 NACK） */
    uint64_t frames_unknown;  /* 未登记命令 */
    uint64_t acks;
    uint64_t nacks;
    uint64_t key_events;
    uint64_t key_dropped;
    uint64_t tx_queued;
    uint64_t tx_dropped;
    uint64_t tx_errors;
    uint64_t read_errors;
    uint64_t session_commands;
} car_link_stats_t;

typedef struct {
    uint8_t cmd_id;
    uint8_t op;
    uint16_t len;
    uint8_t payload[CAR_LINK_TRACE_PAYLOAD];
    bool checksum_ok;
    bool decoded;
    uint64_t at_ms;
} car_trace_entry_t;

typedef struct {
    uint8_t code;
    uint8_t pressed;
} car_key_event_t;

typedef struct {
    car_port_t port;
    car_parser_t parser;
    car_session_t session;
    car_state_store_t store;

    car_state_t pending;      /* 写者私有，攒够一次提交给 store */
    uint64_t pending_mask;

    car_cmd_t tx[CAR_LINK_TX_DEPTH];
    uint8_t tx_head;
    uint8_t tx_tail;
    uint8_t tx_count;

    car_key_event_t keys[CAR_LINK_KEY_DEPTH];
    uint8_t key_head;
    uint8_t key_tail;
    uint8_t key_count;

    car_trace_entry_t trace[CAR_LINK_TRACE_DEPTH];
    uint8_t trace_head;
    uint8_t trace_count;

    car_link_stats_t stats;
    car_lock_t lock;
    uint64_t last_nack_ms;
    bool nack_sent;

    bool running;
    bool ack_enabled;
    _Atomic bool stop_requested;
    bool thread_started;
#ifndef CARMW_NO_THREADS
    pthread_t thread;
#endif
} car_link_t;

/*
 * 初始化并打开 port。open 失败不算致命错误：
 * link 进入"未运行"状态，store 里仍是全 0 的默认状态，pump 是空操作。
 * 返回 true 表示串口已就绪。
 */
bool car_link_init(car_link_t *link, const car_port_t *port);

void car_link_deinit(car_link_t *link);

/*
 * 驱动一次：发握手命令、清 TX 队列、读串口、解析、提交状态。
 * timeout_ms 是本次 read 的最长等待，协作模式下建议 0~10ms。
 */
void car_link_pump(car_link_t *link, int timeout_ms);

/* 可选：把 pump 放到独立线程里（CARMW_NO_THREADS 下返回 false）。 */
bool car_link_start(car_link_t *link);
void car_link_stop(car_link_t *link);

/* 把一条命令放进发送队列（线程安全，不阻塞）。 */
bool car_link_exec(car_link_t *link, const car_cmd_t *command);

/* 取状态快照（线程安全）。 */
void car_link_view(car_link_t *link, car_state_view_t *view);

/* 取出所有待处理的按键事件，返回条数。 */
size_t car_link_take_keys(car_link_t *link, car_key_event_t *out, size_t capacity);

/* 取出最近的收帧记录（诊断用），返回条数。 */
size_t car_link_take_trace(car_link_t *link,
                           car_trace_entry_t *out,
                           size_t capacity);

const car_link_stats_t *car_link_stats(const car_link_t *link);

bool car_link_is_running(const car_link_t *link);

const char *car_link_phase_name(const car_link_t *link);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_CAR_LINK_H */
