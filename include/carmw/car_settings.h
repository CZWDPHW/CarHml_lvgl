/*
 * car_settings.h - 本地持久设置（运行期只驻留内存，ACC 关闭统一写盘）
 *
 * 设计约束（对齐 docs/UI_REWRITE_HANDOFF.md 14.5）：
 *   - 启动读一次配置文件到内存，运行期只改内存并标 dirty，不周期写盘；
 *   - MCU 反馈状态、工厂菜单可见性、运行时 UI 状态都不属于本模块；
 *   - 保存由调用方在 ACC 由开转关时触发，硬件侧约定关闭后留 3 秒供电；
 *   - 写文件用临时文件 + fsync + rename 原子替换，失败保留 dirty。
 *
 * 本模块不依赖 LVGL，可直接做单元测试。
 */
#ifndef CARMW_CAR_SETTINGS_H
#define CARMW_CAR_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 板端默认路径：与程序、资源同目录 */
#define CAR_SETTINGS_DEFAULT_PATH "/opt/car_dashboard/setting.cfg"

#define CAR_SETTINGS_STYLE_MAX 24U /* 含结尾 0 的样式标识缓冲 */

/* 里程单位 */
typedef enum {
    CAR_UNIT_METRIC = 0,
    CAR_UNIT_IMPERIAL = 1,
} car_unit_system_t;

/* ECO 历史记录条数（Qt historyModel 固定 4 条） */
#define CAR_SETTINGS_ECO_HISTORY_SLOTS 4U
/* Y 轴量程 30.0 l/100km（Qt EcoavgShow.maxScale），以 ×10 存 */
#define CAR_SETTINGS_ECO_HISTORY_SCALE_X10 300U

/* 首批字段：6.0 冻结完整清单后在此扩展，未列入的项不得在 UI 里另建存储 */
typedef struct {
    uint8_t unit_mileage;                        /* car_unit_system_t */
    uint8_t unit_temperature;                    /* car_unit_system_t */
    uint8_t dark_mode;                           /* 0 日间 / 1 夜间 */
    char dial_style[CAR_SETTINGS_STYLE_MAX];     /* 样式注册表里的 id */
    uint32_t center_card_mask;                   /* 中心卡片可见位图，0 表示全部关闭 */
    /* 行车信息显示开关，对应 Qt SettingsMgr 的同名字段 */
    uint8_t out_temperature;                     /* OutTemperatureOnOff，缺省关 */
    uint8_t gear_on;                             /* gearOnOff，缺省开 */
    uint8_t avg_fuel_on;                         /* avgFuelOnOff，缺省关 */
    /* 常规设置：Qt PageGeneralSettings 的本地字段 */
    uint8_t daylight;                            /* daylightvalue，0~100，缺省 100 */
    uint8_t nightlight;                          /* nightlightvalue，0~100，缺省 100 */
    uint8_t speed_limit_on;                      /* speedlimitOnOff，缺省关 */
    /* ECO 设置：Qt EcoSettings 的本地字段 */
    uint8_t eco_indicator;                       /* ecoindicator，缺省开 */
    /*
     * ecoOrfuel：ECO 卡显示形态。1 = 油门引导（圆环），0 = 油耗（水平刻度）。
     * Qt：showCircularMode = ecoIndicator && ecoOrfuel && ecoEnable。
     */
    uint8_t eco_or_fuel;
    /*
     * ECO 历史：Qt EcoavgShow.historyModel 的最近 4 次平均油耗（×10，0.1 l/100km）。
     * 索引 0 最旧、3 最新；0 表示该项空（Qt resetAllData 把 val 置 0）。
     */
    uint16_t eco_history_x10[CAR_SETTINGS_ECO_HISTORY_SLOTS];
    /* 雷达显示开关：Qt SettingsMgr.radaronoff（名字取反，实为“雷达开启”），缺省开 */
    uint8_t radar_on;
    /* 四驱灯/提示可见性：Qt SettingsMgr.show4h，缺省关 */
    uint8_t show_4h;
} car_settings_values_t;

/*
 * 中心卡片位（与设置页列表顺序一致）：
 * 0 NAV / 1 4X4-I / 2 TIRE / 3 CHASSIS / 4 ITINERARY /
 * 5 SHORT TERM / 6 FUEL / 7 ECO / 8 MUSIC / 9 CONNECT
 */
#define CAR_SETTINGS_CARD_COUNT 10U
/* Qt 缺省：m_show4iCard=false，其余为 true */
#define CAR_SETTINGS_CARD_DEFAULT UINT32_C(0x3FD)

