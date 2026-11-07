/*
 * test_settings.c - 设置模型与 ACC 关闭统一保存
 *
 * 覆盖：默认值、缺失/非法项回退、dirty 与版本、通知、原子写、
 *       ACC 首次采样不触发、下降沿触发、重复关闭帧不重复写、
 *       保存期间的新修改不丢、无 dirty 不写、退出补存。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "carmw/car_settings.h"
#include "carmw/car_settings_saver.h"

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if(!(cond)) {                                                        \
            ++g_failures;                                                    \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
        }                                                                    \
    } while(0)

static int g_checks;
static int g_failures;
static char s_dir[160];

static void make_path(char *out, size_t capacity, const char *name)
{
    (void)snprintf(out, capacity, "%s/%s", s_dir, name);
}

static void remove_dir_files(void)
{
    char path[200];

    make_path(path, sizeof(path), "setting.cfg");
    (void)unlink(path);
    make_path(path, sizeof(path), "setting.cfg.tmp");
    (void)unlink(path);
}

static uint32_t g_notify_mask;
static int g_notify_calls;

static void on_changed(void *user, uint32_t mask)
{
    (void)user;
    g_notify_mask = mask;
    ++g_notify_calls;
}

static void test_defaults(void)
{
    car_settings_t settings;

    car_settings_reset(&settings);
    CHECK(settings.values.unit_mileage == CAR_UNIT_METRIC);
    CHECK(settings.values.unit_temperature == CAR_UNIT_METRIC);
    CHECK(settings.values.dark_mode == 0U);
    CHECK(strcmp(settings.values.dial_style, "forester1") == 0);
    CHECK(settings.values.center_card_mask == CAR_SETTINGS_CARD_DEFAULT);
    /* Qt SettingsManager：outTemperature / avgFuelOnOff 缺省关，gearOnOff 缺省开 */
    CHECK(settings.values.out_temperature == 0U);
    CHECK(settings.values.gear_on == 1U);
    CHECK(settings.values.avg_fuel_on == 0U);
    /* Qt PageGeneralSettings：亮度缺省 100、超速报警缺省关；ECO 指示缺省开 */
    CHECK(settings.values.daylight == 100U);
    CHECK(settings.values.nightlight == 100U);
    CHECK(settings.values.speed_limit_on == 0U);
    CHECK(settings.values.eco_indicator == 1U);
    /* Qt ecoOrfuel 缺省 true：油门引导 → 圆环模式 */
    CHECK(settings.values.eco_or_fuel == 1U);
    /* Qt EcoavgShow.historyModel 初值 9.0 / 10.5 / 8.0 / 9.2 */
    CHECK(settings.values.eco_history_x10[0] == 90U);
    CHECK(settings.values.eco_history_x10[1] == 105U);
    CHECK(settings.values.eco_history_x10[2] == 80U);
    CHECK(settings.values.eco_history_x10[3] == 92U);
    /* Qt radaronoff 实为“雷达开启”，缺省 true；show4h 缺省 false */
    CHECK(settings.values.radar_on == 1U);
    CHECK(settings.values.show_4h == 0U);
    CHECK(!car_settings_dirty(&settings));
}

static void test_load_missing_file(void)
{
    car_settings_t settings;
    char path[200];

    make_path(path, sizeof(path), "setting.cfg");
    (void)unlink(path);

    car_settings_reset(&settings);
    CHECK(car_settings_load(&settings, path));
    CHECK(!car_settings_dirty(&settings));
    CHECK(strcmp(settings.path, path) == 0);
    CHECK(strcmp(settings.values.dial_style, "forester1") == 0);
}

static void test_invalid_values_fall_back(void)
{
    car_settings_t settings;
    char path[200];
    FILE *fp;

    make_path(path, sizeof(path), "setting.cfg");
    fp = fopen(path, "w");
    CHECK(fp != NULL);
    if(fp == NULL) {
        return;
    }
    fputs("[settings]\n"
          "unit_mileage=7\n"
          "unit_temperature=1\n"
          "dark_mode=9\n"
          "dial_style=\n"
          "center_card_mask=12\n"
          "unknown_key=3\n",
          fp);
    (void)fclose(fp);

    car_settings_reset(&settings);
    CHECK(car_settings_load(&settings, path));
    CHECK(settings.values.unit_mileage == CAR_UNIT_METRIC);      /* 非法回退 */
    CHECK(settings.values.unit_temperature == CAR_UNIT_IMPERIAL); /* 合法保留 */
    CHECK(settings.values.dark_mode == 0U);
    CHECK(strcmp(settings.values.dial_style, "forester1") == 0);
    CHECK(settings.values.center_card_mask == 12U);
    CHECK(!car_settings_dirty(&settings));
    (void)unlink(path);
}

