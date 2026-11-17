/*
 * ui_settings_page.h - 设置菜单覆盖层（阶段6 最小闭环）
 *
 * 对应 Qt qml/main.qml 的 settingStackView（594×504，x=663，y=120）与
 * qml/Settings 下的 PageMainSettings / PageMyCarItem / PageUnitSettings /
 * PageCenterCards。层级状态由 ui_app 持有，覆盖层对象随样式重建。
 */
#ifndef CARMW_UI_SETTINGS_PAGE_H
#define CARMW_UI_SETTINGS_PAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "carmw/car_command.h"
#include "carmw/car_settings.h"
#include "lvgl.h"
#include "ui/ui_node.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 根菜单分类数（行程资讯 / 信息选项 / 我的座驾 / 系统设置 / 互联设置） */
#define UI_SETTINGS_ROOT_COUNT 5U

/* 根菜单下标 */
#define UI_SETTINGS_ROOT_ITINERARY 0U
#define UI_SETTINGS_ROOT_MY_CAR 2U
#define UI_SETTINGS_ROOT_INFO 1U
#define UI_SETTINGS_ROOT_SYSTEM 3U
#define UI_SETTINGS_ROOT_CONNECT 4U

#define UI_SETTINGS_SYSTEM_COUNT 7U
#define UI_SETTINGS_SYSTEM_VERSION 2U
#define UI_SETTINGS_SYSTEM_ANIMATION 5U
#define UI_SETTINGS_SYSTEM_BACK 6U

/* 设置页索引：对应 Qt 的 settingStackView 深度 + 当前页 */
typedef enum {
    UI_SETTINGS_VIEW_ROOT = 0,     /* 根分类横排 */
    UI_SETTINGS_VIEW_MY_CAR,       /* 我的座驾子项 */
    UI_SETTINGS_VIEW_STYLE,        /* 仪表风格 */
    UI_SETTINGS_VIEW_UNIT,         /* 计量单位 */
    UI_SETTINGS_VIEW_CENTER_CARDS, /* 中间卡片设置 */
    UI_SETTINGS_VIEW_DRIVING_INFO, /* 信息选项 → 行车信息 */
    UI_SETTINGS_VIEW_ROUTINE,      /* 我的座驾 → 常规设置 */
    UI_SETTINGS_VIEW_ECO,          /* 我的座驾 → ECO 设置 */
    UI_SETTINGS_VIEW_ECO_HISTORY,  /* 我的座驾 → ECO 历史信息（柱状图） */
    UI_SETTINGS_VIEW_ITINERARY_ASSIST, /* 行程资讯 → 驾驶员辅助子项 */
    UI_SETTINGS_VIEW_DRIVING_ASSIST,   /* 驾驶员辅助 → 驾驶辅助三项 */
    UI_SETTINGS_VIEW_SPEED_WARN,       /* 常规设置 → 超速预警值 */
    UI_SETTINGS_VIEW_CONNECT,
    UI_SETTINGS_VIEW_DATE,
    UI_SETTINGS_VIEW_CLEAR_LOG,
    UI_SETTINGS_VIEW_SYSTEM,            /* 系统设置分类列表 */
    UI_SETTINGS_VIEW_VERSION,           /* 系统设置 → 版本信息 */
    UI_SETTINGS_VIEW_VEHICLE,           /* 行程资讯 → 车辆设置 */
    UI_SETTINGS_VIEW_LIGHT,             /* 车辆设置 → 灯光设置 */
    UI_SETTINGS_VIEW_DOOR,              /* 车辆设置 → 车门设置 */
    UI_SETTINGS_VIEW_TURN,              /* 车辆设置 → 转向灯 */
    UI_SETTINGS_VIEW_WIPER,             /* 车辆设置 → 雨刷器 */
    UI_SETTINGS_VIEW_SERVICE,           /* 车辆设置 → 保养设置主列表 */
    UI_SETTINGS_VIEW_SERVICE_ITEM,      /* 保养设置 → 机油/轮胎/自定义子页 */
    UI_SETTINGS_VIEW_SERVICE_EDIT,      /* 保养设置 → 公里数数字编辑 */
    UI_SETTINGS_VIEW_SELECT,            /* 通用多选：点灯灵敏度/灯光延时/车门自动上锁 */
    UI_SETTINGS_VIEW_LANGUAGE,
    UI_SETTINGS_VIEW_FACTORY_PASSWORD,
    UI_SETTINGS_VIEW_FACTORY,
    UI_SETTINGS_VIEW_FACTORY_FEATURES,
    UI_SETTINGS_VIEW_FACTORY_ODO,
    UI_SETTINGS_VIEW_FACTORY_TANK,
    UI_SETTINGS_VIEW_FACTORY_MODEL,
    UI_SETTINGS_VIEW_EPS,               /* 驾驶员辅助 → 电动助力转向 */
    UI_SETTINGS_VIEW_PARKING,           /* 驾驶员辅助 → 驻车辅助 */
    UI_SETTINGS_VIEW_CHASSIS,           /* 驾驶员辅助 → 底盘控制 */
} ui_settings_view_t;

