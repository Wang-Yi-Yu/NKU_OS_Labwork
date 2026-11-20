#ifndef __KERN_SCHEDULE_SCHED_RR_H__
#define __KERN_SCHEDULE_SCHED_RR_H__

/**
 * @file   default_sched.h
 * @brief  Declarations of the scheduling classes that sched_init() can bind to.
 */
#include <sched.h>

/**
 * @brief The default Round-Robin scheduling class (defined in default_sched.c, lab6 function2).
 * @note sched_init() uses it unless it is changed to stride_sched_class.
 */
extern struct sched_class default_sched_class;
/**
 * @brief The Stride scheduling class (defined in default_sched_stride.c, lab6 function3).
 * @note Only used once sched_init() has been switched to it; the grading tests refer to both classes.
 */
extern struct sched_class stride_sched_class;

#endif /* !__KERN_SCHEDULE_SCHED_RR_H__ */
