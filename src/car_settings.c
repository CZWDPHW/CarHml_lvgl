/*
 * car_settings.c - 本地持久设置的读写（见 car_settings.h 的约束说明）
 *
 * 配置文件是 INI，取值一律以可解析的十进制/字符串写入，读入时逐项校验，
 * 非法或缺失的项回退到默认值，不因为一行坏数据丢掉整份配置。
 */

/* fsync 目录需要 O_DIRECTORY，严格 C11 下要显式打开 POSIX 声明（必须在首个系统头之前） */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "carmw/car_settings.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SETTINGS_SECTION "settings"
#define SETTINGS_TMP_SUFFIX ".tmp"

void car_settings_reset(car_settings_t *settings)
{
    if(settings == NULL) {
        return;
    }

    memset(settings, 0, sizeof(*settings));
    settings->values.unit_mileage = (uint8_t)CAR_UNIT_METRIC;
    settings->values.unit_temperature = (uint8_t)CAR_UNIT_METRIC;
    settings->values.dark_mode = 0U;
    settings->values.center_card_mask = CAR_SETTINGS_CARD_DEFAULT;
    settings->values.out_temperature = 0U; /* Qt OutTemperatureOnOff 缺省 false */
    settings->values.gear_on = 1U;         /* Qt gearOnOff 缺省 true */
    settings->values.avg_fuel_on = 0U;     /* Qt avgFuelOnOff 缺省 false */
    /* Qt GlobalData.items：白天/黑夜亮度缺省 100 */
    settings->values.daylight = 100U;
    settings->values.nightlight = 100U;
    settings->values.speed_limit_on = 0U;  /* Qt speedlimitOnOff 缺省 false */
    settings->values.eco_indicator = 1U;   /* Qt ecoindicator 缺省 true */
    settings->values.eco_or_fuel = 1U;     /* Qt ecoOrfuel 缺省 true（油门引导/圆环） */
    settings->values.radar_on = 1U;        /* Qt radaronoff 实为雷达开启，缺省 true */
    settings->values.show_4h = 0U;         /* Qt show4h 缺省 false */
    /* Qt EcoavgShow 的 historyModel 初始为 9.0 / 10.5 / 8.0 / 9.2 */
    settings->values.eco_history_x10[0] = 90U;
    settings->values.eco_history_x10[1] = 105U;
    settings->values.eco_history_x10[2] = 80U;
    settings->values.eco_history_x10[3] = 92U;
    (void)snprintf(settings->values.dial_style,
                   sizeof(settings->values.dial_style), "%s", "forester1");
}

static bool valid_unit(uint8_t unit)
{
    return unit == (uint8_t)CAR_UNIT_METRIC || unit == (uint8_t)CAR_UNIT_IMPERIAL;
}

static void trim_eol(char *line)
{
    size_t len = strlen(line);

    while(len > 0U) {
        char c = line[len - 1U];

        if(c != '\n' && c != '\r' && c != ' ' && c != '\t') {
            break;
        }
        line[len - 1U] = '\0';
        --len;
    }
}

/* 只接受非空、长度受限的样式标识 */
static bool copy_style_id(char *dst, size_t capacity, const char *src)
{
    size_t len;

    if(dst == NULL || src == NULL || capacity == 0U) {
        return false;
    }

    len = strlen(src);
    if(len == 0U || len >= capacity) {
        return false;
    }

    memcpy(dst, src, len + 1U);
    return true;
}

static bool parse_u32(const char *text, uint32_t *out)
{
    char *end = NULL;
    unsigned long value;

    if(text == NULL || out == NULL || *text == '\0') {
        return false;
    }

    errno = 0;
    value = strtoul(text, &end, 10);
    if(errno != 0 || end == text || *end != '\0' || value > UINT32_MAX) {
        return false;
    }

    *out = (uint32_t)value;
    return true;
}

