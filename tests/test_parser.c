#include "test_util.h"

#include "carmw/car_parser.h"
#include "carmw/car_protocol.h"
#include "carmw/car_registry.h"

typedef struct {
    uint8_t cmd_id;
    uint8_t op;
    uint16_t len;
    uint8_t payload[64];
    bool ok;
    int count;
} capture_t;

static void capture_frame(uint8_t cmd_id,
                          uint8_t op,
                          bool ok,
                          const uint8_t *payload,
                          uint16_t len,
                          void *user)
{
    capture_t *cap = (capture_t *)user;

    cap->cmd_id = cmd_id;
    cap->op = op;
    cap->ok = ok;
    cap->len = len;

    if(payload != NULL && len <= sizeof(cap->payload)) {
        memcpy(cap->payload, payload, len);
    }

    cap->count++;
}

static void capture_reset(capture_t *cap)
{
    memset(cap, 0, sizeof(*cap));
}

void test_parser_suite(void)
{
    car_parser_t parser;
    capture_t cap;
    uint8_t frame[64];

    const uint8_t speed_payload[4] = {0x00, 0x78, 0x13, 0x88};

    /* ---- 1. 组帧结果必须与现场抓包一致 ---- */
    size_t frame_len = car_frame_encode(0x04, (uint8_t)CAR_OP_STATUS,
                                        speed_payload, 4, frame, sizeof(frame));
    CHECK(frame_len == 9);
    CHECK(frame[0] == 0x2E);
    CHECK(frame[1] == 0x04);
    CHECK(frame[2] == 0xC0); /* op 0xC 在 高 4 位，长度高 4 位为 0 */
    CHECK(frame[3] == 0x04);
    CHECK(frame[8] == 0x24); /* 校验和 */

    /* ---- 2. 一次性喂入 ---- */
    car_parser_init(&parser);
    car_parser_set_policy(&parser, car_registry_accept, NULL);
    capture_reset(&cap);
    CHECK(car_parser_feed(&parser, frame, frame_len, capture_frame, &cap) == 1);
    CHECK(cap.count == 1);
    CHECK(cap.ok);
    CHECK(cap.cmd_id == 0x04);
    CHECK(cap.op == 0x0C);
    CHECK(cap.len == 4);
    CHECK(memcmp(cap.payload, speed_payload, 4) == 0);

    /* ---- 3. 逐字节喂入（模拟慢速串口） ---- */
    car_parser_init(&parser);
    car_parser_set_policy(&parser, car_registry_accept, NULL);
    capture_reset(&cap);

    for(size_t i = 0; i < frame_len; ++i) {
        (void)car_parser_feed(&parser, &frame[i], 1, capture_frame, &cap);
        if(i + 1 < frame_len) {
            CHECK(cap.count == 0); /* 半帧不能提前上报 */
        }
    }

    CHECK(cap.count == 1);

    /* ---- 4. 一次读到两帧（粘包） ---- */
    {
        uint8_t two[128];
        memcpy(two, frame, frame_len);
        memcpy(two + frame_len, frame, frame_len);

        car_parser_init(&parser);
        car_parser_set_policy(&parser, car_registry_accept, NULL);
        capture_reset(&cap);
        CHECK(car_parser_feed(&parser, two, frame_len * 2, capture_frame,
                              &cap) == 2);
        CHECK(cap.count == 2);
    }

    /* ---- 5. 帧头前有噪声 ---- */
    {
        uint8_t noisy[80];
        noisy[0] = 0x00;
        noisy[1] = 0xFF;
        memcpy(noisy + 2, frame, frame_len);

        car_parser_init(&parser);
        car_parser_set_policy(&parser, car_registry_accept, NULL);
        capture_reset(&cap);
        CHECK(car_parser_feed(&parser, noisy, frame_len + 2, capture_frame,
                              &cap) == 1);

        car_parser_stats_t stats;
        car_parser_get_stats(&parser, &stats);
        CHECK(stats.noise_bytes == 2);
    }

    /* ---- 6. 校验错误：上报 + 紧跟着的好帧必须能恢复 ---- */
    {
        uint8_t broken[64];
        memcpy(broken, frame, frame_len);
        broken[frame_len - 1] ^= 0x01;

        memcpy(broken + frame_len, frame, frame_len);

        car_parser_init(&parser);
        car_parser_set_policy(&parser, car_registry_accept, NULL);
        capture_reset(&cap);
        CHECK(car_parser_feed(&parser, broken, frame_len * 2, capture_frame,
                              &cap) == 2);
        CHECK(cap.count == 2);
        CHECK(cap.ok); /* 第二帧必须是好的 */

        car_parser_stats_t stats;
        car_parser_get_stats(&parser, &stats);
        CHECK(stats.checksum_errors == 1);
        CHECK(stats.frames_ok == 1);
    }

    /* ---- 6b. 假候选帧把真帧"吞"进自己的载荷：只退 1 字节才能捞出来 ---- */
    {
        uint8_t stream[64];
        size_t offset = 0;

        /* 伪造头：cmd=0x02, len=20 -> 总共 25 字节的候选帧 */
        stream[offset++] = 0x2E;
        stream[offset++] = 0x02;
        stream[offset++] = 0x00; /* op 0 / 长度高位 0 */
        stream[offset++] = 20;

        /* 真帧的 9 个字节整个落在候选帧的"载荷"里 */
        memcpy(stream + offset, frame, frame_len);
        offset += frame_len;

        /* 再补 12 字节（11 字节载荷 + 1 字节假校验）凑满这个候选帧 */
        for(int i = 0; i < 12; ++i) {
            stream[offset++] = (uint8_t)(0xA0 + i);
        }

        CHECK(offset == 25); /* 4 头 + 20 载荷 + 1 校验 */

        car_parser_init(&parser);
        car_parser_set_policy(&parser, car_registry_accept, NULL);
        capture_reset(&cap);

        size_t emitted = car_parser_feed(&parser, stream, offset,
                                         capture_frame, &cap);

        CHECK(emitted == 2); /* 一个坏候选 + 一个真帧 */
        CHECK(cap.count == 2);
        CHECK(cap.ok);
        CHECK(cap.cmd_id == 0x04);
        CHECK(memcmp(cap.payload, speed_payload, 4) == 0);
    }

    /* ---- 7. 长度与协议不符：当成噪声丢掉，后面的真帧照样解析 ---- */
    {
        /* cmd=0x04 的最大长度是 4，这里谎报 767 字节 */
        uint8_t bogus[8] = {0x2E, 0x04, 0x02, 0xFF, 0x11, 0x22, 0x33, 0x44};
        uint8_t stream[80];
        memcpy(stream, bogus, sizeof(bogus));
        memcpy(stream + sizeof(bogus), frame, frame_len);

        car_parser_init(&parser);
        car_parser_set_policy(&parser, car_registry_accept, NULL);
        capture_reset(&cap);
        CHECK(car_parser_feed(&parser, stream, sizeof(bogus) + frame_len,
                              capture_frame, &cap) == 1);
        CHECK(cap.count == 1);
        CHECK(cap.ok);

        car_parser_stats_t stats;
        car_parser_get_stats(&parser, &stats);
        CHECK(stats.length_rejects >= 1);
        CHECK(stats.frames_ok == 1);
    }

    /* ---- 8. 没有 policy 时只能靠 4095 上限，767 字节会被当成候选帧 ---- */
    {
        uint8_t bogus[8] = {0x2E, 0x04, 0x02, 0xFF, 0x11, 0x22, 0x33, 0x44};

        car_parser_init(&parser);
        capture_reset(&cap);
        CHECK(car_parser_feed(&parser, bogus, sizeof(bogus), capture_frame,
                              &cap) == 0);
        CHECK(cap.count == 0);
    }

    /* ---- 9. 载荷里出现 0x2E 也不能被误判成帧头 ---- */
    {
        const uint8_t tricky[4] = {0x2E, 0x2E, 0x2E, 0x2E};
        uint8_t tricky_frame[16];
        size_t len = car_frame_encode(0x04, (uint8_t)CAR_OP_STATUS, tricky, 4,
                                      tricky_frame, sizeof(tricky_frame));
        CHECK(len == 9);

        car_parser_init(&parser);
        car_parser_set_policy(&parser, car_registry_accept, NULL);
        capture_reset(&cap);
        CHECK(car_parser_feed(&parser, tricky_frame, len, capture_frame, &cap) ==
              1);
        CHECK(cap.count == 1);
        CHECK(cap.ok && cap.len == 4);
        CHECK(memcmp(cap.payload, tricky, 4) == 0);
    }

    /* ---- 10. 最大载荷（4095）刚好顶到解析窗口 ---- */
    {
        static uint8_t big_payload[CAR_MAX_PAYLOAD];
        static uint8_t big_frame[CAR_MAX_FRAME];

        for(size_t i = 0; i < sizeof(big_payload); ++i) {
            big_payload[i] = (uint8_t)(i * 7U);
        }
        big_payload[100] = 0x2E;

        size_t len = car_frame_encode(0x01, (uint8_t)CAR_OP_STATUS, big_payload,
                                      (uint16_t)sizeof(big_payload), big_frame,
                                      sizeof(big_frame));
        CHECK(len == CAR_MAX_FRAME);

        car_parser_init(&parser);
        car_parser_set_policy(&parser, car_registry_accept, NULL);
        capture_reset(&cap);

        /* 逐字节喂，确保窗口边界不出错 */
        size_t emitted = 0;
        for(size_t i = 0; i < len; ++i) {
            emitted += car_parser_feed(&parser, &big_frame[i], 1, capture_frame,
                                       &cap);
        }

        CHECK(emitted == 1);
        CHECK(cap.count == 1);
        CHECK(cap.ok);
        CHECK(cap.len == CAR_MAX_PAYLOAD);

        car_parser_stats_t stats;
        car_parser_get_stats(&parser, &stats);
        CHECK(stats.overflows == 0);
    }

    /* ---- 11. car_frame_inspect 的基本行为 ---- */
    {
        car_frame_view_t view;

        CHECK(car_frame_inspect(frame, frame_len, &view));
        CHECK(view.cmd_id == 0x04 && view.op == 0x0C && view.len == 4);
        CHECK(view.checksum_ok);

        CHECK(!car_frame_inspect(frame, frame_len - 1, &view));
        CHECK(!car_frame_inspect(frame + 1, frame_len, &view));
    }

    /* ---- 12. 组帧参数保护 ---- */
    {
        uint8_t out[8];

        CHECK(car_frame_encode(0x04, 0x10, speed_payload, 4, out, sizeof(out)) ==
              0); /* op 超过 4 bit */
        CHECK(car_frame_encode(0x04, 0x0C, speed_payload, 4, out, 5) ==
              0); /* 容量不足 */
        CHECK(car_frame_encode(0x04, 0x0C, NULL, 1, out, sizeof(out)) == 0);
    }
}
