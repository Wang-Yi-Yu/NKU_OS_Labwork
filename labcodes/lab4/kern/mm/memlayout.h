/**
 * @file   memlayout.h
 * @brief  Kernel virtual address space layout constants, and the physical page
 *         descriptor struct Page with its flag bits and helper macros.
 *
 * The kernel is linked at KERNBASE (a high virtual address) while OpenSBI loads
 * it at physical address 0x80200000. entry.S builds the boot page table
 * boot_page_table_sv39 with one 1GiB gigapage that maps
 * [0xFFFFFFFFC0000000, 0xFFFFFFFFFFFFFFFF] linearly onto physical
 * [0x80000000, 0xC0000000), so inside the kernel `pa = va - PHYSICAL_MEMORY_OFFSET`.
 * The same physical range is also identity-mapped at its low address, only so
 * that the instructions right after `csrw satp` can still be fetched (see entry.S).
 *
 * struct Page keeps one descriptor per physical page frame. The physical memory
 * managers link free blocks through Page::page_link (see pmm.h and default_pmm.c).
 */
#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

/* This file contains the definitions for memory management in our OS. */

/* *
 * Virtual memory map:                                          Permissions
 *                                                              kernel/user
 *
 *     4G ------------------> +---------------------------------+
 *                            |                                 |
 *                            |         Empty Memory (*)        |
 *                            |                                 |
 *                            +---------------------------------+ 0xFB000000
 *                            |   Cur. Page Table (Kern, RW)    | RW/-- PTSIZE
 *     VPT -----------------> +---------------------------------+ 0xFAC00000
 *                            |        Invalid Memory (*)       | --/--
 *     KERNTOP -------------> +---------------------------------+ 0xF8000000
 *                            |                                 |
 *                            |    Remapped Physical Memory     | RW/-- KMEMSIZE
 *                            |                                 |
 *     KERNBASE ------------> +---------------------------------+ 0xC0000000
 *                            |                                 |
 *                            |                                 |
 *                            |                                 |
 *                            ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * (*) Note: The kernel ensures that "Invalid Memory" is *never* mapped.
 *     "Empty Memory" is normally unmapped, but user programs may map pages
 *     there if desired.
 *
 * */

/**
 * @brief Kernel virtual address of the first byte of the kernel image.
 *
 * Equals KERNEL_BEGIN_PADDR (0x80200000) + PHYSICAL_MEMORY_OFFSET
 * (0xFFFFFFFF40000000). PADDR() panics on any virtual address below this value.
 */
/* All physical memory mapped at this address */
#define KERNBASE 0xFFFFFFFFC0200000
/**
 * @brief Maximum amount of physical memory (in bytes) the kernel manages: 0x7E00000 = 126 MiB.
 *
 * QEMU's default RAM is [0x80000000, 0x88000000), i.e. 128 MiB, and the first
 * 2 MiB of it is occupied by OpenSBI, hence 128 MiB - 2 MiB.
 */
#define KMEMSIZE 0x7E00000  // the maximum amount of physical memory
/**
 * @brief Kernel virtual address one past the last managed byte: KERNBASE + KMEMSIZE.
 *
 * It is the virtual address of physical address PHYSICAL_MEMORY_END (0x88000000).
 * check_pgdir() asserts `npage <= KERNTOP / PGSIZE`.
 */
#define KERNTOP (KERNBASE + KMEMSIZE)

/**
 * @brief Exclusive upper bound of usable physical RAM (0x88000000, QEMU default 128 MiB).
 *
 * page_init() never manages memory at or above this physical address.
 */
#define PHYSICAL_MEMORY_END 0x88000000
/**
 * @brief Offset of the kernel linear mapping: `va = pa + PHYSICAL_MEMORY_OFFSET`.
 *
 * The same value is stored at run time in the global va_pa_offset (pmm.c).
 */
#define PHYSICAL_MEMORY_OFFSET 0xFFFFFFFF40000000


/**
 * @brief Number of pages in one kernel stack.
 *
 * Used for the boot stack (bootstack in entry.S) and for each kernel thread's
 * stack (setup_kstack() in proc.c).
 */
#define KSTACKPAGE 2  // # of pages in kernel stack
/**
 * @brief Size in bytes of one kernel stack: KSTACKPAGE * PGSIZE (8 KiB).
 */
#define KSTACKSIZE (KSTACKPAGE * PGSIZE)  // sizeof kernel stack

#ifndef __ASSEMBLER__

#include <defs.h>
#include <atomic.h>
#include <list.h>

/**
 * @brief Page table entry (PTE); 64 bits wide under Sv39.
 *
 * Layout: PPN in bits [53:10], flag bits PTE_* in bits [9:0] (see mmu.h).
 */
