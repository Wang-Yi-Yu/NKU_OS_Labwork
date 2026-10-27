/**
 * @file   buddy_pmm.c
 * @brief  Buddy System physical memory manager (buddy_pmm_manager).
 * @author Yiyu Wang(2414007)
 *
 * The free memory is organised as blocks of 2^k contiguous pages (k = "order",
 * 0 <= k <= ::BUDDY_MAX_ORDER). Every block is naturally aligned: the physical
 * page number of its first page is a multiple of 2^k. Two blocks of the same
 * order are "buddies" if they are the two halves of one block of the next
 * order, i.e. their page numbers differ only in bit k:
 * @code
 *   buddy_ppn = ppn ^ (1 << k)
 * @endcode
 *
 * - Allocation rounds the request up to 2^k pages, takes a block of order k
 *   (or splits a larger one, keeping the lower half and freeing the upper half
 *   at every step).
 * - Free puts the block back and, as long as its buddy is also a free block of
 *   the same order, merges the two into a block of the next order.
 *
 * Data layout (the framework's struct Page is reused as it is):
 *  - one doubly linked free list per order, linked through Page::page_link;
 *  - only the head page of a free block carries PG_property, and
 *    Page::property holds the block size in pages (2^k), as in the other
 *    managers of this lab;
 *  - allocated pages have both flags cleared; reserved pages are never touched.
 */
#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_pmm.h>
#include <stdio.h>

//lab2 function3 2414007

/** @brief Largest supported order: 2^15 pages = 128MiB, the size of QEMU's default RAM. */
#define BUDDY_MAX_ORDER 15

/** @brief Order of the scratch block that buddy_check() carves out of the live free memory (64 pages). */
#define BUDDY_TEST_ORDER 6

/**
 * @brief Bookkeeping of the buddy allocator.
 */
typedef struct {
    list_entry_t free_list[BUDDY_MAX_ORDER + 1]; /**< Free blocks, one list per order. */
    unsigned int nr_free;                        /**< Total number of free pages in all lists. */
} buddy_area_t;

/** @brief The only instance of the allocator state. */
static buddy_area_t buddy_area;

/** @brief First physical page number managed by the allocator (inclusive). */
static ppn_t zone_start;
/** @brief End physical page number managed by the allocator (exclusive). */
static ppn_t zone_end;

/**
 * @brief Number of pages of a block of the given order.
 * @param order block order
 * @return 2^order
 */
static inline unsigned int order_pages(unsigned int order) {
    return 1U << order;
}

/**
 * @brief Find the smallest order whose block holds at least n pages.
 * @param n number of pages, must be greater than 0
 * @return the order, or BUDDY_MAX_ORDER + 1 if n exceeds the largest block
 */
static unsigned int pages_to_order(size_t n) {
    if (n > order_pages(BUDDY_MAX_ORDER)) {
        return BUDDY_MAX_ORDER + 1;
    }
    unsigned int order = 0;
    while (order_pages(order) < n) {
        order++;
    }
    return order;
}

/**
 * @brief Make a page the head of a free block of the given order and put it on the free list.
 * @param p     first page of the block
 * @param order block order
 * @note Does not touch `buddy_area.nr_free`.
 */
static void block_insert(struct Page *p, unsigned int order) {
    p->property = order_pages(order);
    SetPageProperty(p);
    list_add(&buddy_area.free_list[order], &p->page_link);
}

/**
 * @brief Take a free block off its free list; its head page is no longer a free-block head.
 * @param p first page of the block
 * @note Does not touch `buddy_area.nr_free`.
 */
static void block_remove(struct Page *p) {
    list_del(&p->page_link);
    ClearPageProperty(p);
    p->property = 0;
}

/**
 * @brief Return a block to the free lists, merging it with its buddies as far as possible.
 * @param p     first page of the block (its pages must not belong to any free block)
 * @param order block order
 *
 * In every round the buddy `ppn ^ 2^order` is looked up. It can be merged only
 * if it lies inside the managed zone and is the head of a free block of exactly
 * the same order; the merged block starts at the lower of the two.
 * @note Does not touch `buddy_area.nr_free`.
 */
static void block_free(struct Page *p, unsigned int order) {
    while (order < BUDDY_MAX_ORDER) {
        ppn_t buddy_ppn = page2ppn(p) ^ order_pages(order);
        if (buddy_ppn < zone_start || buddy_ppn + order_pages(order) > zone_end) {
            break;
        }
        struct Page *buddy = &pages[buddy_ppn - nbase];
        if (!PageProperty(buddy) || buddy->property != order_pages(order)) {
            break;
        }
        block_remove(buddy);
        if (buddy < p) {
            p = buddy;
        }
        order++;
    }
    block_insert(p, order);
}

