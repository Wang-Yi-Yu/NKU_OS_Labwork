/**
 * @file   lock.h
 * @brief  Spin lock for user programs, built on an atomic bit operation and yield() (lab8).
 *
 * A lock_t is a flag that is 1 while the lock is held. try_lock() sets the flag atomically and
 * reports whether it was already set. lock() retries until it succeeds and gives the CPU away
 * between two attempts, so a waiting program does not keep the processor busy. The lock is only
 * used inside user programs; the kernel has its own locks in kern/sync.
 * @note The atomic operations act on the whole unsigned long at the address of the lock (see
 *       libs/atomic.h), so a lock_t should occupy an aligned word of its own.
 */

#ifndef __USER_LIBS_LOCK_H__
#define __USER_LIBS_LOCK_H__

#include <defs.h>
#include <atomic.h>
#include <ulib.h>

/** @brief Initializer for a lock_t that is free (flag cleared). */
#define INIT_LOCK \
    { 0 }

/**
 * @brief A user-level spin lock: 1 while held, 0 while free.
 */
typedef volatile bool lock_t;

/**
 * @brief Set a lock to the free state.
 * @param[out] l  lock to initialise
 */
static inline void lock_init(lock_t *l) {
    *l = 0;
}

/**
 * @brief Try to take the lock once, without waiting.
 *
 * The flag is set atomically with test_and_set_bit(). Despite its name, the function returns
 * true when the attempt FAILED, because the lock was already held.
 * @param[in,out] l  lock to take
 * @return true if the lock was already held (not acquired), false if it was free and is now held
 */
static inline bool try_lock(lock_t *l) {
    return test_and_set_bit(0, l);
}

/**
 * @brief Take the lock, waiting until it is free.
 *
 * While the lock is held, the caller calls yield() between attempts. After every 100 failed
 * attempts it also sleeps for 10 timer ticks (about 100 ms), so that a long wait does not keep
 * the CPU busy.
 * @param[in,out] l  lock to take; the caller holds it when the function returns
 */
static inline void lock(lock_t *l) {
    if (try_lock(l)) {
        int step = 0;
        do {
            yield();
            if (++step == 100) {
                step = 0;
                sleep(10);
            }
        } while (try_lock(l));
    }
}

/**
 * @brief Release a lock taken with lock() or try_lock().
 *
 * The flag is cleared atomically with test_and_clear_bit(). Releasing a lock that is not held is
 * not detected.
 * @param[in,out] l  lock to release
 */
static inline void unlock(lock_t *l) {
    test_and_clear_bit(0, l);
}

#endif /* !__USER_LIBS_LOCK_H__ */
