/**
 * @file   swap.h
 * @brief  Page replacement framework (lab9): the swap_manager interface, swap entries and the
 *         swap_* entry points used by the rest of the kernel.
 *
 * When physical memory runs out, alloc_pages() calls swap_out(), which asks the current swap manager
 * to pick a victim page, writes it to the swap device (swapfs.h) and stores a swap entry in its
 * PTE. A later access faults, and do_pgfault() calls swap_in() to read the page back.
 *
 * Which pages may be evicted, and in which order, is decided by a swap manager: a struct
 * swap_manager filled with function pointers, in the same way as struct pmm_manager. This lab
 * provides the FIFO manager (swap_fifo.c) and asks for the Clock manager (swap_clock.c). swap.c
 * holds the one active manager in a static pointer, and the swap_* functions below forward to it.
 */
#ifndef __KERN_MM_SWAP_H__
#define __KERN_MM_SWAP_H__

#include <defs.h>
#include <memlayout.h>
#include <pmm.h>
#include <vmm.h>

/**
 * @name Layout of a swap entry (swap_entry_t)
 *
 * A swap entry lives in the PTE of a page that is swapped out. Bit 0 (PTE_V) is 0, so the MMU
 * sees an invalid PTE and the access faults.
 * @code
 * --------------------------------------------
 * |         offset        |   reserved   | 0 |
 * --------------------------------------------
 *           24 bits            7 bits    1 bit
 * @endcode
 * The offset (bits [31:8]) is the number of the slot on the swap device. Slot 0 is never used,
 * so an entry of 0 still means "no page".
 */
/** @brief Upper bound of max_swap_offset: the offset field has 24 bits. */
#define MAX_SWAP_OFFSET_LIMIT (1 << 24)

/**
 * @brief Number of page slots on the swap device, set by swapfs_init() (defined in swap.c).
 *
 * It is 0 until swapfs_init() runs, so swap_offset() rejects every entry before that.
 */
extern size_t max_swap_offset;

/**
 * @brief Get the slot number of a swap entry and check that it is in range.
 *
 * Expands to a statement expression. The slot is the entry shifted right by 8 bits, so slot 0 is
 * never valid. The macro panics with "invalid swap_entry_t" if the slot is 0 or is not below
 * max_swap_offset.
 *
 * @param entry swap_entry_t to decode (an expression; evaluated more than once)
 * @return the slot number, of type size_t
 */
#define swap_offset(entry)                                   \
    ({                                                       \
        size_t __offset = (entry >> 8);                      \
        if (!(__offset > 0 && __offset < max_swap_offset)) { \
            panic("invalid swap_entry_t = %08x.\n", entry);  \
        }                                                    \
        __offset;                                            \
    })
/** @} */

/**
 * @brief A page replacement algorithm: a table of function pointers, like struct pmm_manager.
 *
 * The framework calls these entry points and never looks inside the algorithm. A new algorithm
 * only fills in this table (see swap_manager_fifo and swap_manager_clock).
 *
 * @var swap_manager::name            name of the algorithm, printed by swap_init()
 * @var swap_manager::init            global initialisation of the manager; called once by swap_init()
 * @var swap_manager::init_mm         initialise the private data of one mm (mm_struct::sm_priv);
 *                                    called by mm_create() when swap_init_ok is set
 * @var swap_manager::tick_event      called on a timer tick (unused in this lab)
 * @var swap_manager::map_swappable   a page was mapped (swap_in is 0) or swapped in (swap_in is 1) at
 *                                    addr: put it in the set of pages that may be evicted
 * @var swap_manager::set_unswappable a page became shared or pinned: remove it from that set
 * @var swap_manager::swap_out_victim choose a victim page, remove it from the set and store it in
 *                                    *ptr_page; return 0, or non-zero if there is no victim
 * @var swap_manager::check_swap      self-test of the algorithm, run by check_swap() in swap.c
 */
struct swap_manager {
    const char *name;
    /* Global initialization for the swap manager */
    int (*init)(void);
    /* Initialize the priv data inside mm_struct */
    int (*init_mm)(struct mm_struct *mm);
    /* Called when tick interrupt occured */
    int (*tick_event)(struct mm_struct *mm);
    /* Called when map a swappable page into the mm_struct */
    int (*map_swappable)(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in);
    /* When a page is marked as shared, this routine is called to
      * delete the addr entry from the swap manager */
    int (*set_unswappable)(struct mm_struct *mm, uintptr_t addr);
    /* Try to swap out a page, return then victim */
    int (*swap_out_victim)(struct mm_struct *mm, struct Page **ptr_page, int in_tick);
    /* check the page relpacement algorithm */
    int (*check_swap)(void);
};

/**
 * @brief 1 once swap_init() has finished, 0 before.
 *
 * alloc_pages() and mm_create() test it: they use the swap manager only when it is set.
 */
extern volatile int swap_init_ok;
/**
 * @brief Initialise the swap file system and the swap manager, then run check_swap().
 * @return the result of the manager's init (0 on success). Panics if the swap device has fewer than
 *         7 slots or too many for a swap entry (MAX_SWAP_OFFSET_LIMIT)
 */
int swap_init(void);
/**
 * @brief Initialise the swap manager's private data of one mm (calls swap_manager::init_mm).
 * @param mm address space
 * @return the result of the manager's init_mm
 */
int swap_init_mm(struct mm_struct *mm);
/**
 * @brief Forward a timer tick to the swap manager (calls swap_manager::tick_event).
 * @param mm address space
 * @return the result of the manager's tick_event
 */
int swap_tick_event(struct mm_struct *mm);
/**
 * @brief Tell the swap manager that a page may be evicted (calls swap_manager::map_swappable).
 * @param mm      address space the page belongs to
 * @param addr    virtual address of the page
 * @param page    the physical page
 * @param swap_in 1 if the page was just swapped in, 0 if it is newly allocated
 * @return the result of the manager's map_swappable
 */
int swap_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in);
/**
 * @brief Tell the swap manager that a page must not be evicted (calls swap_manager::set_unswappable).
 * @param mm   address space the page belongs to
 * @param addr virtual address of the page
 * @return the result of the manager's set_unswappable
 */
int swap_set_unswappable(struct mm_struct *mm, uintptr_t addr);
/**
 * @brief Evict up to n pages to the swap device.
 *
 * For each page: ask the manager for a victim, write it to the swap device, put the swap entry
 * in its PTE, free the page and invalidate the TLB entry. If the write fails the page goes back
 * to the manager.
 *
 * @param mm      address space to evict from
 * @param n       number of pages to evict
 * @param in_tick 1 if called from a timer tick, 0 otherwise (passed to swap_out_victim)
 * @return the number of loop passes that ran, which is n unless the manager had no victim
 */
int swap_out(struct mm_struct *mm, int n, int in_tick);
/**
 * @brief Read the page of a swap entry back into a newly allocated physical page.
 *
 * The caller maps the page afterwards (page_insert()).
 *
 * @param mm         address space whose PTE holds the swap entry
 * @param addr       virtual address of the page
 * @param ptr_result receives the new physical page
 * @return 0. Panics if no page can be allocated. A failed swapfs_read() is not reported: the
 *         framework code only asserts r != 0 inside the branch where r is already non-zero
 */
int swap_in(struct mm_struct *mm, uintptr_t addr, struct Page **ptr_result);

#endif /* !__KERN_MM_SWAP_H__ */
