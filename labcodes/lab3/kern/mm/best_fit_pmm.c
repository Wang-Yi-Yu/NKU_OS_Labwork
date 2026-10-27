#include <pmm.h>
#include <list.h>
#include <string.h>
#include <best_fit_pmm.h>
#include <stdio.h>

//lab2 请补充你在lab2中的代码

/** @brief Free-block bookkeeping: a list of free block head pages sorted by address, and the total number of free pages. */
static free_area_t free_area;
/** @brief Shorthand for the head of the address-ordered free block list. */
#define free_list (free_area.free_list)
/** @brief Shorthand for the total number of free pages in all free blocks. */
#define nr_free (free_area.nr_free)

/**
 * @brief Reset the allocator to the empty state (no free block, nr_free == 0).
 */
static void best_fit_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

/**
 * @brief Register a range of initially free pages as one free block.
 *
 * Every page must be reserved on entry; their flags, property and reference
 * count are cleared, then `base` becomes the head of a free block of n pages
 * and is inserted into the address-ordered free list.
 *
 * @param base descriptor of the first page of the range
 * @param n    number of pages in the range, must be greater than 0
 */
static void best_fit_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    for (struct Page *p = base; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *page = le2page(le, page_link);
        if (base < page) {
            list_add_before(le, &base->page_link);
            return;
        }
    }
    list_add(list_prev(&free_list), &base->page_link);
}

/**
 * @brief Allocate n contiguous pages from the smallest free block that fits.
 *
 * The whole free list is scanned and the block with the smallest `property`
 * that is still at least n is chosen (the first one on ties, i.e. the lowest
 * address). The first n pages are handed out and the remainder, if any, stays
 * on the list in the place of the original block.
 *
 * @param n number of pages, must be greater than 0
 * @return head page of the allocated range, or NULL if no free block is large enough
 */
static struct Page *best_fit_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free)
        return NULL;
    struct Page *best = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n && (best == NULL || p->property < best->property))
            best = p;
    }
    if (best == NULL)
        return NULL;
    list_entry_t *prev = list_prev(&best->page_link);
    size_t old_size = best->property;
    list_del(&best->page_link);
    if (old_size > n) {
        struct Page *remaining = best + n;
        remaining->property = old_size - n;
        SetPageProperty(remaining);
        list_add(prev, &remaining->page_link);
    }
    nr_free -= n;
    ClearPageProperty(best);
    best->property = 0;
    return best;
}

/**
 * @brief Return n contiguous pages to the free list and merge adjacent free blocks.
 *
 * The range is inserted at its address-ordered position, then merged with the
 * previous and the next block when they are physically adjacent.
 *
 * @param base descriptor of the first page, must come from best_fit_alloc_pages()
 * @param n    number of pages, must be greater than 0 and match the allocation
 * @note Asserts that no page of the range is reserved or already a free block head.
 */
static void best_fit_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    for (struct Page *p = base; p != base + n; p++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *page = le2page(le, page_link);
        if (base < page) {
            list_add_before(le, &base->page_link);
            break;
        }
    }
    if (le == &free_list)
        list_add(list_prev(&free_list), &base->page_link);

    le = list_prev(&base->page_link);
    if (le != &free_list) {
        struct Page *prev = le2page(le, page_link);
        if (prev + prev->property == base) {
            prev->property += base->property;
            ClearPageProperty(base);
            list_del(&base->page_link);
            base = prev;
        }
    }
    le = list_next(&base->page_link);
    if (le != &free_list) {
        struct Page *next = le2page(le, page_link);
        if (base + base->property == next) {
            base->property += next->property;
            ClearPageProperty(next);
            list_del(&next->page_link);
        }
    }
}

/**
 * @brief Return the total number of free pages.
 * @return value of nr_free
 */
static size_t best_fit_nr_free_pages(void) {
    return nr_free;
}

/** @brief Forward declaration of the framework self test, defined below (see best_fit_check()). */
static void best_fit_check(void);

/** @brief The Best-Fit manager instance, selected by default in init_pmm_manager() (pmm.c). */
const struct pmm_manager best_fit_pmm_manager = {
    .name = "best_fit_pmm_manager",
    .init = best_fit_init,
    .init_memmap = best_fit_init_memmap,
    .alloc_pages = best_fit_alloc_pages,
    .free_pages = best_fit_free_pages,
    .nr_free_pages = best_fit_nr_free_pages,
    .check = best_fit_check,
};

// clang-format off
// Framework tests: students must not modify basic_check or best_fit_check.
/**
 * @brief Basic allocate/free sanity test shared by the allocator self tests.
 * @note Provided by the lab framework, do not modify.
 */

static void basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the best fit allocation algorithm (your EXERCISE 1)
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void best_fit_check(void) {
    int score = 0, sumscore = 6;
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

#ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
#endif
    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

#ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
#endif
    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

#ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
#endif
    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    // * - - * -
    free_pages(p0 + 1, 2);
    free_pages(p0 + 4, 1);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 1) && p0[1].property == 2);
    // * - - * *
    assert((p1 = alloc_pages(1)) != NULL);
    assert(alloc_pages(2) != NULL);  // best fit feature
    assert(p0 + 4 == p1);

#ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
#endif
    p2 = p0 + 1;
    free_pages(p0, 5);
    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

#ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
#endif
    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count--, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
#ifdef ucore_test
    score += 1;
    cprintf("grading: %d / %d points\n", score, sumscore);
#endif
}
