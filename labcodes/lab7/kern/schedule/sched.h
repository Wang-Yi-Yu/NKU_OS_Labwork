#ifndef __KERN_SCHEDULE_SCHED_H__
#define __KERN_SCHEDULE_SCHED_H__

/**
 * @file   sched.h
 * @brief  Scheduler framework (lab6): the pluggable scheduling class interface, the run queue and
 *         the entry points used by the rest of the kernel; lab7 adds the timer service.
 *
 * The framework in sched.c knows nothing about a particular algorithm. It only calls the five
 * function pointers of struct sched_class, so RR (default_sched.c) and Stride
 * (default_sched_stride.c) can be exchanged by changing one assignment in sched_init().
 *
 * lab7 adds timers (timer_t): a process asks for a wake-up after a number of ticks, which is what
 * do_sleep() is built on. The timers wait in one list ordered by expiry time, and every timer
 * interrupt calls run_timer_list(), which also hands the tick to the scheduling class.
 */
#include <defs.h>
#include <list.h>
#include <skew_heap.h>

/** @brief Length of a time slice in timer ticks (the value rq->max_time_slice is set to). */
#define MAX_TIME_SLICE 5

struct proc_struct;

/**
 * @brief A one-shot timer that wakes a process up after a number of timer ticks (lab7).
 *
 * Timers are kept in one list sorted by expiry, and expires is stored as a delta: the number of
 * ticks after the previous timer in the list (for the first timer, after now). The timer interrupt
 * therefore only has to decrement the first entry.
 * @var timer_t::expires    ticks left until the timer fires; in the list, relative to the previous timer
 * @var timer_t::proc       process to wake up when the timer expires
 * @var timer_t::timer_link node linking the timer into the timer list; empty when it is not armed
 */
typedef struct {
    unsigned int expires;  // the expire time
    struct proc_struct *
        proc;  // the proc wait in this timer. If the expire time is end, then this proc will be scheduled
    list_entry_t timer_link;  // the timer list
} timer_t;

/** @brief Get the timer_t that contains the list node @p le (the timer_link member is named by @p member). */
#define le2timer(le, member) to_struct((le), timer_t, member)

/**
 * @brief Initialise a timer that is not armed yet.
 * @param timer   storage of the timer (usually on the kernel stack of the caller)
 * @param proc    process to wake up when the timer expires
 * @param expires number of timer ticks to wait; must be greater than 0 when passed to add_timer()
 * @return @p timer
 */
static inline timer_t *timer_init(timer_t *timer, struct proc_struct *proc, int expires) {
    timer->expires = expires;
    timer->proc = proc;
    list_init(&(timer->timer_link));
    return timer;
}

struct run_queue;

/**
 * @brief One scheduling policy, expressed as a table of operations on a run queue.
 *
 * The idea is borrowed from Linux scheduling classes: the core scheduler encapsulates the
 * mechanism and each class encapsulates one policy. All callbacks receive the run queue
 * explicitly. They are called by sched.c with interrupts disabled (except proc_tick, which runs
 * in the timer interrupt, where interrupts are off anyway), so they need no locking.
 * @var sched_class::name      name of the class, printed by sched_init() ("sched class: ...")
 * @var sched_class::init      initialise an empty run queue; called once from sched_init()
 * @var sched_class::enqueue   put a runnable process into the run queue (from wakeup_proc() and from schedule())
 * @var sched_class::dequeue   take a process out of the run queue (from schedule() once it has been picked)
 * @var sched_class::pick_next choose the next process to run; NULL when the queue is empty
 * @var sched_class::proc_tick called on every timer tick for the running process; updates its
 *                             time slice and sets need_resched when the slice is used up
 */
struct sched_class {
    const char *name;
    void (*init)(struct run_queue *rq);
    void (*enqueue)(struct run_queue *rq, struct proc_struct *proc);
    void (*dequeue)(struct run_queue *rq, struct proc_struct *proc);
    struct proc_struct *(*pick_next)(struct run_queue *rq);
    void (*proc_tick)(struct run_queue *rq, struct proc_struct *proc);
    /* for SMP support in the future
     *  load_balance
     *     void (*load_balance)(struct rq* rq);
     *  get some proc from this rq, used in load_balance,
     *  return value is the num of gotten proc
     *  int (*get_proc)(struct rq* rq, struct proc* procs_moved[]);
     */
};

/**
 * @brief The run queue: all runnable processes that are not currently running.
 *
 * ucore has a single CPU, so there is one global instance, owned by the active scheduling class.
 * RR keeps the processes in run_list; Stride keeps them in the skew heap lab6_run_pool (and
 * leaves run_list unused), which is why the structure carries both containers.
 * @var run_queue::run_list       head of the list of processes, linked via proc_struct::run_link (RR)
 * @var run_queue::proc_num       number of processes currently in the queue
 * @var run_queue::max_time_slice length of a time slice in ticks, set by sched_init()
 * @var run_queue::lab6_run_pool  root of the skew heap ordered by stride, NULL when empty (Stride only)
 */
struct run_queue {
    list_entry_t run_list;
    unsigned int proc_num;
    int max_time_slice;
    skew_heap_entry_t *lab6_run_pool;
};

/**
 * @brief Bind the scheduler framework to a scheduling class and initialise the run queue.
 * @note Called once from kern_init() after vmm_init(), before any process is created.
 */
void sched_init(void);
/**
 * @brief Make a sleeping process runnable and put it into the run queue (see sched.c).
 * @param proc process to wake; must not be PROC_ZOMBIE
 */
void wakeup_proc(struct proc_struct *proc);
/**
 * @brief Pick the next process through the scheduling class and switch to it (see sched.c).
 * @note Called from cpu_idle() and from the trap return path when need_resched is set.
 */
void schedule(void);
/**
 * @brief Run idleproc once so it is switched out, then return to the caller.
 *
 * synchronize_rcu() needs a quiescent state from idleproc. schedule() never selects idleproc
 * while the caller is still runnable, so the grace period switches here: the caller is queued,
 * idleproc runs, and its cpu_idle() loop schedules the caller back. The switch out of idleproc
 * goes through proc_run(), which reports the quiescent state.
 */
void sched_switch_idle(void);
/**
 * @brief Arm a timer: insert it into the timer list in expiry order (lab7; see sched.c).
 * @param timer timer set up by timer_init(), with expires > 0 and not already armed
 */
void add_timer(timer_t *timer);
/**
 * @brief Disarm a timer: remove it from the timer list (lab7; see sched.c).
 * @param timer timer previously armed with add_timer(); a timer that already fired or was never armed is ignored
 */
void del_timer(timer_t *timer);
/**
 * @brief Advance the timers by one tick, wake up the processes whose timers expired, and pass the
 *        tick to the scheduling class (lab7; see sched.c).
 * @note Called once per supervisor timer interrupt from interrupt_handler().
 */
void run_timer_list(void);
#endif /* !__KERN_SCHEDULE_SCHED_H__ */