/* 逐行解析：未知键忽略，非法值保留默认 */
static void parse_line(car_settings_t *settings, char *line)
{
    char *eq;
    char *key;
    char *value;
    char *start = line;

    while(*start == ' ' || *start == '\t') {
        ++start;
    }
    if(*start == '\0' || *start == '#' || *start == ';' || *start == '[') {
        return;
    }

    eq = strchr(start, '=');
    if(eq == NULL) {
        return;
    }
    *eq = '\0';
    key = start;
    value = eq + 1;

    while(*value == ' ' || *value == '\t') {
        ++value;
    }

    if(strcmp(key, "unit_mileage") == 0) {
        uint32_t parsed = 0U;

        if(parse_u32(value, &parsed) && valid_unit((uint8_t)parsed)) {
            settings->values.unit_mileage = (uint8_t)parsed;
        }
    }
    else if(strcmp(key, "unit_temperature") == 0) {
        uint32_t parsed = 0U;

        if(parse_u32(value, &parsed) && valid_unit((uint8_t)parsed)) {
            settings->values.unit_temperature = (uint8_t)parsed;
        }
    }
    else if(strcmp(key, "dark_mode") == 0) {
        uint32_t parsed = 0U;

        if(parse_u32(value, &parsed) && parsed <= 1U) {
            settings->values.dark_mode = (uint8_t)parsed;
        }
    }
    else if(strcmp(key, "dial_style") == 0) {
        (void)copy_style_id(settings->values.dial_style,
                            sizeof(settings->values.dial_style), value);
    }
    else if(strcmp(key, "center_card_mask") == 0) {
        uint32_t parsed = 0U;

        if(parse_u32(value, &parsed)) {
            settings->values.center_card_mask = parsed;
        }
    }
    else if(strcmp(key, "out_temperature") == 0 ||
            strcmp(key, "gear_on") == 0 ||
            strcmp(key, "avg_fuel_on") == 0) {
        uint32_t parsed = 0U;

        if(parse_u32(value, &parsed) && parsed <= 1U) {
            if(key[0] == 'o') {
                settings->values.out_temperature = (uint8_t)parsed;
            }
            else if(key[0] == 'g') {
                settings->values.gear_on = (uint8_t)parsed;
            }
            else {
                settings->values.avg_fuel_on = (uint8_t)parsed;
            }
        }
    }
    else if(strcmp(key, "daylight") == 0 || strcmp(key, "nightlight") == 0) {
        uint32_t parsed = 0U;

        if(parse_u32(value, &parsed) && parsed <= 100U) {
            if(key[0] == 'd') {
                settings->values.daylight = (uint8_t)parsed;
            }
            else {
                settings->values.nightlight = (uint8_t)parsed;
            }
        }
    }
    else if(strcmp(key, "speed_limit_on") == 0) {
        uint32_t parsed = 0U;

        if(parse_u32(value, &parsed) && parsed <= 1U) {
            settings->values.speed_limit_on = (uint8_t)parsed;
        }
    }
    else if(strcmp(key, "eco_indicator") == 0) {
        uint32_t parsed = 0U;

        if(parse_u32(value, &parsed) && parsed <= 1U) {
            settings->values.eco_indicator = (uint8_t)parsed;
        }
    }
    else if(strcmp(key, "eco_or_fuel") == 0) {
        uint32_t parsed = 0U;

        if(parse_u32(value, &parsed) && parsed <= 1U) {
            settings->values.eco_or_fuel = (uint8_t)parsed;
        }
    }
    else if(strcmp(key, "radar_on") == 0) {
        uint32_t parsed = 0U;

        if(parse_u32(value, &parsed) && parsed <= 1U) {
            settings->values.radar_on = (uint8_t)parsed;
        }
    }
    else if(strcmp(key, "show_4h") == 0) {
        uint32_t parsed = 0U;

        if(parse_u32(value, &parsed) && parsed <= 1U) {
            settings->values.show_4h = (uint8_t)parsed;
        }
    }
    else if(strncmp(key, "eco_history", 11) == 0) {
        /* eco_history0..3：最近 4 次平均油耗（×10） */
        uint32_t parsed = 0U;
        uint32_t idx = (uint32_t)(key[11] - '0');

        if(key[11] >= '0' && key[11] <= '9' && idx < CAR_SETTINGS_ECO_HISTORY_SLOTS &&
           parse_u32(value, &parsed) && parsed <= CAR_SETTINGS_ECO_HISTORY_SCALE_X10) {
            settings->values.eco_history_x10[idx] = (uint16_t)parsed;
        }
    }
}

