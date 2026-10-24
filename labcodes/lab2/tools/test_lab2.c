#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <pmm.h>
#include <best_fit_pmm.h>
#include <buddy_pmm.h>
#include <slub.h>
#include <memlayout.h>
#include <mmu.h>
#include <dtb.h>
#include <test_lab2.h>

/**
 * @brief Test 1: check the state left behind by page_init().
 *
 * Verifies va_pa_offset, the memory range reported by the device tree, and the
 * layout of the Page descriptors and the free area. Prints one PASS or FAIL
 * line with the first failing reason.
 */
void
test_page_init(void)
{
    extern char end[];
    uint64_t mem_base = get_memory_base();
    uint64_t mem_size = get_memory_size();
    uintptr_t end_pa;

    /* 1. va_pa_offset must be set to the virtual/physical mapping offset */
    if (va_pa_offset != PHYSICAL_MEMORY_OFFSET) {
        cprintf("[Test 1] Page Init: FAIL (va_pa_offset)\n");
        return;
    }

    /* 2. the device tree must report a valid physical memory range */
    if (mem_base == 0 || mem_size == 0) {
        cprintf("[Test 1] Page Init: FAIL (dtb mem 0)\n");
        return;
    }

    /* 3. npage must equal the page number of the reported memory end,
     *    and the upper bound must exceed the DRAM start page number */
    {
        uint64_t mem_end = mem_base + mem_size;
        size_t expect_npage = PPN(mem_end);
        if (npage != expect_npage || npage <= nbase) {
            cprintf("[Test 1] Page Init: FAIL (npage=%lu expect=%lu)\n",
                    (unsigned long)npage, (unsigned long)expect_npage);
            return;
        }
    }

    /* 4. pages must be non-null, page-aligned, and located after the kernel `end` */
    if (pages == NULL) {
        cprintf("[Test 1] Page Init: FAIL (pages NULL)\n");
        return;
    }
    if (((uintptr_t)pages % PGSIZE) != 0) {
        cprintf("[Test 1] Page Init: FAIL (pages not page-aligned)\n");
        return;
    }
    if ((uintptr_t)pages < (uintptr_t)ROUNDUP((uintptr_t)end, PGSIZE)) {
        cprintf("[Test 1] Page Init: FAIL (pages before kernel end)\n");
        return;
    }

    /* 5. the free area must have been built: some free pages, not more than total */
    {
        size_t free_now = nr_free_pages();
        if (free_now == 0 || free_now > (npage - nbase)) {
            cprintf("[Test 1] Page Init: FAIL (nr_free=%lu)\n",
                    (unsigned long)free_now);
            return;
        }
    }

    /* 6. the kernel-image tail page must not be handed to the allocator */
    end_pa = PADDR((uintptr_t)end);
    if (PPN(end_pa) >= npage) {
        cprintf("[Test 1] Page Init: FAIL (end pa out of range)\n");
        return;
    }
    if (!PageReserved(pa2page(end_pa))) {
        cprintf("[Test 1] Page Init: FAIL (kernel tail not reserved)\n");
        return;
    }

    /* 7. the pages occupied by the page-descriptor array must stay reserved too */
    {
        uintptr_t pages_pa = PADDR((uintptr_t)pages);
        if (PPN(pages_pa) >= npage) {
            cprintf("[Test 1] Page Init: FAIL (pages pa out of range)\n");
            return;
        }
        if (!PageReserved(pa2page(pages_pa))) {
            cprintf("[Test 1] Page Init: FAIL (pages array not reserved)\n");
            return;
        }
    }

    /* 8. page2pa / pa2page round-trip must be correct */
    {
        struct Page *q = pa2page(end_pa);
        if (pa2page(page2pa(q)) != q) {
            cprintf("[Test 1] Page Init: FAIL (roundtrip)\n");
            return;
        }
    }

    cprintf("[Test 1] Page Init: PASS\n");
}

/**
 * @brief Test 2: check that Best-Fit is the active physical memory manager.
 *
 * Prints PASS if pmm_manager is best_fit_pmm_manager, otherwise FAIL with the
 * name of the manager in use.
 */
void
test_best_fit(void)
{
    if (pmm_manager != &best_fit_pmm_manager) {
        cprintf("[Test 2] Best-Fit: FAIL (pmm_manager=%s)\n", pmm_manager->name);
        return;
    }
    cprintf("[Test 2] Best-Fit: PASS\n");
}

