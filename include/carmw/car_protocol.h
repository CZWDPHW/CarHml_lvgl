/*
 * car_protocol.h - 帧级编解码（无状态，纯函数，便于测试）
 */
#ifndef CARMW_CAR_PROTOCOL_H
#define CARMW_CAR_PROTOCOL_H

#include "carmw/car_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 计算校验和。payload 可以为 NULL（len 必须为 0）。
 * 参与运算的字节：cmd_id、ctrl、len_lo、payload[0..len-1]，不含帧头。
 */
uint8_t car_frame_checksum(uint8_t cmd_id,
                           uint8_t ctrl,
                           uint8_t len_lo,
                           const uint8_t *payload,
                           uint16_t len);

/*
 * 组帧。成功返回帧总长度（5 + len），失败返回 0（参数非法或容量不足）。
 * out 至少需要 5 + len 字节。
 */
size_t car_frame_encode(uint8_t cmd_id,
                        uint8_t op,
                        const uint8_t *payload,
                        uint16_t len,
                        uint8_t *out,
                        size_t capacity);

/* 一帧的只读视图，payload 指向 frame 内部，生命周期同 frame。 */
typedef struct {
    uint8_t cmd_id;
    uint8_t op;
    uint16_t len;
    const uint8_t *payload;
    bool checksum_ok;
} car_frame_view_t;

/*
 * 解析一段"恰好是一帧"的缓冲区（调试、日志回放、单元测试用）。
 * 帧不完整或帧头不对返回 false。
 */
bool car_frame_inspect(const uint8_t *frame, size_t count, car_frame_view_t *view);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_CAR_PROTOCOL_H */
