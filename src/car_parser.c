#include "carmw/car_parser.h"

#include "carmw/car_protocol.h"

#include <string.h>

/* 定位一帧至少需要的字节：head + cmd + ctrl + len_lo */
#define PARSER_HEAD_BYTES 4U

/**
 * @brief 压缩解析器缓冲区
 * @param parser 解析器
 */
static void parser_compact(car_parser_t *parser)
{
    size_t pending = parser->end - parser->start;

    if(parser->start == 0) {
        return;
    }

    if(pending != 0) {
        memmove(parser->buf, parser->buf + parser->start, pending);
    }

    parser->start = 0;
    parser->end = pending;
}

void car_parser_init(car_parser_t *parser)
{
    if(parser == NULL) {
        return;
    }

    memset(parser, 0, sizeof(*parser));
}

void car_parser_reset(car_parser_t *parser)
{
    if(parser == NULL) {
        return;
    }

    parser->start = 0;
    parser->end = 0;
}

void car_parser_set_policy(car_parser_t *parser,
                           car_parser_policy_fn policy,
                           void *user)
{
    if(parser == NULL) {
        return;
    }

    parser->policy = policy;
    parser->policy_user = user;
}

void car_parser_get_stats(const car_parser_t *parser, car_parser_stats_t *out)
{
    if(parser == NULL || out == NULL) {
        return;
    }

    *out = parser->stats;
}

static bool parser_accepts(const car_parser_t *parser, uint8_t cmd_id, uint16_t len)
{
    if(len > CAR_MAX_PAYLOAD) {
        return false;
    }

    if(parser->policy == NULL) {
        return true;// 没有策略，默认接受所有帧
    }

    return parser->policy(cmd_id, len, parser->policy_user);// 调用策略函数
   }

/*
 * 尽可能多地从窗口里取出帧。
 * 返回本次上报的帧数；窗口里剩下的是"半帧"，留给下一次 append。
 */
static size_t parser_drain(car_parser_t *parser,
                           car_frame_fn on_frame,
                           void *user)
{
    size_t emitted = 0;// 已上报帧数

    for(;;) {
        size_t avail = parser->end - parser->start;// 可用字节数

        if(avail == 0) {
            parser->start = 0;
            parser->end = 0;
            return emitted;
        }

        const uint8_t *base = parser->buf + parser->start;// 基础指针
        const uint8_t *head =
            (const uint8_t *)memchr(base, (int)CAR_FRAME_HEAD, avail);// 查找帧头

        if(head == NULL) {
            /* 整段都没有帧头，全部是噪声 */
            parser->stats.noise_bytes += (uint64_t)avail;
            parser->start = 0;
            parser->end = 0;
            return emitted;
        }

        if(head != base) {
            size_t skip = (size_t)(head - base);// 跳过字节数
            parser->stats.noise_bytes += (uint64_t)skip;
            parser->start += skip;
            avail -= skip;
        }

        if(avail < (size_t)PARSER_HEAD_BYTES) {
            return emitted; /* 头部还没收全 */
        }

        uint8_t cmd_id = parser->buf[parser->start + 1U];// 命令ID
        uint8_t ctrl = parser->buf[parser->start + 2U];// 控制字
        uint8_t len_lo = parser->buf[parser->start + 3U];// �低4位长度
        uint16_t payload_len =
            (uint16_t)((CAR_CTRL_LEN_HI(ctrl) << 8) | (uint16_t)len_lo);

        if(!parser_accepts(parser, cmd_id, payload_len)) {
            /*
             * 长度与协议不符：这个 0x2E 不是真帧头（多半是载荷里的数据），
             * 只丢弃它本身，后面的字节继续参与扫描。
             */
            parser->stats.length_rejects++;
            parser->start += 1U;
            continue;
        }

        size_t total = (size_t)CAR_FRAME_OVERHEAD + (size_t)payload_len;
        if(avail < total) {
            return emitted; /* 半帧：等后续字节 */
        }

        bool ok = car_frame_checksum(cmd_id,
                                     ctrl,
                                     len_lo,
                                     parser->buf + parser->start + 4U,
                                     payload_len) ==
                  parser->buf[parser->start + total - 1U];

        if(ok) {
            parser->stats.frames_ok++;
        }
        else {
            parser->stats.checksum_errors++;
            parser->stats.resyncs++;
        }

        if(on_frame != NULL) {
            on_frame(cmd_id,
                     CAR_CTRL_OP(ctrl),
                     ok,
                     parser->buf + parser->start + 4U,
                     payload_len,
                     user);
        }

        ++emitted;

        /*
         * 好帧整帧消费；坏帧只前移 1 字节重新扫描——真实帧可能就藏在
         * 这个坏候选的"载荷"里，整帧丢弃会连带吃掉后面的有效数据。
         */
        parser->start += ok ? total : 1U;
    }
}
/**
 * @brief 向解析器喂入数据
 * @param parser 解析器
 * @param bytes 数据指针
 * @param count 数据长度
 * @param on_frame 帧处理函数
 * @param user 用户数据指针
 * @return size_t 解析出的帧数
 */
size_t car_parser_feed(car_parser_t *parser,
                       const uint8_t *bytes,
                       size_t count,
                       car_frame_fn on_frame,
                       void *user)
{
    size_t emitted = 0;

    if(parser == NULL || (bytes == NULL && count != 0)) {
        return 0;
    }

    for(size_t i = 0; i < count; ++i) {
        if(parser->end >= (size_t)CAR_MAX_FRAME) {
            parser_compact(parser);

            /*
             * 窗口等于最大帧长，且 drain 之后必然剩余不足一帧，
             * 所以这里只是防御性兜底，正常永远进不来。
             */
            if(parser->end >= (size_t)CAR_MAX_FRAME) {
                parser->start += 1U;
                parser->stats.overflows++;
                parser_compact(parser);
            }
        }

        parser->buf[parser->end++] = bytes[i];
        emitted += parser_drain(parser, on_frame, user);
    }

    return emitted;
}
