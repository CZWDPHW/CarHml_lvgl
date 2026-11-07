/*
 * lv_conf_board.h - lvgl_two 的 LVGL 9.5 配置（板子：Linux framebuffer）
 *
 * 与 config/lv_conf.h（PC/SDL2）的区别：
 *   - 显示改用 /dev/fb0，软件绘制
 *   - 不依赖 SDL2
 *   - 触摸暂未接入（EVDEV 需要目标 sysroot 带 libevdev，后续按需打开）
 *
 * 用法：cmake -B build-board -S . -DCARMW_PLATFORM=BOARD ...
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* ---------------- 基础 ---------------- */
#define LV_COLOR_DEPTH 16
#define LV_USE_OS LV_OS_NONE
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#define LV_DPI_DEF 130
#define LV_DEF_REFR_PERIOD 20 /* 50fps 调度目标，不保证实际绘制帧率 */

/* ---------------- 绘制 ---------------- */
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_DRAW_UNIT_CNT 1
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE (16 * 1024)

/* ---------------- 日志 ---------------- */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* ---------------- 性能监视：右下角显示帧率与 CPU 指标 ---------------- */
#define LV_USE_SYSMON 1
#define LV_USE_PERF_MONITOR 1
#define LV_USE_PERF_MONITOR_POS LV_ALIGN_BOTTOM_RIGHT
#define LV_USE_PERF_MONITOR_LOG_MODE 0

/* ---------------- 文件系统：读取 assets 下的 .bin 图片 ---------------- */
#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 'A'
#define LV_FS_STDIO_PATH "./assets"
#define LV_FS_STDIO_CACHE_SIZE 0

/* ---------------- 字体 ---------------- */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_44 1
#define LV_FONT_MONTSERRAT_46 1
#define LV_FONT_MONTSERRAT_48 1

/* ---------------- 显示/输入后端：framebuffer ---------------- */
#define LV_USE_SDL 0
#define LV_USE_LINUX_FBDEV 1
#define LV_LINUX_FBDEV_BSD 0
#define LV_LINUX_FBDEV_RENDER_MODE LV_DISPLAY_RENDER_MODE_PARTIAL
#define LV_LINUX_FBDEV_BUFFER_COUNT 1
#define LV_LINUX_FBDEV_BUFFER_SIZE 60
#define LV_LINUX_FBDEV_MMAP 1
#define LV_USE_EVDEV 0

/* ---------------- 构建裁剪 ---------------- */
#define LV_BUILD_EXAMPLES 0
#define LV_USE_DEMO_WIDGETS 0

#endif /* LV_CONF_H */
