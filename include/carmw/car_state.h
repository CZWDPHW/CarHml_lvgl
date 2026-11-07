/*
 * car_state.h - 车辆状态模型 + 快照存储
 *
 * 设计：
 *   - car_state_t        纯数据，不含锁、不含指针，可按值拷贝
 *   - car_state_store_t  单写者（UART 线程/pump）写入、多读者快照，
 *                        带 version 与"自上次快照以来的变更掩码"
 *
 * UI 侧只做一件事：car_state_store_view() 拿一份快照，
 * 用 view.changed 判断哪些组需要刷新，避免每帧无脑全量重绘。
 */
#ifndef CARMW_CAR_STATE_H
#define CARMW_CAR_STATE_H

#include "carmw/car_lock.h"
#include "carmw/car_color.h"
#include "carmw/car_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAR_WARNING_SLOTS 256U /* 故障码 0..255 */
#define CAR_TEXT_MAX 48U
#define CAR_MODEL_MAX 10U
#define CAR_MODEL_NAME_MAX 24U
#define CAR_HIGH_FUNC_RAW_MAX 16U

/* ---------------- 变更掩码（每位一组，供 UI 增量刷新） ---------------- */
#define CAR_CH_INIT (UINT64_C(1) << 0)// 初始化状态
#define CAR_CH_WARNINGS (UINT64_C(1) << 1)// 警告灯
#define CAR_CH_POPUP (UINT64_C(1) << 2)// 弹窗
#define CAR_CH_SPEED_RPM (UINT64_C(1) << 3)// 速度RPM
#define CAR_CH_SYS_SETTING (UINT64_C(1) << 4)// 系统设置
#define CAR_CH_FUEL (UINT64_C(1) << 5)// 燃油量
#define CAR_CH_MILEAGE (UINT64_C(1) << 6)// 里程
#define CAR_CH_TIRE (UINT64_C(1) << 7)
#define CAR_CH_DOORS (UINT64_C(1) << 8)
#define CAR_CH_TRIP_STATS (UINT64_C(1) << 9)
#define CAR_CH_REFUEL (UINT64_C(1) << 10)
#define CAR_CH_TRIP (UINT64_C(1) << 11)
#define CAR_CH_ECO (UINT64_C(1) << 12)
#define CAR_CH_INSTANT_FUEL (UINT64_C(1) << 13)
#define CAR_CH_IDLE_STOP (UINT64_C(1) << 14)
#define CAR_CH_CRUISE (UINT64_C(1) << 15)
#define CAR_CH_CAR_MODEL (UINT64_C(1) << 16)
#define CAR_CH_COOLANT (UINT64_C(1) << 17)
#define CAR_CH_HIGH_FUNC (UINT64_C(1) << 18)
#define CAR_CH_WARN_TONE (UINT64_C(1) << 19)
#define CAR_CH_BACKLIGHT (UINT64_C(1) << 20)
#define CAR_CH_DRIVE_MODE (UINT64_C(1) << 21)
#define CAR_CH_KEY (UINT64_C(1) << 22)
#define CAR_CH_RADAR (UINT64_C(1) << 23)
#define CAR_CH_TRAJECTORY (UINT64_C(1) << 24)
#define CAR_CH_VERSION (UINT64_C(1) << 25)
#define CAR_CH_CLOCK (UINT64_C(1) << 26)
#define CAR_CH_GEAR (UINT64_C(1) << 27)
#define CAR_CH_ACC (UINT64_C(1) << 28)
#define CAR_CH_TURN (UINT64_C(1) << 29)
#define CAR_CH_MAINTENANCE (UINT64_C(1) << 30)
#define CAR_CH_SPEED_WARN (UINT64_C(1) << 31)
#define CAR_CH_OTHER_INFO (UINT64_C(1) << 32)
#define CAR_CH_UNITS (UINT64_C(1) << 33)
#define CAR_CH_HUD (UINT64_C(1) << 34)
#define CAR_CH_FUNC_CONFIG (UINT64_C(1) << 35)

/* ---------------- 基础枚举 ---------------- */

typedef enum {
    CAR_GEAR_P = 0,
    CAR_GEAR_R,
    CAR_GEAR_N,
    CAR_GEAR_D,
    CAR_GEAR_S
} car_gear_pos_t;