/*
 * 行程资讯子项（Qt PageItineraryItem）：
 * 行程资讯未迁移，置灰；驾驶员辅助/车辆设置进入下一级。
 */
typedef enum {
    UI_SETTINGS_IT_TRIP = 0,    /* 行程资讯（未迁移） */
    UI_SETTINGS_IT_ASSIST = 1,  /* 驾驶员辅助 */
    UI_SETTINGS_IT_VEHICLE = 2, /* 车辆设置 */
    UI_SETTINGS_IT_BACK = 3,    /* 返回 */
    UI_SETTINGS_IT_COUNT = 4,
} ui_settings_it_item_t;

/*
 * 驾驶员辅助子项（Qt DrivePeopleHelp）：电动助力转向按工厂开关
 * enableElectricPowerSteering（bit6）隐藏。
 */
typedef enum {
    UI_SETTINGS_ASSIST_DRIVING = 0, /* 驾驶辅助 */
    UI_SETTINGS_ASSIST_PARKING = 1, /* 驻车辅助 */
    UI_SETTINGS_ASSIST_CHASSIS = 2, /* 底盘控制 */
    UI_SETTINGS_ASSIST_EPS = 3,     /* 电动助力转向 */
    UI_SETTINGS_ASSIST_BACK = 4,    /* 返回 */
    UI_SETTINGS_ASSIST_COUNT = 5,
} ui_settings_assist_item_t;

/*
 * 驻车辅助（Qt ParkAssist）：移动物体 / 前方雷达 / 倒车雷达 / 显示 /
 * 倒车侧预警（工厂开关 enableRearCrossTrafficAlert bit9）/
 * 雷达音量 / 雷达探测距离 / 驻车辅助传感器（车型相关）/ 返回。
 * 值与 ON/OFF 只显示 MCU 0x16 回读，OK 只发 SetGet。
 */
typedef enum {
    UI_SETTINGS_PARK_MOVING_OBJECT = 0,  /* 0x16 index 4  */
    UI_SETTINGS_PARK_FRONT_RADAR = 1,    /* 0x16 index 5  */
    UI_SETTINGS_PARK_REAR_RADAR = 2,     /* 0x16 index 6  */
    UI_SETTINGS_PARK_DISPLAY = 3,        /* 0x16 index 7  */
    UI_SETTINGS_PARK_REAR_CROSS = 4,     /* 0x16 index 26 */
    UI_SETTINGS_PARK_VOLUME = 5,         /* 0x16 index 17 */
    UI_SETTINGS_PARK_DISTANCE = 6,       /* 0x16 index 18 */
    UI_SETTINGS_PARK_SENSOR = 7,         /* 0x16 index 27 */
    UI_SETTINGS_PARK_BACK = 8,
    UI_SETTINGS_PARK_COUNT = 9,
} ui_settings_parking_item_t;

/* 底盘控制（Qt ChassistControl）：主动弯道控制 / 主动发动机制动 */
typedef enum {
    UI_SETTINGS_CHASSIS_TRACE = 0,  /* 0x16 index 8  */
    UI_SETTINGS_CHASSIS_BRAKE = 1,  /* 0x16 index 19 */
    UI_SETTINGS_CHASSIS_BACK = 2,
    UI_SETTINGS_CHASSIS_COUNT = 3,
} ui_settings_chassis_item_t;

/*
 * 电动助力转向（Qt ElectricPowerStepping）：一行选项 + 返回，
 * 按下 OK 按 MCU 回读值切换 0 常规 / 1 运动（0x16 index 9）。
 */