/**
 * @brief Add n free pages starting at base to the free lists.
 * @param base first page; its page number may have any alignment
 * @param n    number of pages
 *
 * The range is cut greedily into the largest naturally aligned blocks that fit,
 * so no page is wasted even if the range is neither aligned nor a power of two.
 * Each block goes through block_free(), hence it also merges with free neighbours.
 */
static void range_free(struct Page *base, size_t n) {
    ppn_t ppn = page2ppn(base);
    ppn_t end = ppn + n;
    while (ppn < end) {
        unsigned int order = 0;
        while (order < BUDDY_MAX_ORDER && (ppn & (order_pages(order + 1) - 1)) == 0 &&
               ppn + order_pages(order + 1) <= end) {
            order++;
        }
        block_free(&pages[ppn - nbase], order);
        buddy_area.nr_free += order_pages(order);
        ppn += order_pages(order);
    }
}

/**
 * @brief Initialise the allocator: empty free lists, no free page, empty zone.
 */
static void buddy_init(void) {
    for (unsigned int i = 0; i <= BUDDY_MAX_ORDER; i++) {
        list_init(&buddy_area.free_list[i]);
    }
    buddy_area.nr_free = 0;
    zone_start = zone_end = 0;
}

/**
 * @brief Hand an initially free range of reserved pages to the allocator.
 * @param base first page of the range
 * @param n    number of pages, must be greater than 0
 *
 * Every page must still be marked reserved by the caller (page_init()). The
 * flags are cleared, the managed zone is extended to cover the range and the
 * pages are put on the free lists.
 * @note If called more than once, the zone is the hull of all ranges, so the
 *       pages between two ranges must never carry PG_property.
 */
static void buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    for (struct Page *p = base; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    ppn_t first = page2ppn(base);
    if (zone_start == zone_end) {
        zone_start = first;
        zone_end = first + n;
    } else {
        zone_start = first < zone_start ? first : zone_start;
        zone_end = first + n > zone_end ? first + n : zone_end;
    }
    range_free(base, n);
}

/**
 * @brief Allocate n contiguous physical pages.
 * @param n number of pages, must be greater than 0
 * @return first page of the block, or NULL if no block is large enough
 *
 * The request is rounded up to the next power of two, so the allocator really
 * consumes 2^ceil(log2 n) pages. The returned block is naturally aligned. The
 * same n must be passed to buddy_free_pages().
 */
static struct Page *buddy_alloc_pages(size_t n) {
    assert(n > 0);
    unsigned int order = pages_to_order(n);
    if (order > BUDDY_MAX_ORDER) {
        return NULL;
    }
    unsigned int cur = order;
    while (cur <= BUDDY_MAX_ORDER && list_empty(&buddy_area.free_list[cur])) {
        cur++;
    }
    if (cur > BUDDY_MAX_ORDER) {
        return NULL;
    }
    struct Page *p = le2page(list_next(&buddy_area.free_list[cur]), page_link);
    block_remove(p);
    // Split down to the wanted order: keep the lower half, free the upper half.
    while (cur > order) {
        cur--;
        block_insert(p + order_pages(cur), cur);
    }
    buddy_area.nr_free -= order_pages(order);
    return p;
}

/**
 * @brief Free a block obtained from buddy_alloc_pages().
 * @param base first page of the block
 * @param n    the same page count that was passed to buddy_alloc_pages()
 *
 * The block must be naturally aligned and lie inside the managed zone, and
 * none of its pages may be reserved or already be the head of a free block;
 * violations are caught by assertions.
 */
static void buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    unsigned int order = pages_to_order(n);
    assert(order <= BUDDY_MAX_ORDER);
    ppn_t ppn = page2ppn(base);
    assert((ppn & (order_pages(order) - 1)) == 0);
    assert(ppn >= zone_start && ppn + order_pages(order) <= zone_end);
    for (struct Page *p = base; p != base + order_pages(order); p++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    buddy_area.nr_free += order_pages(order);
    block_free(base, order);
}

/**
 * @brief Query the number of free pages.
 * @return total number of pages on all free lists
 */
static size_t buddy_nr_free_pages(void) {
    return buddy_area.nr_free;
}

// The self test is in the second half of this file.
static void buddy_check(void);

