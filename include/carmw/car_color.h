/*
 * car_color.h - 车身颜色素材（对应 QML SettingsMgr.carColor）
 *
 * 结构上独立成单独头文件：本模块只被 UI 与 PC 场景注入使用，
 * 不进入协议解码链路。
 */
#ifndef CARMW_CAR_COLOR_H
#define CARMW_CAR_COLOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 保持 Qt SettingsManager 的取值形态：0xRRGGBB 字符串（如 "#004a6b"）。
 * valid 为 false 时页面按素材原色显示，不做染色。
 */
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    bool valid;
} car_color_t;

/* 解析 "#RRGGBB" / "RRGGBB"（不区分大小写）；格式不合法返回 false 且 out 不变 */
bool car_color_parse(const char *text, car_color_t *out);

/* 取 Qt 默认车身色 #004a6b（SettingsManager 的 carColor 缺省值） */
car_color_t car_color_default(void);

#ifdef __cplusplus
}
#endif

#endif /* CARMW_CAR_COLOR_H */
