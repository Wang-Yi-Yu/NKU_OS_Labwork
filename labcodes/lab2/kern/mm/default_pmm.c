/**
 * @file   default_pmm.c
 * @brief  First-Fit contiguous physical memory manager (default_pmm_manager).
 *
 * Free memory is kept as a list of free blocks sorted by the address of each
 * block's head page. Only the head page of a block carries PG_property and a
 * non-zero Page::property (the number of pages in the block).
 *
 * - allocation: scan the list from the lowest address and take the first block
 *   that is large enough; split off the unused tail as a new free block;
 * - free: re-insert the block in address order, then merge it with the previous
 *   and the next block if they are physically adjacent.
 *
 * Time complexity: allocation and free are O(number of free blocks).
 */
#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

/** @brief The free area managed by this allocator: block list plus total number of free pages. */
static free_area_t free_area;

/** @brief Shorthand for the list head of ::free_area. */
#define free_list (free_area.free_list)
/** @brief Shorthand for the total number of free pages in ::free_area. */
#define nr_free (free_area.nr_free)

/**
 * @brief Initialise the manager: empty free list and zero free pages.
 */
static void default_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

/**
 * @brief Register a range of free pages as one free block.
 *
 * All n pages must currently be reserved (PG_reserved). Their flags and
 * properties are cleared, the head page gets `property = n` and PG_property, and
 * the block is inserted into the free list in address order.
 *
 * @param base head page of the range
 * @param n    number of pages, must be greater than 0
 */
static void default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t *le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page *page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }
}

/**
 * @brief Allocate n contiguous pages with the First-Fit policy.
 *
 * Walks the free list from the lowest address and takes the first block with
 * `property >= n`. The block is unlinked; if it is larger than n, the remaining
 * tail becomes a new free block that takes its place in the list.
 *
 * @param n number of pages, must be greater than 0
 * @return head page of the allocated range (PG_property cleared), or NULL if no
 *         block is large enough
 */
static struct Page *default_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            page = p;
            break;
        }
    }
    if (page != NULL) {
        list_entry_t *prev = list_prev(&(page->page_link));
        list_del(&(page->page_link));
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}

/**
 * @brief Free n contiguous pages and merge them with adjacent free blocks.
 *
 * The pages must have been allocated (neither reserved nor already free). The
 * range is inserted into the free list in address order; it is then merged with
 * the previous block if that block ends exactly at base, and with the next block
 * if the range ends exactly at its head.
 *
 * @param base head page of the range to free
 * @param n    number of pages, must be greater than 0
 */
static void default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;

    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t *le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page *page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }

    list_entry_t *le = list_prev(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }

    le = list_next(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}

/**
 * @brief Return the total number of free pages.
 */
static size_t default_nr_free_pages(void) {
    return nr_free;
}

// clang-format off
/**
 * @brief Basic allocate/free sanity test shared by the allocator self tests.
 * @note Provided by the lab framework, do not modify.
 */
static void
basic_check(void) {
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
// clang-format on

// clang-format off
// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
/**
 * @brief Self test of the First-Fit allocator (checks free list consistency,
 *        splitting, merging and exhaustion).
 * @note Provided by the lab framework, do not modify.
 */
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}
// clang-format on

/** @brief The First-Fit manager instance, selected in init_pmm_manager() (pmm.c). */
const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};