typedef uintptr_t pte_t;
/**
 * @brief Page directory entry; same representation as pte_t. A non-leaf entry
 *        (R/W/X all 0) points to the next-level page table.
 */
typedef uintptr_t pde_t;
/**
 * @brief Swap entry; a PTE may also hold a swap entry when a page is swapped out.
 *
 * Not used by the allocation path of this lab; kept for the page replacement code.
 */
typedef pte_t swap_entry_t;  //the pte can also be a swap entry

/**
 * @brief Physical page descriptor: one struct Page per physical page frame.
 *
 * All descriptors are stored contiguously in the array ::pages, so `pages[i]`
 * describes physical page number `i + nbase`. Use page2ppn(), page2pa(),
 * pa2page() and page2kva() from pmm.h to convert between descriptors and
 * addresses. Fields:
 *  - ref:          number of mappings of this frame (page table entries that point
 *                  to it); 0 for a free page. Page table pages keep 1 while in use.
 *  - flags:        status bits, see PG_reserved and PG_property.
 *  - property:     number of pages in the free block whose head is this page;
 *                  only meaningful when PG_property is set.
 *  - page_link:    node of free_area_t::free_list, used by the pmm managers.
 *  - pra_page_link, pra_vaddr: used by the page replacement (swap) algorithm;
 *                  not touched by the allocator.
 */
/* *
 * struct Page - Page descriptor structures. Each Page describes one
 * physical page. In kern/mm/pmm.h, you can find lots of useful functions
 * that convert Page to other data types, such as physical address.
 * */
struct Page {
    int ref;                     // page frame's reference counter
    uint_t flags;                // array of flags that describe the status of the page frame
    unsigned int property;       // the num of free block, used in first fit pm manager
    list_entry_t page_link;      // free list link
    list_entry_t pra_page_link;  // used for pra (page replace algorithm)
    uintptr_t pra_vaddr;         // used for pra (page replace algorithm)
};

/** @name Bit numbers of struct Page::flags
 *  @{ */
/* Flags describing the status of a page frame */
/**
 * @brief Reserved bit (bit number, not a mask).
 *
 * 1: the page is occupied by the kernel (OpenSBI, kernel image, the Page array)
 *    and must not be handled by alloc_pages()/free_pages().
 * 0: the page can be managed by the allocator.
 */
#define PG_reserved \
    0  // if this bit=1: the Page is reserved for kernel, cannot be used in alloc/free_pages; otherwise, this bit=0
/**
 * @brief Free-block-head bit (bit number, not a mask).
 *
 * 1: the page is the head page of a free block of contiguous pages and can be
 *    handed out by alloc_pages().
 * 0: if the page was the head of a free block, that block has been allocated;
 *    otherwise the page is not the head of a free block.
 */
#define PG_property \
    1  // if this bit=1: the Page is the head page of a free memory block(contains some continuous_addrress pages), and can be used in alloc_pages; if this bit=0: if the Page is the the head page of a free memory block, then this Page and the memory block is alloced. Or this Page isn't the head page.

/** @} */

/** @name Bit operations on struct Page::flags
 *  The `page` argument is a pointer to struct Page. The operations are atomic
 *  (libs/atomic.h), so they are safe even if an interrupt touches the same word.
 *  @{ */
/** @brief Atomically set PG_reserved. */
#define SetPageReserved(page)   set_bit(PG_reserved, &((page)->flags))
#define ClearPageReserved(page) clear_bit(PG_reserved, &((page)->flags))
#define PageReserved(page)      test_bit(PG_reserved, &((page)->flags))
#define SetPageProperty(page)   set_bit(PG_property, &((page)->flags))
#define ClearPageProperty(page) clear_bit(PG_property, &((page)->flags))
#define PageProperty(page)      test_bit(PG_property, &((page)->flags))

/** @} */

/**
 * @brief Get the struct Page that contains a list node (container_of).
 * @param le     pointer to a list_entry_t
 * @param member name of the node inside struct Page, normally `page_link`
 * @return pointer to the enclosing struct Page
 */
// convert list entry to page
#define le2page(le, member) to_struct((le), struct Page, member)

/**
 * @brief Free-page area: a doubly linked list recording all free blocks of a manager.
 *
 * free_list is a sentinel list head. Each element is the head page of a block,
 * linked through Page::page_link; nr_free is the total number of free pages in
 * all blocks on the list (not the number of blocks).
 */
/* free_area_t - maintains a doubly linked list to record free (unused) pages */
typedef struct {
    list_entry_t free_list;  // the list header
    unsigned int nr_free;    // # of free pages in this free list
} free_area_t;

#endif /* !__ASSEMBLER__ */

#endif /* !__KERN_MM_MEMLAYOUT_H__ */
