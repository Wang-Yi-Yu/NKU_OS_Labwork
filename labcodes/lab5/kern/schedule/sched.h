#ifndef __KERN_SCHEDULE_SCHED_H__
#define __KERN_SCHEDULE_SCHED_H__

/**
 * @file   sched.h
 * @brief  Scheduler interface: picking the next runnable process and waking a sleeping one.
 */
#include <proc.h>

/**
 * @brief Choose the next PROC_RUNNABLE process and switch to it (exercise, see sched.c).
 * @note Called from cpu_idle() and from the paths that give up the CPU (for example after
 *       need_resched has been set).
 */
void schedule(void);
/**
 * @brief Move a process from a sleeping state to PROC_RUNNABLE so that schedule() can pick it.
 * @param proc process to wake; must not be PROC_ZOMBIE or already PROC_RUNNABLE
 */
void wakeup_proc(struct proc_struct *proc);

#endif /* !__KERN_SCHEDULE_SCHED_H__ */
