/*
 * main.c - 仪表页入口（LVGL 9.5 + carmw）
 *
 * PC   (CARMW_PLATFORM_PC)    ：SDL2 窗口 + 内存串口 + 假 MCU + 协作式 pump
 * 板子 (CARMW_PLATFORM_BOARD) ：/dev/fb0 + 真实串口 + 后台收帧线程
 *
 * 两种模式共用同一套 carmw 中间件和 UI 样式，只有数据源/驱动方式不同。
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "lvgl.h"

#include "carmw/car_link.h"
#include "carmw/car_color.h"
#include "ui/ui_app.h"

#if defined(CARMW_PLATFORM_BOARD)
#include <signal.h>

#include "carmw/car_port_posix.h"
#include "carmw/car_settings.h"
#include "carmw/car_settings_saver.h"
#include "ui/car_ui_key.h"
#else
#include "carmw/car_command.h"
#include "carmw/car_port_memory.h"
#include "carmw/car_protocol.h"
#include "ui/ui_snapshot.h"
#include <SDL2/SDL.h>
#include <signal.h>
#endif

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 720
#define REFRESH_MS 20  /* 界面刷新周期 */
#define REPORT_EVERY 3 /* PC 假 MCU 约 100ms 上报一帧车况 */

#if defined(CARMW_PLATFORM_BOARD)
#define SERIAL_DEVICE "/dev/ttyS2"
#define SERIAL_BAUD 115200

static car_port_posix_t s_posix;
static volatile sig_atomic_t s_quit;

static void on_quit_signal(int signo)
{
    (void)signo;
    s_quit = 1;
}

static int ui_should_quit(void)
{
    return s_quit != 0;
}
#else
static car_port_memory_t s_mem;
static unsigned s_tick;
/* 演示轮播当前点亮的故障码下标，换码时先把上一盏关掉 */
static unsigned s_lamp_idx;
static unsigned s_lamp_has_prev;

static int ui_should_quit(void)
{
    return 0; /* 关闭 SDL 窗口时由驱动直接退出进程 */
}
#endif

static car_port_t s_port;
static car_link_t s_link;
static lv_timer_t *s_refresh_timer;

#if defined(CARMW_PLATFORM_BOARD)
/* 本地持久设置：运行期只在内存，ACC 关闭统一落盘 */
static car_settings_t s_settings;
static car_settings_saver_t s_settings_saver;
#else
/* PC 演示：设置只在内存里，进程退出即丢（文件读写只在板端验证） */
static car_settings_t s_settings;
#endif

/* 设置变更 -> 刷新设置页显示（设置者线程内同步回调） */
static void settings_changed(void *user, uint32_t mask)
{
    (void)user;
    (void)mask;
    ui_app_settings_refresh();
}

/* 注入给设置页：常规设置的背光/超速报警直接下发到 MCU */
static void settings_send_cmd(const car_cmd_t *cmd)
{
    (void)car_link_exec(&s_link, cmd);
}

#if defined(CARMW_PLATFORM_BOARD)
/* 注入给设置页：还有改动未落盘时显示提示 */
static bool settings_pending(void)
{
    return car_settings_saver_pending(&s_settings_saver);
}
#endif

/* 启动样式取持久设置；设置里的 id 在注册表中不存在时回落到 forester1 */
static const char *settings_start_style(void)
{
    const char *id = s_settings.values.dial_style;

    if(id[0] == '\0' || ui_style_find(id) == NULL) {
        return "forester1";
    }

    return id;
}

/* ------------------------------------------------------------------ */
/* 假 MCU（仅 PC 演示用）                                              */
/* ------------------------------------------------------------------ */

#if !defined(CARMW_PLATFORM_BOARD)

static void push_frame(uint8_t cmd_id, uint8_t op,
                       const uint8_t *payload, uint16_t len)
{
    uint8_t frame[CAR_FRAME_OVERHEAD + CAR_CMD_PAYLOAD_MAX];
    size_t frame_len;

    if(len > CAR_CMD_PAYLOAD_MAX) {
        return;
    }

    frame_len = car_frame_encode(cmd_id, op, payload, len, frame, sizeof(frame));

    if(frame_len != 0) {
        car_port_memory_push(&s_mem, frame, frame_len);
    }
}

/* 处理屏发出的字节：对初始化请求回 STATUS{0x01}（演示与固定场景共用） */
static void fake_mcu_service(void)
{
    uint8_t buffer[256];
    size_t count = car_port_memory_take(&s_mem, buffer, sizeof(buffer));
    size_t offset = 0;

    while(offset < count) {
        if(buffer[offset] == CAR_FRAME_HEAD) {
            car_frame_view_t view;

            if(!car_frame_inspect(buffer + offset, count - offset, &view)) {
                break;
            }

            if(view.cmd_id == 0x00 && view.op == (uint8_t)CAR_OP_SET_GET) {
                const uint8_t initialized[1] = {0x01};
                push_frame(0x00, (uint8_t)CAR_OP_STATUS, initialized, 1);
            }

            offset += (size_t)CAR_FRAME_OVERHEAD + view.len;
        }
        else {
            offset++; /* 单字节 ACK/NACK */
        }
    }
}