bool car_settings_load(car_settings_t *settings, const char *path)
{
    FILE *fp;
    char line[256];
    size_t path_len;

    if(settings == NULL || path == NULL) {
        return false;
    }

    /* 保留已注册的通知回调，只重置取值与版本 */
    {
        car_settings_notify_t notify = settings->notify;
        void *user = settings->notify_user;

        car_settings_reset(settings);
        settings->notify = notify;
        settings->notify_user = user;
    }

    path_len = strlen(path);
    if(path_len == 0U || path_len >= sizeof(settings->path) ||
       path_len + sizeof(SETTINGS_TMP_SUFFIX) > sizeof(settings->tmp_path)) {
        return false;
    }

    memcpy(settings->path, path, path_len + 1U);
    (void)snprintf(settings->tmp_path, sizeof(settings->tmp_path), "%s%s", path,
                   SETTINGS_TMP_SUFFIX);

    settings->loaded = true;

    fp = fopen(path, "r");
    if(fp == NULL) {
        /* 首次启动：文件不存在是正常情况，保持默认值且无 dirty */
        return errno == ENOENT;
    }

    while(fgets(line, sizeof(line), fp) != NULL) {
        trim_eol(line);
        parse_line(settings, line);
    }

    if(ferror(fp)) {
        (void)fclose(fp);
        return false;
    }

    (void)fclose(fp);

    /* 读入的值就是已保存状态 */
    settings->version = 0U;
    settings->saved_version = 0U;
    return true;
}

void car_settings_set_notify(car_settings_t *settings,
                             car_settings_notify_t notify,
                             void *user)
{
    if(settings == NULL) {
        return;
    }

    settings->notify = notify;
    settings->notify_user = user;
}

bool car_settings_dirty(const car_settings_t *settings)
{
    if(settings == NULL) {
        return false;
    }

    return settings->version != settings->saved_version;
}

/* 值确实变化时才推进版本并通知 */
static bool mark_changed(car_settings_t *settings, uint32_t mask)
{
    ++settings->version;

    if(settings->notify != NULL) {
        settings->notify(settings->notify_user, mask);
    }

    return true;
}

bool car_settings_set_unit(car_settings_t *settings,
                           uint8_t unit_mileage,
                           uint8_t unit_temperature)
{
    uint32_t mask = 0U;

    if(settings == NULL || !valid_unit(unit_mileage) ||
       !valid_unit(unit_temperature)) {
        return false;
    }

    if(settings->values.unit_mileage != unit_mileage) {
        settings->values.unit_mileage = unit_mileage;
        mask |= CAR_SETTINGS_CHANGED_UNIT_MILEAGE;
    }
    if(settings->values.unit_temperature != unit_temperature) {
        settings->values.unit_temperature = unit_temperature;
        mask |= CAR_SETTINGS_CHANGED_UNIT_TEMP;
    }

    if(mask == 0U) {
        return false;
    }

    return mark_changed(settings, mask);
}

bool car_settings_set_dark_mode(car_settings_t *settings, uint8_t dark_mode)
{
    if(settings == NULL || dark_mode > 1U ||
       settings->values.dark_mode == dark_mode) {
        return false;
    }

    settings->values.dark_mode = dark_mode;
    return mark_changed(settings, CAR_SETTINGS_CHANGED_DARK_MODE);
}

bool car_settings_set_dial_style(car_settings_t *settings, const char *style_id)
{
    if(settings == NULL || style_id == NULL) {
        return false;
    }

    if(strcmp(settings->values.dial_style, style_id) == 0) {
        return false;
    }

    if(!copy_style_id(settings->values.dial_style,
                      sizeof(settings->values.dial_style), style_id)) {
        return false;
    }

    return mark_changed(settings, CAR_SETTINGS_CHANGED_DIAL_STYLE);
}

bool car_settings_set_center_card_mask(car_settings_t *settings, uint32_t mask)
{
    if(settings == NULL || settings->values.center_card_mask == mask) {
        return false;
    }

    settings->values.center_card_mask = mask;
    return mark_changed(settings, CAR_SETTINGS_CHANGED_CENTER_CARD);
}

static bool set_flag(car_settings_t *settings, uint8_t *field, bool on,
                     uint32_t changed)
{
    uint8_t value = on ? 1U : 0U;

    if(settings == NULL || *field == value) {
        return false;
    }

    *field = value;
    return mark_changed(settings, changed);
}

bool car_settings_set_out_temperature(car_settings_t *settings, bool on)
{
    if(settings == NULL) {
        return false;
    }
    return set_flag(settings, &settings->values.out_temperature, on,
                    CAR_SETTINGS_CHANGED_OUT_TEMP);
}

bool car_settings_set_gear_on(car_settings_t *settings, bool on)
{
    if(settings == NULL) {
        return false;
    }
    return set_flag(settings, &settings->values.gear_on, on,
                    CAR_SETTINGS_CHANGED_GEAR);
}

bool car_settings_set_avg_fuel_on(car_settings_t *settings, bool on)
{
    if(settings == NULL) {
        return false;
    }
    return set_flag(settings, &settings->values.avg_fuel_on, on,
                    CAR_SETTINGS_CHANGED_AVG_FUEL);
}

