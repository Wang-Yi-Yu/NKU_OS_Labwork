/**
 * @file   default_pmm.h
 * @brief  Declaration of the First-Fit physical memory manager.
 */
#ifndef __KERN_MM_DEFAULT_PMM_H__
#define __KERN_MM_DEFAULT_PMM_H__

#include <pmm.h>

/** @brief First-Fit manager instance, implemented in default_pmm.c. */
extern const struct pmm_manager default_pmm_manager;

#endif /* ! __KERN_MM_DEFAULT_PMM_H__ */