typedef enum {
    UI_SETTINGS_EPS_MODE = 0,
    UI_SETTINGS_EPS_BACK = 1,
    UI_SETTINGS_EPS_COUNT = 2,
} ui_settings_eps_item_t;

/*
 * 驾驶辅助三项（Qt DrivingHelp）：值为 MCU 真实状态，OK 只发 0x16 SetGet，
 * 显示不本地翻转，等 0x16 Status 回读更新。
 */
typedef enum {
    UI_SETTINGS_DA_EMERGENCY = 0, /* 0x16 index 1 */
    UI_SETTINGS_DA_LANE = 1,      /* 0x16 index 2 */
    UI_SETTINGS_DA_BLIND = 2,     /* 0x16 index 3 */
    UI_SETTINGS_DA_BACK = 3,      /* 返回 */
    UI_SETTINGS_DA_COUNT = 4,
} ui_settings_driver_assist_item_t;

/* 信息选项子项（Qt PageSettingDrivingInfo 的 leftModel 顺序） */
typedef enum {
    UI_SETTINGS_INFO_DRIVING = 0, /* 行车信息 */
    UI_SETTINGS_INFO_BACK = 1,    /* 返回 */
    UI_SETTINGS_INFO_COUNT = 2,
} ui_settings_info_item_t;

/* 行车信息子页行序：开关 + 返回 */
typedef enum {
    UI_SETTINGS_DRV_OUT_TEMP = 0, /* 车外温度 */
    UI_SETTINGS_DRV_GEAR = 1,     /* 档位显示 */
    UI_SETTINGS_DRV_AVG_FUEL = 2, /* 平均油耗 */
    UI_SETTINGS_DRV_BACK = 3,     /* 返回 */
    UI_SETTINGS_DRV_COUNT = 4,
} ui_settings_driving_item_t;

/* 常规设置行序（Qt PageGeneralSettings） */
typedef enum {
    UI_SETTINGS_ROUTINE_DAYLIGHT = 0,  /* 白天亮度 0~100 */
    UI_SETTINGS_ROUTINE_NIGHTLIGHT = 1,/* 黑夜亮度 0~100 */
    UI_SETTINGS_ROUTINE_SPEED_LIMIT = 2, /* 超速报警开关 */
    UI_SETTINGS_ROUTINE_SPEED_WARN = 3,  /* 超速预警值：未迁移 */
    UI_SETTINGS_ROUTINE_BACK = 4,      /* 返回 */
    UI_SETTINGS_ROUTINE_COUNT = 5,
} ui_settings_routine_item_t;

/*
 * ECO 设置行序（Qt EcoSettings leftModel）：
 *   “显示”在 Qt 里进子页选“油门引导 / 油耗”，这里用同一行左右切换，
 *   值映射 SettingsMgr.ecoOrfuel（true = 油门引导 = 圆环，false = 油耗 = 水平）。
 */
typedef enum {
    UI_SETTINGS_ECO_DISPLAY = 0,   /* 显示：THROTTLE / FUEL */
    UI_SETTINGS_ECO_INDICATOR = 1, /* ECO 指示 */
    UI_SETTINGS_ECO_BACK = 2,      /* 返回 */
    UI_SETTINGS_ECO_COUNT = 3,
} ui_settings_eco_item_t;

/*
 * 车辆设置子项（Qt CarSettings.leftModel 顺序）。
 */
typedef enum {
    UI_SETTINGS_VEH_LIGHT = 0,   /* 灯光设置 */
    UI_SETTINGS_VEH_TURN = 1,    /* 转向灯 */
    UI_SETTINGS_VEH_DOOR = 2,    /* 车门设置 */
    UI_SETTINGS_VEH_WIPER = 3,   /* 雨刷器 */
    UI_SETTINGS_VEH_SERVICE = 4, /* 保养设置 */
    UI_SETTINGS_VEH_REMOTE_START = 5, /* 远程启动发动机（index 23） */
    UI_SETTINGS_VEH_BACK = 6,    /* 返回 */
    UI_SETTINGS_VEH_COUNT = 7,
} ui_settings_vehicle_item_t;

/*
 * 灯光设置（Qt LightSettings）：迎宾灯 index 10、室内自动点灯 index 11、
 * 点灯灵敏度（扩展 control 2）、灯光关闭延时（扩展 control 1）。
 */