typedef enum {
    CAR_COOLANT_NORMAL = 0,
    CAR_COOLANT_HIGH = 1,
    CAR_COOLANT_OVERHEAT = 2
} car_coolant_state_t;

typedef enum {
    CAR_COOLANT_SHOW_VALUE = 0,
    CAR_COOLANT_SHOW_UNKNOWN,
    CAR_COOLANT_SHOW_HIDDEN,
    CAR_COOLANT_SHOW_ERROR
} car_coolant_display_t;

typedef enum {
    CAR_WARN_OFF = 0,
    CAR_WARN_ON = 1,
    CAR_WARN_BLINK_FAST = 2,
    CAR_WARN_BLINK_SLOW = 3
} car_warning_level_t;

/* ---------------- 各组状态 ---------------- */

typedef struct {
    uint16_t speed_kph;
    uint16_t engine_rpm;
    bool valid;
} car_speed_rpm_t;

typedef struct {
    uint8_t state; /* 0 未初始化 / 1 已初始化 / 2 初始化中 */
    bool valid;
} car_init_t;

typedef struct {
    /*
     * levels 持久保存每个故障码的灯态：
     *   len == 1 -> 覆盖全部 levels
     *   len >= 2 -> 仅更新报文列出的故障码
     * 按接收顺序后发覆盖先发；global/list 仅记录各形态最近帧诊断。
     */
    uint8_t levels[CAR_WARNING_SLOTS];
    bool global_valid;
    uint8_t global_state;
    bool list_valid;
    uint8_t list_state;
    uint16_t list_count;
    uint8_t list[CAR_WARNING_SLOTS];
    bool valid;
} car_warnings_t;

typedef struct {
    uint32_t text_id;
    uint8_t icon;
    uint8_t duration_s;
    bool valid;
} car_popup_t;

typedef struct {
    bool reverse_sound;
    bool reverse_trackline;
    uint8_t current_car;
    uint8_t key;
    uint8_t reserved;
    uint8_t time_source;
    bool valid;
} car_sys_setting_t;

// 燃油量
typedef struct {
    uint16_t remaining_range_km;
    uint8_t percent;
    uint8_t icon_state; /* 0 无数据 1 正常 2 低油量 3 报警 */
    bool port_left;
    uint8_t main_tank_l;
    uint8_t sub_tank_l;
    bool valid;
} car_fuel_t;

// 里程
typedef struct {
    uint8_t trip_type; /* 0 ODO / 1 TRIP A / 2 TRIP B */
    uint32_t odometer_raw;
    uint32_t trip_a_raw;
    uint32_t trip_b_raw;
    bool valid;
} car_mileage_t;

typedef struct {
    uint16_t kpa;
    uint8_t state; /* 0 无数据 1 正常 2 偏低 3 偏高 */
    bool valid;
} car_tire_t;

typedef struct {
    car_tire_t wheels[5]; /* FL FR RL RR 备胎 */
    uint32_t sequence; /* 每成功解码一帧递增，重复内容也视为新事件 */
    bool valid;
} car_tires_t;

typedef struct {
    bool left_front;
    bool right_front;
    bool left_rear;
    bool right_rear;
    bool trunk;
    bool hood;
    bool valid;
} car_doors_t;

typedef struct {
    uint16_t avg_fuel_x10;
    uint16_t avg_speed_kph;
    uint32_t drive_minutes;
    uint32_t distance_km;
    bool valid;
} car_trip_stats_t;

typedef struct {
    uint16_t remaining_range_km;
    uint16_t avg_fuel_x10;
    uint16_t avg_speed_kph;
    bool valid;
} car_refuel_t;

typedef struct {
    uint16_t avg_fuel_x10;
    uint16_t avg_speed_kph;
    uint16_t drive_minutes;
    uint16_t distance_km;
    bool valid;
} car_trip_t;

typedef struct {
    bool enabled;
    bool energy_save;
    uint8_t flags;
    uint8_t value;
    bool valid;
} car_eco_t;

typedef struct {
    uint16_t raw;
    uint16_t value_x10;
    bool visible;
    bool per_hour;
    bool valid;
} car_instant_fuel_t;

typedef struct {
    uint8_t time_mode;
    uint16_t used_s;
    uint16_t start_saved_fuel;
    uint16_t reset_saved_fuel;
    uint32_t start_total_s;
    uint32_t reset_total_s;
    bool valid;
} car_idle_stop_t;

