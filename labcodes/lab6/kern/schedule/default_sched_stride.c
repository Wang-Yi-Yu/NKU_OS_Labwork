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

//lab6 function3 YOUR CODE
