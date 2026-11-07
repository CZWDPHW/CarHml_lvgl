#include "carmw/car_link.h"

#include "carmw/car_protocol.h"
#include "carmw/car_registry.h"

#include <string.h>

#define CAR_CMD_INIT 0x00U
#define CAR_CMD_KEY 0x20U

/* ---------------- 内部：应答 ---------------- */

static void link_reply(car_link_t *link, uint8_t byte)
{
    if(link->port.write != NULL) {
        (void)link->port.write(link->port.ctx, &byte, 1);
    }
}

/*
 * NACK 限速：链路被噪声占满时，每个坏候选都会触发一次 NACK，
 * 真按帧回会把上行带宽吃光。只要求"最近 20ms 内已经告诉过 MCU"。
 * 注意用 now > last 做前置条件，这样虚拟时钟不前进的测试里也能正常计数。
 */
static void link_nack(car_link_t *link, uint8_t reason, uint64_t now_ms)
{
    if(link->nack_sent && now_ms > link->last_nack_ms &&
       now_ms - link->last_nack_ms < CAR_LINK_NACK_MIN_INTERVAL_MS) {
        return;
    }

    link->nack_sent = true;
    link->last_nack_ms = now_ms;
    link->stats.nacks++;
    link_reply(link, reason);
}

/* ---------------- 内部：诊断记录 / 按键队列 ---------------- */
/**
 * @brief 推送诊断记录
 * @param link 链接
 * @param cmd_id 命令ID
 * @param op 操作码
 * @param payload 数据负载指针
 * @param len 数据负载长度
 */
static void link_trace_push(car_link_t *link,
                            uint8_t cmd_id,
                            uint8_t op,
                            const uint8_t *payload,
                            uint16_t len,
                            bool checksum_ok,
                            bool decoded,
                            uint64_t now_ms)
{
    car_lock_acquire(&link->lock);

    car_trace_entry_t *entry = &link->trace[link->trace_head];
    uint16_t copy = len > CAR_LINK_TRACE_PAYLOAD ? CAR_LINK_TRACE_PAYLOAD : len;

    memset(entry, 0, sizeof(*entry));
    entry->cmd_id = cmd_id;
    entry->op = op;
    entry->len = len;
    entry->checksum_ok = checksum_ok;
    entry->decoded = decoded;
    entry->at_ms = now_ms;

    if(copy != 0 && payload != NULL) {
        memcpy(entry->payload, payload, copy);
    }

    link->trace_head = (uint8_t)((link->trace_head + 1U) % CAR_LINK_TRACE_DEPTH);

    if(link->trace_count < CAR_LINK_TRACE_DEPTH) {
        link->trace_count++;
    }

    car_lock_release(&link->lock);
}

/* 按键是边沿事件：队列满时丢最旧的，保证"最近按下的"一定拿得到。 */
static void link_key_push(car_link_t *link, uint8_t code, uint8_t pressed)
{
    car_lock_acquire(&link->lock);

    if(link->key_count >= CAR_LINK_KEY_DEPTH) {
        link->key_tail = (uint8_t)((link->key_tail + 1U) % CAR_LINK_KEY_DEPTH);
        link->key_count--;
        link->stats.key_dropped++;
    }

    link->keys[link->key_head].code = code;
    link->keys[link->key_head].pressed = pressed;
    link->key_head = (uint8_t)((link->key_head + 1U) % CAR_LINK_KEY_DEPTH);
    link->key_count++;
    link->stats.key_events++;

    car_lock_release(&link->lock);
}

/* ---------------- 内部：发送 ---------------- */

static bool link_send_frame(car_link_t *link,
                            uint8_t cmd_id,
                            uint8_t op,
                            const uint8_t *payload,
                            uint16_t len)
{
    if(link == NULL || link->port.write == NULL || len > CAR_CMD_PAYLOAD_MAX) {
        return false;
    }

    uint8_t frame[CAR_FRAME_OVERHEAD + CAR_CMD_PAYLOAD_MAX];
    size_t frame_len =
        car_frame_encode(cmd_id, op, payload, len, frame, sizeof(frame));

    if(frame_len == 0) {
        return false;
    }

    if(link->port.write(link->port.ctx, frame, frame_len) < 0) {
        link->stats.tx_errors++;
        return false;
    }

    return true;
}

/* ---------------- 内部：收帧回调 ---------------- */
/**
 * @brief 收到数据帧时的回调函数
 * @param cmd_id 命令ID
 * @param op 操作码
 * @param checksum_ok 校验和是否正确
 * @param payload 数据负载指针
 * @param len 数据负载长度
 * @param user 用户数据指针
 */