typedef struct {
    uint8_t text_id;
    uint8_t cruise_state;
    uint8_t set_speed_kph;
    uint8_t acc_state;
    uint8_t limit_speed_kph;
    bool valid;
} car_cruise_t;

typedef struct {
    uint8_t id;
    char name[CAR_MODEL_NAME_MAX];
} car_model_entry_t;

// 车辆型号
typedef struct {
    uint8_t selected;
    uint8_t count;
    uint8_t parsed;
    car_model_entry_t entries[CAR_MODEL_MAX];
    bool valid;
} car_car_model_t;

// 水温
typedef struct {
    uint8_t percent;
    int16_t celsius;
    uint8_t state;
    car_coolant_display_t display;
    bool valid;
} car_coolant_t;

typedef struct {
    uint8_t lane_center;
    uint8_t car_state;
    uint8_t show_area;
    uint8_t show_car;
    bool valid;
} car_lane_assist_t;

/*
 * 驾驶辅助三项的 MCU 真实状态（Qt SettingsMgr.emergencyBraking /
 * laneDepartureWarning / blindSpotWarning，由 0x16 SetGet 回读）。
 * 缺省 true 与 Qt 一致；valid 表示是否已收到过 MCU 回读。
 */
typedef struct {
    bool emergency_braking;
    bool lane_departure_warning;
    bool blind_spot_warning;
    bool valid;
} car_driver_assist_t;

/*
 * 车辆设置项（Qt SettingsMgr，由 0x16 SetGet 的 0x86 ack 回读）。
 * 与驾驶辅助三项一样只保存 MCU 真实值，UI 不本地翻转。
 * 下标与 Qt SettingsManager::updateQashqaiSetting 的 index 一致。
 */
typedef struct {
    bool welcome_light;           /* index 10 迎宾灯 */
    bool auto_interior_light;     /* index 11 室内自动点灯 */
    bool external_doors;          /* index 13 外部车门开关 */
    bool single_door_unlock;      /* index 14 单车门解锁 */
    bool horn_feedback;           /* index 15 喇叭响应 */
    uint8_t auto_door_unlock;     /* index 16 车门自动解锁：0 点火关闭 / 1 P 挡 / 2 OFF */
    bool smart_door_unlock;       /* index 24 智能钥匙车门上锁 */
    bool select_unlock;           /* index 25 选择解锁 */
    uint8_t auto_door_lock;       /* index 28 车门自动上锁：0 车速 / 1 P 挡 / 2 OFF */
    uint8_t headlight_sensitivity;/* index 33 点灯灵敏度：0~3 */
    uint8_t headlight_off_delay;  /* index 32 灯光关闭延时：0~7 */
    bool valid;                   /* 是否已收到过 MCU 回读 */
} car_vehicle_setting_t;

typedef struct {
    uint8_t sub_id;
    uint8_t drive_mode;
    int16_t pitch;
    int16_t roll;
    car_lane_assist_t lane; /* 仅由完整的 0x0E 子帧更新 */
    uint8_t raw[CAR_HIGH_FUNC_RAW_MAX]; /* 最后接收子帧，仅用于诊断 */
    uint8_t raw_len;
    bool valid;
} car_high_func_t;

/* ---------------- 0x16/0x1C 警告文本事件 ----------------
 *
 * Qt 的 0x1C 链：advance_parse -> Handler.js 0x1C -> GlobalData.messageNotify
 *               -> main.qml onMessageNotify -> GeneralTipsWindow.showBriefly
 * showBriefly 使用默认 2s，不使用协议里的 showTime_s。
 *
 * 这里只保存"待 UI 消费"的有界事件队列：解码线程写，UI 线程通过
 * car_state_view_t 的整份拷贝读取。seq 单调递增，UI 用 seq > seen_seq
 * 判断哪些事件还没显示过，避免重复 apply 时重播。
 */
#define CAR_WARN_EVENT_QUEUE_MAX 8U

typedef struct {
    uint16_t text_id;
    uint32_t seq;
} car_warn_event_t;

typedef struct {
    car_warn_event_t items[CAR_WARN_EVENT_QUEUE_MAX];
    uint8_t head;      /* 最旧一条的位置 */
    uint8_t count;     /* 当前条数 */
    uint32_t seq;      /* 已入队的事件总数（同时作为最新事件号） */
    uint32_t dropped;  /* 队列满时挤掉的最旧事件数，仅诊断 */
} car_warn_events_t;

