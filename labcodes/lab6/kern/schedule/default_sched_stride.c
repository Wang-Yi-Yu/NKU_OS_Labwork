/**
 * @file   default_sched_stride.c
 * @brief  Stride scheduling class (lab6 function3): CPU time proportional to the process priority.
 *
 * The code generated for this file has to provide the comparison function, five static functions
 * and the class object stride_sched_class (name "stride_scheduler"):
 *  - proc_stride_comp_f(a, b): compare two skew-heap nodes by the lab6_stride of their processes
 *    and return -1, 0 or 1. The stride is an unsigned 32-bit counter that wraps around, so the
 *    comparison must be done on the signed difference `(int32_t)(stride_a - stride_b)`, which stays
 *    correct as long as BIG_STRIDE does not exceed INT32_MAX;
 *  - stride_init(rq): rq->run_list empty, rq->lab6_run_pool NULL, rq->proc_num 0;
 *  - stride_enqueue(rq, proc): insert proc->lab6_run_pool into the skew heap rq->lab6_run_pool;
 *    reset proc->time_slice to rq->max_time_slice when it is 0 or too large; set proc->rq and
 *    increment rq->proc_num. A priority of 0 must be treated as 1;
 *  - stride_dequeue(rq, proc): remove proc->lab6_run_pool from the heap, clear proc->rq and
 *    decrement rq->proc_num;
 *  - stride_pick_next(rq): NULL for an empty heap; otherwise the process of the heap root (the
 *    smallest stride), whose lab6_stride is advanced by the pass BIG_STRIDE / lab6_priority;
 *  - stride_proc_tick(rq, proc): same time-slice handling as RR (decrement, need_resched at 0).
 * lab6_run_pool, rq and proc_num must stay consistent after every operation. BIG_STRIDE is not
 * defined by the skeleton; this file has to define it.
 */
#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <default_sched.h>
#include <stdio.h>

//lab6 function3 2414015

/* Largest pass that still fits in a signed 32-bit difference of two uint32_t strides. */
#define BIG_STRIDE 0x7FFFFFFF

static int proc_stride_comp_f(void *a, void *b) {
    struct proc_struct *p = le2proc(a, lab6_run_pool);
    struct proc_struct *q = le2proc(b, lab6_run_pool);
    int32_t c = (int32_t)(p->lab6_stride - q->lab6_stride);
    if (c < 0) {
        return -1;
    }
    if (c > 0) {
        return 1;
    }
    return 0;
}

static void stride_init(struct run_queue *rq) {
    list_init(&(rq->run_list));
    rq->lab6_run_pool = NULL;
    rq->proc_num = 0;
    rq->max_time_slice = MAX_TIME_SLICE;
}

static void stride_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    rq->lab6_run_pool =
        skew_heap_insert(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
    if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
        proc->time_slice = rq->max_time_slice;
    }
    if (proc->lab6_priority == 0) {
        proc->lab6_priority = 1;
    }
    proc->rq = rq;
    rq->proc_num++;
}

static void stride_dequeue(struct run_queue *rq, struct proc_struct *proc) {
    assert(proc->rq == rq);
    rq->lab6_run_pool =
        skew_heap_remove(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
    proc->rq = NULL;
    rq->proc_num--;
}

static struct proc_struct *stride_pick_next(struct run_queue *rq) {
    if (rq->lab6_run_pool == NULL) {
        return NULL;
    }
    struct proc_struct *proc = le2proc(rq->lab6_run_pool, lab6_run_pool);
    uint32_t priority = proc->lab6_priority == 0 ? 1 : proc->lab6_priority;
    proc->lab6_stride += BIG_STRIDE / priority;
    return proc;
}

static void stride_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
    (void)rq;
    if (proc->time_slice > 0) {
        proc->time_slice--;
    }
    if (proc->time_slice == 0) {
        proc->need_resched = 1;
    }
}

struct sched_class stride_sched_class = {
    .name = "stride_scheduler",
    .init = stride_init,
    .enqueue = stride_enqueue,
    .dequeue = stride_dequeue,
    .pick_next = stride_pick_next,
    .proc_tick = stride_proc_tick,
};
