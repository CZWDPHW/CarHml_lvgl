/*
 * car_settings_saver.h - ACC 关闭统一保存设置
 *
 * 约定（docs/UI_REWRITE_HANDOFF.md 14.5）：
 *   - 运行期只改内存设置，不周期写盘；
 *   - 收到 0x34 且 ACC 由开转关时立即提交保存，硬件侧保留 3 秒供电；
 *   - 保存由单一后台线程串行执行，UI 线程只提交快照；
 *   - 窗口内产生的新修改会合并补存一次，靠快照版本号避免误清 dirty。
 *
 * 线程模型：调用 car_settings_saver_acc() 与 car_settings_saver_flush() 都在
 * UI 线程；内部临界区只保护版本号与条件变量，落盘在后台线程完成。
 */
#ifndef CARMW_CAR_SETTINGS_SAVER_H
#define CARMW_CAR_SETTINGS_SAVER_H

#include <stdbool.h>
#include <stdint.h>

#include "carmw/car_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 关闭时等待保存完成的最长时间，必须小于硬件给的 3 秒窗口 */
#define CAR_SETTINGS_SAVE_BUDGET_MS 2000U

#ifndef CARMW_NO_THREADS
#include <pthread.h>

typedef struct {
    car_settings_t *settings;
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    uint64_t requested_version; /* 待保存的目标版本，0 表示无请求 */
    uint64_t done_version;      /* 最近一次成功落盘的版本 */
    uint64_t failed_version;    /* 最近一次失败的版本，仅诊断 */
    uint64_t saved_count;
    uint64_t failed_count;
    bool seen;                  /* 已收到过真实 ACC 帧 */
    bool last_on;               /* 最近一次采样的 ACC 状态 */
    bool ever_on;               /* 运行期是否见过 ACC 开启 */
    bool started;
    bool stop;
} car_settings_saver_t;

/*
 * 启动后台保存线程。失败返回 false（此时 ac 关闭保存不可用，但设置仍在内存）。
 */
bool car_settings_saver_start(car_settings_saver_t *saver,
                              car_settings_t *settings);

/* ACC 采样：首次调用只记录状态；仅 true -> false 触发保存。 */
void car_settings_saver_acc(car_settings_saver_t *saver, bool acc_on);

/*
 * 是否有修改还没落盘（含正在等待/正在写入的版本）。
 * 供界面显示“设置已保存”提示，避免用户在等待 ACC 关闭期间看不出保存状态。
 */
bool car_settings_saver_pending(car_settings_saver_t *saver);

/*
 * 立即请求保存并等待完成（正常退出时用）。没有 dirty 时立即返回 true。
 * timeout_ms 为最长等待时间。
 */
bool car_settings_saver_flush(car_settings_saver_t *saver, uint32_t timeout_ms);

/* 停线程：先补存一次未落盘修改，再退出。 */
void car_settings_saver_stop(car_settings_saver_t *saver);

#endif /* CARMW_NO_THREADS */

#ifdef __cplusplus
}
#endif

#endif /* CARMW_CAR_SETTINGS_SAVER_H */
