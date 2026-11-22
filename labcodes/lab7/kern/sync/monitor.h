/**
 * @file   monitor.h
 * @brief  Monitor and condition variables built on kernel semaphores (lab7).
 *
 * A monitor makes a group of routines mutually exclusive: at most one process is active inside it.
 * Condition variables let a process that is inside the monitor wait until some condition holds,
 * without blocking the others. This file defines the data structures, and declares the four
 * operations. monitor_init() and monitor_free() are provided; cond_wait() and cond_signal() are the
 * lab7 exercise (kern/sync/monitor.c, marker `lab7 function1`). The comment block below, written
 * by the TA, describes the idea, the data structures and an implementation.
 */
#ifndef __KERN_SYNC_MONITOR_CONDVAR_H__
#define __KERN_SYNC_MONITOR_CONDVAR_H__

#include <sem.h>

/* In [OS CONCEPT] 7.7 section, the accurate define and approximate implementation of MONITOR was introduced.
 * INTRODUCTION:
 *  Monitors were invented by C. A. R. Hoare and Per Brinch Hansen, and were first implemented in Brinch Hansen's
 *  Concurrent Pascal language. Generally, a monitor is a language construct and the compiler usually enforces mutual exclusion. Compare this with semaphores, which are usually an OS construct.
 * DEFNIE & CHARACTERISTIC:
 *  A monitor is a collection of procedures, variables, and data structures grouped together.
 *  Processes can call the monitor procedures but cannot access the internal data structures.
 *  Only one process at a time may be be active in a monitor.
 *  Condition variables allow for blocking and unblocking.
 *     cv.wait() blocks a process.
 *        The process is said to be waiting for (or waiting on) the condition variable cv.
 *     cv.signal() (also called cv.notify) unblocks a process waiting for the condition variable cv.
 *        When this occurs, we need to still require that only one process is active in the monitor. This can be done in several ways:
 *            on some systems the old process (the one executing the signal) leaves the monitor and the new one enters
 *            on some systems the signal must be the last statement executed inside the monitor.
 *            on some systems the old process will block until the monitor is available again.
 *            on some systems the new process (the one unblocked by the signal) will remain blocked until the monitor is available again.
 *   If a condition variable is signaled with nobody waiting, the signal is lost. Compare this with semaphores, in which a signal will allow a process that executes a wait in the future to no block.
 *   You should not think of a condition variable as a variable in the traditional sense.
 *     It does not have a value.
 *     Think of it as an object in the OOP sense.
 *     It has two methods, wait and signal that manipulate the calling process.
 * IMPLEMENTATION:
 *   monitor mt {
 *     ----------------variable------------------
 *     semaphore mutex;
 *     semaphore next;
 *     int next_count;
 *     condvar {int count, sempahore sem}  cv[N];
 *     other variables in mt;
 *     --------condvar wait/signal---------------
 *     cond_wait (cv) {
 *         cv.count ++;
 *         if(mt.next_count>0)
 *            signal(mt.next)
 *         else
 *            signal(mt.mutex);
 *         wait(cv.sem);
 *         cv.count --;
 *      }
 *
 *      cond_signal(cv) {
 *          if(cv.count>0) {
 *             mt.next_count ++;
 *             signal(cv.sem);
 *             wait(mt.next);
 *             mt.next_count--;
 *          }
 *       }
 *     --------routines in monitor---------------
 *     routineA_in_mt () {
 *        wait(mt.mutex);
 *        ...
 *        real body of routineA
 *        ...
 *        if(next_count>0)
 *            signal(mt.next);
 *        else
 *            signal(mt.mutex);
 *     }
 */

/**
 * @brief Forward declaration: condvar_t refers to monitor_t before it is defined.
 */
typedef struct monitor monitor_t;

/**
 * @brief A condition variable: a place for processes inside a monitor to wait for a condition.
 *
 * A condition variable has no value. It only remembers who waits.
 * @var condvar::sem   semaphore (initial value 0) that the waiting processes sleep on; the signaling
 *                     process ups it to wake one of them
 * @var condvar::count number of processes currently waiting on this condition variable
 * @var condvar::owner the monitor that this condition variable belongs to
 */
typedef struct condvar {
    semaphore_t
        sem;  // the sem semaphore  is used to down the waiting proc, and the signaling proc should up the waiting proc
    int count;         // the number of waiters on condvar
    monitor_t *owner;  // the owner(monitor) of this condvar
} condvar_t;

/**
 * @brief A monitor: the mutual exclusion lock of the monitor, the queue of suspended signalers and
 *        its condition variables.
 * @var monitor::mutex      binary semaphore (initial value 1) taken by a process on entering the monitor
 *                          and released on leaving it
 * @var monitor::next       semaphore (initial value 0) on which a signaling process sleeps while the process
 *                          it woke up is inside the monitor; the monitor is handed back by upping it
 * @var monitor::next_count number of processes sleeping on next
 * @var monitor::cv         array of condition variables, allocated by monitor_init()
 */
typedef struct monitor {
    semaphore_t
        mutex;  // the mutex lock for going into the routines in monitor, should be initialized to 1
    semaphore_t
        next;  // the next semaphore is used to down the signaling proc itself, and the other OR wakeuped waiting proc should wake up the sleeped signaling proc.
    int next_count;  // the number of of sleeped signaling proc
    condvar_t *cv;   // the condvars in monitor
} monitor_t;

/**
 * @brief Initialise a monitor and allocate its condition variables.
 *
 * The monitor starts unlocked (mutex 1), with no suspended signaler (next 0, next_count 0). Every
 * condition variable starts with no waiter (count 0, sem 0) and points back to the monitor.
 * @param cvp    monitor to initialise
 * @param num_cv number of condition variables to create; must be greater than 0
 * @note The array is allocated with kmalloc(), so it must be released with monitor_free().
 */
void monitor_init(monitor_t *cvp, size_t num_cv);
/**
 * @brief Release the condition variables allocated by monitor_init().
 * @param cvp    monitor to free
 * @param num_cv number of condition variables given to monitor_init() (not used by the framework version)
 * @note No process may be waiting in the monitor any more.
 */
void monitor_free(monitor_t *cvp, size_t num_cv);
/**
 * @brief Wake up one process waiting on a condition variable (lab7 exercise, monitor.c).
 *
 * Called by a process that is inside the monitor. If nobody waits on @p cvp, the system state is
 * left unchanged (the signal is lost). Otherwise at most one waiting process is woken up, with the
 * monitor semantics described at the top of this file.
 * @param cvp condition variable to signal
 */
void cond_signal(condvar_t *cvp);
/**
 * @brief Wait on a condition variable until another process signals it (lab7 exercise, monitor.c).
 *
 * Called by a process that is inside the monitor but cannot go on for now. The process stops
 * occupying the monitor, so that others can enter, and sleeps on @p cvp. After it has been woken up
 * by cond_signal() it gets the monitor back and the function returns. No busy waiting.
 * @param cvp condition variable to wait on
 */
void cond_wait(condvar_t *cvp);

#endif /* !__KERN_SYNC_MONITOR_CONDVAR_H__ */
