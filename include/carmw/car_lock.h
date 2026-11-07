/*
 * car_lock.h - 极简互斥封装
 *
 * 目的：核心（parser / decode / state）不绑定 pthread。
 *   - 默认（POSIX / Linux）：pthread_mutex_t
 *   - 定义 CARMW_NO_THREADS：空实现，用于单线程 MCU 移植或纯离线测试
 *
 * 只有 car_link 的"独立线程模式"和 car_state_store 真正需要它；
 * 单线程协作模式（在 LVGL 定时器里调 car_link_pump）不需要任何锁。
 */
#ifndef CARMW_CAR_LOCK_H
#define CARMW_CAR_LOCK_H

/*
 * 没有 pthread 的平台（Windows/MSVC、部分 RTOS）自动退化为单线程模式：
 * 核心库照常编译，只是 car_link 只支持协作式 pump。
 * 想显式控制就自己 -DCARMW_NO_THREADS。
 */
#if defined(_WIN32) && !defined(CARMW_NO_THREADS)
#define CARMW_NO_THREADS 1
#endif

#ifdef CARMW_NO_THREADS

typedef struct {
    int unused;
} car_lock_t;

#define car_lock_init(lock) ((void)(lock))
#define car_lock_destroy(lock) ((void)(lock))
#define car_lock_acquire(lock) ((void)(lock))//获取锁
#define car_lock_release(lock) ((void)(lock))//释放锁

#else /* !CARMW_NO_THREADS */

#include <pthread.h>

typedef pthread_mutex_t car_lock_t;

#define car_lock_init(lock) ((void)pthread_mutex_init((lock), NULL))
#define car_lock_destroy(lock) ((void)pthread_mutex_destroy((lock)))
#define car_lock_acquire(lock) ((void)pthread_mutex_lock((lock)))
#define car_lock_release(lock) ((void)pthread_mutex_unlock((lock)))

#endif /* CARMW_NO_THREADS */

#endif /* CARMW_CAR_LOCK_H */