/** @brief The Buddy System manager instance, selected by `make PMM=buddy` in init_pmm_manager() (pmm.c). */
const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};

/* ------------------------------------------------------------------------- */
/* Self test                                                                  */
/* ------------------------------------------------------------------------- */

/**
 * @brief Count the blocks on the free list of one order.
 * @param order block order
 * @return number of blocks
 */
static size_t count_blocks(unsigned int order) {
    size_t count = 0;
    list_entry_t *le = &buddy_area.free_list[order];
    while ((le = list_next(le)) != &buddy_area.free_list[order]) {
        count++;
    }
    return count;
}

/**
 * @brief Check all invariants of the free lists.
 *
 * For every block on the list of order k: the head page carries PG_property and
 * is not reserved, Page::property equals 2^k, the page number is a multiple of
 * 2^k, the block lies inside the zone, and its buddy is not a free block of the
 * same order (otherwise the allocator failed to merge). Finally the block sizes
 * must add up to `buddy_area.nr_free`.
 */
static void verify_lists(void) {
    size_t total = 0;
    for (unsigned int k = 0; k <= BUDDY_MAX_ORDER; k++) {
        list_entry_t *le = &buddy_area.free_list[k];
        while ((le = list_next(le)) != &buddy_area.free_list[k]) {
            struct Page *p = le2page(le, page_link);
            assert(PageProperty(p) && !PageReserved(p));
            assert(p->property == order_pages(k));
            ppn_t ppn = page2ppn(p);
            assert((ppn & (order_pages(k) - 1)) == 0);
            assert(ppn >= zone_start && ppn + order_pages(k) <= zone_end);
            ppn_t buddy_ppn = ppn ^ order_pages(k);
            if (k < BUDDY_MAX_ORDER && buddy_ppn >= zone_start &&
                buddy_ppn + order_pages(k) <= zone_end) {
                struct Page *buddy = &pages[buddy_ppn - nbase];
                assert(!(PageProperty(buddy) && buddy->property == order_pages(k)));
            }
            total += p->property;
        }
    }
    assert(total == buddy_area.nr_free);
}

/**
 * @brief Check that the free lists hold exactly the given number of blocks per order.
 * @param expect expect[k] is the wanted number of blocks of order k, for k in [0, BUDDY_TEST_ORDER]
 *
 * Orders above BUDDY_TEST_ORDER must be empty.
 */
static void expect_blocks(const size_t *expect) {
    for (unsigned int k = 0; k <= BUDDY_MAX_ORDER; k++) {
        assert(count_blocks(k) == (k <= BUDDY_TEST_ORDER ? expect[k] : 0));
    }
}

/**
 * @brief Check the free lists when the scratch zone consists of one free block of order 6.
 */
static void expect_one_whole_block(void) {
    static const size_t whole[BUDDY_TEST_ORDER + 1] = {0, 0, 0, 0, 0, 0, 1};
    expect_blocks(whole);
    verify_lists();
}

/**
 * @brief Assert that a page pointer is the page at the given offset of the scratch block.
 */
#define assert_at(p, s, off) assert((p) == (s) + (off))

/**
 * @brief Deterministic pseudo random generator (LCG) for the stress test.
 * @param state generator state, updated in place
 * @return a pseudo random number
 */
static unsigned int next_random(unsigned int *state) {
    *state = *state * 1103515245U + 12345U;
    return *state >> 16;
}

/**
 * @brief Self test of the Buddy System allocator.
 *
 * The test does not depend on the state of the machine: it carves a naturally
 * aligned scratch block of 64 pages out of the live free memory, saves the
 * allocator state, runs every scenario on an empty allocator whose zone is
 * narrowed to the scratch block (so no merge can leave it), and finally
 * restores the state and gives the scratch block back. It calls the buddy
 * functions directly, so it also works when another manager is the active one.
 *
 * Scenarios: initial layout, splitting, buddy addressing, merging in any order,
 * rounding of the request size, exhaustion and invalid sizes, external
 * fragmentation, a randomised alloc/free stress test with overlap checks, and
 * the zone boundary (a free buddy outside the zone is never merged).
 */