/* 变更通知：mask 为 CAR_SETTINGS_CHANGED_* 的组合 */
#define CAR_SETTINGS_CHANGED_UNIT_MILEAGE (UINT32_C(1) << 0)
#define CAR_SETTINGS_CHANGED_UNIT_TEMP (UINT32_C(1) << 1)
#define CAR_SETTINGS_CHANGED_DARK_MODE (UINT32_C(1) << 2)
#define CAR_SETTINGS_CHANGED_DIAL_STYLE (UINT32_C(1) << 3)
#define CAR_SETTINGS_CHANGED_CENTER_CARD (UINT32_C(1) << 4)
#define CAR_SETTINGS_CHANGED_OUT_TEMP (UINT32_C(1) << 5)
#define CAR_SETTINGS_CHANGED_GEAR (UINT32_C(1) << 6)
#define CAR_SETTINGS_CHANGED_AVG_FUEL (UINT32_C(1) << 7)
#define CAR_SETTINGS_CHANGED_DAYLIGHT (UINT32_C(1) << 8)
#define CAR_SETTINGS_CHANGED_NIGHTLIGHT (UINT32_C(1) << 9)
#define CAR_SETTINGS_CHANGED_SPEED_LIMIT (UINT32_C(1) << 10)
#define CAR_SETTINGS_CHANGED_ECO_INDICATOR (UINT32_C(1) << 11)
#define CAR_SETTINGS_CHANGED_ECO_HISTORY (UINT32_C(1) << 12)
#define CAR_SETTINGS_CHANGED_ECO_OR_FUEL (UINT32_C(1) << 13)
#define CAR_SETTINGS_CHANGED_RADAR (UINT32_C(1) << 14)
#define CAR_SETTINGS_CHANGED_SHOW_4H (UINT32_C(1) << 15)

typedef void (*car_settings_notify_t)(void *user, uint32_t changed_mask);

typedef struct {
    car_settings_values_t values;
    uint64_t saved_version;          /* 已成功落盘的版本号 */
    uint64_t version;                /* 每次变更 +1 */
    char path[192];
    char tmp_path[196];
    car_settings_notify_t notify;
    void *notify_user;
    bool loaded;                     /* 已尝试过读取（含文件不存在） */
} car_settings_t;

/* 出厂默认值（Qt loadAllSettings 的默认子集），不触碰文件 */
void car_settings_reset(car_settings_t *settings);

/*
 * 设置存储路径并尝试读取。文件不存在属于正常情况：保留默认值、saved_version
 * 与 version 相等（无 dirty）。tmp_path 由正式路径派生（同目录，保证 rename 原子）。
 */
bool car_settings_load(car_settings_t *settings, const char *path);

/* 注册变更通知（可为 NULL）。回调在设置者线程内同步调用。 */
void car_settings_set_notify(car_settings_t *settings,
                             car_settings_notify_t notify,
                             void *user);

/* 逐字段修改：值非法或未变化时返回 false，不产生 dirty/通知 */
bool car_settings_set_unit(car_settings_t *settings,
                           uint8_t unit_mileage,
                           uint8_t unit_temperature);
bool car_settings_set_dark_mode(car_settings_t *settings, uint8_t dark_mode);
bool car_settings_set_dial_style(car_settings_t *settings, const char *style_id);
bool car_settings_set_center_card_mask(car_settings_t *settings, uint32_t mask);

/* 行车信息显示开关：值为 0/1，未变化返回 false */
bool car_settings_set_out_temperature(car_settings_t *settings, bool on);
bool car_settings_set_gear_on(car_settings_t *settings, bool on);
bool car_settings_set_avg_fuel_on(car_settings_t *settings, bool on);

/* 亮度：0~100，超出范围返回 false */
bool car_settings_set_brightness(car_settings_t *settings, uint8_t day, uint8_t night);
/* 超速报警与 ECO 指示：值为 0/1，未变化返回 false */
bool car_settings_set_speed_limit_on(car_settings_t *settings, bool on);
bool car_settings_set_eco_indicator(car_settings_t *settings, bool on);
/* ECO 卡形态：true = 油门引导（圆环），false = 油耗（水平刻度），未变化返回 false */
bool car_settings_set_eco_or_fuel(car_settings_t *settings, bool throttle_guide);
/* 雷达开关与四驱显示：值为 0/1，未变化返回 false */
bool car_settings_set_radar_on(car_settings_t *settings, bool on);
bool car_settings_set_show_4h(car_settings_t *settings, bool on);

/*
 * ECO 历史（Qt EcoavgShow）：
 *   push   = updateHistory()：丢弃最旧一条，把 avg_x10 追加为最新；
 *   reset  = resetAllData()：4 条全部清零。
 * 值相同或越界（> 300，即 30.0 l/100km 量程）返回 false。
 */
bool car_settings_eco_history_push(car_settings_t *settings, uint16_t avg_x10);
bool car_settings_eco_history_reset(car_settings_t *settings);

/* 是否有未落盘修改 */
bool car_settings_dirty(const car_settings_t *settings);

/*
 * 生成一份带版本号的快照，供后台保存线程使用。
 * 返回快照覆盖到的版本号；没有 dirty 时返回 0。
 */
uint64_t car_settings_snapshot(car_settings_t *settings,
                               car_settings_t *out);

/*
 * 落盘快照。成功后仅当当前版本仍等于 snapshot_version 才清除 dirty；
 * 期间又发生修改则保持 dirty，由调用方决定是否补存。
 */
bool car_settings_save(const car_settings_t *snapshot,
                       uint64_t snapshot_version,
                       car_settings_t *owner);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_CAR_SETTINGS_H */
