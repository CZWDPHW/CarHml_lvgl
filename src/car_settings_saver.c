/*
 * car_settings_saver.c - 见 car_settings_saver.h
 *
 * 后台线程只做一件事：有保存请求就取快照落盘，成功后按版本号清除 dirty，
 * 若期间又产生修改则再存一轮。ACC 下降沿只负责把当前版本提交上来。
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "carmw/car_settings_saver.h"

#include <errno.h>
#include <stdio.h>
#include <time.h>

#ifndef CARMW_NO_THREADS

static void add_ms(struct timespec *ts, uint32_t ms)
{
    ts->tv_sec += (time_t)(ms / 1000U);
    ts->tv_nsec += (long)(ms % 1000U) * 1000000L;
    if(ts->tv_nsec >= 1000000000L) {
        ts->tv_nsec -= 1000000000L;
        ts->tv_sec += 1;
    }
}

static void *saver_thread(void *arg)
{
    car_settings_saver_t *saver = (car_settings_saver_t *)arg;

    for(;;) {
        car_settings_t snapshot;
        uint64_t target = 0U;
        bool stop;
        bool ok = false;

        (void)pthread_mutex_lock(&saver->lock);
        while(saver->requested_version == 0U && !saver->stop) {
            (void)pthread_cond_wait(&saver->cond, &saver->lock);
        }
        stop = saver->stop;
        target = saver->requested_version;
        if(target != 0U) {
            saver->requested_version = 0U;
            /* 快照在锁内取，保证与 version 一致；文件 IO 仍在锁外 */
            if(!car_settings_snapshot(saver->settings, &snapshot)) {
                target = 0U;
            }
        }
        (void)pthread_mutex_unlock(&saver->lock);

        if(target == 0U) {
            break; /* 只剩 stop 请求且无待保存内容 */
        }

        ok = car_settings_save(&snapshot, target, saver->settings);

        (void)pthread_mutex_lock(&saver->lock);
        if(ok) {
            if(saver->done_version < target) {
                saver->done_version = target;
            }
            ++saver->saved_count;
        }
        else {
            saver->failed_version = target;
            ++saver->failed_count;
            fprintf(stderr, "settings: save failed (version %llu)\n",
                    (unsigned long long)target);
        }
        (void)pthread_cond_broadcast(&saver->cond);
        (void)pthread_mutex_unlock(&saver->lock);

        if(stop) {
            break;
        }
    }

    return NULL;
}

/* 提交当前版本（无 dirty 时什么也不做） */
static void request_current(car_settings_saver_t *saver)
{
    uint64_t version;

    if(saver == NULL || saver->settings == NULL) {
        return;
    }

    if(!car_settings_dirty(saver->settings)) {
        return;
    }

    version = saver->settings->version;

    (void)pthread_mutex_lock(&saver->lock);
    if(version > saver->requested_version) {
        saver->requested_version = version;
    }
    (void)pthread_cond_broadcast(&saver->cond);
    (void)pthread_mutex_unlock(&saver->lock);
}

static bool wait_done(car_settings_saver_t *saver, uint32_t timeout_ms)
{
    struct timespec deadline;
    bool saved;

    (void)clock_gettime(CLOCK_REALTIME, &deadline);
    add_ms(&deadline, timeout_ms);

    (void)pthread_mutex_lock(&saver->lock);
    for(;;) {
        /* 没有任何待保存版本，且最近一次失败不是当前版本，即视为完成 */
        if(saver->requested_version == 0U) {
            uint64_t version = saver->settings->version;

            if(saver->failed_version == version && saver->done_version < version) {
                saved = false;
                break;
            }
            saved = saver->done_version >= version;
            break;
        }

        if(pthread_cond_timedwait(&saver->cond, &saver->lock, &deadline) ==
           ETIMEDOUT) {
            saved = saver->done_version >= saver->settings->version;
            break;
        }
    }
    (void)pthread_mutex_unlock(&saver->lock);

    return saved;
}

bool car_settings_saver_start(car_settings_saver_t *saver,
                              car_settings_t *settings)
{
    if(saver == NULL || settings == NULL) {
        return false;
    }

    saver->settings = settings;
    saver->requested_version = 0U;
    saver->done_version = settings->saved_version;
    saver->failed_version = 0U;
    saver->saved_count = 0U;
    saver->failed_count = 0U;
    saver->seen = false;
    saver->last_on = false;
    saver->ever_on = false;
    saver->stop = false;
    saver->started = false;

    if(pthread_mutex_init(&saver->lock, NULL) != 0) {
        return false;
    }
    if(pthread_cond_init(&saver->cond, NULL) != 0) {
        (void)pthread_mutex_destroy(&saver->lock);
        return false;
    }
    if(pthread_create(&saver->thread, NULL, saver_thread, saver) != 0) {
        (void)pthread_cond_destroy(&saver->cond);
        (void)pthread_mutex_destroy(&saver->lock);
        return false;
    }

    saver->started = true;
    return true;
}

void car_settings_saver_acc(car_settings_saver_t *saver, bool acc_on)
{
    if(saver == NULL) {
        return;
    }

    /*
     * 首次收到 ACC 帧只建立基线，避免上电首帧被误判成关闭；
     * 但若首帧就是关闭（或一直没见过开启），说明已经处于关机流程，
     * 内存里的修改仍需写入，否则本次改动会随断电丢失。
     */
    if(!saver->seen) {
        saver->seen = true;
        saver->last_on = acc_on;
        if(!acc_on) {
            request_current(saver);
        }
        return;
    }

    /* 下降沿触发；若之前从未处于开启态，任何一帧关闭也提交（同上） */
    if(!acc_on && (saver->last_on || !saver->ever_on)) {
        request_current(saver);
    }
    if(acc_on) {
        saver->ever_on = true;
    }
    saver->last_on = acc_on;
}

bool car_settings_saver_pending(car_settings_saver_t *saver)
{
    bool pending;

    if(saver == NULL || saver->settings == NULL) {
        return false;
    }

    (void)pthread_mutex_lock(&saver->lock);
    pending = (saver->requested_version != 0U) ||
              (saver->done_version < saver->settings->version);
    (void)pthread_mutex_unlock(&saver->lock);

    return pending;
}

bool car_settings_saver_flush(car_settings_saver_t *saver, uint32_t timeout_ms)
{
    if(saver == NULL || !saver->started) {
        return false;
    }

    request_current(saver);
    return wait_done(saver, timeout_ms);
}

void car_settings_saver_stop(car_settings_saver_t *saver)
{
    if(saver == NULL || !saver->started) {
        return;
    }

    request_current(saver);

    (void)pthread_mutex_lock(&saver->lock);
    saver->stop = true;
    (void)pthread_cond_broadcast(&saver->cond);
    (void)pthread_mutex_unlock(&saver->lock);

    (void)pthread_join(saver->thread, NULL);
    (void)pthread_cond_destroy(&saver->cond);
    (void)pthread_mutex_destroy(&saver->lock);
    saver->started = false;
}

#else /* CARMW_NO_THREADS：无线程构建下保存器不可用 */

bool car_settings_saver_pending(car_settings_saver_t *saver)
{
    (void)saver;
    return false;
}

#endif /* CARMW_NO_THREADS */