typedef enum {
    UI_SETTINGS_LIGHT_WELCOME = 0,       /* 0x16 index 10 */
    UI_SETTINGS_LIGHT_AUTO_INTERIOR = 1, /* 0x16 index 11 */
    UI_SETTINGS_LIGHT_SENSITIVITY = 2,   /* 0x16 扩展 control 2 */
    UI_SETTINGS_LIGHT_DELAY = 3,         /* 0x16 扩展 control 1 */
    UI_SETTINGS_LIGHT_BACK = 4,          /* 返回 */
    UI_SETTINGS_LIGHT_COUNT = 5,
} ui_settings_light_item_t;

/*
 * 车门设置（Qt DoorSetting）：喇叭响应 index 15、车门自动解锁 index 16、
 * 外部车门 index 13、单车门解锁 index 14、智能钥匙 index 24、
 * 选择解锁 index 25、车门自动上锁 index 28。
 */
typedef enum {
    UI_SETTINGS_DOOR_HORN = 0,        /* 0x16 index 15 */
    UI_SETTINGS_DOOR_AUTO_UNLOCK = 1, /* 0x16 index 16 */
    UI_SETTINGS_DOOR_EXTERNAL = 2,    /* 0x16 index 13 */
    UI_SETTINGS_DOOR_SINGLE_UNLOCK = 3, /* 0x16 index 14 */
    UI_SETTINGS_DOOR_SMART_KEY = 4,   /* 0x16 index 24 */
    UI_SETTINGS_DOOR_SELECT_UNLOCK = 5, /* 0x16 index 25 */
    UI_SETTINGS_DOOR_AUTO_LOCK = 6,   /* 0x16 index 28 */
    UI_SETTINGS_DOOR_BACK = 7,        /* 返回 */
    UI_SETTINGS_DOOR_COUNT = 8,
} ui_settings_door_item_t;

/* 转向灯（Qt TurnLightSet）：变更车道闪灯 3 次 index 12 */
typedef enum {
    UI_SETTINGS_TURN_LANE_CHANGE = 0, /* 0x16 index 12 */
    UI_SETTINGS_TURN_BACK = 1,
    UI_SETTINGS_TURN_COUNT = 2,
} ui_settings_turn_item_t;

/* 雨刷器（Qt WiperSet）：index 20 / 21 / 22 */
typedef enum {
    UI_SETTINGS_WIPER_SPEED_SENSE = 0, /* 0x16 index 20 */
    UI_SETTINGS_WIPER_REAR_LINKAGE = 1,/* 0x16 index 21 */
    UI_SETTINGS_WIPER_MIST = 2,        /* 0x16 index 22 */
    UI_SETTINGS_WIPER_BACK = 3,
    UI_SETTINGS_WIPER_COUNT = 4,
} ui_settings_wiper_item_t;

/* 保养设置主列表（Qt Service） */
typedef enum {
    UI_SETTINGS_SERVICE_OIL = 0,
    UI_SETTINGS_SERVICE_TIRE = 1,
    UI_SETTINGS_SERVICE_CUSTOM = 2,
    UI_SETTINGS_SERVICE_BACK = 3,
    UI_SETTINGS_SERVICE_COUNT = 4,
} ui_settings_service_item_t;

/* 保养子页（Qt PageOilChangeSetting 第一层） */
typedef enum {
    UI_SETTINGS_SVC_SET_KM = 0,
    UI_SETTINGS_SVC_REMAIN_KM = 1,
    UI_SETTINGS_SVC_RESET = 2,
    UI_SETTINGS_SVC_BACK = 3,
    UI_SETTINGS_SVC_COUNT = 4,
} ui_settings_service_row_t;

#define UI_SETTINGS_SVC_DIGITS 6U
#define UI_SETTINGS_SVC_EDIT_CONFIRM 6U
#define UI_SETTINGS_SVC_EDIT_BACK 7U
#define UI_SETTINGS_SVC_EDIT_COUNT 8U

/* 通用多选页：点灯灵敏度 / 灯光关闭延时 / 车门自动上锁 */
typedef enum {
    UI_SETTINGS_SELECT_SENSITIVITY = 0, /* 4 项，扩展 control 2 */
    UI_SETTINGS_SELECT_DELAY = 1,       /* 8 项，扩展 control 1 */
    UI_SETTINGS_SELECT_AUTO_LOCK = 2,   /* 3 项，index 28 */
    UI_SETTINGS_SELECT_ANIMATION = 3,   /* 2 项，本地配置 */
} ui_settings_select_kind_t;

