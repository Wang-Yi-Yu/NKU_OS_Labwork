/**
 * @file   default_pmm.h
 * @brief  Declaration of the default physical memory manager (Best-Fit, carried over from lab2).
 */
#ifndef __KERN_MM_DEFAULT_PMM_H__
#define __KERN_MM_DEFAULT_PMM_H__

#include <pmm.h>

/** @brief Default manager instance, implemented in default_pmm.c. */
extern const struct pmm_manager default_pmm_manager;
/** @brief The free block list and the free page counter of the default manager, defined in default_pmm.c. */
extern free_area_t free_area;
#endif /* ! __KERN_MM_DEFAULT_PMM_H__ */
