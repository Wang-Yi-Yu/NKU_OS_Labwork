/**
 * @file   sync.h
 * @brief  Interrupt save/restore helpers that protect short critical sections on a single hart.
 *
 * Typical use, as in alloc_pages():
 * @code
 * bool intr_flag;
 * local_intr_save(intr_flag);
 * {
 *     // critical section: no interrupt can run here
 * }
 * local_intr_restore(intr_flag);
 * @endcode
 * The flag remembers whether interrupts were enabled, so nested sections do not enable
 * interrupts too early.
 */
#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include <defs.h>
#include <intr.h>
#include <sched.h>
#include <riscv.h>
#include <assert.h>

/**
 * @brief Disable interrupts if they are enabled.
 * @return 1 if sstatus.SIE was set (interrupts were on and are now off), 0 otherwise
 */
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();
        return 1;
    }
    return 0;
}

/**
 * @brief Re-enable interrupts if __intr_save() had turned them off.
 * @param flag value returned by the matching __intr_save()
 */
static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}

/**
 * @brief Disable interrupts and store the old state in @p x (an lvalue of type bool).
 *
 * The do { } while (0) wrapper makes the macro behave like a single statement, so it is safe
 * after an unbraced `if` and still needs a trailing semicolon.
 */
#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
/** @brief Restore the interrupt state saved by local_intr_save(). */
#define local_intr_restore(x) __intr_restore(x);

/** @brief Simple lock word: bit 0 is 1 while some context holds the lock. */
typedef volatile bool lock_t;

/**
 * @brief Put a lock into the unlocked state.
 * @param lock lock to initialise
 */
static inline void lock_init(lock_t *lock) {
    *lock = 0;
}

/**
 * @brief Try to take the lock once without waiting.
 * @param lock lock to take
 * @return true if the lock was free and is now held by the caller, false if it was already held
 */
static inline bool try_lock(lock_t *lock) {
    return !test_and_set_bit(0, lock);
}

/**
 * @brief Take the lock; while it is held by someone else, yield the CPU with schedule() and retry.
 * @param lock lock to take
 * @note Intended for a single hart: the holder can only run again because schedule() lets it.
 */
static inline void lock(lock_t *lock) {
    while (!try_lock(lock)) {
        schedule();
    }
}

/**
 * @brief Release the lock.
 * @param lock lock to release
 * @note Panics if the lock was not held ("Unlock failed.").
 */
static inline void unlock(lock_t *lock) {
    if (!test_and_clear_bit(0, lock)) {
        panic("Unlock failed.\n");
    }
}

#endif /* !__KERN_SYNC_SYNC_H__ */