bool car_settings_set_brightness(car_settings_t *settings, uint8_t day,
                                 uint8_t night)
{
    uint32_t mask = 0U;

    if(settings == NULL || day > 100U || night > 100U) {
        return false;
    }

    if(settings->values.daylight != day) {
        settings->values.daylight = day;
        mask |= CAR_SETTINGS_CHANGED_DAYLIGHT;
    }
    if(settings->values.nightlight != night) {
        settings->values.nightlight = night;
        mask |= CAR_SETTINGS_CHANGED_NIGHTLIGHT;
    }

    if(mask == 0U) {
        return false;
    }

    return mark_changed(settings, mask);
}

bool car_settings_set_speed_limit_on(car_settings_t *settings, bool on)
{
    if(settings == NULL) {
        return false;
    }
    return set_flag(settings, &settings->values.speed_limit_on, on,
                    CAR_SETTINGS_CHANGED_SPEED_LIMIT);
}

bool car_settings_set_eco_indicator(car_settings_t *settings, bool on)
{
    if(settings == NULL) {
        return false;
    }
    return set_flag(settings, &settings->values.eco_indicator, on,
                    CAR_SETTINGS_CHANGED_ECO_INDICATOR);
}

bool car_settings_set_eco_or_fuel(car_settings_t *settings, bool throttle_guide)
{
    if(settings == NULL) {
        return false;
    }
    return set_flag(settings, &settings->values.eco_or_fuel, throttle_guide,
                    CAR_SETTINGS_CHANGED_ECO_OR_FUEL);
}

bool car_settings_set_radar_on(car_settings_t *settings, bool on)
{
    if(settings == NULL) {
        return false;
    }
    return set_flag(settings, &settings->values.radar_on, on,
                    CAR_SETTINGS_CHANGED_RADAR);
}

bool car_settings_set_show_4h(car_settings_t *settings, bool on)
{
    if(settings == NULL) {
        return false;
    }
    return set_flag(settings, &settings->values.show_4h, on,
                    CAR_SETTINGS_CHANGED_SHOW_4H);
}

/* Qt updateHistory：满 4 条先丢最旧，再追加当前值 */
bool car_settings_eco_history_push(car_settings_t *settings, uint16_t avg_x10)
{
    uint16_t *hist;

    if(settings == NULL || avg_x10 > CAR_SETTINGS_ECO_HISTORY_SCALE_X10) {
        return false;
    }

    hist = settings->values.eco_history_x10;
    if(hist[CAR_SETTINGS_ECO_HISTORY_SLOTS - 1U] == avg_x10) {
        return false;
    }

    for(uint8_t i = 0U; i + 1U < CAR_SETTINGS_ECO_HISTORY_SLOTS; ++i) {
        hist[i] = hist[i + 1U];
    }
    hist[CAR_SETTINGS_ECO_HISTORY_SLOTS - 1U] = avg_x10;

    return mark_changed(settings, CAR_SETTINGS_CHANGED_ECO_HISTORY);
}

/* Qt resetAllData：4 条历史与最佳值一起清零 */
bool car_settings_eco_history_reset(car_settings_t *settings)
{
    bool dirty = false;

    if(settings == NULL) {
        return false;
    }

    for(uint8_t i = 0U; i < CAR_SETTINGS_ECO_HISTORY_SLOTS; ++i) {
        if(settings->values.eco_history_x10[i] != 0U) {
            settings->values.eco_history_x10[i] = 0U;
            dirty = true;
        }
    }

    return dirty ? mark_changed(settings, CAR_SETTINGS_CHANGED_ECO_HISTORY)
                 : false;
}

uint64_t car_settings_snapshot(car_settings_t *settings, car_settings_t *out)
{
    if(settings == NULL || out == NULL || !car_settings_dirty(settings)) {
        return 0U;
    }

    *out = *settings;
    /* 快照不携带通知回调，后台线程不会回调界面 */
    out->notify = NULL;
    out->notify_user = NULL;
    return settings->version;
}

static bool write_all(int fd, const char *text, size_t len)
{
    while(len > 0U) {
        ssize_t written = write(fd, text, len);

        if(written < 0) {
            if(errno == EINTR) {
                continue;
            }
            return false;
        }
        text += written;
        len -= (size_t)written;
    }

    return true;
}

