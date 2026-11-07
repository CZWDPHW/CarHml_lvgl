#include "carmw/car_protocol.h"

#include <string.h>
/**
 * @brief 计算校验和
 * 
 * @param cmd_id 命令ID
 * @param ctrl 控制字
 * @param len_lo 长度低8位
 * @param payload 数据指针
 * @param len 数据长度
 * @return uint8_t 校验和
 */
uint8_t car_frame_checksum(uint8_t cmd_id,
                           uint8_t ctrl,
                           uint8_t len_lo,
                           const uint8_t *payload,
                           uint16_t len)
{
    uint8_t sum = (uint8_t)(cmd_id + ctrl + len_lo);

    if(payload != NULL) {
        for(uint16_t i = 0; i < len; ++i) {
            sum = (uint8_t)(sum + payload[i]);
        }
    }

    return (uint8_t)(sum ^ 0xFFU);
}
/**
 * @brief 编码数据帧
 * 
 * @param cmd_id 命令ID
 * @param op 操作码
 * @param payload 数据指针
 * @param len 数据长度
 * @param out 输出缓冲区
 * @param capacity 缓冲区容量
 * @return size_t 编码后的数据长度
 */
size_t car_frame_encode(uint8_t cmd_id,
                        uint8_t op,
                        const uint8_t *payload,
                        uint16_t len,
                        uint8_t *out,
                        size_t capacity)
{
    if(out == NULL || op > 0x0FU || len > CAR_MAX_PAYLOAD ||
       (payload == NULL && len != 0)) {
        return 0;
    }

    size_t total = (size_t)CAR_FRAME_OVERHEAD + (size_t)len;
    if(capacity < total) {
        return 0;
    }

    uint8_t ctrl = CAR_CTRL_MAKE(op, len);

    out[0] = CAR_FRAME_HEAD;
    out[1] = cmd_id;
    out[2] = ctrl;
    out[3] = (uint8_t)(len & 0xFFU);

    if(len != 0) {
        memcpy(&out[4], payload, len);
    }

    out[4U + (size_t)len] =
        car_frame_checksum(cmd_id, ctrl, out[3], payload, len);

    return total;
}
/**
 * @brief 解析数据帧
 * 
 * @param frame 输入缓冲区
 * @param count 输入缓冲区大小
 * @param view 输出结构体指针
 * @return bool 是否成功解析
 */
bool car_frame_inspect(const uint8_t *frame, size_t count, car_frame_view_t *view)
{
    if(frame == NULL || view == NULL || count < CAR_FRAME_OVERHEAD) {
        return false;
    }

    if(frame[0] != CAR_FRAME_HEAD) {
        return false;
    }

    uint8_t ctrl = frame[2];
    uint16_t len = (uint16_t)((CAR_CTRL_LEN_HI(ctrl) << 8) | (uint16_t)frame[3]);

    if(count < (size_t)CAR_FRAME_OVERHEAD + (size_t)len) {
        return false;
    }

    view->cmd_id = frame[1];
    view->op = CAR_CTRL_OP(ctrl);
    view->len = len;
    view->payload = &frame[4];
    view->checksum_ok =
        car_frame_checksum(frame[1], ctrl, frame[3], &frame[4], len) ==
        frame[4U + (size_t)len];

    return true;
}
