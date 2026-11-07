/*
 * demo.c - 无硬件端到端演示
 *
 * 一条 car_link 挂在一个"内存串口"上，另一端由一个极简的假 MCU 驱动：
 *   - 假 MCU 会响应 0x00 初始化请求
 *   - 每 100ms 上报车速/转速，并按周期上报油量、水温、档位、车门
 * 整个流程（握手、解析、ACK、状态快照、UI 读取）与接真机时完全一致。
 *
 * 构建：make ; ./bin/carmw_demo
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "carmw/car_command.h"
#include "carmw/car_link.h"
#include "carmw/car_port_memory.h"
#include "carmw/car_protocol.h"

#include <stdio.h>
#include <time.h>

#define TICK_MS 100
#define TICK_COUNT 40

static void push_frame(car_port_memory_t *mem,
                       uint8_t cmd_id,
                       uint8_t op,
                       const uint8_t *payload,
                       uint16_t len)
{
    uint8_t frame[CAR_FRAME_OVERHEAD + CAR_CMD_PAYLOAD_MAX];
    size_t frame_len;

    if(len > CAR_CMD_PAYLOAD_MAX) {
        return;
    }

    frame_len = car_frame_encode(cmd_id, op, payload, len, frame, sizeof(frame));

    if(frame_len != 0) {
        car_port_memory_push(mem, frame, frame_len);
    }
}

/* 假 MCU：吃掉屏发来的字节，对初始化请求回 STATUS{0x01} */
static void mcu_task(car_port_memory_t *mem)
{
    uint8_t buffer[256];
    size_t count = car_port_memory_take(mem, buffer, sizeof(buffer));
    size_t offset = 0;

    while(offset < count) {
        if(buffer[offset] == CAR_FRAME_HEAD) {
            car_frame_view_t view;

            if(!car_frame_inspect(buffer + offset, count - offset, &view)) {
                break;
            }

            if(view.cmd_id == 0x00 && view.op == (uint8_t)CAR_OP_SET_GET) {
                const uint8_t initialized[1] = {0x01};
                push_frame(mem, 0x00, (uint8_t)CAR_OP_STATUS, initialized, 1);
            }

            offset += (size_t)CAR_FRAME_OVERHEAD + view.len;
        }
        else {
            offset++; /* 单字节 ACK/NACK */
        }
    }
}

/* 假 MCU：周期上报 */
static void mcu_report(car_port_memory_t *mem, unsigned tick)
{
    uint16_t speed = (uint16_t)(40U + (tick * 3U) % 120U);
    uint16_t rpm = (uint16_t)(900U + (tick * 137U) % 5000U);
    uint8_t speed_rpm[4];

    speed_rpm[0] = (uint8_t)(speed >> 8);
    speed_rpm[1] = (uint8_t)(speed & 0xFFU);
    speed_rpm[2] = (uint8_t)(rpm >> 8);
    speed_rpm[3] = (uint8_t)(rpm & 0xFFU);
    push_frame(mem, 0x04, (uint8_t)CAR_OP_STATUS, speed_rpm, 4);

    if(tick % 5U == 0U) {
        uint8_t fuel[6];
        uint16_t range = (uint16_t)(520U - tick * 2U);
        uint8_t percent = (uint8_t)(78U - tick / 2U);

        fuel[0] = (uint8_t)(range >> 8);
        fuel[1] = (uint8_t)(range & 0xFFU);
        fuel[2] = percent;
        fuel[3] = 0x01; /* 状态正常 */
        fuel[4] = 65;   /* 主油箱 65L */
        fuel[5] = 0;
        push_frame(mem, 0x06, (uint8_t)CAR_OP_STATUS, fuel, 6);
    }

    if(tick % 10U == 0U) {
        uint8_t gear[5];

        gear[0] = 0x03; /* 可见 + 闪烁 */
        gear[1] = (char)'D';
        gear[2] = (char)('1' + (tick / 10U) % 4U);
        gear[3] = 0;
        gear[4] = 0;
        push_frame(mem, 0x33, (uint8_t)CAR_OP_STATUS, gear, 5);
    }

    if(tick % 15U == 0U) {
        uint8_t doors[1] = {0x04}; /* 左后门开 */
        push_frame(mem, 0x0B, (uint8_t)CAR_OP_STATUS, doors, 1);
    }

    if(tick % 20U == 0U) {
        uint8_t coolant[3] = {60, (uint8_t)(50 + 40), 1}; /* 60%, 40C, 偏高 */
        push_frame(mem, 0x15, (uint8_t)CAR_OP_STATUS, coolant, 3);
    }
}

static void print_state(const car_state_view_t *view, unsigned tick)
{
    printf("t=%5.1fs phase=%-5s speed=%3u rpm=%4u fuel=%3u%% %3dC gear=%-2s "
           "doors=[%c%c%c%c] warn=%u tx=%u\n",
           (double)tick * TICK_MS / 1000.0,
           view->state.init.valid ? "ok" : "??",
           (unsigned)view->state.speed_rpm.speed_kph,
           (unsigned)view->state.speed_rpm.engine_rpm,
           (unsigned)view->state.fuel.percent,
           (int)view->state.coolant.celsius,
           view->state.gear.text,
           view->state.doors.left_front ? 'L' : '.',
           view->state.doors.right_front ? 'R' : '.',
           view->state.doors.left_rear ? 'l' : '.',
           view->state.doors.right_rear ? 'r' : '.',
           (unsigned)view->state.warnings.list_count,
           (unsigned)view->version);
}

int main(void)
{
    car_port_memory_t mem;
    car_port_t port;
    car_link_t link;

    car_port_memory_attach(&port, &mem);

    if(!car_link_init(&link, &port)) {
        printf("link init failed\n");
        return 1;
    }

    printf("carmw demo: 内存串口 + 假 MCU\n");
    printf("phase 由握手状态机推进，速度/转速每 100ms 一帧\n\n");

    for(unsigned tick = 0; tick < TICK_COUNT; ++tick) {
        /* 1. 假 MCU 先处理屏发出的字节 */
        mcu_task(&mem);

        /* 2. 假 MCU 上报一帧数据 */
        mcu_report(&mem, tick);

        /* 3. 推进链路（真实项目里这一步放在 lv_timer 中） */
        car_link_pump(&link, 0);

        /* 4. UI 取快照 */
        car_state_view_t view;
        car_link_view(&link, &view);
        print_state(&view, tick);

        car_port_memory_advance(&mem, TICK_MS);

        struct timespec delay;
        delay.tv_sec = 0;
        delay.tv_nsec = (long)TICK_MS * 1000000L;
        (void)nanosleep(&delay, NULL);
    }

    const car_link_stats_t *stats = car_link_stats(&link);

    printf("\n统计: 收帧 %llu / 解码 %llu / 拒收 %llu / 未知 %llu\n",
           (unsigned long long)stats->frames_rx,
           (unsigned long long)stats->frames_decoded,
           (unsigned long long)stats->frames_rejected,
           (unsigned long long)stats->frames_unknown);
    printf("      ACK %llu / NACK %llu / 握手命令 %llu\n",
           (unsigned long long)stats->acks,
           (unsigned long long)stats->nacks,
           (unsigned long long)stats->session_commands);
    printf("      解析器: 噪声字节 %llu / 长度拒收 %llu / 重同步 %llu\n",
           (unsigned long long)link.parser.stats.noise_bytes,
           (unsigned long long)link.parser.stats.length_rejects,
           (unsigned long long)link.parser.stats.resyncs);

    car_link_deinit(&link);
    return 0;
}