static void buddy_check(void) {
    const unsigned int scratch_pages = order_pages(BUDDY_TEST_ORDER);
    size_t free_before = buddy_area.nr_free;
    verify_lists();

    // Carve a scratch block out of the live free memory and save the state.
    struct Page *s = buddy_alloc_pages(scratch_pages);
    assert(s != NULL);
    assert((page2ppn(s) & (scratch_pages - 1)) == 0);
    assert(buddy_area.nr_free == free_before - scratch_pages);
    verify_lists();
    buddy_area_t saved_area = buddy_area;
    ppn_t saved_start = zone_start, saved_end = zone_end;

    // Scenario 1: an aligned block of 64 pages becomes one block of order 6.
    buddy_init();
    zone_start = page2ppn(s);
    zone_end = zone_start + scratch_pages;
    for (unsigned int i = 0; i < scratch_pages; i++) {
        s[i].flags = s[i].property = 0;
        set_page_ref(&s[i], 0);
    }
    range_free(s, scratch_pages);
    assert(buddy_area.nr_free == scratch_pages);
    expect_one_whole_block();
    cprintf("buddy_check: initial layout OK\n");

    // Scenario 2: allocating one page splits 64 -> 32+16+8+4+2+1+1; the lower half is kept.
    struct Page *a = buddy_alloc_pages(1);
    assert_at(a, s, 0);
    assert(!PageProperty(a) && buddy_area.nr_free == scratch_pages - 1);
    static const size_t after_split[BUDDY_TEST_ORDER + 1] = {1, 1, 1, 1, 1, 1, 0};
    expect_blocks(after_split);
    assert(PageProperty(s + 1) && s[1].property == 1);
    assert(PageProperty(s + 2) && s[2].property == 2);
    assert(PageProperty(s + 4) && s[4].property == 4);
    assert(PageProperty(s + 8) && s[8].property == 8);
    assert(PageProperty(s + 16) && s[16].property == 16);
    assert(PageProperty(s + 32) && s[32].property == 32);
    verify_lists();
    cprintf("buddy_check: split OK\n");

    // Scenario 3: the next allocations use the split remainders and are naturally aligned.
    struct Page *b = buddy_alloc_pages(1);
    struct Page *c = buddy_alloc_pages(1);
    assert_at(b, s, 1);
    assert_at(c, s, 2);
    assert(page2ppn(a) == (page2ppn(b) ^ 1));
    verify_lists();

    // Scenario 4: free in an order that exercises "buddy busy" and chain merging.
    buddy_free_pages(b, 1);  // buddy a is still allocated: no merge
    assert(PageProperty(b) && b->property == 1);
    verify_lists();
    buddy_free_pages(a, 1);  // merges with b into an order-1 block at s
    assert(PageProperty(s) && s[0].property == 2 && !PageProperty(s + 1));
    verify_lists();
    buddy_free_pages(c, 1);  // merges up the whole chain back to order 6
    expect_one_whole_block();
    assert(PageProperty(s) && s[0].property == scratch_pages);
    cprintf("buddy_check: merge OK\n");

    // Scenario 5: sizes are rounded up to a power of two and blocks are aligned.
    static const size_t asked[] = {1, 2, 3, 4, 5, 8, 9, 16, 17, 33};
    static const unsigned int taken[] = {1, 2, 4, 4, 8, 8, 16, 16, 32, 64};
    for (unsigned int i = 0; i < sizeof(asked) / sizeof(asked[0]); i++) {
        struct Page *p = buddy_alloc_pages(asked[i]);
        assert(p != NULL);
        assert(buddy_area.nr_free == scratch_pages - taken[i]);
        assert((page2ppn(p) & (taken[i] - 1)) == 0);
        verify_lists();
        buddy_free_pages(p, asked[i]);
        expect_one_whole_block();
    }
    cprintf("buddy_check: rounding and alignment OK\n");

    // Scenario 6: exhaustion and invalid sizes.
    struct Page *whole = buddy_alloc_pages(scratch_pages);
    assert_at(whole, s, 0);
    assert(buddy_area.nr_free == 0);
    verify_lists();
    assert(buddy_alloc_pages(1) == NULL);
    buddy_free_pages(whole, scratch_pages);
    expect_one_whole_block();
    assert(buddy_alloc_pages(scratch_pages + 1) == NULL);
    assert(buddy_alloc_pages(order_pages(BUDDY_MAX_ORDER)) == NULL);
    assert(buddy_alloc_pages(order_pages(BUDDY_MAX_ORDER) + 1) == NULL);
    assert(buddy_alloc_pages((size_t)-1) == NULL);
    expect_one_whole_block();
    cprintf("buddy_check: exhaustion OK\n");

    // Scenario 7: external fragmentation; blocks merge only with their own buddy.
    struct Page *q[4];
    for (unsigned int i = 0; i < 4; i++) {
        q[i] = buddy_alloc_pages(16);
        assert_at(q[i], s, 16 * i);
    }
    assert(buddy_area.nr_free == 0 && buddy_alloc_pages(1) == NULL);
    buddy_free_pages(q[1], 16);
    buddy_free_pages(q[3], 16);
    static const size_t two_apart[BUDDY_TEST_ORDER + 1] = {0, 0, 0, 0, 2, 0, 0};
    expect_blocks(two_apart);
    verify_lists();
    assert(buddy_area.nr_free == 32 && buddy_alloc_pages(32) == NULL);
    buddy_free_pages(q[2], 16);  // merges with q[3] only
    static const size_t one_half[BUDDY_TEST_ORDER + 1] = {0, 0, 0, 0, 1, 1, 0};
    expect_blocks(one_half);
    verify_lists();
    buddy_free_pages(q[0], 16);  // merges with q[1], then both halves merge
    expect_one_whole_block();
    cprintf("buddy_check: fragmentation OK\n");

    // Scenario 8: randomised alloc/free with an overlap check on every page.
    struct Page *held[32];
    unsigned int held_n[32];
    unsigned char used[64];
    unsigned int seed = 2414007U;
    memset(held, 0, sizeof(held));
    memset(held_n, 0, sizeof(held_n));
    memset(used, 0, sizeof(used));
    for (unsigned int round = 0; round < 3000; round++) {
        unsigned int slot = next_random(&seed) % 32;
        if (held[slot] != NULL) {
            unsigned int off = held[slot] - s;
            unsigned int size = order_pages(pages_to_order(held_n[slot]));
            for (unsigned int i = 0; i < size; i++) {
                assert(used[off + i] == 1);
                used[off + i] = 0;
            }
            buddy_free_pages(held[slot], held_n[slot]);
            held[slot] = NULL;
        } else {
            size_t want = 1 + next_random(&seed) % 20;
            unsigned int size = order_pages(pages_to_order(want));
            struct Page *p = buddy_alloc_pages(want);
            if (p == NULL) {
                continue;
            }
            unsigned int off = p - s;
            assert(off + size <= scratch_pages && (off & (size - 1)) == 0);
            for (unsigned int i = 0; i < size; i++) {
                assert(used[off + i] == 0);
                used[off + i] = 1;
            }
            held[slot] = p;
            held_n[slot] = want;
        }
        verify_lists();
    }
    for (unsigned int slot = 0; slot < 32; slot++) {
        if (held[slot] != NULL) {
            buddy_free_pages(held[slot], held_n[slot]);
        }
    }
    expect_one_whole_block();
    cprintf("buddy_check: random stress OK\n");

    // Scenario 9: a free buddy outside the managed zone must never be merged. The zone is
    // narrowed to one half of the scratch block while the other half is a real free block.
    const unsigned int half_order = BUDDY_TEST_ORDER - 1;
    const unsigned int half_pages = order_pages(half_order);
    ppn_t zone_low = zone_start, zone_high = zone_end;
    struct Page *whole_block = buddy_alloc_pages(scratch_pages);
    assert_at(whole_block, s, 0);
    // (a) the buddy lies behind the end of the zone
    zone_end = zone_low + half_pages;
    block_insert(s + half_pages, half_order);
    block_free(s, half_order);
    static const size_t two_halves[BUDDY_TEST_ORDER + 1] = {0, 0, 0, 0, 0, 2, 0};
    expect_blocks(two_halves);
    assert(PageProperty(s) && s[0].property == half_pages);
    block_remove(s);
    block_remove(s + half_pages);
    // (b) the buddy lies in front of the start of the zone
    zone_start = zone_low + half_pages;
    zone_end = zone_high;
    block_insert(s, half_order);
    block_free(s + half_pages, half_order);
    expect_blocks(two_halves);
    assert(PageProperty(s + half_pages) && s[half_pages].property == half_pages);
    block_remove(s);
    block_remove(s + half_pages);
    zone_start = zone_low;
    buddy_area.nr_free = 0;
    buddy_free_pages(whole_block, scratch_pages);
    expect_one_whole_block();
    cprintf("buddy_check: zone boundary OK\n");

    // Give the scratch block back and restore the live state.
    struct Page *last = buddy_alloc_pages(scratch_pages);
    assert_at(last, s, 0);
    buddy_area = saved_area;
    zone_start = saved_start;
    zone_end = saved_end;
    buddy_free_pages(s, scratch_pages);
    assert(buddy_area.nr_free == free_before);
    verify_lists();
}