static void fake_mcu_tick(void)
{
    unsigned tick;

    fake_mcu_service();

    /* 每 3 个刷新周期（约 100ms）上报一帧车况 */
    s_tick++;
    if(s_tick % REPORT_EVERY != 0U) {
        return;
    }
    tick = s_tick / REPORT_EVERY;

    {
        uint16_t speed = (uint16_t)(40U + (tick * 3U) % 120U);
        uint16_t rpm = (uint16_t)(900U + (tick * 137U) % 5000U);
        uint8_t speed_rpm[4];
        uint8_t fuel[6];
        uint8_t gear[5];
        uint8_t doors[1]; /* 车门按相位轮换，便于验证覆盖层 */
        uint8_t coolant[3] = {60, 88, 1};
        unsigned door_phase = (tick / 50U) % 4U;

        /* 0 关闭 / 1 右前 / 2 关闭 / 3 左前 + 后备箱 */
        if(door_phase == 1U) {
            doors[0] = 0x02U;
        }
        else if(door_phase == 3U) {
            doors[0] = 0x11U;
        }
        else {
            doors[0] = 0x00U;
        }

        speed_rpm[0] = (uint8_t)(speed >> 8);
        speed_rpm[1] = (uint8_t)(speed & 0xFFU);
        speed_rpm[2] = (uint8_t)(rpm >> 8);
        speed_rpm[3] = (uint8_t)(rpm & 0xFFU);
        push_frame(0x04, (uint8_t)CAR_OP_STATUS, speed_rpm, 4);

        fuel[0] = (uint8_t)((520U - (tick % 40U) * 6U) >> 8);
        fuel[1] = (uint8_t)((520U - (tick % 40U) * 6U) & 0xFFU);
        fuel[2] = (uint8_t)(78U - (tick % 40U));
        fuel[3] = (uint8_t)(((tick / 50U) % 4U == 2U) ? 0x02U : 0x01U);
        fuel[4] = 65;
        fuel[5] = 0;
        push_frame(0x06, (uint8_t)CAR_OP_STATUS, fuel, 6);

        gear[0] = 0x03;
        gear[1] = (uint8_t)'D';
        gear[2] = (uint8_t)('1' + (tick / 5U) % 4U);
        gear[3] = 0;
        gear[4] = 0;
        push_frame(0x33, (uint8_t)CAR_OP_STATUS, gear, 5);

        /* 单位 0x39：约每 10s 在公制/英里制之间切换，验证刻度、指针、续航与里程 */
        {
            uint8_t unit[3];

            unit[0] = (uint8_t)(((tick / 100U) % 2U != 0U) ? 0x01U : 0x00U);
            unit[1] = 0x00U;
            unit[2] = 0x00U;
            push_frame(0x39, (uint8_t)CAR_OP_STATUS, unit, 3);
        }

        push_frame(0x0B, (uint8_t)CAR_OP_STATUS, doors, 1);
        push_frame(0x15, (uint8_t)CAR_OP_STATUS, coolant, 3);

        {
            /* 转向灯：约 2s 左、2s 右交替，方便本地看闪烁 */
            uint8_t turn[1];

            turn[0] = ((tick / 20U) % 2U == 0U) ? 0x01U : 0x02U;
            push_frame(0x35, (uint8_t)CAR_OP_STATUS, turn, 1);
        }
        {
            /* 警示灯/巡航/小车门：约 0.5s 换一个码，按 Qt iconBindMap 顺序验证 */
            static const uint8_t warn_codes[] = {
                16U, 12U, 15U, 229U, 9U, 11U, 10U,  /* 顶部 6 盏灯 + 近光故障图 */
                22U,                                 /* 小车门 */
                31U, 99U, 239U, 240U, 241U,          /* 巡航 5 种图标 */
                /* 左表盘灯群：主图标 + 备选图标 */
                5U, 23U, 46U, 24U, 210U, 216U, 228U,
                107U, 108U, 207U, 19U, 20U,
                49U, 52U, 50U, 51U,
                8U, 204U, 232U, 233U, 234U,
                235U, 220U, 13U, 14U, 6U, 4U,
                59U, 129U, 173U,
                /* 右表盘灯群 */
                206U, 25U, 139U, 140U, 203U, 36U, 33U, 238U,
                236U, 237U, 222U, 17U, 18U, 3U,
                /* 底部灯排 */
                2U, 26U, 27U, 219U, 28U, 30U, 94U
            };
            uint8_t warn[2];
            unsigned idx = (tick / 5U) % (unsigned)(sizeof(warn_codes) /
                                                    sizeof(warn_codes[0]));

            /*
             * 单灯轮播：灯态按码持久保存，只发 ON 会让旧灯一直亮。
             * 换码时先发上一盏 OFF，再发当前盏 ON；多灯并亮见固定场景。
             */
            if(s_lamp_has_prev && idx != s_lamp_idx) {
                warn[0] = 0U;
                warn[1] = warn_codes[s_lamp_idx];
                push_frame(0x02, (uint8_t)CAR_OP_STATUS, warn, 2);
            }

            if(!s_lamp_has_prev || idx != s_lamp_idx) {
                /* 241 用快闪，验证巡航图标的 300ms 闪烁 */
                warn[0] = (idx == 12U) ? 2U : 1U;
                warn[1] = warn_codes[idx];
                push_frame(0x02, (uint8_t)CAR_OP_STATUS, warn, 2);

                s_lamp_idx = idx;
                s_lamp_has_prev = 1U;
            }
        }
        {
            /* 时钟：跟随本机时间，bit7 置位表示 24 小时制 */
            time_t now = time(NULL);
            struct tm tm_now;
            uint8_t clock[6];
            uint8_t other[6];

            (void)localtime_r(&now, &tm_now);

            clock[0] = (uint8_t)(tm_now.tm_year % 100);
            clock[1] = (uint8_t)(tm_now.tm_mon + 1);
            clock[2] = (uint8_t)tm_now.tm_mday;
            clock[3] = (uint8_t)(tm_now.tm_hour | 0x80);
            clock[4] = (uint8_t)tm_now.tm_min;
            clock[5] = (uint8_t)tm_now.tm_sec;
            push_frame(0x32, (uint8_t)CAR_OP_STATUS, clock, 6);

            /* 其他信息：车外温度 raw 0xA8 → (0xA8-0x7C)×0.5 = 22.0 ℃ */
            other[0] = 60;
            other[1] = 124;
            other[2] = 40;
            other[3] = 0xA8;
            other[4] = 70;
            other[5] = (uint8_t)(0x30 + 90);
            push_frame(0x38, (uint8_t)CAR_OP_STATUS, other, 6);
        }
        {
            /* 定速巡航：设定速度每 2s 在 60~99 之间变化；
             * controlLed 每 20s 亮 5s（与车道演示同窗口），acc_state 带前车/跟车距离 */
            uint8_t cruise[5];
            unsigned cruise_on = (tick / 50U) % 4U;

            cruise[0] = 0U;                              /* text_id */
            cruise[1] = (cruise_on == 0U) ? 0x01U : 0x00U;   /* controlLed */
            cruise[2] = (uint8_t)(60U + (tick / 20U) % 40U);
            cruise[3] = (uint8_t)(0x48U |                    /* showacc + showfront */
                                  ((cruise_on == 0U) ? ((tick / 10U) % 4U) : 0U));
            cruise[4] = 0U;                              /* limit_speed_kph */
            push_frame(0x13, (uint8_t)CAR_OP_STATUS, cruise, 5);
        }
        {
            /* 车道偏离预警 0x16 sub_id 0x0E：演示窗口内车身 1/2/3 每秒轮换，
             * 交替点亮盲区(showarea)与紧急制动(showcar) */
            uint8_t hf[7];
            unsigned nav_on = (tick / 50U) % 4U;

            hf[0] = 0x0EU;
            hf[1] = 0x00U;
            hf[2] = (nav_on == 0U) ? 0x01U : 0x00U;      /* laneCenter */
            hf[3] = 0x00U;                                /* 车道线状态 */
            hf[4] = (uint8_t)((nav_on == 0U)
                              ? (uint8_t)(1U + (tick / 10U) % 3U)
                              : 0x00U);                   /* car.position */
            hf[5] = (uint8_t)((nav_on == 0U && (tick / 30U) % 2U != 0U) ? 2U : 0U);
            hf[6] = (uint8_t)((nav_on == 0U && (tick / 30U) % 2U != 0U) ? 1U : 0U);
            push_frame(0x16, (uint8_t)CAR_OP_STATUS, hf, 7);
        }
        {
            /* 通用提示弹窗 0x03：id 23/35/4 每 5s 轮换，显示 3s，验证提示层 */
            static const uint16_t popup_ids[] = {23U, 35U, 4U};
            uint8_t popup[4];
            unsigned idx = (tick / 50U) % 3U;

            popup[0] = (uint8_t)((popup_ids[idx] >> 8) & 0xFFU);
            popup[1] = (uint8_t)(popup_ids[idx] & 0xFFU);
            popup[2] = (uint8_t)popup_ids[idx];   /* icon */
            popup[3] = 3U;                        /* duration_s */
            push_frame(0x03, (uint8_t)CAR_OP_STATUS, popup, 4);
        }
        {
            /* 复位后统计：平均油耗 8.5、平均车速 62、行驶 3h12m、里程 1560km */
            uint8_t trip_stats[10];

            trip_stats[0] = 0U;
            trip_stats[1] = 85U;
            trip_stats[2] = 0U;
            trip_stats[3] = 62U;
            trip_stats[4] = 0U;                          /* drive_minutes 高字节 */
            trip_stats[5] = 0U;                          /* drive_minutes 中字节 */
            trip_stats[6] = 192U;                        /* 192min = 3h12m */
            trip_stats[7] = 0U;                          /* distance_km 高字节 */
            trip_stats[8] = (uint8_t)((1560U >> 8) & 0xFFU);
            trip_stats[9] = (uint8_t)(1560U & 0xFFU);
            push_frame(0x0C, (uint8_t)CAR_OP_STATUS, trip_stats, 10);
        }
                {
            /* 胎压：FL 240 / FR 245 / RL 238 / RR 243 kPa；
             * 每 5s 轮换一次，第 2 相位把左前设为偏低（state=2）验证覆盖层 */
            uint8_t tire[10];
            static const uint16_t s_kpa[4] = {240U, 245U, 238U, 243U};
            unsigned tire_phase = (tick / 50U) % 4U;
            uint8_t i;

            for(i = 0U; i < 4U; ++i) {
                uint8_t state = (i == 0U && tire_phase == 2U) ? 2U : 1U;

                tire[i * 2U] = (uint8_t)(((s_kpa[i] >> 8) & 0x0FU) |
                                         (uint8_t)(state << 4));
                tire[i * 2U + 1U] = (uint8_t)(s_kpa[i] & 0xFFU);
            }
            tire[8] = 0x00U;   /* 备胎 0 */
            tire[9] = 0x00U;
            push_frame(0x0A, (uint8_t)CAR_OP_STATUS, tire, 10);
        }
        {
            /* 泊车雷达 0x22：前 4 路轮流绿/黄/红，display_on 约 4s 开 4s 关 */
            uint8_t radar[11];
            unsigned radar_on = (tick / 40U) % 2U;
            unsigned radar_color = (tick / 5U) % 3U;
            uint8_t i;

            for(i = 0U; i < 5U; ++i) {
                radar[i] = 0x3FU;          /* 前 5 路默认无目标 */
                radar[i + 5U] = 0x3FU;     /* 后 5 路默认无目标 */
            }

            if(radar_on == 0U) {
                for(i = 0U; i < 4U; ++i) {
                    uint8_t color = (uint8_t)((radar_color + i) % 3U);

                    radar[i] = (uint8_t)((color << 6) | (uint8_t)(20U + i * 3U));
                }
            }

            radar[10] = (radar_on == 0U) ? 0x01U : 0x00U;
            push_frame(0x22, (uint8_t)CAR_OP_STATUS, radar, 11);
        }
                {
            /* 转向角 64° 向右，底盘升降 + 四轮驱动指示全亮 */
            uint8_t traj[2];

            traj[0] = (uint8_t)(0x80U | 64U);
            traj[1] = 0x1FU;
            push_frame(0x26, (uint8_t)CAR_CH_TRAJECTORY, traj, 2);
        }
                {
            /* 短期记忆：平均油耗 8.5、平均车速 62、时长 192 分钟、里程 12km */
            uint8_t trip[8];

            trip[0] = 0x00U; trip[1] = 85U;    /* avg_fuel_x10 */
            trip[2] = 0x00U; trip[3] = 62U;    /* avg_speed_kph */
            trip[4] = 0x00U; trip[5] = 192U;   /* drive_minutes */
            trip[6] = 0x00U; trip[7] = 12U;    /* distance_km */
            push_frame(0x0E, (uint8_t)CAR_CH_TRIP, trip, 8);
        }
        {
            /* 瞬时油耗 0x11：L/100km → L/h → 0x7FFF 三种状态轮流，每种约 3s */
            uint16_t value_x10 = (uint16_t)(50U + (tick * 3U) % 80U);   /* 5.0~12.9 */
            unsigned mode = (tick / 30U) % 3U;
            uint16_t raw;
            uint8_t instant[2];

            if(mode == 1U) {
                raw = (uint16_t)(0x8000U | value_x10);   /* 按小时油耗 */
            }
            else if(mode == 2U) {
                raw = 0x7FFFU;                           /* “---.-” */
            }
            else {
                raw = value_x10;                         /* L/100km */
            }

            instant[0] = (uint8_t)(raw >> 8);
            instant[1] = (uint8_t)(raw & 0xFFU);
            push_frame(0x11, (uint8_t)CAR_OP_STATUS, instant, 2);
        }
                        {
            /* ECO 0x0F：value 0~255 循环递增；每约 4s 切换 enabled，验证两种模式 */
            uint8_t eco[2];
            uint8_t value = (uint8_t)((tick * 17U) % 256U);
            uint8_t enabled = (uint8_t)(((tick / 40U) % 2U) == 0U ? 0x10U : 0x00U);

            eco[0] = enabled;    /* bit4 = ecoEnabled */
            eco[1] = value;
            push_frame(0x0F, (uint8_t)CAR_OP_STATUS, eco, 2);
        }
        {
            /* 音乐卡演示：每约 5s 在 FM / 歌词 / 来电三种模式间轮换 */
            switch((s_tick / 50U) % 3U) {
            case 0U:
                ui_app_demo_media(0U, "FM TP", "FM RADIO", "98.5");
                break;
            case 1U:
                ui_app_demo_media(4U, "MEDIA", "Demo Song - Demo Artist",
                                  "Now playing: demo lyric line one, line two and line three.");
                break;
            default:
                ui_app_demo_media(2U, "INCOMING CALL", "Demo Contact",
                                  "138 0000 0000");
                break;
            }
        }
                {
            /* 互联卡演示：每约 5s 轮换 关闭 / 待连接 / 已连接 / 连接中 */
            switch((s_tick / 50U) % 4U) {
            case 0U:
                ui_app_demo_connect(0U, "Android Auto", "DASH-BT");
                break;
            case 1U:
                ui_app_demo_connect(1U, "CarPlay", "DASH-BT");
                break;
            case 2U:
                ui_app_demo_connect(2U, "CarPlay", "DASH-BT");
                break;
            default:
                ui_app_demo_connect(3U, "CarPlay", "DASH-BT");
                break;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* PC 固定场景（仅 PC 演示用，板端不编译）                             */
/*                                                                    */
/* 目的：截图/页面验收时关闭自动轮播与真实时钟，用一组固定输入复现      */
/* 同一状态。场景 ID 稳定，重复运行必须得到相同数值与图层。             */
/* ------------------------------------------------------------------ */

/* 各组警示灯的故障码，全部按常亮下发；超过 15 个码时按帧上限分包 */
static const uint8_t s_codes_top[] = {16, 12, 15, 229, 9, 11, 10};
static const uint8_t s_codes_left[] = {
    5, 23, 46, 91, 24, 210, 216, 228, 107, 108, 207, 19, 20, 49, 52,
    50, 51, 8, 204, 232, 233, 234, 235, 220, 13, 14, 6, 4, 59, 129, 173,
};
static const uint8_t s_codes_right[] = {
    206, 25, 139, 140, 241, 203, 36, 33, 238, 236, 237, 222, 17, 18, 3,
};
static const uint8_t s_codes_bottom[] = {2, 27, 219, 28, 26, 30, 94};

#define SCENARIO_NO_INJECT 0xFFU

typedef struct {
    const char *id;          /* 稳定场景 ID，命令行 --scenario=<id> */
    uint16_t speed_kph;
    uint16_t rpm;
    uint8_t gear_pos;        /* 'P' / 'R' / 'N' / 'D' */
    bool mile;               /* true 走英里制 */
    uint8_t card;            /* 中心卡片序号，0 = 导航 */
    uint8_t door_bits;       /* 0 全关；位映射同 Qt 非右舵 */
    uint8_t tire_state;      /* 1 正常 / 2 偏低（触发胎压覆盖层） */
    bool radar_on;
    bool cruise_on;
    bool fuel_low;
    uint16_t tips_id;        /* 0 不弹通用提示 */
    uint8_t media_mode;      /* SCENARIO_NO_INJECT 不注入 */
    uint8_t connect_state;   /* SCENARIO_NO_INJECT 不注入 */
    const uint8_t *lamp_codes;
    uint8_t lamp_count;
    bool no_data;            /* true 时不发任何车况帧 */
} scenario_t;

static const scenario_t s_scenarios[] = {
    {"normal", 100, 2000, 'D', false, 0, 0, 1, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, false},
    {"unit_mile", 100, 2000, 'D', true, 0, 0, 1, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, false},
    {"no_data", 0, 0, 'P', false, 0, 0, 0, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, true},
    {"card_nav", 100, 2000, 'D', false, 0, 0, 1, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, false},
    {"card_4i", 100, 2000, 'D', false, 1, 0, 1, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, false},
    {"card_tire", 100, 2000, 'D', false, 2, 0, 1, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, false},
    {"card_itinerary", 100, 2000, 'D', false, 3, 0, 1, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, false},
    {"card_chassis", 100, 2000, 'D', false, 4, 0, 1, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, false},
    {"card_short_term", 100, 2000, 'D', false, 5, 0, 1, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, false},
    {"card_instant", 100, 2000, 'D', false, 6, 0, 1, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, false},
    {"card_eco", 100, 2000, 'D', false, 7, 0, 1, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, false},
    {"card_music", 100, 2000, 'D', false, 8, 0, 1, false, false, false, 0,
     4U, SCENARIO_NO_INJECT, NULL, 0, false},
    {"card_connect", 100, 2000, 'D', false, 9, 0, 1, false, false, false, 0,
     SCENARIO_NO_INJECT, 2U, NULL, 0, false},
    {"overlay_door", 40, 1400, 'P', false, 0, 0x1FU, 1, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, false},
    {"overlay_tire", 40, 1400, 'D', false, 0, 0, 2, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, false},
    {"overlay_radar", 40, 1400, 'D', false, 0, 0, 1, true, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, false},
    {"overlay_cruise", 80, 1600, 'D', false, 0, 0, 1, false, true, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, false},
    {"lamps_top", 60, 1500, 'D', false, 0, 0, 1, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, s_codes_top, 7, false},
    {"lamps_left", 60, 1500, 'D', false, 0, 0, 1, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, s_codes_left,
     (uint8_t)(sizeof(s_codes_left) / sizeof(s_codes_left[0])), false},
    {"lamps_right", 60, 1500, 'D', false, 0, 0, 1, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, s_codes_right,
     (uint8_t)(sizeof(s_codes_right) / sizeof(s_codes_right[0])), false},
    {"lamps_bottom", 60, 1500, 'D', false, 0, 0, 1, false, false, false, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, s_codes_bottom,
     (uint8_t)(sizeof(s_codes_bottom) / sizeof(s_codes_bottom[0])), false},
    {"fuel_low", 60, 1500, 'D', false, 0, 0, 1, false, false, true, 0,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, false},
    {"tips_warning", 60, 1500, 'D', false, 0, 0, 1, false, false, false, 23U,
     SCENARIO_NO_INJECT, SCENARIO_NO_INJECT, NULL, 0, false},
};

static const scenario_t *s_scenario;
static bool s_scenario_error;

/* 把场景固定输入拆成各业务帧，数值与会变化的演示状态无关 */
static void scenario_push_frames(const scenario_t *sc)
{
    uint8_t payload[16];

    /* 车速 / 转速 */
    payload[0] = (uint8_t)(sc->speed_kph >> 8);
    payload[1] = (uint8_t)(sc->speed_kph & 0xFFU);
    payload[2] = (uint8_t)(sc->rpm >> 8);
    payload[3] = (uint8_t)(sc->rpm & 0xFFU);
    push_frame(0x04, (uint8_t)CAR_OP_STATUS, payload, 4);

    /* 燃油：续航 520km、油量 78%、icon_state 2 为燃油低 */
    payload[0] = (uint8_t)(520U >> 8);
    payload[1] = (uint8_t)(520U & 0xFFU);
    payload[2] = 78U;
    payload[3] = sc->fuel_low ? 2U : 1U;
    payload[4] = 65U;
    payload[5] = 0U;
    push_frame(0x06, (uint8_t)CAR_OP_STATUS, payload, 6);

    /* 档位：字符串形式，与 Qt 固定状态一致只发单字符（不带 D1..D4 序号） */
    payload[0] = 0x03U;
    payload[1] = sc->gear_pos;
    payload[2] = 0U;
    payload[3] = 0U;
    payload[4] = 0U;
    push_frame(0x33, (uint8_t)CAR_OP_STATUS, payload, 5);

    /* 里程：trip_type 1 = TRIP A；ODO 1122232 km，Trip A 399.8 km，
     * Trip B 322.2 km，与 Qt GlobalData 默认值一致 */
    payload[0] = 1U;
    payload[1] = (uint8_t)((1122232UL >> 16) & 0xFFU);
    payload[2] = (uint8_t)((1122232UL >> 8) & 0xFFU);
    payload[3] = (uint8_t)(1122232UL & 0xFFU);
    payload[4] = (uint8_t)((3998UL >> 16) & 0xFFU);
    payload[5] = (uint8_t)((3998UL >> 8) & 0xFFU);
    payload[6] = (uint8_t)(3998UL & 0xFFU);
    payload[7] = (uint8_t)((3222UL >> 16) & 0xFFU);
    payload[8] = (uint8_t)((3222UL >> 8) & 0xFFU);
    payload[9] = (uint8_t)(3222UL & 0xFFU);
    push_frame(0x08, (uint8_t)CAR_OP_STATUS, payload, 10);

    /* 单位：bit0 英里制 */
    payload[0] = sc->mile ? 0x01U : 0x00U;
    payload[1] = 0x00U;
    payload[2] = 0x00U;
    push_frame(0x39, (uint8_t)CAR_OP_STATUS, payload, 3);

    /* 车门 / 水温 */
    payload[0] = sc->door_bits;
    push_frame(0x0B, (uint8_t)CAR_OP_STATUS, payload, 1);

    payload[0] = 60U;
    payload[1] = 88U;
    payload[2] = 1U;
    push_frame(0x15, (uint8_t)CAR_OP_STATUS, payload, 3);

    /* 时钟固定为 12:34:00（24 小时制），不受本机时间影响 */
    payload[0] = 26U;
    payload[1] = 11U;
    payload[2] = 4U;
    payload[3] = 12U | 0x80U;
    payload[4] = 34U;
    payload[5] = 0U;
    push_frame(0x32, (uint8_t)CAR_OP_STATUS, payload, 6);

    /* 车外温度 22.0℃（raw 0xA8）；转向灯全灭 */
    payload[0] = 60U;
    payload[1] = 124U;
    payload[2] = 40U;
    payload[3] = 0xA8U;
    payload[4] = 70U;
    payload[5] = (uint8_t)(0x30U + 90U);
    push_frame(0x38, (uint8_t)CAR_OP_STATUS, payload, 6);

    payload[0] = 0U;
    push_frame(0x35, (uint8_t)CAR_OP_STATUS, payload, 1);

    /* 复位后统计：3h12m / 1560km / 62km/h */
    payload[0] = 0U;
    payload[1] = 85U;
    payload[2] = 0U;
    payload[3] = 62U;
    payload[4] = 0U;
    payload[5] = 0U;
    payload[6] = 192U;
    payload[7] = 0U;
    payload[8] = (uint8_t)((1560U >> 8) & 0xFFU);
    payload[9] = (uint8_t)(1560U & 0xFFU);
    push_frame(0x0C, (uint8_t)CAR_OP_STATUS, payload, 10);

    /* 短期记忆：平均油耗 8.5 / 平均车速 62 / 192min / 12km */
    payload[0] = 0x00U;
    payload[1] = 85U;
    payload[2] = 0x00U;
    payload[3] = 62U;
    payload[4] = 0x00U;
    payload[5] = 192U;
    payload[6] = 0x00U;
    payload[7] = 12U;
    push_frame(0x0E, (uint8_t)CAR_CH_TRIP, payload, 8);

    /* 瞬时油耗 8.5 L/100km；ECO 开启、评分 128 */
    payload[0] = 0x00U;
    payload[1] = 85U;
    push_frame(0x11, (uint8_t)CAR_OP_STATUS, payload, 2);

    payload[0] = 0x10U;
    payload[1] = 128U;
    push_frame(0x0F, (uint8_t)CAR_OP_STATUS, payload, 2);

    /* 底盘：转向角 64° 向右，升降 + 四驱指示全亮 */
    payload[0] = (uint8_t)(0x80U | 64U);
    payload[1] = 0x1FU;
    push_frame(0x26, (uint8_t)CAR_CH_TRAJECTORY, payload, 2);

    /* 定速巡航：设定 80km/h；车道辅助按是否演示巡航覆盖层切换 */
    payload[0] = 0x00U;
    payload[1] = sc->cruise_on ? 0x01U : 0x00U;
    payload[2] = 80U;
    payload[3] = 0x48U;
    payload[4] = 0x00U;
    push_frame(0x13, (uint8_t)CAR_OP_STATUS, payload, 5);

    payload[0] = 0x0EU;
    payload[1] = 0x00U;
    payload[2] = sc->cruise_on ? 1U : 0U;   /* laneCenter */
    payload[3] = 0x00U;
    payload[4] = sc->cruise_on ? 1U : 0U;   /* car.position */
    payload[5] = 0x00U;
    payload[6] = 0x00U;
    push_frame(0x16, (uint8_t)CAR_OP_STATUS, payload, 7);

    /* 胎压：四轮 240/245/238/243，左前按 tire_state 取 2 触发覆盖层 */
    {
        static const uint16_t kpa[4] = {240U, 245U, 238U, 243U};
        uint8_t i;

        for(i = 0U; i < 4U; ++i) {
            uint8_t state = (i == 0U) ? sc->tire_state : 1U;

            payload[i * 2U] = (uint8_t)(((kpa[i] >> 8) & 0x0FU) |
                                        (uint8_t)(state << 4));
            payload[i * 2U + 1U] = (uint8_t)(kpa[i] & 0xFFU);
        }
        payload[8] = 0x00U;
        payload[9] = 0x00U;
        push_frame(0x0A, (uint8_t)CAR_OP_STATUS, payload, 10);
    }

    /* 泊车雷达：前 4 路有目标（颜色按路号 0/1/2 轮换），display_on 跟随场景 */
    {
        uint8_t i;

        for(i = 0U; i < 5U; ++i) {
            payload[i] = 0x3FU;
            payload[i + 5U] = 0x3FU;
        }

        if(sc->radar_on) {
            for(i = 0U; i < 4U; ++i) {
                payload[i] = (uint8_t)(((i % 3U) << 6) | (uint8_t)(20U + i * 3U));
            }
        }

        payload[10] = sc->radar_on ? 0x01U : 0x00U;
        push_frame(0x22, (uint8_t)CAR_OP_STATUS, payload, 11);
    }

    /* 警示灯：整组常亮，按 15 码/帧分包 */
    if(sc->lamp_codes != NULL) {
        uint8_t start = 0U;

        while(start < sc->lamp_count) {
            uint8_t count = (uint8_t)(sc->lamp_count - start);
            uint8_t i;

            if(count > 15U) {
                count = 15U;
            }

            payload[0] = 1U;   /* 常亮 */
            for(i = 0U; i < count; ++i) {
                payload[i + 1U] = sc->lamp_codes[start + i];
            }
            push_frame(0x02, (uint8_t)CAR_OP_STATUS, payload,
                       (uint16_t)(count + 1U));
            start = (uint8_t)(start + count);
        }
    }

    /* 通用提示：id / icon / 3 秒 */
    if(sc->tips_id != 0U) {
        payload[0] = (uint8_t)((sc->tips_id >> 8) & 0xFFU);
        payload[1] = (uint8_t)(sc->tips_id & 0xFFU);
        payload[2] = (uint8_t)sc->tips_id;
        payload[3] = 3U;
        push_frame(0x03, (uint8_t)CAR_OP_STATUS, payload, 4);
    }
}

/* 固定场景的 33ms 回调：不轮播、不读真实时钟、不注入演示媒体轮换 */
static void scenario_tick(void)
{
    const scenario_t *sc = s_scenario;

    fake_mcu_service();

    if(sc == NULL || sc->no_data) {
        return;
    }

    scenario_push_frames(sc);

    if(sc->media_mode != SCENARIO_NO_INJECT) {
        if(sc->media_mode == 0U) {
            ui_app_demo_media(0U, "FM TP", "FM RADIO", "98.5");
        }
        else {
            ui_app_demo_media(sc->media_mode, "MEDIA", "Demo Song - Demo Artist",
                              "Now playing: demo lyric line one, line two and line three.");
        }
    }

    if(sc->connect_state != SCENARIO_NO_INJECT) {
        ui_app_demo_connect(sc->connect_state, "CarPlay", "DASH-BT");
    }
}

/* 解析 --scenario=<id>；没有该参数返回 NULL（走普通演示） */
static const scenario_t *scenario_parse(int argc, char **argv)
{
    size_t i;

    for(i = 1; i < (size_t)argc; ++i) {
        const char *arg = argv[i];
        const char *id;
        size_t s;

        if(strncmp(arg, "--scenario=", 11) != 0) {
            continue;
        }

        id = arg + 11;

        for(s = 0; s < sizeof(s_scenarios) / sizeof(s_scenarios[0]); ++s) {
            if(strcmp(id, s_scenarios[s].id) == 0) {
                return &s_scenarios[s];
            }
        }

        fprintf(stderr, "unknown scenario: %s\n", id);
        fprintf(stderr, "available:");
        for(s = 0; s < sizeof(s_scenarios) / sizeof(s_scenarios[0]); ++s) {
            fprintf(stderr, " %s", s_scenarios[s].id);
        }
        fprintf(stderr, "\n");

        s_scenario_error = true;
        return NULL;
    }

    return NULL;
}

/* --snapshot=<path>：配合 SIGUSR1 在当前界面状态下截一张 PNG（PC 专用，用于 D1 视觉对比） */
static const char *s_snapshot_path;
static volatile sig_atomic_t g_snapshot_requested;

static void on_snapshot_signal(int signo)
{
    (void)signo;
    g_snapshot_requested = 1;
}

static const char *snapshot_parse(int argc, char **argv)
{
    size_t i;

    for(i = 1; i < (size_t)argc; ++i) {
        if(strncmp(argv[i], "--snapshot=", 11) == 0) {
            return argv[i] + 11;
        }
    }

    return NULL;
}

/*
 * --car-color=#RRGGBB：覆盖车身颜色（对应 Qt 设置项 SettingsMgr.carColor）。
 * 颜色属本地设置而非串口数据，因此在取到快照后覆盖视图字段。
 */
static bool s_car_color_set;
static car_color_t s_car_color;

static void car_color_parse_args(int argc, char **argv)
{
    size_t i;

    for(i = 1; i < (size_t)argc; ++i) {
        if(strncmp(argv[i], "--car-color=", 12) != 0) {
            continue;
        }
        if(car_color_parse(argv[i] + 12, &s_car_color)) {
            s_car_color_set = true;
        }
        else {
            fprintf(stderr, "invalid --car-color (want #RRGGBB): %s\n", argv[i] + 12);
        }
    }
}

/*
 * --out-temp：显示车外温度（对应 Qt 设置项 SettingsMgr.outTemperature）。
 * 该设置项缺省关闭，PC 上由启动参数打开，板端等待阶段6设置体系接入。
 */
static bool s_out_temp_on;

static void out_temp_parse_args(int argc, char **argv)
{
    size_t i;

    for(i = 1; i < (size_t)argc; ++i) {
        if(strcmp(argv[i], "--out-temp") == 0) {
            s_out_temp_on = true;
        }
    }
}

#endif /* !CARMW_PLATFORM_BOARD */

/* ------------------------------------------------------------------ */
/* 入口                                                               */
/* ------------------------------------------------------------------ */

#if !defined(CARMW_PLATFORM_BOARD)
/*
 * PC 演示：用键盘模拟方向盘按键（板端由真实 0x20 帧驱动同一接口）。
 * ← / ↑ = 上一张卡片，→ / ↓ = 下一张卡片
 * Enter = 23 确定，Backspace/Esc = 13 返回
 */
static void poll_keys(void)
{
    static uint8_t s_prev[6];
    static const SDL_Scancode s_sc[6] = {
        SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_UP,   SDL_SCANCODE_DOWN,
        SDL_SCANCODE_RETURN, SDL_SCANCODE_BACKSPACE,
    };
    static const uint8_t s_code[6] = {
        UI_KEY_PREV, UI_KEY_NEXT, UI_KEY_PREV, UI_KEY_NEXT,
        UI_KEY_OK,   UI_KEY_OK, /* 返回键单独处理，见下 */
    };
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    uint8_t i;

    for(i = 0U; i < 6U; ++i) {
        uint8_t now = (keys[s_sc[i]] != 0) ? 1U : 0U;

        /* 只在按下沿触发一次，避免按住时连续翻页 */
        if(now != 0U && s_prev[i] == 0U) {
            ui_key_event_t evt;

            evt.code = s_code[i];
            evt.pressed = 1U;

            if(i == 4U) {
                /* 23：车速 < 5 时打开设置（未收到速度帧时也允许打开） */
                if(!ui_app_settings_is_open() &&
                   (ui_app_settings_speed_ok() || !ui_app_has_data())) {
                    ui_app_settings_open();
                }
            }
            else if(i == 5U) {
                /* 13：设置页可见时返回上级，否则回首页 */
                if(!ui_app_settings_back()) {
                    ui_app_handle_key(&evt);
                }
            }
            else {
                ui_app_handle_key(&evt);
            }
        }

        s_prev[i] = now;
    }
}
#endif

/* 33ms 周期刷新：PC 先推进假 MCU 并 pump，再取一次快照交给 UI */
static void refresh_timer(lv_timer_t *timer)
{
    car_state_view_t view;

    (void)timer;

#if defined(CARMW_PLATFORM_BOARD)
    /* 真实方向盘按键（0x20）：取出队列并映射到界面，与 Qt 行为一致 */
    (void)car_ui_key_pump(&s_link);
#else
    if(s_scenario != NULL) {
        /* 固定场景：只回固定输入，不轮播、不读真实时钟 */
        scenario_tick();
        car_link_pump(&s_link, 0);
    }
    else {
        fake_mcu_tick();
        car_link_pump(&s_link, 0);
        poll_keys();
    }
#endif

    car_link_view(&s_link, &view);
#if !defined(CARMW_PLATFORM_BOARD)
    /* 车身颜色是本地设置项，链接层不产生该字段，PC 上由启动参数覆盖 */
    if(s_car_color_set) {
        view.state.car_color = s_car_color;
    }
    if(s_out_temp_on) {
        view.state.outside_temp_on = true;
    }
#endif

#if defined(CARMW_PLATFORM_BOARD)
    /* 采样 ACC（0x34）：仅在真实帧到达后参与判定，下降沿触发统一落盘 */
    if(view.state.acc.valid) {
        car_settings_saver_acc(&s_settings_saver, view.state.acc.on);
    }
#endif

    ui_app_apply(&view);

#if !defined(CARMW_PLATFORM_BOARD)
    car_port_memory_advance(&s_mem, REFRESH_MS);
#endif
}

#if !defined(CARMW_PLATFORM_BOARD)
static lv_obj_t *s_style_label;

/* 按钮回调：循环切到下一个样式，并同步按钮上的样式名 */
static void style_button_cb(lv_event_t *e)
{
    const ui_style_desc_t *desc;

    (void)e;

    if(!ui_app_next_style()) {
        return;
    }

    desc = ui_app_current_style();
    if(desc != NULL && s_style_label != NULL) {
        lv_label_set_text(s_style_label, desc->name);
    }
}

/* PC 演示用：右上角临时按钮，循环切换样式 */
static void style_button_create(void)
{
    const ui_style_desc_t *desc = ui_app_current_style();
    lv_obj_t *btn = lv_button_create(lv_screen_active());

    lv_obj_set_size(btn, 190, 36);
    lv_obj_set_pos(btn, 1710, 10);
    lv_obj_add_event_cb(btn, style_button_cb, LV_EVENT_CLICKED, NULL);

    s_style_label = lv_label_create(btn);
    lv_label_set_text(s_style_label, desc != NULL ? desc->name : "STYLE");
    lv_obj_center(s_style_label);
}
#endif

static void ui_loop(void)
{
    while(!ui_should_quit()) {
        uint32_t idle = lv_timer_handler();
        struct timespec delay;

#if !defined(CARMW_PLATFORM_BOARD)
        if(g_snapshot_requested != 0) {
            g_snapshot_requested = 0;
            ui_snapshot_save(lv_screen_active(), s_snapshot_path);
        }
#endif

        if(idle < 1U) {
            idle = 1U;
        }
        if(idle > 20U) {
            idle = 20U;
        }

        delay.tv_sec = 0;
        delay.tv_nsec = (long)idle * 1000000L;
        (void)nanosleep(&delay, NULL);
    }
}

int main(int argc, char **argv)
{
    lv_display_t *display;

#if defined(CARMW_PLATFORM_BOARD)
    (void)argc;
    (void)argv;

    signal(SIGINT, on_quit_signal);
    signal(SIGTERM, on_quit_signal);
    car_port_posix_attach(&s_port, &s_posix, SERIAL_DEVICE, SERIAL_BAUD);
#else
    car_port_memory_attach(&s_port, &s_mem);

    /* --scenario=<id> 进入固定场景；其余参数保持原演示行为 */
    s_scenario = scenario_parse(argc, argv);
    if(s_scenario_error) {
        return 1;
    }

    s_snapshot_path = snapshot_parse(argc, argv);
    if(s_snapshot_path != NULL) {
        signal(SIGUSR1, on_snapshot_signal);
    }

    car_color_parse_args(argc, argv);
    out_temp_parse_args(argc, argv);
#endif

    if(!car_link_init(&s_link, &s_port)) {
        fprintf(stderr, "car_link_init failed\n");
        return 1;
    }

#if defined(CARMW_PLATFORM_BOARD)
    /* 后台收帧线程：所有串口读写都在该线程，UI 线程只取快照 */
    if(!car_link_start(&s_link)) {
        fprintf(stderr, "car_link_start failed\n");
        car_link_deinit(&s_link);
        return 1;
    }
#endif

    lv_init();

#if defined(CARMW_PLATFORM_BOARD)
    display = lv_linux_fbdev_create();

    if(display == NULL || lv_linux_fbdev_set_file(display, "/dev/fb0") != LV_RESULT_OK) {
        fprintf(stderr, "fbdev init failed: /dev/fb0\n");
        car_link_stop(&s_link);
        car_link_deinit(&s_link);
        return 1;
    }
#else
    display = lv_sdl_window_create(WINDOW_WIDTH, WINDOW_HEIGHT);

    if(display == NULL) {
        fprintf(stderr, "SDL window create failed\n");
        car_link_deinit(&s_link);
        return 1;
    }

    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();
#endif

#if defined(CARMW_PLATFORM_BOARD)
    /* 本地设置：启动时读入内存，之后运行期只改内存，ACC 关闭时统一落盘 */
    if(!car_settings_load(&s_settings, CAR_SETTINGS_DEFAULT_PATH)) {
        fprintf(stderr, "car_settings_load failed: %s, 使用默认设置\n",
                CAR_SETTINGS_DEFAULT_PATH);
    }
    else {
        fprintf(stderr, "settings: load %s (unit=%u style=%s cards=0x%lx)\n",
                CAR_SETTINGS_DEFAULT_PATH,
                (unsigned)s_settings.values.unit_mileage,
                s_settings.values.dial_style,
                (unsigned long)s_settings.values.center_card_mask);
    }
    if(!car_settings_saver_start(&s_settings_saver, &s_settings)) {
        fprintf(stderr, "car_settings_saver_start failed\n");
        car_link_stop(&s_link);
        car_link_deinit(&s_link);
        return 1;
    }
#else
    car_settings_reset(&s_settings);
#endif
    car_settings_set_notify(&s_settings, settings_changed, NULL);

    if(!ui_app_init()) {
        fprintf(stderr, "ui_app_init failed\n");
        return 1;
    }
    ui_app_settings_set_settings(&s_settings);
    ui_app_settings_set_cmd_cb(settings_send_cmd);
#if defined(CARMW_PLATFORM_BOARD)
    ui_app_settings_set_pending_cb(settings_pending);
#endif

    if(!ui_app_set_style(settings_start_style())) {
        fprintf(stderr, "ui_app_set_style failed\n");
        return 1;
    }

#if !defined(CARMW_PLATFORM_BOARD)
    if(s_scenario != NULL) {
        /* 场景启动后直接切到目标卡片；场景演示不需要样式切换按钮 */
        ui_key_event_t evt;
        uint8_t i;

        evt.code = UI_KEY_NEXT;
        evt.pressed = 1U;
        for(i = 0U; i < s_scenario->card; ++i) {
            ui_app_handle_key(&evt);
        }
    }
    else {
        style_button_create();
    }
#endif

    s_refresh_timer = lv_timer_create(refresh_timer, REFRESH_MS, NULL);

#if defined(CARMW_PLATFORM_BOARD)
    printf("carmw UI demo: fbdev /dev/fb0 + %s@%d, Ctrl+C 退出\n",
           SERIAL_DEVICE, SERIAL_BAUD);
#else
    printf("carmw UI demo: %dx%d, 关闭窗口退出\n", WINDOW_WIDTH, WINDOW_HEIGHT);
#endif
    ui_loop();

#if defined(CARMW_PLATFORM_BOARD)
    /* 关闭顺序：停 UI 定时器 -> 补存设置 -> join 收帧线程 -> 关串口 -> 销毁 UI */
    lv_timer_delete(s_refresh_timer);
    if(!car_settings_saver_flush(&s_settings_saver, CAR_SETTINGS_SAVE_BUDGET_MS)) {
        fprintf(stderr, "car_settings flush failed, 仍有未落盘修改\n");
    }
    car_settings_saver_stop(&s_settings_saver);
    car_link_stop(&s_link);
    car_link_deinit(&s_link);
    ui_app_deinit();
    lv_deinit();
    printf("carmw UI demo: exit\n");
#endif

    return 0;
}
