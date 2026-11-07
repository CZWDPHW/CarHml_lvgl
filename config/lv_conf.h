/*
 * lv_conf.h - lvgl_two 的 LVGL 9.5 配置（Linux 桌面 / SDL2 模拟器）
 *
 * 只覆盖需要显式指定的项，其余全部走 lv_conf_internal.h 的默认值。
 * 后续上板（fbdev/drm）时新增一个 config/lv_conf_fbdev.h 即可，不动这份。
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* ---------------- 基础 ---------------- */
#define LV_COLOR_DEPTH 32
#define LV_USE_OS LV_OS_NONE
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#define LV_DPI_DEF 130

/* ---------------- 绘制 ---------------- */
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_DRAW_UNIT_CNT 1

/* ---------------- 日志 ---------------- */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* ---------------- 文件系统：读取 assets 下的 PNG 图片 ---------------- */
#define LV_USE_FS_STDIO 1
#define LV_USE_LODEPNG 1
#define LV_FS_STDIO_LETTER 'A'
#define LV_FS_STDIO_PATH "./assets"
#define LV_FS_STDIO_CACHE_SIZE 0

/* ---------------- 字体 ---------------- */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_44 1
#define LV_FONT_MONTSERRAT_46 1
#define LV_FONT_MONTSERRAT_48 1

/* ---------------- 显示/输入后端：SDL2 ---------------- */
#define LV_USE_SDL 1
#define LV_SDL_RENDER_MODE LV_DISPLAY_RENDER_MODE_DIRECT
#define LV_SDL_BUF_COUNT 1
#define LV_SDL_ACCELERATED 1
#define LV_SDL_FULLSCREEN 0
#define LV_SDL_DIRECT_EXIT 1
#define LV_SDL_MOUSEWHEEL_MODE LV_SDL_MOUSEWHEEL_MODE_ENCODER
#define LV_USE_EVDEV 0

/* ---------------- 构建裁剪 ---------------- */
#define LV_BUILD_EXAMPLES 0
#define LV_USE_DEMO_WIDGETS 0

#endif /* LV_CONF_H */
