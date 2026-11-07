#include "test_util.h"

#include "carmw/car_link.h"
#include "carmw/car_port_memory.h"
#include "carmw/car_protocol.h"

void test_link_suite(void)
{
    car_port_memory_t mem;
    car_port_t port;
    car_link_t link;
    car_state_view_t view;
    uint8_t out[512];
    uint8_t frame[64];
    size_t n;

    car_port_memory_attach(&port, &mem);
    CHECK(car_link_init(&link, &port));
    CHECK(car_link_is_running(&link));
    CHECK(strcmp(car_link_phase_name(&link), "init") == 0);

    /* ---- 1. 上电后先发握手：0x00 SETGET{0x02} + 0x14 GET ---- */
    car_link_pump(&link, 0);
    n = car_port_memory_take(&mem, out, sizeof(out));
    CHECK(n == 11);
    CHECK(out[0] == 0x2E && out[1] == 0x00 && out[2] == 0x20 && out[3] == 0x01 &&
          out[4] == 0x02 && out[5] == 0xDC);
    CHECK(out[6] == 0x2E && out[7] == 0x14 && out[8] == 0x10 && out[9] == 0x00 &&
          out[10] == 0xDB);

    /* ---- 2. 正常帧：状态入库 + 回 ACK ---- */
    {
        const uint8_t payload[4] = {0x00, 0x78, 0x13, 0x88}; /* 120km/h 5000rpm */
        size_t flen = car_frame_encode(0x04, (uint8_t)CAR_OP_STATUS, payload, 4,
                                       frame, sizeof(frame));

        car_port_memory_advance(&mem, 100);
        car_port_memory_push(&mem, frame, flen);
        car_link_pump(&link, 0);

        car_link_view(&link, &view);
        CHECK(view.has_data);
        CHECK(view.state.speed_rpm.speed_kph == 120);
        CHECK(view.state.speed_rpm.engine_rpm == 5000);
        CHECK((view.changed & CAR_CH_SPEED_RPM) != 0);

        n = car_port_memory_take(&mem, out, sizeof(out));
        CHECK(n == 1);
        CHECK(out[0] == CAR_ACK_OK);
    }

    /* ---- 3. 校验错误：回 0xF0，状态不动 ---- */
    {
        const uint8_t payload[4] = {0x00, 0x10, 0x02, 0x00};
        size_t flen = car_frame_encode(0x04, (uint8_t)CAR_OP_STATUS, payload, 4,
                                       frame, sizeof(frame));

        frame[flen - 1] ^= 0xFF;

        car_port_memory_advance(&mem, 100);
        car_port_memory_push(&mem, frame, flen);
        car_link_pump(&link, 0);

        car_link_view(&link, &view);
        CHECK(view.state.speed_rpm.speed_kph == 120);

        n = car_port_memory_take(&mem, out, sizeof(out));
        CHECK(n == 1);
        CHECK(out[0] == CAR_ACK_CHECKSUM);
        CHECK(car_link_stats(&link)->nacks == 1);
    }

    /* ---- 4. 未登记命令：回 0xF3 ---- */
    {
        const uint8_t payload[2] = {0xAA, 0xBB};
        size_t flen = car_frame_encode(0x99, (uint8_t)CAR_OP_STATUS, payload, 2,
                                       frame, sizeof(frame));

        car_port_memory_advance(&mem, 100);
        car_port_memory_push(&mem, frame, flen);
        car_link_pump(&link, 0);

        n = car_port_memory_take(&mem, out, sizeof(out));
        CHECK(n == 1);
        CHECK(out[0] == CAR_ACK_INVALID);
        CHECK(car_link_stats(&link)->frames_unknown == 1);
    }

    /* ---- 5. 长度合法但内容非法（转速 10001）：回 0xF3 ---- */
    {
        const uint8_t payload[4] = {0x00, 0x10, 0x27, 0x11}; /* 10001 rpm */
        size_t flen = car_frame_encode(0x04, (uint8_t)CAR_OP_STATUS, payload, 4,
                                       frame, sizeof(frame));

        car_port_memory_advance(&mem, 100);
        car_port_memory_push(&mem, frame, flen);
        car_link_pump(&link, 0);

        car_link_view(&link, &view);
        CHECK(view.state.speed_rpm.engine_rpm == 5000); /* 保持上一次的值 */

        n = car_port_memory_take(&mem, out, sizeof(out));
        CHECK(n == 1);
        CHECK(out[0] == CAR_ACK_INVALID);
        CHECK(car_link_stats(&link)->frames_rejected == 1);
    }

    /* ---- 6. 下发命令：入队 -> pump 写出，字节完全符合协议 ---- */
    {
        car_cmd_t req;
        car_frame_view_t decoded;

        CHECK(car_cmd_set_time(&req, 2026, 11, 1, 18, 30));
        CHECK(car_link_exec(&link, &req));

        car_port_memory_advance(&mem, 100);
        car_link_pump(&link, 0);

        n = car_port_memory_take(&mem, out, sizeof(out));
        CHECK(n == 11);
        CHECK(car_frame_inspect(out, n, &decoded));
        CHECK(decoded.cmd_id == 0x32);
        CHECK(decoded.op == CAR_OP_SET_GET);
        CHECK(decoded.len == 6);
        CHECK(decoded.checksum_ok);
        CHECK(decoded.payload[3] == (uint8_t)(18 | 0x80));
    }

    /* ---- 7. 方向盘按键：事件队列 + 状态 ---- */
    {
        const uint8_t payload[2] = {0x25, 1};
        size_t flen = car_frame_encode(0x20, (uint8_t)CAR_OP_STATUS, payload, 2,
                                       frame, sizeof(frame));
        car_key_event_t keys[CAR_LINK_KEY_DEPTH];

        car_port_memory_advance(&mem, 100);
        car_port_memory_push(&mem, frame, flen);
        car_link_pump(&link, 0);

        (void)car_port_memory_take(&mem, out, sizeof(out)); /* ACK */

        CHECK(car_link_take_keys(&link, keys, CAR_LINK_KEY_DEPTH) == 1);
        CHECK(keys[0].code == 0x25);
        CHECK(keys[0].pressed == 1);

        car_link_view(&link, &view);
        CHECK(view.state.key.valid);
        CHECK(view.state.key.code == 0x25);

        /* 队列已空 */
        CHECK(car_link_take_keys(&link, keys, CAR_LINK_KEY_DEPTH) == 0);
    }

    /* ---- 8. 半帧跨两次 pump ---- */
    {
        const uint8_t payload[4] = {0x00, 0x50, 0x0B, 0xB8}; /* 80km/h 3000rpm */
        size_t flen = car_frame_encode(0x04, (uint8_t)CAR_OP_STATUS, payload, 4,
                                       frame, sizeof(frame));

        car_port_memory_advance(&mem, 100);
        car_port_memory_push(&mem, frame, 4);
        car_link_pump(&link, 0);

        car_link_view(&link, &view);
        CHECK(view.state.speed_rpm.speed_kph == 120); /* 半帧还不算数 */

        car_port_memory_advance(&mem, 100);
        car_port_memory_push(&mem, frame + 4, flen - 4);
        car_link_pump(&link, 0);

        car_link_view(&link, &view);
        CHECK(view.state.speed_rpm.speed_kph == 80);
        CHECK(view.state.speed_rpm.engine_rpm == 3000);
    }

    /* ---- 9. 握手流程：0x02 -> 批量拉取 13 条 -> 0x01 -> ready ---- */
    {
        const uint8_t initializing[1] = {0x02};
        const uint8_t ready[1] = {0x01};
        size_t flen;
        int frames = 0;

        flen = car_frame_encode(0x00, (uint8_t)CAR_OP_STATUS, initializing, 1,
                                frame, sizeof(frame));

        car_port_memory_advance(&mem, 100);
        car_port_memory_push(&mem, frame, flen);
        car_link_pump(&link, 0);
        (void)car_port_memory_take(&mem, out, sizeof(out)); /* ACK */

        CHECK(strcmp(car_link_phase_name(&link), "bulk") == 0);

        for(int i = 0; i < 13; ++i) {
            car_port_memory_advance(&mem, CAR_SESSION_BULK_INTERVAL_MS);
            car_link_pump(&link, 0);

            n = car_port_memory_take(&mem, out, sizeof(out));
            if(n >= CAR_FRAME_OVERHEAD && out[0] == CAR_FRAME_HEAD) {
                frames++;
            }
        }

        CHECK(frames == 13);
        CHECK(strcmp(car_link_phase_name(&link), "init") == 0);

        flen = car_frame_encode(0x00, (uint8_t)CAR_OP_STATUS, ready, 1, frame,
                                sizeof(frame));
        car_port_memory_advance(&mem, 100);
        car_port_memory_push(&mem, frame, flen);
        car_link_pump(&link, 0);

        CHECK(strcmp(car_link_phase_name(&link), "ready") == 0);
    }

    /* ---- 10. 诊断记录 ---- */
    {
        car_trace_entry_t entries[CAR_LINK_TRACE_DEPTH];
        size_t count = car_link_take_trace(&link, entries, CAR_LINK_TRACE_DEPTH);

        CHECK(count > 0);
        CHECK(entries[0].cmd_id == 0x04); /* 最早一条就是步骤 2 的车速帧 */
        CHECK(entries[0].decoded);

        /* 取过就空了 */
        CHECK(car_link_take_trace(&link, entries, CAR_LINK_TRACE_DEPTH) == 0);
    }

    /* ---- 11. 统计口径 ---- */
    {
        const car_link_stats_t *stats = car_link_stats(&link);

        /* 收帧：2 正常车速 + 1 坏校验 + 1 未登记 + 1 非法转速 + 1 按键
         *       + 1 拆分车速 + 2 握手状态 = 8 */
        CHECK(stats->frames_rx == 8);
        CHECK(stats->frames_decoded == 5);
        CHECK(stats->frames_rejected == 1);
        CHECK(stats->frames_unknown == 1);
        CHECK(stats->tx_queued == 1);
        CHECK(stats->session_commands == 15); /* 2 条握手 + 13 条批量 */
        CHECK(stats->acks == 5);
        CHECK(stats->nacks == 3); /* 0xF0 x1 + 0xF3 x2 */
    }

    /* ---- 12. 未打开 port 时的降级：不崩、状态仍可读 ---- */
    {
        car_link_t cold;
        car_port_t closed_port;

        memset(&closed_port, 0, sizeof(closed_port));
        CHECK(!car_link_init(&cold, &closed_port));
        CHECK(!car_link_is_running(&cold));

        car_link_pump(&cold, 0); /* 空操作 */

        car_state_view_t cold_view;
        car_link_view(&cold, &cold_view);
        CHECK(!cold_view.has_data);
        CHECK(cold_view.state.speed_rpm.speed_kph == 0);

        car_cmd_t req;
        CHECK(car_cmd_request(&req, 0x04));
        CHECK(!car_link_exec(&cold, &req));

        car_link_deinit(&cold);
    }

    car_link_deinit(&link);
    CHECK(!car_link_is_running(&link));
}
