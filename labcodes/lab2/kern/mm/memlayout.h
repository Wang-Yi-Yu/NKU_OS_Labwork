/**
 * @file   memlayout.h
 * @brief  Kernel virtual address space layout constants and the physical page
 *         descriptor struct Page.
 *
 * The kernel is linked at the high address KERNBASE, while OpenSBI loads it at
 * physical address 0x80200000. The boot page table in entry.S maps the 1GiB
 * range [0xFFFFFFFFC0000000, 0xFFFFFFFFFFFFFFFF] linearly onto physical
 * [0x80000000, 0xC0000000), so inside the kernel
 * `pa = va - PHYSICAL_MEMORY_OFFSET`. The same physical range is also
 * identity-mapped at its low address, only so that the few instructions right
 * after enabling paging can still be fetched (see entry.S).
 *
 * struct Page keeps one descriptor per physical page frame. A physical memory
 * manager (pmm_manager) links free blocks together through Page::page_link; see
 * pmm.h.
 */
#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

/** @name Kernel virtual address space layout
 *  @{ */

/**
 * @brief Virtual start address of the kernel image.
 *
 * Equals 0x80200000 (physical start of the kernel, KERNEL_BEGIN_PADDR) plus
 * 0xFFFFFFFF40000000 (virtual-to-physical offset, PHYSICAL_MEMORY_OFFSET).
 */
#define KERNBASE 0xFFFFFFFFC0200000

/**
 * @brief Maximum number of bytes of physical memory the kernel manages.
 *
 * 0x7E00000 = 0x8000000 - 0x200000: QEMU's default RAM is
 * [0x80000000, 0x88000000), i.e. 128MiB, and the first 2MiB
 * [0x80000000, 0x80200000) is occupied by OpenSBI.
 */
#define KMEMSIZE 0x7E00000

/** @brief Kernel virtual address of physical address 0x88000000 (end of RAM): KERNBASE + KMEMSIZE. */
#define KERNTOP (KERNBASE + KMEMSIZE)

/** @brief Exclusive upper bound of QEMU's default 128MiB RAM (physical address). */
#define PHYSICAL_MEMORY_END 0x88000000
/** @brief Offset of the kernel linear mapping: `va = pa + PHYSICAL_MEMORY_OFFSET`. */
#define PHYSICAL_MEMORY_OFFSET 0xFFFFFFFF40000000
/** @brief Physical start address of the kernel image (right after OpenSBI). */
#define KERNEL_BEGIN_PADDR 0x80200000
/** @brief Virtual start address of the kernel image; same as KERNBASE. */
#define KERNEL_BEGIN_VADDR 0xFFFFFFFFC0200000

/** @brief Number of pages in the boot kernel stack. */
#define KSTACKPAGE 2
/** @brief Size of the boot kernel stack in bytes. */
#define KSTACKSIZE (KSTACKPAGE * PGSIZE)

/** @} */

#ifndef __ASSEMBLER__

#include <defs.h>
#include <list.h>

/** @brief Page table entry; 64 bits wide under Sv39. */
typedef uintptr_t pte_t;
/** @brief Page directory entry; same width as pte_t. */
typedef uintptr_t pde_t;

/**
 * @brief Physical page descriptor: one struct Page per physical page frame.
 *
 * All descriptors are laid out contiguously in the array ::pages; index i
 * describes physical page number `i + nbase` (nbase is the page number of the
 * start of DRAM). Use page2ppn() / page2pa() / pa2page() from pmm.h to convert.
 */
struct Page {
    int ref;        /**< Reference count of the page frame; 0 for a free page. */
    uint64_t flags; /**< Status bits of the page frame, see ::PG_reserved and ::PG_property. */
    unsigned int
        property; /**< Number of pages in the free block headed by this page; valid only if PG_property is 1. */
    list_entry_t
        page_link; /**< Free-list node: links the head page of a free block into free_area_t::free_list. */
};

/** @name Bit numbers of struct Page::flags
 *  @{ */

/**
 * @brief Reserved bit.
 *
 * 1: the page is occupied by the kernel (OpenSBI, kernel image, the Page array,
 * ...) and must not be handled by alloc_pages()/free_pages().
 * 0: the page can be managed by the allocator.
 */
#define PG_reserved 0
/**
 * @brief Free-block-head bit.
 *
 * 1: the page is the head page of a free block of contiguous pages and can be
 * handed out by alloc_pages().
 * 0: if the page was the head of a block, that block has been allocated;
 * otherwise the page is simply not the head of a free block.
 */
#define PG_property 1
/** @} */

/** @name Bit operations on flags (the `page` argument is a pointer to struct Page)
 *  @{ */
/** @brief Set PG_reserved. */
#define SetPageReserved(page) ((page)->flags |= (1UL << PG_reserved))
/** @brief Clear PG_reserved. */
#define ClearPageReserved(page) ((page)->flags &= ~(1UL << PG_reserved))
/** @brief Test PG_reserved; evaluates to 0 or 1. */
#define PageReserved(page) (((page)->flags >> PG_reserved) & 1)
/** @brief Set PG_property. */
#define SetPageProperty(page) ((page)->flags |= (1UL << PG_property))
/** @brief Clear PG_property. */
#define ClearPageProperty(page) ((page)->flags &= ~(1UL << PG_property))
/** @brief Test PG_property; evaluates to 0 or 1. */
#define PageProperty(page) (((page)->flags >> PG_property) & 1)
/** @} */

/**
 * @brief Get the struct Page that contains a list node (container_of).
 * @param le     pointer to a list_entry_t
 * @param member name of the node inside struct Page, normally `page_link`
 * @return pointer to the struct Page containing the node
 */
#define le2page(le, member) to_struct((le), struct Page, member)

/**
 * @brief Free-page area: a doubly linked list that records all free blocks.
 *
 * The implementation in default_pmm.c keeps the list sorted by the address of
 * each block's head page, and relies on that order to merge neighbouring blocks
 * when pages are freed.
 */
typedef struct {
    list_entry_t free_list; /**< List head (sentinel node). */
    unsigned int nr_free;   /**< Total number of free pages in all blocks on the list. */
} free_area_t;

#endif /* !__ASSEMBLER__ */

#endif /* !__KERN_MM_MEMLAYOUT_H__ */
