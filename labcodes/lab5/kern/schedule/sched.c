/**
 * @file   sched.c
 * @brief  Round-robin scheduler: wakeup_proc() and the schedule() exercise.
 *
 * schedule() is called from two places: cpu_idle() in the idle thread whenever need_resched is
 * set, and trap() when a trap from user mode returns while the current process has need_resched
 * set (timer tick, yield) or after do_exit()/do_wait() have put the process to sleep. It chooses
 * the next PROC_RUNNABLE process and hands the CPU over with proc_run().
 */
#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <assert.h>

/**
 * @brief Mark a process runnable so that schedule() may select it (provided by the lab framework).
 * @fn void wakeup_proc(struct proc_struct *proc)
 *
 * Sets the state to PROC_RUNNABLE and clears wait_state, with interrupts disabled. The process
 * is not switched to here. Used by do_fork() for a new child, by do_exit() to wake a parent that
 * waits in do_wait(), and by do_kill() to wake a sleeping victim.
 *
 * @param proc process to wake up
 * @note The assert rejects a PROC_ZOMBIE process. Waking a process that is already runnable only
 *       prints a warning and changes nothing.
 */
void wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (proc->state != PROC_RUNNABLE) {
            proc->state = PROC_RUNNABLE;
            proc->wait_state = 0;
        } else {
            warn("wakeup runnable process.\n");
        }
    }
    local_intr_restore(intr_flag);
}

//LAB4 function3 2414007
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
void schedule(void) {
    bool intr_flag;
    list_entry_t *le, *last;
    struct proc_struct *next = NULL;

    local_intr_save(intr_flag);
    {
        // The request that brought us here is being served now.
        current->need_resched = 0;

        // idleproc is not on proc_list, so it scans from the list head; any other thread scans
        // from its own position, which makes the order round-robin.
        last = (current == idleproc) ? &proc_list : &(current->list_link);
        le = last;

        // Walk the circular list at most once. The list head is not embedded in a proc_struct,
        // so it must be skipped instead of being converted with le2proc().
        while ((le = list_next(le)) != last) {
            if (le == &proc_list) {
                continue;
            }
            next = le2proc(le, list_link);
            if (next->state == PROC_RUNNABLE) {
                break;
            }
            next = NULL;
        }

        // Nothing else is runnable: let the idle thread keep the CPU.
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