static void test_dirty_and_notify(void)
{
    car_settings_t settings;
    char path[200];

    make_path(path, sizeof(path), "setting.cfg");
    (void)unlink(path);

    g_notify_mask = 0U;
    g_notify_calls = 0;

    car_settings_reset(&settings);
    CHECK(car_settings_load(&settings, path));
    car_settings_set_notify(&settings, on_changed, NULL);

    /* 值未变化：不产生 dirty、不通知 */
    CHECK(!car_settings_set_dark_mode(&settings, 0U));
    CHECK(!car_settings_set_dial_style(&settings, "forester1"));
    CHECK(!car_settings_set_unit(&settings, CAR_UNIT_METRIC, CAR_UNIT_METRIC));
    CHECK(!car_settings_dirty(&settings));
    CHECK(g_notify_calls == 0);

    /* 非法值直接拒绝 */
    CHECK(!car_settings_set_dark_mode(&settings, 2U));
    CHECK(!car_settings_set_unit(&settings, 5U, CAR_UNIT_METRIC));
    CHECK(!car_settings_set_center_card_mask(&settings,
                                             CAR_SETTINGS_CARD_DEFAULT));
    CHECK(g_notify_calls == 0);

    CHECK(car_settings_set_unit(&settings, CAR_UNIT_IMPERIAL,
                                CAR_UNIT_IMPERIAL));
    CHECK(g_notify_calls == 1);
    CHECK(g_notify_mask == (CAR_SETTINGS_CHANGED_UNIT_MILEAGE |
                            CAR_SETTINGS_CHANGED_UNIT_TEMP));
    CHECK(car_settings_set_dark_mode(&settings, 1U));
    CHECK(g_notify_mask == CAR_SETTINGS_CHANGED_DARK_MODE);
    CHECK(car_settings_set_dial_style(&settings, "general1"));
    CHECK(g_notify_mask == CAR_SETTINGS_CHANGED_DIAL_STYLE);
    CHECK(car_settings_set_center_card_mask(&settings, 0x1F));
    CHECK(g_notify_mask == CAR_SETTINGS_CHANGED_CENTER_CARD);
    /* 行车信息开关：同值返回 false 且不通知 */
    CHECK(!car_settings_set_gear_on(&settings, true));
    CHECK(!car_settings_set_out_temperature(&settings, false));
    CHECK(car_settings_set_out_temperature(&settings, true));
    CHECK(g_notify_mask == CAR_SETTINGS_CHANGED_OUT_TEMP);
    CHECK(car_settings_set_gear_on(&settings, false));
    CHECK(g_notify_mask == CAR_SETTINGS_CHANGED_GEAR);
    CHECK(car_settings_set_avg_fuel_on(&settings, true));
    CHECK(g_notify_mask == CAR_SETTINGS_CHANGED_AVG_FUEL);
    CHECK(settings.values.out_temperature == 1U);
    CHECK(settings.values.gear_on == 0U);
    CHECK(settings.values.avg_fuel_on == 1U);
    CHECK(settings.version == 7U);

    /* 常规设置：亮度越界拒绝，同值不通知；超速报警与 ECO 指示同值不通知 */
    {
        int calls = g_notify_calls;

        CHECK(!car_settings_set_brightness(&settings, 100U, 100U));
        CHECK(!car_settings_set_brightness(&settings, 101U, 50U));
        CHECK(!car_settings_set_brightness(&settings, 50U, 101U));
        CHECK(g_notify_calls == calls);
    }
    CHECK(car_settings_set_brightness(&settings, 80U, 100U));
    CHECK(g_notify_mask == CAR_SETTINGS_CHANGED_DAYLIGHT);
    CHECK(car_settings_set_brightness(&settings, 80U, 60U));
    CHECK(g_notify_mask == CAR_SETTINGS_CHANGED_NIGHTLIGHT);
    CHECK(car_settings_set_brightness(&settings, 80U, 60U) == false);
    CHECK(!car_settings_set_speed_limit_on(&settings, false));
    CHECK(car_settings_set_speed_limit_on(&settings, true));
    CHECK(g_notify_mask == CAR_SETTINGS_CHANGED_SPEED_LIMIT);
    CHECK(!car_settings_set_eco_indicator(&settings, true));
    CHECK(car_settings_set_eco_indicator(&settings, false));
    CHECK(g_notify_mask == CAR_SETTINGS_CHANGED_ECO_INDICATOR);
    CHECK(!car_settings_set_eco_or_fuel(&settings, true));
    CHECK(car_settings_set_eco_or_fuel(&settings, false));
    CHECK(g_notify_mask == CAR_SETTINGS_CHANGED_ECO_OR_FUEL);
    CHECK(settings.values.daylight == 80U);
    CHECK(settings.values.nightlight == 60U);
    CHECK(settings.values.speed_limit_on == 1U);
    CHECK(settings.values.eco_indicator == 0U);
    CHECK(settings.values.eco_or_fuel == 0U);
    CHECK(settings.version == 12U);

    /* 雷达开关与四驱显示：同值不通知，变化置对应 notify 位 */
    CHECK(!car_settings_set_radar_on(&settings, true));
    CHECK(car_settings_set_radar_on(&settings, false));
    CHECK(g_notify_mask == CAR_SETTINGS_CHANGED_RADAR);
    CHECK(!car_settings_set_show_4h(&settings, false));
    CHECK(car_settings_set_show_4h(&settings, true));
    CHECK(g_notify_mask == CAR_SETTINGS_CHANGED_SHOW_4H);
    CHECK(settings.values.radar_on == 0U);
    CHECK(settings.values.show_4h == 1U);
    CHECK(settings.version == 14U);
    CHECK(car_settings_dirty(&settings));
}