static void link_on_frame(uint8_t cmd_id,
                          uint8_t op,
                          bool checksum_ok,
                          const uint8_t *payload,
                          uint16_t len,
                          void *user)
{
    car_link_t *link = (car_link_t *)user;
    uint64_t now_ms = car_port_now(&link->port);

    link->stats.frames_rx++;

    if(!checksum_ok) {
        link_trace_push(link, cmd_id, op, payload, len, false, false, now_ms);
        link_nack(link, CAR_ACK_CHECKSUM, now_ms);
        return;
    }

    const car_cmd_desc_t *desc = car_registry_find(cmd_id);

    if(desc == NULL) {
        /* 长度放行了，但协议里没有这个命令：记录并 NACK，不改状态 */
        link->stats.frames_unknown++;
        link_trace_push(link, cmd_id, op, payload, len, true, false, now_ms);
        link_nack(link, CAR_ACK_INVALID, now_ms);
        return;
    }

    if(cmd_id == CAR_CMD_INIT) {
        car_session_on_frame(&link->session, op, payload, len, now_ms);
    }

    if(desc->decode != NULL) {
        /*
         * pending 是"当前状态 + 本帧字段"：解码直接写进 pending，
         * 未被本帧覆盖的字段保留原值（否则只收到 ACC 0x34 时会把车速清 0）。
         */
        car_state_t decoded = link->pending;

        if(!desc->decode(payload, len, &decoded)) {
            link->stats.frames_rejected++;
            link_trace_push(link, cmd_id, op, payload, len, true, false, now_ms);
            link_nack(link, CAR_ACK_INVALID, now_ms);
            return;
        }

        link->pending = decoded;
        link->pending_mask |= desc->mask;
        link->stats.frames_decoded++;

        if(cmd_id == CAR_CMD_KEY) {
            link_key_push(link, link->pending.key.code,
                          link->pending.key.pressed);
        }
    }

    link_trace_push(link, cmd_id, op, payload, len, true, desc->decode != NULL,
                    now_ms);

    if(link->ack_enabled) {
        link->stats.acks++;
        link_reply(link, CAR_ACK_OK);
    }
}

/* ---------------- 内部：提交 / 出队 ---------------- */
/**
 * @brief 提交待处理状态
 * @param link 链接
 */
static void link_commit(car_link_t *link)
{
    if(link->pending_mask == 0) {
        return;
    }

    car_state_store_apply(&link->store, &link->pending, link->pending_mask);
    link->pending_mask = 0;
}
/**
 * @brief 从发送队列中出队并发送命令
 * @param link 链接
 */
static void link_drain_tx(car_link_t *link)
{
    for(;;) {
        car_cmd_t item;

        car_lock_acquire(&link->lock);

        if(link->tx_count == 0) {
            car_lock_release(&link->lock);
            return;
        }

        item = link->tx[link->tx_tail];
        link->tx_tail = (uint8_t)((link->tx_tail + 1U) % CAR_LINK_TX_DEPTH);
        link->tx_count--;

        car_lock_release(&link->lock);

        (void)link_send_frame(link, item.cmd_id, item.op, item.payload, item.len);
    }
}

/* ---------------- 公共 API ---------------- */

bool car_link_init(car_link_t *link, const car_port_t *port)
{
    if(link == NULL || port == NULL) {
        return false;
    }

    memset(link, 0, sizeof(*link));
    link->port = *port;
    link->ack_enabled = true;
    link->stats.key_dropped = 0;

    car_lock_init(&link->lock);
    car_parser_init(&link->parser);
    car_parser_set_policy(&link->parser, car_registry_accept, NULL);
    car_state_init(&link->pending);
    car_state_store_init(&link->store);
    car_session_init(&link->session, car_port_now(&link->port));

    if(link->port.open != NULL) {
        link->running = link->port.open(link->port.ctx);
    }

    return link->running;
}

void car_link_deinit(car_link_t *link)
{
    if(link == NULL) {
        return;
    }

    car_link_stop(link);

    if(link->port.close != NULL) {
        link->port.close(link->port.ctx);
    }

    car_state_store_deinit(&link->store);
    car_lock_destroy(&link->lock);
    link->running = false;
}

static void link_pump_impl(car_link_t *link, int timeout_ms)
{
    if(link == NULL || !link->running) {
        return;
    }
    uint64_t now_ms = car_port_now(&link->port);
    car_session_cmd_t session_cmd;

    /* 1. 握手命令优先，且绕过用户队列，保证不会被业务命令挤掉 */
    while(car_session_next(&link->session, now_ms, &session_cmd)) {
        (void)link_send_frame(link, session_cmd.cmd_id, session_cmd.op,
                              session_cmd.payload, session_cmd.len);
        link->stats.session_commands++;
        now_ms = car_port_now(&link->port);
    }

    /* 2. 用户命令 */
    link_drain_tx(link);//从发送队列中出队并发送命令

    /* 3. 收：先把当前可读的全读干，再等下一次 pump */
    uint8_t buffer[256];

    for(;;) {
        if(link->port.read == NULL) {
            break;
        }

        int got = link->port.read(link->port.ctx, buffer, sizeof(buffer),
                                  timeout_ms);

        if(got < 0) {
            link->stats.read_errors++;
            break;
        }

        if(got == 0) {
            break;
        }

        (void)car_parser_feed(&link->parser, buffer, (size_t)got, link_on_frame,
                              link);//解析数据

        timeout_ms = 0; /* 首轮之后不再等待 */
    }

    /* 4. 一次性提交，UI 只会看到"已完整解码"的状态 */
    link_commit(link);
}