/**
 * @brief Let the Buddy System manage a region lent by the active manager and run a body on it.
 * @author Yiyu Wang(2414007)
 *
 * Takes @p n pages from the active manager, marks them reserved as page_init() would,
 * switches pmm_manager to buddy_pmm_manager with the region as its only free memory and
 * calls @p body. The body must give back every page it takes. Afterwards the active
 * manager is restored and the region is returned to it, so the rest of the kernel and
 * the other tests are not affected.
 *
 * @param n    number of pages to lend
 * @param body function to run while the Buddy System is the active manager
 * @return NULL on success, otherwise a short reason; pmm_manager is restored in every case
 */
static const char *
run_on_buddy_region(size_t n, void (*body)(void))
{
    const struct pmm_manager *active = pmm_manager;

    struct Page *region = alloc_pages(n);
    if (region == NULL) {
        return "no region";
    }
    for (size_t i = 0; i < n; i++) {
        SetPageReserved(region + i);
    }

    pmm_manager = &buddy_pmm_manager;
    buddy_pmm_manager.init();
    buddy_pmm_manager.init_memmap(region, n);
    if (nr_free_pages() != n) {
        pmm_manager = active;
        return "nr_free after init_memmap";
    }
    body();
    if (nr_free_pages() != n) {
        pmm_manager = active;
        return "body changed nr_free";
    }

    /* hand the region back to the active manager as freshly allocated pages */
    pmm_manager = active;
    for (size_t i = 0; i < n; i++) {
        region[i].flags = 0;
        region[i].property = 0;
        set_page_ref(region + i, 0);
    }
    free_pages(region, n);
    return NULL;
}

/** @brief Body of Test 3: the self test of the Buddy System manager. */
static void
buddy_body(void)
{
    buddy_pmm_manager.check();
}

/**
 * @brief Test 3: run the Buddy System self test next to the active manager.
 * @author Yiyu Wang(2414007)
 *
 * Runs the manager's self test (an assertion failure stops the kernel before the
 * PASS line is printed) on a region of 256 pages lent by the active manager, see
 * run_on_buddy_region().
 */
void
test_buddy_system(void)
{
    if (pmm_manager == &buddy_pmm_manager) {
        /* built with PMM=buddy: the self test already ran in check_alloc_page() */
        buddy_pmm_manager.check();
        cprintf("[Test 3] Buddy System: PASS\n");
        return;
    }

    const char *why = run_on_buddy_region(256, buddy_body);
    if (why != NULL) {
        cprintf("[Test 3] Buddy System: FAIL (%s)\n", why);
        return;
    }
    cprintf("[Test 3] Buddy System: PASS\n");
}

/** @brief Body of Tests 4 and 5: initialise SLUB on the current page allocator and run its self test. */
static void
slub_body(void)
{
    slub_init();
    slub_check();
}

/**
 * @brief Test 4: run the SLUB self test on the active physical memory manager.
 * @author Yiyu Wang(2414007)
 *
 * slub_check() asserts on every failure, so reaching the PASS line means all of its
 * cases passed (size classes, slab reuse, page return, multi-page objects, huge
 * requests, page exhaustion). SLUB gives back every page it took, so the kernel's own
 * slub_init() and slub_check() in kern_init() afterwards start from a clean state.
 */
void
test_slub(void)
{
    size_t before = nr_free_pages();

    slub_body();
    if (nr_free_pages() != before) {
        cprintf("[Test 4] SLUB: FAIL (leaked %ld pages)\n", (long)(before - nr_free_pages()));
        return;
    }
    cprintf("[Test 4] SLUB: PASS\n");
}

/**
 * @brief Test 5: run the SLUB self test on top of the Buddy System.
 * @author Yiyu Wang(2414007)
 *
 * The default kernel uses Best-Fit, so a region of 1024 pages (enough for the 300
 * live multi-page objects of slub_check) is lent to the Buddy System and SLUB is
 * initialised and checked there. This covers the rounding of the Buddy System (3
 * pages cost 4) and a second page allocator below the same SLUB code. SLUB holds no
 * page when slub_check() returns, so switching pmm_manager back is safe.
 */
void
test_slub_buddy(void)
{
    if (pmm_manager == &buddy_pmm_manager) {
        /* built with PMM=buddy: Test 4 already ran on the Buddy System */
        slub_body();
        cprintf("[Test 5] SLUB on Buddy System: PASS\n");
        return;
    }

    const char *why = run_on_buddy_region(1024, slub_body);
    if (why != NULL) {
        cprintf("[Test 5] SLUB on Buddy System: FAIL (%s)\n", why);
        return;
    }
    cprintf("[Test 5] SLUB on Buddy System: PASS\n");
}