typedef struct {
    bool door_unlocked;
    bool horn;
    bool handbrake;
    bool seatbelt_fast;
    bool seatbelt_slow;
    bool valid;
} car_warn_tone_t;

typedef struct {
    uint8_t brightness;
    uint8_t current_mode;
    uint8_t set_mode;
    uint8_t day_value;
    uint8_t night_value;
    bool tft_led;
    bool valid;
} car_backlight_t;

typedef struct {
    uint8_t code; /* 原始键值，与 Qt 端一致不做掩码 */
    uint8_t pressed;
    bool valid;
} car_key_t;

typedef struct {
    uint8_t color;
    uint8_t value;
} car_radar_point_t;

typedef struct {
    car_radar_point_t front[5];
    car_radar_point_t rear[5];
    bool display_on;
    bool valid;
} car_radar_t;

typedef struct {
    bool right;
    uint8_t angle;
    bool chassis_lift;
    bool wheel_lf;
    bool wheel_rf;
    bool wheel_lr;
    bool wheel_rr;
    bool valid;
} car_trajectory_t;

typedef struct {
    char wired[CAR_TEXT_MAX];
    char main[CAR_TEXT_MAX];
    char chip[CAR_TEXT_MAX];
    char debug[CAR_TEXT_MAX];
    bool wired_valid;
    bool main_valid;
    bool chip_valid;
    bool debug_valid;
} car_version_t;

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    bool hour24;
    bool valid;
} car_clock_t;

typedef struct {
    char text[4];
    bool visible;
    bool blink;
    bool reverse_gear;
    bool reverse_beep;
    bool four_present;
    bool four_blink;
    uint8_t four_state;
    bool valid;
} car_gear_t;

typedef struct {
    bool on;
    bool valid;
} car_acc_t;

typedef struct {
    bool left;
    bool right;
    bool high_beam_key;
    bool valid;
} car_turn_t;

typedef struct {
    uint8_t state;
    uint8_t item;
    uint32_t set_km;
    int32_t now_km;
    uint8_t show_time;
    bool valid;
} car_maintenance_t;

typedef struct {
    bool remind_on;
    bool blink;
    uint8_t speed_kph;
    bool valid;
} car_speed_warning_t;

typedef struct {
    uint8_t oil_pressure_pct;
    uint16_t voltage_x10;
    uint8_t gearbox_temp_c;
    uint8_t outside_temp_raw;
    uint8_t oil_temp_pct;
    int16_t oil_temp_c;
    bool valid;
} car_other_info_t;

typedef struct {
    bool mile;
    bool hour24;
    bool fahrenheit;
    uint8_t fuel_unit;
    uint8_t pressure_unit;
    bool valid;
} car_units_t;

typedef struct {
    bool show_turn;
    bool show_check_control;
    bool show_nav;
    bool show_driver_assist;
    bool show_ma_view;
    bool show_left_turn;
    bool show_right_turn;
    bool show_speed_unit;
    bool speed_unit_mi;
    uint8_t language;
    uint8_t color;
    bool valid;
} car_hud_t;

typedef struct {
    uint8_t show_time;
    char text[CAR_TEXT_MAX];
    bool valid;
} car_func_config_t;

/* ---------------- 聚合状态 ---------------- */

