/**
 * @file   swap_clock.c
 * @brief  Clock (second chance) page replacement (lab9 exercise, `lab9 function1`).
 *
 * Clock is an approximation of LRU that costs little more than FIFO. The swappable pages sit in a
 * circular list, like the face of a clock, and a "hand" points at the page to be examined next.
 * Every page has a reference flag (struct Page::visited). To find a victim the hand moves round
 * the list: a page whose flag is set gets a second chance (the flag is cleared and the hand moves
 * on); the first page whose flag is already clear is the victim. If all flags are set, the hand
 * makes one full turn, clears them all, and then takes the first page. Compared with FIFO, pages
 * that are used again before the hand returns survive.
 *
 * The framework provides the manager table swap_manager_clock at the end of this file and the
 * three trivial handlers (init, set_unswappable, tick_event). The exercise is the three handlers
 * that decide the algorithm, written at the marker below: _clock_init_mm() (initialise the circular
 * list and the hand for an mm), _clock_map_swappable() (register a page that became resident) and
 * _clock_swap_out_victim() (run the hand and pick the victim). They are plugged into
 * swap_manager_clock by name, so their signatures are fixed by struct swap_manager (swap.h).
 */
#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_clock.h>
#include <list.h>

/**
 * @brief Head of the circular list of swappable pages, and the clock hand.
 *
 * pra_list_head is a sentinel node (the list is empty when it points to itself) and curr_ptr is
 * the hand. Both are shared by all address spaces. The same name pra_list_head is also defined in
 * swap_fifo.c; the Makefile builds with -fcommon so the two definitions are merged.
 */
list_entry_t pra_list_head, *curr_ptr;

//lab9 function1 YOUR CODE

/**
 * @brief Access sequence for check_swap(): the page fault count after every access under Clock.
 *
 * Only runs when swap.c selects swap_manager_clock. Four frames hold the pages at 0x1000 .. 0x4000;
 * the sequence touches them, writes 0x5000 (the fifth page, which forces a swap-out) and then
 * accesses all five pages again, asserting pgfault_num after every access. A wrong count fails an
 * assertion. (The `ucore_test` branch is a variant that prints a score; it is not compiled.)
 * @return 0
 */
static int _clock_check_swap(void) {
#ifdef ucore_test
    int score = 0, totalscore = 5;
    cprintf("%d\n", &score);
    ++score;
    cprintf("grading %d/%d points", score, totalscore);
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 4);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 4);
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 4);
    *(unsigned char *)0x2000 = 0x0b;
    ++score;
    cprintf("grading %d/%d points", score, totalscore);
    assert(pgfault_num == 4);
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    ++score;
    cprintf("grading %d/%d points", score, totalscore);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 5);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 5);
    ++score;
    cprintf("grading %d/%d points", score, totalscore);
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 5);
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    assert(*(unsigned char *)0x1000 == 0x0a);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 6);
    ++score;
    cprintf("grading %d/%d points", score, totalscore);
#else
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 4);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 4);
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 4);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 4);
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 5);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 5);
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 5);
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    assert(*(unsigned char *)0x1000 == 0x0a);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 6);
#endif
    return 0;
}


/** @brief Global initialisation of the manager: nothing to do. @return 0 */
static int _clock_init(void) {
    return 0;
}

/** @brief A page became unswappable: Clock does not support that and does nothing. @return 0 */
static int _clock_set_unswappable(struct mm_struct *mm, uintptr_t addr) {
    return 0;
}

/** @brief Timer tick hook: Clock does nothing here. @return 0 */
static int _clock_tick_event(struct mm_struct *mm) {
    return 0;
}


/**
 * @brief The Clock swap manager: binds the handlers of this file to the swap_manager interface.
 *
 * init_mm, map_swappable and swap_out_victim refer to the three functions of the exercise.
 */
struct swap_manager swap_manager_clock = {
    .name = "clock swap manager",
    .init = &_clock_init,
    .init_mm = &_clock_init_mm,
    .tick_event = &_clock_tick_event,
    .map_swappable = &_clock_map_swappable,
    .set_unswappable = &_clock_set_unswappable,
    .swap_out_victim = &_clock_swap_out_victim,
    .check_swap = &_clock_check_swap,
};
