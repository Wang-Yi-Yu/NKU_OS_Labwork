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
 *
 * This is the only primitive left in this file. The lab5/lab6 spin lock (lock_t, lock(), unlock())
 * was removed in lab7: on a kernel that cannot be preempted it is useless, and sleeping locks are
 * provided by semaphores (kern/sync/sem.h) instead.
 */
#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include <defs.h>
#include <intr.h>
#include <sched.h>
#include <riscv.h>
#include <assert.h>
#include <atomic.h>

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

#endif /* !__KERN_SYNC_SYNC_H__ */
