/**
 * @file   rcu.c
 * @brief  Quiescent-state-based reclamation for one hart (lab7).
 *
 * rcu_gp is the grace period currently in progress (0 means none has started). A process records
 * that number in rcu_gp_seen when its read-side depth drops to 0, when it is switched out with
 * depth 0, or when synchronize_rcu() finds it already sleeping outside a read-side section.
 * The caller of synchronize_rcu() is itself counted as quiescent for the period it starts.
 */
#include <rcu.h>
#include <proc.h>
#include <sched.h>
#include <sync.h>
#include <stdio.h>
#include <assert.h>

/** @brief Grace period that has been started. The next synchronize_rcu() advances it by one. */
static unsigned rcu_gp = 0;

/** @brief Object published before the grace period. The writer stores the sentinel here afterwards. */
static volatile int rcu_obj_a = 1;
/** @brief Object published by the writer at the start of the grace period. */
static volatile int rcu_obj_b = 2;
/** @brief Pointer readers follow. Starts at rcu_obj_a; the writer swings it to rcu_obj_b. */
static volatile int *volatile rcu_shared = &rcu_obj_a;
/** @brief Set by the reader after it has entered the read-side section and read rcu_obj_a. */
static volatile int rcu_reader_entered = 0;
/** @brief Set by the writer only after synchronize_rcu() has returned. */
static volatile int rcu_writer_done = 0;

void rcu_read_lock(void) {
    bool intr_flag;

    // The increment must not be split by a timer preemption: a switch in the middle would see
    // depth 0 and report a quiescent state for a section that has already begun.
    local_intr_save(intr_flag);
    current->rcu_read_depth++;
    local_intr_restore(intr_flag);
}

void rcu_read_unlock(void) {
    bool intr_flag;

    local_intr_save(intr_flag);
    if (current->rcu_read_depth <= 0) {
        panic("rcu_read_unlock\n");
    }
    current->rcu_read_depth--;
    if (current->rcu_read_depth == 0) {
        current->rcu_gp_seen = rcu_gp;
    }
    local_intr_restore(intr_flag);
}

void rcu_note_context_switch(struct proc_struct *prev) {
    if (prev->rcu_read_depth == 0) {
        prev->rcu_gp_seen = rcu_gp;
    }
}

/**
 * @brief Account for one process during the grace period @p target.
 * @param proc    process to look at; the caller has already skipped itself and zombies
 * @param target  grace-period number that must be observed
 * @return 1 if this process has not yet passed the period, 0 if it has
 */
static int rcu_one_pending(struct proc_struct *proc, unsigned target) {
    if (proc->rcu_gp_seen >= target) {
        return 0;
    }
    if (proc->rcu_read_depth > 0 && proc->state == PROC_SLEEPING) {
        cprintf("sleep in rcu read-side\n");
        panic("sleep in rcu read-side\n");
    }
    // Sleeping outside a read-side section is a quiescent state: the process holds no CPU and
    // will not touch a pointer published before this grace period.
    if (proc->rcu_read_depth == 0 && proc->state == PROC_SLEEPING) {
        proc->rcu_gp_seen = target;
        return 0;
    }
    return 1;
}

/**
 * @brief Walk idleproc and proc_list for processes that still owe grace period @p target.
 * @return 1 if synchronize_rcu() must schedule and look again
 */
static int rcu_gp_pending(unsigned target) {
    list_entry_t *le;
    int pending = 0;

    if (idleproc != current && rcu_one_pending(idleproc, target)) {
        pending = 1;
    }
    le = list_next(&proc_list);
    while (le != &proc_list) {
        struct proc_struct *proc = le2proc(le, list_link);
        le = list_next(le);
        if (proc == current || proc->state == PROC_ZOMBIE) {
            continue;
        }
        if (rcu_one_pending(proc, target)) {
            pending = 1;
        }
    }
    return pending;
}

void synchronize_rcu(void) {
    bool intr_flag;
    unsigned target;

    if (current->rcu_read_depth > 0) {
        cprintf("synchronize_rcu in read-side\n");
        return;
    }

    local_intr_save(intr_flag);
    target = ++rcu_gp;
    while (rcu_gp_pending(target)) {
        current->need_resched = 1;
        local_intr_restore(intr_flag);
        // A runnable reader is switched out only when the scheduler actually runs it. Sleeping
        // outside the read side was already stamped above, so this yield is what lets a reader
        // reach rcu_read_unlock().
        schedule();
        // schedule() keeps a runnable caller on the run queue, so it will not select idleproc.
        // One direct switch gives idleproc the context-switch quiescent state.
        if (idleproc != current && idleproc->rcu_gp_seen < target &&
            idleproc->rcu_read_depth == 0) {
            sched_switch_idle();
        }
        local_intr_save(intr_flag);
    }
    current->rcu_gp_seen = target;
    local_intr_restore(intr_flag);
}

/** @brief Reader: hold the old object across a voluntary context switch. */
static int rcu_reader(void *arg) {
    volatile int *local;
    int first, second;

    (void)arg;
    rcu_read_lock();
    local = rcu_shared;
    first = *local;
    rcu_reader_entered = 1;
    schedule();
    // The writer publishes the new pointer before synchronize_rcu(). Stay in the read-side
    // section until that publish is visible, which is also while the writer is still inside
    // the grace period.
    while (rcu_shared != &rcu_obj_b) {
        schedule();
    }
    second = *local;
    if (rcu_writer_done != 0 || first != 1 || second != 1) {
        panic("rcu check: reader saw a reclaimed object\n");
    }
    rcu_read_unlock();
    return 0;
}

/** @brief Writer: publish a new object, wait for readers, then retire the old one. */
static int rcu_writer(void *arg) {
    (void)arg;
    while (rcu_reader_entered == 0) {
        schedule();
    }
    rcu_shared = &rcu_obj_b;
    synchronize_rcu();
    rcu_obj_a = -1;
    rcu_writer_done = 1;
    return 0;
}

/** @brief Reap one kernel thread created by rcu_check(). */
static void rcu_reap(int pid) {
    while (do_wait(pid, NULL) != 0) {
        schedule();
    }
}

void rcu_check(void) {
    int pid_reader, pid_writer;

    rcu_obj_a = 1;
    rcu_obj_b = 2;
    rcu_shared = &rcu_obj_a;
    rcu_reader_entered = 0;
    rcu_writer_done = 0;

    pid_reader = kernel_thread(rcu_reader, NULL, 0);
    pid_writer = kernel_thread(rcu_writer, NULL, 0);
    if (pid_reader <= 0 || pid_writer <= 0) {
        panic("rcu check: create thread failed\n");
    }
    rcu_reap(pid_reader);
    rcu_reap(pid_writer);

    if (rcu_writer_done != 1 || rcu_obj_a != -1 || rcu_shared != &rcu_obj_b) {
        panic("rcu check: writer did not finish\n");
    }
    cprintf("rcu check ok\n");
}
