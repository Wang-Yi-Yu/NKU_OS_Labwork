/**
 * @file   buddy_pmm.h
 * @brief  Declaration of the Buddy System physical memory manager.
 * @author Yiyu Wang(2414007)
 */
#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

/** @brief Buddy System manager instance, implemented in buddy_pmm.c (lab2 exercise 3). */
extern const struct pmm_manager buddy_pmm_manager;

#endif /* ! __KERN_MM_BUDDY_PMM_H__ */