static void test_save_and_reload(void)
{
    car_settings_t settings;
    car_settings_t reloaded;
    car_settings_t snapshot;
    char path[200];
    uint64_t version;

    make_path(path, sizeof(path), "setting.cfg");

    car_settings_reset(&settings);
    CHECK(car_settings_load(&settings, path));
    CHECK(car_settings_set_unit(&settings, CAR_UNIT_IMPERIAL, CAR_UNIT_IMPERIAL));
    CHECK(car_settings_set_dark_mode(&settings, 1U));
    CHECK(car_settings_set_dial_style(&settings, "general1"));
    CHECK(car_settings_set_center_card_mask(&settings, 0x15));
    CHECK(car_settings_set_out_temperature(&settings, true));
    CHECK(car_settings_set_gear_on(&settings, false));
    CHECK(car_settings_set_avg_fuel_on(&settings, true));
    CHECK(car_settings_set_brightness(&settings, 70U, 30U));
    CHECK(car_settings_set_speed_limit_on(&settings, true));
    CHECK(car_settings_set_eco_indicator(&settings, false));
    CHECK(car_settings_set_eco_or_fuel(&settings, false));
    CHECK(car_settings_set_radar_on(&settings, false));
    CHECK(car_settings_set_show_4h(&settings, true));

    version = car_settings_snapshot(&settings, &snapshot);
    CHECK(version == settings.version);
    CHECK(car_settings_save(&snapshot, version, &settings));
    CHECK(!car_settings_dirty(&settings));

    /* 临时文件不应残留 */
    {
        char tmp[220];
        FILE *fp;

        (void)snprintf(tmp, sizeof(tmp), "%s.tmp", path);
        fp = fopen(tmp, "r");
        CHECK(fp == NULL);
        if(fp != NULL) {
            (void)fclose(fp);
        }
    }

    car_settings_reset(&reloaded);
    CHECK(car_settings_load(&reloaded, path));
    CHECK(reloaded.values.unit_mileage == CAR_UNIT_IMPERIAL);
    CHECK(reloaded.values.unit_temperature == CAR_UNIT_IMPERIAL);
    CHECK(reloaded.values.dark_mode == 1U);
    CHECK(strcmp(reloaded.values.dial_style, "general1") == 0);
    CHECK(reloaded.values.center_card_mask == 0x15U);
    CHECK(reloaded.values.out_temperature == 1U);
    CHECK(reloaded.values.gear_on == 0U);
    CHECK(reloaded.values.avg_fuel_on == 1U);
    CHECK(reloaded.values.daylight == 70U);
    CHECK(reloaded.values.nightlight == 30U);
    CHECK(reloaded.values.speed_limit_on == 1U);
    CHECK(reloaded.values.eco_indicator == 0U);
    CHECK(reloaded.values.eco_or_fuel == 0U);
    CHECK(reloaded.values.radar_on == 0U);
    CHECK(reloaded.values.show_4h == 1U);
    CHECK(!car_settings_dirty(&reloaded));

    (void)unlink(path);
}

