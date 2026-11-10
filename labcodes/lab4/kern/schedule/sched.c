#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

/**
 * @file   sched.c
 * @brief  Round-robin scheduler: wakeup_proc() and the schedule() exercise.
 *
 * In lab4 schedule() is driven by cpu_idle() in the idle thread: whenever need_resched is set,
 * schedule() chooses the next PROC_RUNNABLE process and hands the CPU over with proc_run().
 */
#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

/**
 * @brief Mark a process runnable so that schedule() may select it.
 * @fn void wakeup_proc(struct proc_struct *proc)
 *
 * Only the state field is changed; the process is not switched to here. Used by do_fork() for a
 * new child and, in later labs, by wakeups of sleeping processes.
 *
 * @param proc process to wake up
 * @note The assert rejects a PROC_ZOMBIE or PROC_RUNNABLE process. A freshly allocated child must
 *       therefore still be PROC_UNINIT when wakeup_proc() is called on it.
 */
void wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    proc->state = PROC_RUNNABLE;
}

/**
 * @fn void schedule(void)
 * @brief Select the next runnable process and switch to it.
 *
 * @details Contract (lab4 module 3):
 *  - clear the caller's need_resched flag, so that the same request is not handled twice;
 *  - search proc_list for a PROC_RUNNABLE process, starting after the current one (round-robin);
 *  - if none is runnable, fall back to idleproc;
 *  - run the chosen process with proc_run() only if it is not already current.
 *  The search must be done with interrupts disabled (local_intr_save/local_intr_restore), since
 *  a timer interrupt could otherwise change proc_list or the states during the scan.
 *
 * @note Context: runs in the kernel thread that calls it (normally idleproc). The call returns only
 *       when this thread is switched back in later.
 */
//LAB4 function3 YOUR CODE