#define UI_SETTINGS_SELECT_MAX 9U /* 最多 8 个选项 + 返回 */

/* 我的座驾子项下标（固定清单，未迁移项置灰不可执行） */
typedef enum {
    UI_SETTINGS_MYCAR_STYLE = 0,        /* 仪表风格 */
    UI_SETTINGS_MYCAR_UNIT = 1,         /* 计量单位 */
    UI_SETTINGS_MYCAR_CENTER_CARDS = 2, /* 中间卡片设置 */
    UI_SETTINGS_MYCAR_ROUTINE = 3,      /* 常规设置 */
    UI_SETTINGS_MYCAR_ECO = 4,          /* ECO 设置 */
    UI_SETTINGS_MYCAR_ECO_HISTORY = 5,  /* ECO 历史信息 */
    UI_SETTINGS_MYCAR_BACK = 6,         /* 返回 */
    UI_SETTINGS_MYCAR_COUNT = 7,
} ui_settings_mycar_item_t;

/*
 * ECO 历史：Qt EcoavgShow 的 historyModel 固定保留最近 4 次记录，
 * 柱高 = val / maxScale × 180px，Y 轴量程 30.0 l/100km。
 */
#define UI_SETTINGS_ECO_HISTORY_SLOTS CAR_SETTINGS_ECO_HISTORY_SLOTS
#define UI_SETTINGS_ECO_HISTORY_BAR_H 180

/* 中心卡片数量（与 PageCenterCards 的勾选清单一致） */
#define UI_SETTINGS_CARD_COUNT 10U

/*
 * 层级状态：Qt 的 settingStackView 深度 + 各页 selectedIndex。
 * 由 ui_app 持有，覆盖层销毁重建后按这份状态恢复。
 */
typedef struct {
    ui_settings_view_t view;
    bool root_locked;    /* PageMainSettings.isOk */
    uint8_t root_index;  /* selectedIndex，0xFF = -1 */
    bool mycar_locked;   /* PageMyCarItem.isOk */
    uint8_t mycar_index; /* menuIndex */
    uint8_t mycar_child; /* childIndex，0xFF = -1 */
    uint8_t style_index;
    uint8_t unit_index;
    uint8_t card_index;  /* PageCenterCards.selectIndex 初值 0 */
    uint8_t info_index;  /* 信息选项子项，0xFF = -1 */
    uint8_t drv_index;   /* 行车信息行，0xFF = -1 */
    uint8_t routine_index; /* 常规设置行，0xFF = -1 */
    uint8_t eco_index;     /* ECO 设置行，0xFF = -1 */
    uint8_t it_index;      /* 行程资讯子项，0xFF = -1 */
    uint8_t assist_index;  /* 驾驶员辅助子项，0xFF = -1 */
    uint8_t driving_assist_index; /* 驾驶辅助三项，0xFF = -1 */
    uint8_t eps_index;            /* 电动助力转向行，0xFF = -1 */
    uint8_t parking_index;        /* 驻车辅助行，0xFF = -1 */
    uint8_t chassis_index;        /* 底盘控制行，0xFF = -1 */
    uint8_t vehicle_index;        /* 车辆设置子项，0xFF = -1 */
    uint8_t light_index;          /* 灯光设置行，0xFF = -1 */
    uint8_t door_index;           /* 车门设置行，0xFF = -1 */
    uint8_t turn_index;           /* 转向灯行，0xFF = -1 */
    uint8_t wiper_index;          /* 雨刷器行，0xFF = -1 */
    uint8_t service_index;        /* 保养主列表行，0xFF = -1 */
    uint8_t service_type;         /* 保养项目：0 机油 / 1 轮胎 / 2 自定义 */
    uint8_t service_row;          /* 保养子页行，0xFF = -1 */
    uint8_t service_digit_index;  /* 数字编辑当前位 0~5，6 确认，7 返回 */
    uint8_t service_digits[UI_SETTINGS_SVC_DIGITS]; /* 数字编辑缓冲 */
    bool service_editing;         /* 是否处于数字编辑页 */
    uint8_t speed_warn_digits[3]; /* 百/十/个位，运行期编辑缓冲 */
    uint8_t speed_warn_digit_index;
    uint8_t select_kind;          /* ui_settings_select_kind_t */
    uint8_t select_index;         /* 多选当前行，0xFF = -1 */
    uint8_t select_saved_index;   /* 多选已保存项（回读值） */
    uint8_t system_index;
    uint8_t date_index;
    uint16_t date_values[5];
    uint16_t date_year_base;      /* 年份滚轮起点：max(2000, 当前年 - 5)，Qt yearModel */
    uint8_t date_focus;           /* 滚轮焦点列 0~4，5 = 确认；0xFF = 未进入滚轮 */
    /* Qt PageDateSettings：itemIndex 0 滚轮 / 1 时间制式 / 2 时间源 / 3 返回；
     * childNextOrBack = 光标移动到端点后反向 */
    uint8_t date_item;
    bool date_child_back;
    uint8_t factory_index;
    uint8_t factory_digits[6];
    bool factory_unlocked;
    bool factory_error;
} ui_settings_state_t;