typedef struct {
    car_init_t init;
    car_warnings_t warnings;
    car_popup_t popup;
    car_speed_rpm_t speed_rpm;
    car_sys_setting_t sys_setting;
    car_fuel_t fuel;
    car_mileage_t mileage;
    car_tires_t tires;
    car_doors_t doors;
    car_trip_stats_t trip_stats;
    car_refuel_t refuel;
    car_trip_t trip;
    car_eco_t eco;
    car_instant_fuel_t instant_fuel;
    car_idle_stop_t idle_stop;
    car_cruise_t cruise;
    car_car_model_t car_model;
    car_coolant_t coolant;
    car_high_func_t high_func;
    car_warn_events_t warn_events; /* 0x1C 警告事件队列，UI 按 seq 消费 */
    car_warn_tone_t warn_tone;
    car_backlight_t backlight;
    uint8_t drive_mode;
    bool drive_mode_valid;
    car_key_t key;
    car_radar_t radar;
    car_trajectory_t trajectory;
    car_version_t version;
    car_clock_t clock;
    car_gear_t gear;
    car_acc_t acc;
    car_turn_t turn;
    car_maintenance_t maintenance;
    car_speed_warning_t speed_warning;
    car_other_info_t other_info;
    car_units_t units;
    car_hud_t hud;
    car_func_config_t func_config;
    /* 车身颜色：Qt SettingsMgr.carColor 的本地映射，不由串口帧产生 */
    car_color_t car_color;
    /* 车外温度显隐：Qt SettingsMgr.outTemperature 的本地映射，缺省关闭 */
    bool outside_temp_on;
    /* 档位显隐：Qt SettingsMgr.gearOnOff 的本地映射，缺省开启 */
    bool gear_on;
    /* 中心卡片平均油耗显隐：Qt SettingsMgr.avgFuelOnOff 的本地映射，缺省关闭 */
    bool avg_fuel_on;
    /* ECO 指示：Qt SettingsMgr.ecoIndicator，与 0x0F 的 ecoEnable 共同决定图标 */
    bool eco_indicator;
    /*
     * ECO 卡形态：Qt SettingsMgr.ecoOrfuel。
     * true = 油门引导（圆环），false = 油耗（水平刻度）；
     * Qt EcoShow：showCircularMode = ecoIndicator && ecoOrfuel && ecoEnable。
     */
    bool eco_or_fuel;
    /*
     * 背光亮度 0~100：Qt GlobalData.items.daylightvalue / nightlightvalue。
     * current_mode 0=白天 1=黑夜，MCU 背光帧到达前先按白天处理。
     */
    uint8_t daylight;
    uint8_t nightlight;
    /* 超速报警开关：Qt SettingsMgr.speedlimitOnOff */
    bool speed_limit_on;
    /* 驾驶辅助三项：MCU 回读的真实状态，不由本地翻转 */
    car_driver_assist_t driver_assist;
    /* 车辆设置项：灯光/车门等开关的 MCU 回读真实状态 */
    car_vehicle_setting_t vehicle_setting;
    /* 雷达显示开关：Qt SettingsMgr.radaronoff（实为“雷达开启”），缺省开启 */
    bool radar_on;
    /* 四驱灯/提示可见性：Qt SettingsMgr.show4h，缺省关闭 */
    bool show_4h;
    /* 互联（CarPlay/AA/HiCar）会话进行中：Qt GlobalData.items.carPlayModel */
    bool carplay_active;
} car_state_t;//cmd_id

/* ---------------- 快照存储 ---------------- */

typedef struct {
    car_lock_t lock;
    car_state_t state;
    uint64_t changed; /* 自上次 view 起的累计变更掩码 */
    uint32_t version; /* 每 apply 一次 +1 */
    bool has_data;
} car_state_store_t;

typedef struct {
    car_state_t state;
    uint64_t changed;
    uint32_t version;
    bool has_data;
} car_state_view_t;//状态视图

/* ---------------- API ---------------- */

void car_state_init(car_state_t *state);

/*
 * 直接读取每故障码持久灯态 levels，不回退到 global/list 诊断字段。
 * 初始化后全部为 OFF；全局与局部报文按接收顺序覆盖。
 */
uint8_t car_state_warning_level(const car_state_t *state, uint8_t code);

/* 档位字符串 -> 枚举，无法识别时返回 fallback */
car_gear_pos_t car_gear_from_text(const char *text, car_gear_pos_t fallback);

/*
 * 追加一条 0x1C 警告事件。队列满时挤掉最旧的一条并累加 dropped，
 * 只由解码线程调用。
 */
void car_state_warn_event_push(car_state_t *state, uint16_t text_id);

void car_state_store_init(car_state_store_t *store);
void car_state_store_deinit(car_state_store_t *store);

/*
 * 提交一份权威副本。sample 必须是写者线程持有的完整状态（它会被整体拷贝），
 * mask 说明本次哪些组发生了变化。
 */
void car_state_store_apply(car_state_store_t *store,
                           const car_state_t *sample,
                           uint64_t mask);

/* 取快照，并清空 changed（单读者语义：一个 UI 线程）。 */
void car_state_store_view(car_state_store_t *store, car_state_view_t *view);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_CAR_STATE_H */
