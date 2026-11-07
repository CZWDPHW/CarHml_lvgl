/*
 * ui_snapshot.h - PC 截图（仅 CARMW_PLATFORM_PC）
 *
 * 桌面会话是 Wayland + Xwayland，客户端读不到窗口像素（XGetImage 恒 BadMatch），
 * 因此截图在渲染进程内部完成：把当前屏幕绘到离屏 canvas，再读回像素写成 PNG。
 */
#ifndef CARMW_UI_SNAPSHOT_H
#define CARMW_UI_SNAPSHOT_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 把 obj（通常是 lv_screen_active()）当前内容保存为 PNG；成功返回 true */
bool ui_snapshot_save(lv_obj_t *obj, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_UI_SNAPSHOT_H */