typedef struct {
    lv_obj_t *root;                 /* 窗口容器（594×504），即页面根对象 */
    lv_obj_t *title;                /* 标题（根页留空） */
    lv_obj_t *body;                 /* 分类列表 y=160；压栈子页 y=140 */
    lv_obj_t *root_bar;             /* 根分类横排 */
    lv_obj_t *root_menu[UI_SETTINGS_ROOT_COUNT];
    lv_obj_t *mycar_list;           /* 我的座驾子项列表 */
    lv_obj_t *mycar_rows[UI_SETTINGS_MYCAR_COUNT];
    lv_obj_t *style_list;
    lv_obj_t *unit_list;            /* 里程/温度选项 + 确定 */
    lv_obj_t *card_list;            /* 中心卡片勾选 + 返回 */
    lv_obj_t *info_list;            /* 信息选项：行车信息 + 返回 */
    lv_obj_t *info_rows[UI_SETTINGS_INFO_COUNT];
    lv_obj_t *drv_list;             /* 行车信息开关 + 返回 */
    lv_obj_t *drv_rows[UI_SETTINGS_DRV_COUNT];
    lv_obj_t *routine_list;         /* 常规设置：亮度 / 超速 + 返回 */
    lv_obj_t *routine_rows[UI_SETTINGS_ROUTINE_COUNT];
    lv_obj_t *eco_list;             /* ECO 设置：ECO 指示 + 返回 */
    lv_obj_t *eco_rows[UI_SETTINGS_ECO_COUNT];
    /* ECO 历史信息：复用列表容器，内部是一张柱状图卡片 */
    lv_obj_t *eco_hist_list;
    lv_obj_t *eco_hist_bars[UI_SETTINGS_ECO_HISTORY_SLOTS]; /* 历史 4 根蓝柱 */
    lv_obj_t *eco_hist_cur_bar;    /* 当前平均油耗（白柱） */
    lv_obj_t *eco_hist_best_bar;   /* 最佳（绿柱） */
    lv_obj_t *eco_hist_cur_label;  /* 底部“当前 x.x” */
    lv_obj_t *eco_hist_best_label; /* 底部“最佳 x.x” */
    lv_obj_t *it_list;              /* 行程资讯：驾驶员辅助 + 返回 */
    lv_obj_t *it_rows[UI_SETTINGS_IT_COUNT];
    lv_obj_t *assist_list;          /* 驾驶员辅助：驾驶辅助 + 返回 */
    lv_obj_t *assist_rows[UI_SETTINGS_ASSIST_COUNT];
    lv_obj_t *drv_assist_list;      /* 驾驶辅助：三项开关 + 返回 */
    lv_obj_t *drv_assist_rows[UI_SETTINGS_DA_COUNT];
    lv_obj_t *eps_list;             /* 电动助力转向：模式 + 返回 */
    lv_obj_t *eps_rows[UI_SETTINGS_EPS_COUNT];
    lv_obj_t *parking_list;         /* 驻车辅助：开关/选项 + 返回 */
    lv_obj_t *parking_rows[UI_SETTINGS_PARK_COUNT];
    lv_obj_t *chassis_list;         /* 底盘控制：两项开关 + 返回 */
    lv_obj_t *chassis_rows[UI_SETTINGS_CHASSIS_COUNT];
    lv_obj_t *vehicle_list;         /* 车辆设置：灯光/车门/返回 */
    lv_obj_t *vehicle_rows[UI_SETTINGS_VEH_COUNT];
    lv_obj_t *light_list;           /* 灯光设置：迎宾灯/室内自动点灯 + 返回 */
    lv_obj_t *light_rows[UI_SETTINGS_LIGHT_COUNT];
    lv_obj_t *door_list;            /* 车门设置：喇叭响应/自动解锁 + 返回 */
    lv_obj_t *door_rows[UI_SETTINGS_DOOR_COUNT];
    lv_obj_t *turn_list;            /* 转向灯 */
    lv_obj_t *turn_rows[UI_SETTINGS_TURN_COUNT];
    lv_obj_t *wiper_list;           /* 雨刷器 */
    lv_obj_t *wiper_rows[UI_SETTINGS_WIPER_COUNT];
    lv_obj_t *service_list;         /* 保养主列表 */
    lv_obj_t *service_rows[UI_SETTINGS_SERVICE_COUNT];
    lv_obj_t *service_item_list;    /* 保养子页 */
    lv_obj_t *service_item_rows[UI_SETTINGS_SVC_COUNT];
    lv_obj_t *service_edit_list;    /* 数字编辑 */
    lv_obj_t *service_edit_digits[UI_SETTINGS_SVC_DIGITS];
    lv_obj_t *service_edit_rows[2]; /* 确认 / 返回 */
    lv_obj_t *select_list;          /* 通用多选页 */
    lv_obj_t *select_rows[UI_SETTINGS_SELECT_MAX];
    lv_obj_t *speed_warn_list;
    lv_obj_t *date_list;
    lv_obj_t *date_rows[9];
    /* 日期时间滚轮页（Qt PageDateSettings instrumentMode） */
    lv_obj_t *date_wheel_list;
    lv_obj_t *date_wheel_group;    /* 滚轮条整体（Qt dateTimeItem，负责 scale 动画） */
    lv_obj_t *date_wheel_box;      /* 滚轮背景 */
    lv_obj_t *date_wheel_cols[5];  /* 年/月/日/时/分 三行文本栏 */
    lv_obj_t *date_wheel_cells[5][3];
    lv_obj_t *date_wheel_confirm;
    lv_obj_t *date_wheel_format;
    lv_obj_t *date_wheel_source;
    lv_obj_t *date_wheel_back;
    lv_obj_t *date_wheel_rows[3];  /* 时间制式 / 时间源 / 返回 */
    car_clock_t date_clock;
    car_units_t date_units;
    uint8_t time_source;
    char connect_name[96];
    lv_obj_t *factory_list;
    lv_obj_t *factory_rows[20];
    car_car_model_t factory_models;
    lv_obj_t *system_list;
    lv_obj_t *system_rows[UI_SETTINGS_SYSTEM_COUNT];
    lv_obj_t *version_list;
    lv_obj_t *version_values[5];
    lv_obj_t *version_updated;
    char version_text[3][CAR_TEXT_MAX];
    bool version_valid[3];
    lv_obj_t *speed_warn_digits[3];
    uint8_t speed_warn_kph;
    lv_obj_t *pending_hint;         /* 底部“改动待保存”提示 */

    bool active;                    /* 已打开，且车速 < 5 */
    bool view_ready;
    ui_settings_view_t shown_view;
    uint8_t shown_root;
    unsigned shown_cleanup_status;
    bool system_version_loaded;
    char system_version[CAR_TEXT_MAX];
    ui_settings_state_t st;         /* 层级状态（与 ui_app 同步） */
} ui_settings_page_t;

