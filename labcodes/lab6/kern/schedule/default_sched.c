/**
 * @file   default_sched.c
 * @brief  Round-Robin (RR) scheduling class (lab6 function2): the time-slice based default policy.
 *
 * The code generated for this file has to provide five static functions and the class object
 * default_sched_class (name "RR_scheduler") that connects them to the framework in sched.c:
 *  - RR_init(rq): empty rq->run_list and set rq->proc_num to 0;
 *  - RR_enqueue(rq, proc): append proc at the tail of rq->run_list through proc->run_link; if
 *    proc->time_slice is 0 or larger than rq->max_time_slice it becomes rq->max_time_slice; set
 *    proc->rq = rq and increment rq->proc_num;
 *  - RR_dequeue(rq, proc): remove proc from the list (proc must be in rq), clear proc->rq and
 *    decrement rq->proc_num;
 *  - RR_pick_next(rq): return the process at the head of the list, NULL if the list is empty;
 *  - RR_proc_tick(rq, proc): decrement proc->time_slice if it is positive and set
 *    proc->need_resched when it has reached 0.
 * run_link, rq and proc_num must stay consistent after every operation.
 */
#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <default_sched.h>

//lab6 function2 2414015

static void RR_init(struct run_queue *rq) {
    list_init(&(rq->run_list));
    rq->proc_num = 0;
    rq->max_time_slice = MAX_TIME_SLICE;
}

static void RR_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    assert(list_empty(&(proc->run_link)));
    list_add_before(&(rq->run_list), &(proc->run_link));
    if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
        proc->time_slice = rq->max_time_slice;
    }
    proc->rq = rq;
    rq->proc_num++;
}

static void RR_dequeue(struct run_queue *rq, struct proc_struct *proc) {
    assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
    list_del_init(&(proc->run_link));
    proc->rq = NULL;
    rq->proc_num--;
}

static struct proc_struct *RR_pick_next(struct run_queue *rq) {
    if (list_empty(&(rq->run_list))) {
        return NULL;
    }
    return le2proc(list_next(&(rq->run_list)), run_link);
}

static void RR_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
    (void)rq;
    if (proc->time_slice > 0) {
        proc->time_slice--;
    }
    if (proc->time_slice == 0) {
        proc->need_resched = 1;
    }
}

struct sched_class default_sched_class = {
    .name = "RR_scheduler",
    .init = RR_init,
    .enqueue = RR_enqueue,
    .dequeue = RR_dequeue,
    .pick_next = RR_pick_next,
    .proc_tick = RR_proc_tick,
};