/* 保存期间又改了一版：成功落盘旧快照后仍保持 dirty，补存后清干净 */
static void test_newer_change_after_snapshot(void)
{
    car_settings_t settings;
    car_settings_t snapshot;
    car_settings_t reloaded;
    char path[200];
    uint64_t version;

    make_path(path, sizeof(path), "setting.cfg");

    car_settings_reset(&settings);
    CHECK(car_settings_load(&settings, path));
    CHECK(car_settings_set_dark_mode(&settings, 1U));
    version = car_settings_snapshot(&settings, &snapshot);
    CHECK(version == 1U);

    /* 模拟保存线程落盘期间界面又改了一次 */
    CHECK(car_settings_set_center_card_mask(&settings, 0x3U));
    CHECK(car_settings_save(&snapshot, version, &settings));
    CHECK(car_settings_dirty(&settings)); /* 新版本仍未落盘 */

    version = car_settings_snapshot(&settings, &snapshot);
    CHECK(version == 2U);
    CHECK(car_settings_save(&snapshot, version, &settings));
    CHECK(!car_settings_dirty(&settings));

    car_settings_reset(&reloaded);
    CHECK(car_settings_load(&reloaded, path));
    CHECK(reloaded.values.dark_mode == 1U);
    CHECK(reloaded.values.center_card_mask == 0x3U);
    (void)unlink(path);
}

static void test_save_failure_keeps_dirty(void)
{
    car_settings_t settings;
    car_settings_t snapshot;
    /* 正常读取一份配置，再把快照路径指向只读目录：落盘失败必须保留 dirty */
    char good[200];
    char ro_dir[200];
    char missing[200];
    uint64_t version;

    make_path(good, sizeof(good), "setting.cfg");
    (void)snprintf(ro_dir, sizeof(ro_dir), "%s/ro_dir", s_dir);
    (void)snprintf(missing, sizeof(missing), "%s/setting.cfg", ro_dir);
    (void)unlink(missing);
    (void)rmdir(ro_dir);
    if(mkdir(ro_dir, 0555) != 0) {
        return;
    }

    car_settings_reset(&settings);
    CHECK(car_settings_load(&settings, good));
    CHECK(car_settings_set_dark_mode(&settings, 1U));
    version = car_settings_snapshot(&settings, &snapshot);
    (void)snprintf(snapshot.path, sizeof(snapshot.path), "%s", missing);
    (void)snprintf(snapshot.tmp_path, sizeof(snapshot.tmp_path), "%s.tmp",
                   missing);
    /* root 用户无视目录权限，此时跳过“写失败”这一断言 */
    if(access(ro_dir, W_OK) != 0) {
        CHECK(!car_settings_save(&snapshot, version, &settings));
        CHECK(car_settings_dirty(&settings));
    }

    (void)chmod(ro_dir, 0755);
    (void)unlink(missing);
    (void)rmdir(ro_dir);
}