/* 层级状态的 Qt 初始值（reSettings 后的状态） */
void ui_settings_state_init(ui_settings_state_t *st);

/* 创建覆盖层对象（默认隐藏），parent 为舞台 */
bool ui_settings_page_create(ui_settings_page_t *page, lv_obj_t *parent);

/* 销毁覆盖层对象 */
void ui_settings_page_destroy(ui_settings_page_t *page);

/* 注入设置存储（板端为持久设置，PC 为内存默认值） */
void ui_settings_page_set_settings(car_settings_t *settings);

/*
 * 注入命令发送（板端接 car_link_exec，PC 传 NULL 静默丢弃）。
 * 目前只用于常规设置的背光与超速报警下发。
 */
typedef void (*ui_settings_page_cmd_cb_t)(const car_cmd_t *cmd);
void ui_settings_page_set_cmd_cb(ui_settings_page_cmd_cb_t cb);

/* Platform callbacks run on the UI thread; start must only enqueue work. */
typedef bool (*ui_settings_cleanup_start_cb_t)(void);
typedef unsigned (*ui_settings_cleanup_status_cb_t)(void);
void ui_settings_page_set_cleanup_cb(ui_settings_cleanup_start_cb_t start,
                                     ui_settings_cleanup_status_cb_t status);

/* 是否已打开且车速 < 5 */
bool ui_settings_page_is_active(const ui_settings_page_t *page);

