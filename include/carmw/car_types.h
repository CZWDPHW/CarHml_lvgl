/*
 * car_types.h - 车载 MCU 串口协议的基础定义
 *
 * 帧格式（与 MCU 固件、Qt 版 SerialComManager 完全一致）：
 *
 *   0x2E | cmd_id | ctrl | len_lo | payload[0..4095] | checksum
 *
 *   ctrl    = (op_type << 4) | (len >> 8)        op_type 只占 4 bit
 *   len     = ((ctrl & 0x0F) << 8) | len_lo      最大 4095
 *   chk     = (cmd_id + ctrl + len_lo + sum(payload)) ^ 0xFF
 *
 * 注意：长度是 12 bit 且与 op_type 共用第 3 字节，校验和是"累加后取反"，
 * 不含帧头。这两点是和常见"整字节 op + 16 位长度 + 累加和"实现的根本差异。
 */
#ifndef CARMW_CAR_TYPES_H
#define CARMW_CAR_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAR_FRAME_HEAD 0x2EU
#define CAR_FRAME_OVERHEAD 5U /* head + cmd + ctrl + len_lo + checksum */
#define CAR_MAX_PAYLOAD 4095U
#define CAR_MAX_FRAME (CAR_FRAME_OVERHEAD + CAR_MAX_PAYLOAD) /* 4100 */
#define CAR_CMD_COUNT 256U

#define CAR_CTRL_OP(ctrl) ((uint8_t)(((uint8_t)(ctrl) >> 4) & 0x0FU))// 操作类型
#define CAR_CTRL_LEN_HI(ctrl) ((uint16_t)((uint8_t)(ctrl) & 0x0FU)) // 长度高4位
// 构造控制字
#define CAR_CTRL_MAKE(op, len)                                                 \
    ((uint8_t)((((uint8_t)(op) & 0x0FU) << 4) |                                \
               (((uint16_t)(len) >> 8) & 0x0FU)))

/* 操作类型。只有低 4 bit 线上可用，因此取值必须 <= 0x0F。 */
typedef enum {
    CAR_OP_SET = 0x00,
    CAR_OP_GET = 0x01,
    CAR_OP_SET_GET = 0x02,
    CAR_OP_INCREMENT = 0x03,
    CAR_OP_DECREMENT = 0x04,
    CAR_OP_PROCESSING = 0x0B,
    CAR_OP_STATUS = 0x0C,
    CAR_OP_ERROR = 0x0F
} car_op_t;

/*
 * 屏 -> MCU 的单字节应答。
 * 现场联调确认：每收到一帧都必须回一个字节，MCU 据此决定是否重发。
 */
#define CAR_ACK_OK 0xFFU
#define CAR_ACK_CHECKSUM 0xF0U
#define CAR_ACK_INVALID 0xF3U
#define CAR_ACK_NO_MEMORY 0xFCU

#ifdef __cplusplus
}
#endif

#endif /* CARMW_CAR_TYPES_H */