void car_link_pump(car_link_t *link, int timeout_ms)
{
    if(link == NULL || !link->running) {
        return;
    }

    /* 线程模式下只允许后台线程驱动，避免两个线程同时当写者 */
    if(link->thread_started) {
        return;
    }

    link_pump_impl(link, timeout_ms);
}

#ifndef CARMW_NO_THREADS
static void *link_thread_main(void *argument)
{
    car_link_t *link = (car_link_t *)argument;

    while(!atomic_load(&link->stop_requested)) {
        link_pump_impl(link, 20);
    }

    return NULL;
}
#endif

bool car_link_start(car_link_t *link)
{
#ifdef CARMW_NO_THREADS
    (void)link;
    return false;
#else
    if(link == NULL || !link->running) {
        return false;
    }

    if(link->thread_started) {
        return true;
    }

    atomic_store(&link->stop_requested, false);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    (void)pthread_attr_setstacksize(&attr, 256U * 1024U);

    link->thread_started = true;

    if(pthread_create(&link->thread, &attr, link_thread_main, link) != 0) {
        link->thread_started = false;
        pthread_attr_destroy(&attr);
        return false;
    }

    pthread_attr_destroy(&attr);
    return true;
#endif
}

void car_link_stop(car_link_t *link)
{
#ifdef CARMW_NO_THREADS
    (void)link;
#else
    if(link == NULL || !link->thread_started) {
        return;
    }

    atomic_store(&link->stop_requested, true);
    (void)pthread_join(link->thread, NULL);
    link->thread_started = false;
#endif
}

bool car_link_exec(car_link_t *link, const car_cmd_t *command)
{
    if(link == NULL || command == NULL || !link->running) {
        return false;
    }

    if(command->len > CAR_CMD_PAYLOAD_MAX) {
        return false;
    }

    car_lock_acquire(&link->lock);

    if(link->tx_count >= CAR_LINK_TX_DEPTH) {
        car_lock_release(&link->lock);
        link->stats.tx_dropped++;
        return false;
    }

    link->tx[link->tx_head] = *command;
    link->tx_head = (uint8_t)((link->tx_head + 1U) % CAR_LINK_TX_DEPTH);
    link->tx_count++;
    link->stats.tx_queued++;

    car_lock_release(&link->lock);
    return true;
}

/**
 * @brief 获取链接状态视图
 * @param link 链接
 * @param view 视图指针
 */
void car_link_view(car_link_t *link, car_state_view_t *view)
{
    if(link == NULL || view == NULL) {
        return;
    }

    car_state_store_view(&link->store, view);
}

size_t car_link_take_keys(car_link_t *link, car_key_event_t *out, size_t capacity)
{
    size_t taken = 0;

    if(link == NULL || out == NULL) {
        return 0;
    }

    car_lock_acquire(&link->lock);

    while(taken < capacity && link->key_count > 0) {
        out[taken++] = link->keys[link->key_tail];
        link->key_tail = (uint8_t)((link->key_tail + 1U) % CAR_LINK_KEY_DEPTH);
        link->key_count--;
    }

    car_lock_release(&link->lock);
    return taken;
}

size_t car_link_take_trace(car_link_t *link,
                           car_trace_entry_t *out,
                           size_t capacity)
{
    size_t taken = 0;

    if(link == NULL || out == NULL) {
        return 0;
    }

    car_lock_acquire(&link->lock);

    while(taken < capacity && link->trace_count > 0) {
        uint8_t tail = (uint8_t)((link->trace_head + CAR_LINK_TRACE_DEPTH -
                                  link->trace_count) %
                                 CAR_LINK_TRACE_DEPTH);
        out[taken++] = link->trace[tail];
        link->trace_count--;
    }

    car_lock_release(&link->lock);
    return taken;
}

const car_link_stats_t *car_link_stats(const car_link_t *link)
{
    return link != NULL ? &link->stats : NULL;
}

bool car_link_is_running(const car_link_t *link)
{
    return link != NULL && link->running;
}

const char *car_link_phase_name(const car_link_t *link)
{
    return link != NULL ? car_session_phase_name(link->session.phase) : "?";
}
