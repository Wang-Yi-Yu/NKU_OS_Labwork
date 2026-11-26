#ifndef __KERN_SYNC_DEADLOCK_H__
#define __KERN_SYNC_DEADLOCK_H__

#include <sem.h>

/**
 * @brief Record that the current process has taken one unit of a mutex.
 *
 * Called with interrupts disabled, after value has been decremented. Semaphores that are not
 * mutexes are ignored. A mutex with no owner becomes owned by current. Taking a mutex this
 * process already owns, or one owned by somebody else, is reported and does not change owner.
 *
 * @param sem semaphore that was just acquired
 */
void deadlock_on_acquire(semaphore_t *sem);

/**
 * @brief Record that the current process is about to sleep on a mutex, and look for a cycle.
 *
 * Called with interrupts disabled, before the process is queued. A return of 1 means the caller
 * must not sleep: the wait was a re-entry, a cycle, or the edge table is full. A return of 0
 * means the caller should sleep as usual. A wait edge is kept only in that last case.
 *
 * @param sem semaphore whose value is already 0
 * @return 1 if the wait must be skipped, 0 if the process should sleep
 */
int deadlock_on_wait(semaphore_t *sem);

/**
 * @brief Drop ownership of a mutex the current process is releasing.
 *
 * Called with interrupts disabled, before up() wakes a waiter or increments value. Only a mutex
 * whose owner is current is cleared. Wait edges that name this process as the holder of @p sem
 * are removed with the ownership.
 *
 * @param sem semaphore that is about to be released
 */
void deadlock_on_release(semaphore_t *sem);

/**
 * @brief Run the re-entry and circular-wait self-test.
 *
 * The test replaces the report hook so a detection does not enter the kernel monitor, then
 * restores it. Threads it creates are reaped before it returns. Called from init_main() before
 * check_sync().
 */
void deadlock_check(void);

#endif /* !__KERN_SYNC_DEADLOCK_H__ */
