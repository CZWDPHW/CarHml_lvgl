/*
 * ui_app.h - 页面舞台与样式切换
 *
 * 对应 QML 的 main.qml 页面管理：持有舞台容器，负责样式的装载、卸载与切换。
 */
#ifndef CARMW_UI_APP_H
#define CARMW_UI_APP_H

#include <stdbool.h>

#include "carmw/car_command.h"
#include "carmw/car_settings.h"
#include "ui/ui_node.h"
#include "ui/ui_style.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 初始化页面舞台（全屏透明容器），重复调用安全。
 * 需在 lv_init 与显示驱动创建之后调用。
 */
bool ui_app_init(void);

/* 卸载当前样式组件并销毁舞台，需在 lv_deinit 之前调用 */
void ui_app_deinit(void);

/*
 * 切换到指定样式：销毁旧组件 → 按描述创建新组件。
 * 若已有车状态快照，装好后立即补一次 apply，避免切换瞬间显示空数据。
 * id 不存在时返回 false，当前样式保持不变。
 */
bool ui_app_set_style(const char *id);

/* 按注册表顺序切到下一个样式（循环）；样式表为空时返回 false */
bool ui_app_next_style(void);

/* 取当前样式的描述，未装载任何样式时返回 NULL */
const ui_style_desc_t *ui_app_current_style(void);

/* 当前样式页面的根对象，未装载样式时返回 NULL（供页面级测试/截图遍历） */
lv_obj_t *ui_app_current_root(void);

/* 分发车状态快照：缓存后转发给当前样式组件 */
void ui_app_apply(const car_state_view_t *view);

/* 分发按键事件：设置页可见时优先消费，其余转发给当前样式组件 */
void ui_app_handle_key(const ui_key_event_t *evt);

/*
 * 设置菜单（Qt settingStackView 覆盖层）。对象随 ui_app 创建/销毁，
 * 层级状态跨样式切换保留在这里。
 *   打开：Qt 23 在车速 < 5 时设置 settingsPageOpen；
 *   关闭：Qt reSettings（13 在根页 / 8 秒无按键 / 车速超门限后按键）。
 */
void ui_app_settings_set_settings(car_settings_t *settings);
/* 注入设置页的命令通道（板端为 car_link_exec，PC 演示可传 NULL） */
typedef void (*ui_app_settings_cmd_cb_t)(const car_cmd_t *cmd);
void ui_app_settings_set_cmd_cb(ui_app_settings_cmd_cb_t cb);
bool ui_app_settings_is_open(void);
/* 设置页当前是否可见（已打开且车速 < 5，Qt settingsPageOpen && speed < 5） */
bool ui_app_settings_visible(void);
/* 车速是否允许打开设置（Qt speed < 5） */
bool ui_app_settings_speed_ok(void);
/* 是否已收到过含有效数据的车身帧（无帧时速度门控不可用） */
bool ui_app_has_data(void);

/* 设置是否有改动尚未落盘（由 main 注入，用于显示“待保存”提示） */
typedef bool (*ui_app_pending_cb_t)(void);
void ui_app_settings_set_pending_cb(ui_app_pending_cb_t cb);
void ui_app_settings_open(void);
void ui_app_settings_close(void);
/* 设置值变化后刷新显示（notify 回调里调用） */
void ui_app_settings_refresh(void);
/* 中心卡片可见位图（CAR_SETTINGS_CARD_*）；未注入设置时返回 Qt 缺省 */
uint32_t ui_app_center_card_mask(void);
/* 13：返回上一级，根页则关闭；返回 true 表示本次按键已被设置页处理 */
bool ui_app_settings_back(void);
/* 26：Qt setNextOrBackIndex */
void ui_app_settings_next_or_back(void);
/* 设置页覆盖层根对象，未创建时为 NULL（供测试/截图遍历） */
lv_obj_t *ui_app_settings_root(void);

/*
 * PC 演示：向当前样式注入媒体数据（FM / 歌词 / 来电）。
 * 样式未实现 on_demo_media 时静默忽略，板端不调用。
 */ 
void ui_app_demo_media(uint8_t mode, const char *title,
                       const char *line1, const char *line2);

/* PC 演示：注入互联状态（0 关闭 / 1 待连接 / 2 已连接 / 3 连接中） */
void ui_app_demo_connect(uint8_t state, const char *mode_text,
                         const char *bt_name);

/*
 * 互联会话进行中标志（Qt GlobalData.items.carPlayModel）：
 * 进入会话后雷达覆盖层隐藏。6.6 真实 SDK 接入前由演示“已连接”模拟。
 */
void ui_app_set_carplay_active(bool active);
 
#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_APP_H */
