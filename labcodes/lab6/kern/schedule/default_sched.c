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

//lab6 function2 YOUR CODE