static void test_saver_acc_edge(void)
{
    car_settings_t settings;
    car_settings_saver_t saver;
    car_settings_t reloaded;
    char path[200];

    make_path(path, sizeof(path), "setting.cfg");
    (void)unlink(path);

    car_settings_reset(&settings);
    CHECK(car_settings_load(&settings, path));
    CHECK(car_settings_saver_start(&saver, &settings));

    /* 首次采样只建立基线：仅改内存不应产生文件 */
    car_settings_saver_acc(&saver, true);
    CHECK(car_settings_set_dark_mode(&settings, 1U));
    {
        FILE *fp = fopen(path, "r");

        CHECK(fp == NULL);
        if(fp != NULL) {
            (void)fclose(fp);
        }
    }
    /* flush 显式保存（正常退出补存路径） */
    CHECK(car_settings_saver_flush(&saver, CAR_SETTINGS_SAVE_BUDGET_MS));
    CHECK(!car_settings_dirty(&settings));

    /* 回到"已保存"后重新制造 dirty，用 ACC 下降沿触发 */
    CHECK(car_settings_set_dark_mode(&settings, 0U));
    car_settings_saver_acc(&saver, true);  /* 保持开启 */
    car_settings_saver_acc(&saver, false); /* 下降沿 -> 提交 */
    car_settings_saver_acc(&saver, false); /* 重复关闭帧不重复提交 */
    CHECK(car_settings_saver_flush(&saver, CAR_SETTINGS_SAVE_BUDGET_MS));
    CHECK(!car_settings_dirty(&settings));

    car_settings_reset(&reloaded);
    CHECK(car_settings_load(&reloaded, path));
    CHECK(reloaded.values.dark_mode == 0U);

    /* 窗口内重新开启不取消已提交的保存 */
    CHECK(car_settings_set_center_card_mask(&settings, 0x7U));
    car_settings_saver_acc(&saver, true);
    car_settings_saver_acc(&saver, false);
    car_settings_saver_acc(&saver, true);
    CHECK(car_settings_saver_flush(&saver, CAR_SETTINGS_SAVE_BUDGET_MS));
    CHECK(!car_settings_dirty(&settings));

    car_settings_reset(&reloaded);
    CHECK(car_settings_load(&reloaded, path));
    CHECK(reloaded.values.center_card_mask == 0x7U);

    car_settings_saver_stop(&saver);
    (void)unlink(path);
}

static void test_saver_first_acc_off(void)
{
    car_settings_t settings;
    car_settings_saver_t saver;
    car_settings_t reloaded;
    char path[200];

    make_path(path, sizeof(path), "setting_first_off.cfg");
    (void)unlink(path);

    car_settings_reset(&settings);
    CHECK(car_settings_load(&settings, path));
    CHECK(car_settings_saver_start(&saver, &settings));

    CHECK(car_settings_set_unit(&settings, 1U, settings.values.unit_temperature));
    CHECK(car_settings_saver_pending(&saver));

    /* 上电首帧就是 ACC 关闭（没见过开启）：内存里的修改仍要落盘 */
    car_settings_saver_acc(&saver, false);
    CHECK(car_settings_saver_flush(&saver, CAR_SETTINGS_SAVE_BUDGET_MS));
    CHECK(!car_settings_dirty(&settings));
    CHECK(!car_settings_saver_pending(&saver));

    car_settings_reset(&reloaded);
    CHECK(car_settings_load(&reloaded, path));
    CHECK(reloaded.values.unit_mileage == 1U);

    car_settings_saver_stop(&saver);
    (void)unlink(path);
}

static void test_save_creates_missing_dir(void)
{
    car_settings_t settings;
    car_settings_t reloaded;
    char path[200];
    char dir[200];

    /* 目标目录不存在（首次启动 /opt/car_dashboard 未创建）时应自动建目录 */
    (void)snprintf(dir, sizeof(dir), "%s/newdir", s_dir);
    (void)snprintf(path, sizeof(path), "%s/setting.cfg", dir);
    (void)rmdir(dir);
    (void)unlink(path);

    car_settings_reset(&settings);
    CHECK(car_settings_load(&settings, path));
    CHECK(car_settings_set_unit(&settings, 1U, settings.values.unit_temperature));

    {
        car_settings_t snapshot;
        uint64_t version = car_settings_snapshot(&settings, &snapshot);

        CHECK(version != 0U);
        CHECK(car_settings_save(&snapshot, version, &settings));
    }

    car_settings_reset(&reloaded);
    CHECK(car_settings_load(&reloaded, path));
    CHECK(reloaded.values.unit_mileage == 1U);

    (void)unlink(path);
    (void)rmdir(dir);
}