/* 设置是否显示；显隐不改变层级状态 */
void ui_settings_page_set_active(ui_settings_page_t *page, bool active);

/* 按 page->st 重新应用界面（层级变化、设置变化后调用） */
void ui_settings_page_apply(ui_settings_page_t *page);

/* 内容刷新：设置值变化后同步勾选/选项文字 */
void ui_settings_page_refresh(ui_settings_page_t *page);

/*
 * ECO 历史页的“当前”平均油耗（×10，Qt GlobalData.items.avgfuel）。
 * ui_app 每次 push 视图状态时注入，历史页按它画白柱并参与最佳值。
 */
void ui_settings_page_set_eco_current(ui_settings_page_t *page, uint16_t avg_x10);

/*
 * 驾驶辅助三项的 MCU 回读值（ui_app 每次 apply 注入）。
 * 设置页只显示这三项，OK 时按当前值取反下发 0x16，不本地翻转。
 */
void ui_settings_page_set_driver_assist(ui_settings_page_t *page,
                                        bool emergency,
                                        bool lane,
                                        bool blind);
/* 电动助力转向回读值（0x16 index 9）：0 常规 / 1 运动，只显示 MCU 值 */
void ui_settings_page_set_steering_mode(ui_settings_page_t *page, uint8_t mode);
/*
 * 驻车辅助 / 底盘控制回读值（ui_app 每次 apply 注入）。
 * 设置页只显示回读值，OK 时取反（或循环）下发 0x16，不本地翻转。
 */
void ui_settings_page_set_parking_chassis(ui_settings_page_t *page,
                                          const car_parking_chassis_t *pc);
/*
 * 车辆设置项的 MCU 回读值（ui_app 每次 apply 注入）。
 * 设置页只显示回读值，OK 时取反下发 0x16，不本地翻转。
 */
/* 车辆设置回读值注入（0x16），仅在快照变化时刷新 */
void ui_settings_page_set_vehicle_settings(ui_settings_page_t *page,
                                           const car_vehicle_setting_t *vs);

/* 保养公里数（0x36 Status 回读）：按项目 0 机油 / 1 轮胎 / 2 自定义注入 */
void ui_settings_page_set_service(ui_settings_page_t *page,
                                  const car_maintenance_t *maint);
void ui_settings_page_set_speed_warning(ui_settings_page_t *page, bool valid,
                                        uint8_t speed_kph);
void ui_settings_page_set_date(ui_settings_page_t *page, const car_clock_t *clock,
                               const car_units_t *units, uint8_t source);
void ui_settings_page_set_version(ui_settings_page_t *page,
                                  const char *main_version, bool main_valid,
                                  const char *chip_version, bool chip_valid,
                                  const char *wired_version, bool wired_valid);

/* 是否还有改动未落盘：显示“改动待保存”提示（ACC 关闭时统一写入） */
void ui_settings_page_set_pending(ui_settings_page_t *page, bool pending);

/*
 * 按键处理，按 Qt main.qml 的 GlobalData 连接分发。
 * 返回 true 表示设置页消费了该按键，不再传给表盘。
 */
bool ui_settings_page_key(ui_settings_page_t *page, const ui_key_event_t *evt);

/*
 * 子页返回上一级；返回 true 表示还在设置里，false 表示根页没有上一级
 * （Qt reSettings：关闭设置）。
 */
bool ui_settings_page_back(ui_settings_page_t *page);

/* Qt setNextOrBackIndex：根页/我的座驾按序号前进，子页等同返回 */
void ui_settings_page_next_or_back(ui_settings_page_t *page);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_SETTINGS_PAGE_H */
