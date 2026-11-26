/**
 * @file   sem.h
 * @brief  Kernel counting semaphores, built on wait queues.
 *
 * A semaphore holds a counter and a wait queue. down() (the P operation) takes one unit and sleeps
 * if none is left; up() (the V operation) gives one unit back or wakes up the process that has
 * waited longest. A semaphore with initial value 1 is a mutex, one with value 0 is used to wait for
 * an event. The monitor and condition variables of monitor.h are made of semaphores.
 */
#ifndef __KERN_SYNC_SEM_H__
#define __KERN_SYNC_SEM_H__

#include <defs.h>
#include <atomic.h>
#include <wait.h>

/**
 * @brief A counting semaphore.
 * @var semaphore_t::value      number of units that can be taken without sleeping. It never goes below 0
 *                              in this implementation: the sleeping processes are counted by wait_queue,
 *                              not by a negative value
 * @var semaphore_t::wait_queue processes sleeping in down(), in arrival order
 * @var semaphore_t::is_mutex   1 if sem_init was given 1 (a mutex tracked by the deadlock detector)
 * @var semaphore_t::owner      pid of the process that holds a mutex, or 0 if nobody holds it
 */
typedef struct {
    int value;
    wait_queue_t wait_queue;
    int is_mutex;
    int owner;
} semaphore_t;

/**
 * @brief Initialise a semaphore.
 * @param sem   semaphore to initialise
 * @param value initial number of units; 1 for a mutex, 0 for an event
 */
void sem_init(semaphore_t *sem, int value);
/**
 * @brief The V operation: release one unit.
 *
 * If a process is sleeping in down() on this semaphore, the first one is woken up and takes the
 * unit directly (value is not incremented); otherwise value is incremented. Never blocks.
 * @param sem semaphore to release
 */
void up(semaphore_t *sem);
/**
 * @brief The P operation: take one unit, sleeping until one is available.
 *
 * If value is positive it is decremented and the call returns at once. Otherwise the current
 * process sleeps on the wait queue (state PROC_SLEEPING, wait_state WT_KSEM) and gives up the CPU
 * with schedule(). It returns after an up() has handed a unit to it.
 * @param sem semaphore to acquire
 */
void down(semaphore_t *sem);
/**
 * @brief Try to take one unit without sleeping.
 * @param sem semaphore to acquire
 * @return true if a unit was taken, false if none was available
 */
bool try_down(semaphore_t *sem);

#endif /* !__KERN_SYNC_SEM_H__ */