/* ECO 历史：Qt updateHistory 满 4 条丢最旧再追加；resetAllData 全清零 */
static void test_eco_history(void)
{
    car_settings_t settings;
    car_settings_t snapshot;
    car_settings_t reloaded;
    char path[200];
    uint64_t version;

    make_path(path, sizeof(path), "setting_eco.cfg");
    (void)unlink(path);

    g_notify_mask = 0U;
    g_notify_calls = 0;

    car_settings_reset(&settings);
    CHECK(car_settings_load(&settings, path));
    car_settings_set_notify(&settings, on_changed, NULL);

    /* 越界（> 30.0 l/100km 量程）直接拒绝，不通知 */
    CHECK(!car_settings_eco_history_push(&settings, 301U));
    CHECK(g_notify_calls == 0);

    /* 同值（与最新一条相同）不产生修改 */
    CHECK(!car_settings_eco_history_push(&settings, 92U));
    CHECK(g_notify_calls == 0);
    CHECK(!car_settings_dirty(&settings));
    CHECK(settings.version == 0U);

    /* 推入 12.0：最旧 9.0 被丢弃，其余左移，新值落到末尾 */
    CHECK(car_settings_eco_history_push(&settings, 120U));
    CHECK(g_notify_calls == 1);
    CHECK(g_notify_mask == CAR_SETTINGS_CHANGED_ECO_HISTORY);
    CHECK(settings.values.eco_history_x10[0] == 105U);
    CHECK(settings.values.eco_history_x10[1] == 80U);
    CHECK(settings.values.eco_history_x10[2] == 92U);
    CHECK(settings.values.eco_history_x10[3] == 120U);
    CHECK(settings.version == 1U);

    /* 落盘再读回 4 条历史 */
    version = car_settings_snapshot(&settings, &snapshot);
    CHECK(car_settings_save(&snapshot, version, &settings));
    CHECK(!car_settings_dirty(&settings));

    car_settings_reset(&reloaded);
    CHECK(car_settings_load(&reloaded, path));
    CHECK(reloaded.values.eco_history_x10[0] == 105U);
    CHECK(reloaded.values.eco_history_x10[1] == 80U);
    CHECK(reloaded.values.eco_history_x10[2] == 92U);
    CHECK(reloaded.values.eco_history_x10[3] == 120U);
    CHECK(!car_settings_dirty(&reloaded));

    /* resetAllData：4 条全部清零；已全 0 时再 reset 返回 false */
    CHECK(car_settings_eco_history_reset(&settings));
    CHECK(g_notify_mask == CAR_SETTINGS_CHANGED_ECO_HISTORY);
    CHECK(settings.values.eco_history_x10[0] == 0U);
    CHECK(settings.values.eco_history_x10[1] == 0U);
    CHECK(settings.values.eco_history_x10[2] == 0U);
    CHECK(settings.values.eco_history_x10[3] == 0U);
    CHECK(!car_settings_eco_history_reset(&settings));
    CHECK(!car_settings_eco_history_reset(NULL));
    CHECK(!car_settings_eco_history_push(NULL, 10U));

    /* 空历史下推入一条：旧 0 依次左移，最新值在末尾 */
    CHECK(car_settings_eco_history_push(&settings, 55U));
    CHECK(settings.values.eco_history_x10[3] == 55U);
    CHECK(settings.values.eco_history_x10[0] == 0U);

    (void)unlink(path);
}

int main(void)
{
    (void)snprintf(s_dir, sizeof(s_dir), "/tmp/carmw_settings_test_XXXXXX");
    if(mkdtemp(s_dir) == NULL) {
        fprintf(stderr, "mkdtemp failed\n");
        return 1;
    }

    test_defaults();
    test_load_missing_file();
    test_invalid_values_fall_back();
    test_dirty_and_notify();
    test_save_and_reload();
    test_newer_change_after_snapshot();
    test_save_failure_keeps_dirty();
    test_saver_acc_edge();
    test_saver_first_acc_off();
    test_save_creates_missing_dir();
    test_eco_history();

    remove_dir_files();
    (void)rmdir(s_dir);

    printf("settings: %d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
