/**
 * @file   sem.c
 * @brief  Kernel counting semaphores: sem_init(), up() (V), down() (P) and try_down().
 *
 * The real work is done by __up() and __down(), which take the wait state as a parameter so that
 * other kinds of wait could reuse them; the public functions pass WT_KSEM. Interrupts are disabled
 * around every access to value and to the wait queue, which is enough to make the operations
 * atomic on this single-CPU kernel. A woken process is only made runnable by wakeup_proc(): it
 * continues inside __down() after schedule() returns, and removes its own wait entry if the waker
 * has not done so.
 */
#include <defs.h>
#include <wait.h>
#include <atomic.h>
#include <kmalloc.h>
#include <sem.h>
#include <proc.h>
#include <sync.h>
#include <assert.h>
#include <deadlock.h>

/**
 * @brief Initialise a semaphore.
 * @param sem   semaphore to initialise
 * @param value initial number of units; 1 for a mutex, 0 for an event
 */
void sem_init(semaphore_t *sem, int value) {
    sem->value = value;
    sem->is_mutex = (value == 1);
    sem->owner = 0;
    wait_queue_init(&(sem->wait_queue));
}

/**
 * @brief Implementation of the V operation.
 *
 * With interrupts off: if nobody sleeps on the queue, value is incremented. Otherwise the first
 * waiter is woken up and removed from the queue, and value is left alone, because the unit
 * goes straight to that waiter.
 *
 * @param sem        semaphore to release
 * @param wait_state wait state the sleeping process must have (asserted)
 */
static __noinline void __up(semaphore_t *sem, uint32_t wait_state) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        wait_t *wait;
        deadlock_on_release(sem);
        if ((wait = wait_queue_first(&(sem->wait_queue))) == NULL) {
            sem->value++;
        } else {
            assert(wait->proc->wait_state == wait_state);
            wakeup_wait(&(sem->wait_queue), wait, wait_state, 1);
        }
    }
    local_intr_restore(intr_flag);
}

/**
 * @brief Implementation of the P operation.
 *
 * With interrupts off: if value is positive it is decremented and the function returns 0. Otherwise
 * the current process is put on the wait queue (wait_current_set()), interrupts are enabled again
 * and schedule() gives up the CPU. After the process has been woken up, it removes its wait entry
 * if the waker did not (wait_current_del()) and reports why it was woken.
 *
 * @param sem        semaphore to acquire
 * @param wait_state wait state to sleep with
 * @return 0 for a normal wake-up (an up() gave a unit to this process), otherwise the wakeup_flags
 *         set by whoever interrupted the wait
 */
static __noinline uint32_t __down(semaphore_t *sem, uint32_t wait_state) {
    bool intr_flag;
    local_intr_save(intr_flag);
    if (sem->value > 0) {
        sem->value--;
        deadlock_on_acquire(sem);
        local_intr_restore(intr_flag);
        return 0;
    }
    if (deadlock_on_wait(sem)) {
        local_intr_restore(intr_flag);
        return 0;
    }
    wait_t __wait, *wait = &__wait;
    wait_current_set(&(sem->wait_queue), wait, wait_state);
    local_intr_restore(intr_flag);

    schedule();

    local_intr_save(intr_flag);
    wait_current_del(&(sem->wait_queue), wait);
    local_intr_restore(intr_flag);

    if (wait->wakeup_flags != wait_state) {
        return wait->wakeup_flags;
    }
    return 0;
}

/**
 * @brief The V operation: release one unit (see __up()).
 * @param sem semaphore to release
 */
void up(semaphore_t *sem) {
    __up(sem, WT_KSEM);
}

/**
 * @brief The P operation: take one unit, sleeping until one is available (see __down()).
 * @param sem semaphore to acquire
 * @note The wait of a semaphore is not interruptible (WT_KSEM has no WT_INTERRUPTED bit), so the
 *       assert on the flags only fails if something broke that rule.
 */
void down(semaphore_t *sem) {
    uint32_t flags = __down(sem, WT_KSEM);
    assert(flags == 0);
}

/**
 * @brief Try to take one unit without sleeping.
 * @param sem semaphore to acquire
 * @return true if value was positive and has been decremented, false otherwise
 */
bool try_down(semaphore_t *sem) {
    bool intr_flag, ret = 0;
    local_intr_save(intr_flag);
    if (sem->value > 0) {
        sem->value--;
        ret = 1;
        deadlock_on_acquire(sem);
    }
    local_intr_restore(intr_flag);
    return ret;
}
