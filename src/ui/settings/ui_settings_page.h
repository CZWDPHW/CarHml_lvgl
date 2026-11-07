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

/* 根菜单下标：本批只迁移“行程资讯 / 我的座驾 / 信息选项”，其余分类暂不进入 */
#define UI_SETTINGS_ROOT_ITINERARY 0U
#define UI_SETTINGS_ROOT_MY_CAR 2U
#define UI_SETTINGS_ROOT_INFO 1U
#define UI_SETTINGS_ROOT_SYSTEM 3U

#define UI_SETTINGS_SYSTEM_COUNT 7U
#define UI_SETTINGS_SYSTEM_VERSION 2U
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
    UI_SETTINGS_VIEW_SYSTEM,            /* 系统设置分类列表 */
    UI_SETTINGS_VIEW_VERSION,           /* 系统设置 → 版本信息 */
} ui_settings_view_t;

/*
 * 行程资讯子项（Qt PageItineraryItem）：
 * 行程资讯 / 车辆设置未迁移，置灰；驾驶员辅助进入下一级。
 */
typedef enum {
    UI_SETTINGS_IT_TRIP = 0,    /* 行程资讯（未迁移） */
    UI_SETTINGS_IT_ASSIST = 1,  /* 驾驶员辅助 */
    UI_SETTINGS_IT_VEHICLE = 2, /* 车辆设置（未迁移） */
    UI_SETTINGS_IT_BACK = 3,    /* 返回 */
    UI_SETTINGS_IT_COUNT = 4,
} ui_settings_it_item_t;

/* 驾驶员辅助子项（Qt DrivePeopleHelp）：驻车辅助/底盘控制未迁移，置灰 */
typedef enum {
    UI_SETTINGS_ASSIST_DRIVING = 0, /* 驾驶辅助 */
    UI_SETTINGS_ASSIST_PARKING = 1, /* 驻车辅助（未迁移） */
    UI_SETTINGS_ASSIST_CHASSIS = 2, /* 底盘控制（未迁移） */
    UI_SETTINGS_ASSIST_BACK = 3,    /* 返回 */
    UI_SETTINGS_ASSIST_COUNT = 4,
} ui_settings_assist_item_t;

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
    uint8_t speed_warn_digits[3]; /* 百/十/个位，运行期编辑缓冲 */
    uint8_t speed_warn_digit_index;
    uint8_t system_index;
} ui_settings_state_t;

typedef struct {
    lv_obj_t *root;                 /* 窗口容器（594×504），即页面根对象 */
    lv_obj_t *title;                /* 标题（根页留空） */
    lv_obj_t *body;                 /* 子页内容容器（y=160，高 370） */
    lv_obj_t *root_bar;             /* 根分类横排 */
    lv_obj_t *root_menu[UI_SETTINGS_ROOT_COUNT];
    lv_obj_t *root_label;           /* 轮盘正前方卡片的名称 */
    int32_t root_pos;               /* 轮盘当前位置 ×1000（动画中为小数） */
    int32_t root_target;            /* 轮盘目标位置 ×1000 */
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
    lv_obj_t *speed_warn_list;
    lv_obj_t *system_list;
    lv_obj_t *system_rows[7];
    lv_obj_t *version_list;
    lv_obj_t *version_values[5];
    lv_obj_t *version_updated;
    char version_text[3][CAR_TEXT_MAX];
    bool version_valid[3];
    lv_obj_t *speed_warn_digits[3];
    uint8_t speed_warn_kph;
    lv_obj_t *pending_hint;         /* 底部“改动待保存”提示 */

    bool active;                    /* 已打开，且车速 < 5 */
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
void ui_settings_page_set_speed_warning(ui_settings_page_t *page, bool valid,
                                        uint8_t speed_kph);
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
