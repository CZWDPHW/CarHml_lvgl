/*
 * car_parser.h - 字节流 -> 帧 的流式解析器
 *
 * 设计要点（每条都对应旧实现里踩过的坑）：
 *
 * 1. 零动态内存、零回调分配：内部只有一个 4100 字节的滑动窗口，
 *    窗口大小恰好等于最大帧长，所以"候选帧等待中"永远不会溢出。
 *
 * 2. 先验长度校验：读到 ctrl/len_lo 后立刻用 policy 判断
 *    (cmd_id, len) 是否是协议里可能存在的组合。是假帧头就当场丢弃 1 字节，
 *    不会因为一个乱码长度字段而卡住等 4095 字节。
 *
 * 3. 最优重同步：校验失败时只前移 1 字节（而不是整帧），
 *    坏候选帧内部藏着的真实帧仍能被扫出来；如果"校验字节"本身正是
 *    下一帧的帧头，也会被正确当作新帧处理。
 *
 * 4. 任意切分：一次喂 1 字节、一次喂 4KB、跨帧粘连，结果完全一致。
 *
 * 5. 坏帧也上报（checksum_ok = false），由上层决定是否回 NACK；
 *    解析器本身不依赖任何平台设施。
 */
#ifndef CARMW_CAR_PARSER_H
#define CARMW_CAR_PARSER_H

#include "carmw/car_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 帧回调。payload 指向解析器内部缓冲区，仅在本次回调内有效；
 * 需要留存请自行拷贝。checksum_ok 为 false 时 payload 内容不可信。
 */
typedef void (*car_frame_fn)(uint8_t cmd_id,
                             uint8_t op,
                             bool checksum_ok,
                             const uint8_t *payload,
                             uint16_t len,
                             void *user);

/*
 * 帧可行性判断：返回 false 表示"协议里不可能出现 cmd_id + len 这种组合"，
 * 解析器会把它当噪声丢弃。user 由 car_parser_set_policy 传入。
 */
typedef bool (*car_parser_policy_fn)(uint8_t cmd_id, uint16_t len, void *user);

typedef struct {
    uint64_t frames_ok;       /* 完整且校验通过的帧 */
    uint64_t checksum_errors; /* 长度合理但校验错误 */
    uint64_t length_rejects;  /* 因长度不合理被当作噪声的候选帧头 */
    uint64_t noise_bytes;     /* 丢弃的无效字节数 */
    uint64_t overflows;       /* 理论不可达；留作断言用 */
    uint64_t resyncs;         /* 校验失败后的重新同步次数 */
} car_parser_stats_t;

typedef struct {
    uint8_t buf[CAR_MAX_FRAME];
    size_t start;
    size_t end;
    car_parser_policy_fn policy;
    void *policy_user;
    car_parser_stats_t stats;
} car_parser_t;

void car_parser_init(car_parser_t *parser);

/* 丢弃窗口内残留数据（保留统计），用于串口重连后清场。 */
void car_parser_reset(car_parser_t *parser);

/* policy 可以为 NULL，表示只做 4095 上限检查。 */
void car_parser_set_policy(car_parser_t *parser,
                           car_parser_policy_fn policy,
                           void *user);

/*
 * 喂入字节流，返回本次解析出的帧数（含校验失败的帧）。
 * bytes 可以为 NULL 当 count 为 0（空操作）。
 */
size_t car_parser_feed(car_parser_t *parser,
                       const uint8_t *bytes,
                       size_t count,
                       car_frame_fn on_frame,
                       void *user);

void car_parser_get_stats(const car_parser_t *parser, car_parser_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_CAR_PARSER_H */
