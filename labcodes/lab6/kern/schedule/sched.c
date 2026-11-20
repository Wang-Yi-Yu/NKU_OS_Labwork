/**
 * @file   sched.c
 * @brief  Scheduler framework (lab6): binds a scheduling class to the kernel and implements
 *         sched_init(), wakeup_proc() and schedule() on top of the class interface.
 *
 * Compared with lab5 this file no longer contains any scheduling policy. Runnable processes are
 * kept in a run queue managed by the active struct sched_class (RR by default, Stride after the
 * switch in sched_init()). schedule() is called from cpu_idle() whenever need_resched is set and
 * from trap() when a trap from user mode returns with need_resched set; it asks the class for
 * the next process and hands the CPU over with proc_run().
 */
#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <stdio.h>
#include <assert.h>
#include <default_sched.h>

/** @brief List of timers; initialised by sched_init() but not used by lab6. */
static list_entry_t timer_list;

/** @brief The active scheduling class, chosen in sched_init(). */
static struct sched_class *sched_class;

/** @brief The run queue of the single CPU; points to __rq after sched_init(). */
static struct run_queue *rq;

/**
 * @brief Put a process into the run queue through the active class.
 * @param proc process to enqueue
 * @note idleproc is never queued: it runs only when the queue is empty.
 */
static inline void sched_class_enqueue(struct proc_struct *proc) {
    if (proc != idleproc) {
        sched_class->enqueue(rq, proc);
    }
}

/**
 * @brief Remove a process from the run queue through the active class.
 * @param proc process to dequeue; it must be in the queue
 */
static inline void sched_class_dequeue(struct proc_struct *proc) {
    sched_class->dequeue(rq, proc);
}

/**
 * @brief Ask the active class for the next process to run.
 * @return the chosen process (still in the queue), or NULL if the queue is empty
 */
static inline struct proc_struct *sched_class_pick_next(void) {
    return sched_class->pick_next(rq);
}

void sched_class_proc_tick(struct proc_struct *proc) {
    if (proc != idleproc) {
        sched_class->proc_tick(rq, proc);
    } else {
        // The idle thread has no time slice: ask for a reschedule on every tick so that a newly
        // runnable process is picked up quickly.
        proc->need_resched = 1;
    }
}

/** @brief Storage of the one and only run queue. */
static struct run_queue __rq;

void sched_init(void) {
    list_init(&timer_list);

    // The switch of the scheduling algorithm: stride_sched_class (default_sched_stride.c) can be
    // assigned here instead of default_sched_class (RR, default_sched.c).
    sched_class = &default_sched_class;

    rq = &__rq;
    rq->max_time_slice = MAX_TIME_SLICE;
    sched_class->init(rq);

    cprintf("sched class: %s\n", sched_class->name);
}

/**
 * @brief Mark a process runnable and put it into the run queue (provided by the lab framework).
 * @fn void wakeup_proc(struct proc_struct *proc)
 *
 * Sets the state to PROC_RUNNABLE and clears wait_state, with interrupts disabled, then enqueues
 * the process through the scheduling class. The process is not switched to here. Used by
 * do_fork() for a new child, by do_exit() to wake a parent that waits in do_wait(), and by
 * do_kill() to wake a sleeping victim.
 *
 * @param proc process to wake up
 * @note The assert rejects a PROC_ZOMBIE process. Waking a process that is already runnable only
 *       prints a warning. The running process is not enqueued: schedule() puts it back itself,
 *       which prevents it from being in the queue twice.
 */
void wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (proc->state != PROC_RUNNABLE) {
            proc->state = PROC_RUNNABLE;
            proc->wait_state = 0;
            if (proc != current) {
                sched_class_enqueue(proc);
            }
        } else {
            warn("wakeup runnable process.\n");
        }
    }
    local_intr_restore(intr_flag);
}

//LAB4 function3 2414007
/**
 * @fn void schedule(void)
 * @brief Select the next runnable process through the scheduling class and switch to it.
 *
 * @details Contract (lab4 module 3, rewritten for the lab6 framework: the lab4/lab5 version
 * scanned proc_list for a PROC_RUNNABLE process, which would bypass the run queue and therefore
 * the scheduling class):
 *  - clear the caller's need_resched flag, so that the same request is not handled twice;
 *  - if the caller is still PROC_RUNNABLE (preempted or yielding), put it back into the run queue
 *    with sched_class_enqueue(); a sleeping or exiting caller must not be queued;
 *  - choose the next process with sched_class_pick_next() and, if there is one, remove it from
 *    the queue with sched_class_dequeue(): a running process is not in the queue;
 *  - if the queue is empty, fall back to idleproc;
 *  - run the chosen process with proc_run() only if it is not already current.
 *  All of it is done with interrupts disabled (local_intr_save/local_intr_restore), since a timer
 *  interrupt could otherwise change the run queue or the states during the operation.
 *
 * @note Context: runs in the kernel thread that calls it (normally idleproc or a user process
 *       trapping into the kernel). The call returns only when this thread is switched back in later.
 */
void schedule(void) {
    bool intr_flag;
    struct proc_struct *next;

    local_intr_save(intr_flag);
    {
        // The request that brought us here is being served now.
        current->need_resched = 0;

        // A process that gave up the CPU but can still run goes back to the run queue.
        if (current->state == PROC_RUNNABLE) {
            sched_class_enqueue(current);
        }

        if ((next = sched_class_pick_next()) != NULL) {
            sched_class_dequeue(next);
        }

        // Nothing is runnable: let the idle thread have the CPU.
        if (next == NULL) {
            next = idleproc;
        }
        next->runs++;

        if (next != current) {
            proc_run(next);
        }
    }
    local_intr_restore(intr_flag);
}
