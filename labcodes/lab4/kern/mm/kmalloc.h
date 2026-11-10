/**
 * @file   kmalloc.h
 * @brief  Interface of the kernel small-object allocator: kmalloc() / kfree().
 *
 * The implementation (kmalloc.c) is a SLOB-style allocator built on top of the
 * page allocator alloc_pages()/free_pages() from pmm.h.
 */
#ifndef __KERN_MM_KMALLOC_H__
#define __KERN_MM_KMALLOC_H__

#include <defs.h>

/**
 * @brief Largest allocation order for kmalloc(), as log2 of the number of pages (2^10 pages = 4 MiB).
 * @warning Defined here but not checked anywhere in kmalloc.c in this lab.
 */
#define KMALLOC_MAX_ORDER 10

/**
 * @brief Initialise the kernel small-object allocator; called once from pmm_init().
 */
void kmalloc_init(void);

/**
 * @brief Allocate n bytes of kernel memory.
 * @param n number of bytes requested
 * @return kernel virtual address of the block, or NULL if memory is insufficient.
 *         The contents are not initialised. Release it with kfree().
 */
void *kmalloc(size_t n);
/**
 * @brief Free a block returned by kmalloc(). NULL is accepted and ignored.
 * @param objp pointer returned by kmalloc()
 */
void kfree(void *objp);

#endif /* !__KERN_MM_KMALLOC_H__ */
