/**
 * @file   monitor.c
 * @brief  Monitor and condition variables (lab7): monitor_init() and monitor_free() are provided;
 *         cond_wait() and cond_signal() are the exercise of this file (marker `lab7 function1`
 *         at the end).
 *
 * The data structures and the meaning of their fields are described in monitor.h. cond_wait() and
 * cond_signal() use only the semaphores of the monitor (mutex, next) and of the condition variable
 * (sem), and the counters next_count and count. The monitor is of the Hoare kind: the signaling
 * process hands the monitor to the process it wakes up and sleeps on next until it gets it back.
 */
#include <stdio.h>
#include <monitor.h>
#include <kmalloc.h>
#include <assert.h>

/**
 * @brief Initialise a monitor and allocate its condition variables.
 *
 * Sets next_count to 0, mutex to 1 (unlocked) and next to 0, allocates @p num_cv condition
 * variables with kmalloc() and initialises each of them: count 0, sem 0, owner pointing back to the
 * monitor.
 *
 * @param mtp    monitor to initialise
 * @param num_cv number of condition variables; must be greater than 0 (asserted)
 * @note The assert on the kmalloc() result panics on out of memory.
 */
void monitor_init(monitor_t *mtp, size_t num_cv) {
    int i;
    assert(num_cv > 0);
    mtp->next_count = 0;
    mtp->cv = NULL;
    sem_init(&(mtp->mutex), 1);  // unlocked
    sem_init(&(mtp->next), 0);
    mtp->cv = (condvar_t *)kmalloc(sizeof(condvar_t) * num_cv);
    assert(mtp->cv != NULL);
    for (i = 0; i < num_cv; i++) {
        mtp->cv[i].count = 0;
        sem_init(&(mtp->cv[i].sem), 0);
        mtp->cv[i].owner = mtp;
    }
}

/**
 * @brief Release the condition variables of a monitor.
 * @param mtp    monitor to free; mtp->cv must have been allocated by monitor_init()
 * @param num_cv number of condition variables (not needed to free the array, so unused)
 * @note No process may be waiting on the monitor or its condition variables.
 */
void monitor_free(monitor_t *mtp, size_t num_cv) {
    kfree(mtp->cv);
}

//lab7 function1 YOUR CODE

/**
 * @brief Wait on a condition variable until another process signals it.
 *
 * The caller is inside the monitor. It is counted as a waiter (cvp->count), then it gives the
 * monitor away: to a signaler that sleeps on mtp->next if there is one (next_count > 0), otherwise
 * by upping mtp->mutex so that a new process can enter. Then it sleeps on cvp->sem. When
 * cond_signal() wakes it, the signaler has not released the monitor but sleeps on mtp->next, so the
 * woken process owns the monitor on return and does not have to take mtp->mutex again.
 *
 * @param cvp condition variable to wait on; cvp->owner is the monitor the caller is in
 * @note Because the monitor is handed over directly, the condition that was signaled still holds on
 *       return; the caller may use `if` instead of `while` around the call.
 */
void cond_wait(condvar_t *cvp) {
    monitor_t *mtp = cvp->owner;

    cvp->count++;
    if (mtp->next_count > 0) {
        up(&(mtp->next));
    } else {
        up(&(mtp->mutex));
    }

    down(&(cvp->sem));
    cvp->count--;
}

/**
 * @brief Wake up one process waiting on a condition variable and let it run first.
 *
 * The caller is inside the monitor. If nobody waits on @p cvp (cvp->count == 0) nothing changes and
 * the signal is lost. Otherwise the caller is counted as a suspended signaler (mtp->next_count),
 * ups cvp->sem to wake one waiter, and sleeps on mtp->next. The woken process runs inside the
 * monitor; when it leaves the monitor or waits again it ups mtp->next, which wakes the caller, and
 * the caller continues with next_count decremented.
 *
 * @param cvp condition variable to signal; cvp->owner is the monitor the caller is in
 * @note The caller may sleep inside this function, so shared state can change before it returns
 *       if the woken process modified it.
 */
void cond_signal(condvar_t *cvp) {
    monitor_t *mtp = cvp->owner;

    if (cvp->count > 0) {
        mtp->next_count++;
        up(&(cvp->sem));
        down(&(mtp->next));
        mtp->next_count--;
    }
}
