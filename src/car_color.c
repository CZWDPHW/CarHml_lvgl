/*
 * car_color.c - 车身颜色素材解析
 *
 * Qt 侧 carColor 是设置项中的 "#RRGGBB" 字符串（SettingsManager.cpp
 * m_carColor 缺省 "#004a6b"）。这里只做同样的解析，不做持久化：
 * 本项目的设置体系属后置阶段，当前由 PC 场景/启动参数注入。
 */
#include "carmw/car_color.h"

#include <stddef.h>

static int hex_digit(char c)
{
    if(c >= '0' && c <= '9') {
        return c - '0';
    }
    if(c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if(c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static bool parse_byte(const char *p, uint8_t *out)
{
    int hi = hex_digit(p[0]);
    int lo = hex_digit(p[1]);

    if(hi < 0 || lo < 0) {
        return false;
    }
    *out = (uint8_t)((hi << 4) | lo);
    return true;
}

bool car_color_parse(const char *text, car_color_t *out)
{
    car_color_t color;

    if(text == NULL || out == NULL) {
        return false;
    }
    if(text[0] == '#') {
        ++text;
    }
    if(!parse_byte(text, &color.red) || !parse_byte(text + 2, &color.green) ||
       !parse_byte(text + 4, &color.blue)) {
        return false;
    }
    if(text[6] != '\0') {
        return false;
    }
    color.valid = true;
    *out = color;
    return true;
}

car_color_t car_color_default(void)
{
    car_color_t color;

    color.red = 0x00U;
    color.green = 0x4AU;
    color.blue = 0x6BU;
    color.valid = true;
    return color;
}