/* 逐级创建目录（父目录已存在时直接返回成功） */
static bool make_dirs(const char *path)
{
    char dir[192];
    size_t len = strlen(path);
    size_t i;

    if(len == 0U || len >= sizeof(dir)) {
        return false;
    }

    memcpy(dir, path, len + 1U);

    for(i = 1U; i < len; ++i) {
        if(dir[i] != '/') {
            continue;
        }

        dir[i] = '\0';
        if(mkdir(dir, 0755) != 0 && errno != EEXIST) {
            return false;
        }
        dir[i] = '/';
    }

    /* 最后一级没有尾随 '/'，需单独创建 */
    if(mkdir(dir, 0755) != 0 && errno != EEXIST) {
        return false;
    }

    return true;
}

/* 临时文件 -> fsync -> rename -> fsync 目录 */
static bool write_config(const char *path, const char *tmp_path,
                         const car_settings_values_t *values)
{
    char text[768];
    char dir[192];
    char *slash;
    int fd;
    int dir_fd;
    int len;

    /* /opt/car_dashboard 在首次启动时可能还不存在：先建目录再写 */
    (void)snprintf(dir, sizeof(dir), "%s", path);
    slash = strrchr(dir, '/');
    if(slash != NULL && slash != dir) {
        *slash = '\0';
        if(!make_dirs(dir)) {
            fprintf(stderr, "settings: mkdir %s failed: %s\n", dir,
                    strerror(errno));
            return false;
        }
    }

    len = snprintf(text, sizeof(text),
                   "[%s]\n"
                   "unit_mileage=%u\n"
                   "unit_temperature=%u\n"
                   "dark_mode=%u\n"
                   "dial_style=%s\n"
                   "center_card_mask=%lu\n"
                   "out_temperature=%u\n"
                   "gear_on=%u\n"
                   "avg_fuel_on=%u\n"
                   "daylight=%u\n"
                   "nightlight=%u\n"
                   "speed_limit_on=%u\n"
                   "eco_indicator=%u\n"
                   "eco_or_fuel=%u\n"
                   "eco_history0=%u\n"
                   "eco_history1=%u\n"
                   "eco_history2=%u\n"
                   "eco_history3=%u\n"
                   "radar_on=%u\n"
                   "show_4h=%u\n",
                   SETTINGS_SECTION, (unsigned)values->unit_mileage,
                   (unsigned)values->unit_temperature,
                   (unsigned)values->dark_mode, values->dial_style,
                   (unsigned long)values->center_card_mask,
                   (unsigned)values->out_temperature,
                   (unsigned)values->gear_on,
                   (unsigned)values->avg_fuel_on,
                   (unsigned)values->daylight,
                   (unsigned)values->nightlight,
                   (unsigned)values->speed_limit_on,
                   (unsigned)values->eco_indicator,
                   (unsigned)values->eco_or_fuel,
                   (unsigned)values->eco_history_x10[0],
                   (unsigned)values->eco_history_x10[1],
                   (unsigned)values->eco_history_x10[2],
                   (unsigned)values->eco_history_x10[3],
                   (unsigned)values->radar_on,
                   (unsigned)values->show_4h);
    if(len < 0 || (size_t)len >= sizeof(text)) {
        return false;
    }

    fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd < 0) {
        return false;
    }

    if(!write_all(fd, text, (size_t)len) || fsync(fd) != 0) {
        (void)close(fd);
        (void)unlink(tmp_path);
        return false;
    }

    if(close(fd) != 0) {
        (void)unlink(tmp_path);
        return false;
    }

    if(rename(tmp_path, path) != 0) {
        (void)unlink(tmp_path);
        return false;
    }

    /* rename 之后目录项也要落盘，否则掉电可能丢文件名 */
    (void)snprintf(dir, sizeof(dir), "%s", path);
    slash = strrchr(dir, '/');
    if(slash == NULL) {
        (void)snprintf(dir, sizeof(dir), ".");
    }
    else if(slash == dir) {
        dir[1] = '\0';
    }
    else {
        *slash = '\0';
    }

    dir_fd = open(dir, O_RDONLY | O_DIRECTORY);
    if(dir_fd < 0) {
        return false;
    }
    if(fsync(dir_fd) != 0) {
        (void)close(dir_fd);
        return false;
    }
    (void)close(dir_fd);

    return true;
}

bool car_settings_save(const car_settings_t *snapshot,
                       uint64_t snapshot_version,
                       car_settings_t *owner)
{
    if(snapshot == NULL || owner == NULL || snapshot_version == 0U) {
        return false;
    }

    if(!write_config(snapshot->path, snapshot->tmp_path, &snapshot->values)) {
        return false;
    }

    if(owner->saved_version < snapshot_version) {
        owner->saved_version = snapshot_version;
    }

    return true;
}
