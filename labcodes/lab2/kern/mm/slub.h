/**
 * @file   slub.h
 * @brief  Simplified SLUB object allocator built on the current page allocator.
 * @author 鲁昕宁(2414015)
 */
#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <defs.h>

/**
 * @brief Initialise the SLUB caches. Pages are not taken until the first allocation.
 *
 * Safe to call again: objects still held by SLUB are returned to the page allocator.
 * @author 鲁昕宁(2414015)
 */
void slub_init(void);

/**
 * @brief Allocate an object of at least @p size bytes.
 * @param size requested size in bytes; 0 fails
 * @return kernel virtual address of the object, or NULL if @p size is 0, too large, or memory is exhausted
 * @author 鲁昕宁(2414015)
 */
void *slub_alloc(size_t size);

/**
 * @brief Free an object returned by slub_alloc().
 * @param obj object address, or NULL to do nothing
 * @author 鲁昕宁(2414015)
 */
void slub_free(void *obj);

/**
 * @brief Check allocation, reuse and page return for in-page and multi-page objects.
 *
 * Panics through assert on failure. Prints "slub_check() succeeded" when every case passes.
 * @author 鲁昕宁(2414015)
 */
void slub_check(void);

#endif /* __KERN_MM_SLUB_H__ */
