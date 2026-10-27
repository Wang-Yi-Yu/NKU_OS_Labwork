/**
 * @file   best_fit_pmm.h
 * @brief  Declaration of the Best-Fit physical memory manager.
 */
#ifndef __KERN_MM_BEST_FIT_PMM_H__
#define __KERN_MM_BEST_FIT_PMM_H__

#include <pmm.h>

/** @brief Best-Fit manager instance, implemented in best_fit_pmm.c (lab2 exercise 2). */
extern const struct pmm_manager best_fit_pmm_manager;

#endif /* ! __KERN_MM_BEST_FIT_PMM_H__ */
