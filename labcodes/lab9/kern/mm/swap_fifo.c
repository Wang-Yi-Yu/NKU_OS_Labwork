/**
 * @file   swap_fifo.c
 * @brief  First-In-First-Out page replacement (lab9): the reference swap manager of the framework.
 *
 * FIFO keeps all swappable pages in a queue in the order they became resident (the most recent at
 * the back) and always evicts the page at the front, i.e. the one that has been in memory the
 * longest. The bookkeeping is tiny, but FIFO ignores how often a page is used, so a hot page that
 * was loaded early is evicted anyway, and it shows Belady's anomaly (more frames can cause more
 * page faults).
 *
 * The queue is a circular doubly linked list (libs/list.h) whose head is the global pra_list_head.
 * Pages are linked through struct Page::pra_page_link, and le2page() (memlayout.h) turns a list
 * node back into its page. The manager is made of the functions below and the table
 * swap_manager_fifo at the end of the file, which is what swap.c uses.
 */
#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_fifo.h>
#include <list.h>

/**
 * @brief Head of the FIFO queue of swappable pages (a sentinel node). Front = list_next(head).
 * @note The list is global, so all address spaces share one queue.
 */
list_entry_t pra_list_head;
/**
 * @brief Initialise an empty queue and make mm->sm_priv point to it.
 * @param mm address space; afterwards the FIFO queue can be reached from mm
 * @return 0
 */
static int _fifo_init_mm(struct mm_struct *mm) {
    list_init(&pra_list_head);
    mm->sm_priv = &pra_list_head;
    return 0;
}
/**
 * @brief Register a page as swappable: link it at the back of the queue (the most recent arrival).
 * @param mm      address space; mm->sm_priv is the queue head
 * @param addr    virtual address of the page (unused)
 * @param page    the physical page
 * @param swap_in 1 if the page was just swapped in, 0 if newly allocated (unused)
 * @return 0
 */
static int _fifo_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page,
                               int swap_in) {
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    list_entry_t *entry = &(page->pra_page_link);

    assert(entry != NULL && head != NULL);
    list_add_before(head, entry);
    return 0;
}
/**
 * @brief Choose the victim: unlink the earliest arrival, at the front of the queue.
 * @param mm       address space; mm->sm_priv is the queue head
 * @param ptr_page receives the victim page
 * @param in_tick  must be 0
 * @return 0
 * @warning The queue must not be empty: an empty queue makes list_next(head) the head itself, which
 *          is not inside a struct Page.
 */
static int _fifo_swap_out_victim(struct mm_struct *mm, struct Page **ptr_page, int in_tick) {
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    assert(head != NULL);
    assert(in_tick == 0);
    list_entry_t *entry = list_next(head);
    list_del(entry);
    *ptr_page = le2page(entry, pra_page_link);
    return 0;
}

/**
 * @brief Access sequence for check_swap(): the page fault count after every access under FIFO.
 *
 * Four frames hold the pages a (0x1000), b, c and d, which arrived in this order. Writing page e
 * (0x5000) evicts the oldest page, a, and each later access then faults or not exactly as the
 * asserts say (pgfault_num ends at 11). A wrong count fails an assertion.
 * @return 0
 */
static int _fifo_check_swap(void) {
    cprintf("write Virt Page c in fifo_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 4);
    cprintf("write Virt Page a in fifo_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 4);
    cprintf("write Virt Page d in fifo_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 4);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 4);
    cprintf("write Virt Page e in fifo_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    cprintf("write Virt Page a in fifo_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 6);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 7);
    cprintf("write Virt Page c in fifo_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 8);
    cprintf("write Virt Page d in fifo_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 9);
    cprintf("write Virt Page e in fifo_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 10);
    cprintf("write Virt Page a in fifo_check_swap\n");
    assert(*(unsigned char *)0x1000 == 0x0a);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 11);
    return 0;
}


/** @brief Global initialisation of the manager: nothing to do. @return 0 */
static int _fifo_init(void) {
    return 0;
}

/** @brief A page became unswappable: FIFO does not support that and does nothing. @return 0 */
static int _fifo_set_unswappable(struct mm_struct *mm, uintptr_t addr) {
    return 0;
}

/** @brief Timer tick hook: FIFO does nothing. @return 0 */
static int _fifo_tick_event(struct mm_struct *mm) {
    return 0;
}


/** @brief The FIFO swap manager: binds the functions above to the swap_manager interface of swap.h. */
struct swap_manager swap_manager_fifo = {
    .name = "fifo swap manager",
    .init = &_fifo_init,
    .init_mm = &_fifo_init_mm,
    .tick_event = &_fifo_tick_event,
    .map_swappable = &_fifo_map_swappable,
    .set_unswappable = &_fifo_set_unswappable,
    .swap_out_victim = &_fifo_swap_out_victim,
    .check_swap = &_fifo_check_swap,
};
