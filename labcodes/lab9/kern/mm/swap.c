/**
 * @file   swap.c
 * @brief  Page replacement framework (lab9): the swap_* entry points, swap_out() and swap_in(), and
 *         the self-test check_swap().
 *
 * The file keeps one pointer `sm` to the active swap manager (a struct swap_manager, see swap.h).
 * swap_init() selects the manager (FIFO here, see swap_fifo.c; the Clock manager of swap_clock.c
 * has to be selected by changing that line), starts the swap file system and runs check_swap().
 * The other swap_* functions forward to the manager; swap_out() and swap_in() add the swap device
 * transfer and the PTE update, which do not depend on the algorithm.
 *
 * Call chain of a swap-out: alloc_pages() finds no free page -> swap_out() -> sm->swap_out_victim()
 * -> swapfs_write() -> the PTE becomes a swap entry -> free_page(). Call chain of a swap-in:
 * access to the swapped-out page -> page fault -> do_pgfault() -> swap_in() -> swapfs_read().
 */
#include <swap.h>
#include <swapfs.h>
#include <swap_fifo.h>
#include <stdio.h>
#include <string.h>
#include <memlayout.h>
#include <pmm.h>
#include <mmu.h>
#include <default_pmm.h>
#include <kdebug.h>

/** @brief Number of virtual pages used by the self-test (0x1000 .. 0x5000). */
#define CHECK_VALID_VIR_PAGE_NUM 5
/** @brief First virtual address of the test VMA. */
#define BEING_CHECK_VALID_VADDR 0X1000
/** @brief End of the test VMA (exclusive): the valid test addresses are below this value. */
#define CHECK_VALID_VADDR (CHECK_VALID_VIR_PAGE_NUM + 1) * 0x1000
/** @brief Number of physical pages left free for the test: fewer than virtual pages, so swapping is forced. */
#define CHECK_VALID_PHY_PAGE_NUM 4
/** @brief Size of the access sequence arrays swap_in_seq_no and swap_out_seq_no. */
#define MAX_SEQ_NO 10

/** @brief The active swap manager, chosen in swap_init(). */
static struct swap_manager *sm;
/** @brief Number of page slots on the swap device; set by swapfs_init(), declared in swap.h. */
size_t max_swap_offset;

/** @brief 1 once swap_init() has finished; alloc_pages() and mm_create() look at it. */
volatile int swap_init_ok = 0;

/** @brief Per-page record for the self-test; unused by the code of this lab. */
unsigned int swap_page[CHECK_VALID_VIR_PAGE_NUM];

/** @brief Access sequence records for the self-test; filled with -1 by check_swap() and not read. */
unsigned int swap_in_seq_no[MAX_SEQ_NO], swap_out_seq_no[MAX_SEQ_NO];

static void check_swap(void);

/**
 * @brief Start the swap file system, select the swap manager and run the self-test.
 *
 * The swap device must hold at least 7 pages (the self-test needs them) and at most
 * MAX_SWAP_OFFSET_LIMIT. The manager is swap_manager_fifo. On success swap_init_ok is set and
 * check_swap() runs, which prints its progress and "check_swap() succeeded!".
 *
 * @return the result of the manager's init (0 on success); panics on a bad device size
 */
int swap_init(void) {
    swapfs_init();

    // Since the IDE is faked, it can only store 7 pages at most to pass the test
    if (!(7 <= max_swap_offset && max_swap_offset < MAX_SWAP_OFFSET_LIMIT)) {
        panic("bad max_swap_offset %08x.\n", max_swap_offset);
    }

    sm = &swap_manager_fifo;
    int r = sm->init();

    if (r == 0) {
        swap_init_ok = 1;
        cprintf("SWAP: manager = %s\n", sm->name);
        check_swap();
    }

    return r;
}

/**
 * @brief Initialise the swap manager's private data of one mm (calls swap_manager::init_mm).
 * @param mm address space
 * @return the result of the manager's init_mm
 */
int swap_init_mm(struct mm_struct *mm) {
    return sm->init_mm(mm);
}

/**
 * @brief Forward a timer tick to the swap manager (calls swap_manager::tick_event).
 * @param mm address space
 * @return the result of the manager's tick_event
 */
int swap_tick_event(struct mm_struct *mm) {
    return sm->tick_event(mm);
}

/**
 * @brief Tell the swap manager that a page may be evicted (calls swap_manager::map_swappable).
 * @param mm      address space the page belongs to
 * @param addr    virtual address of the page
 * @param page    the physical page
 * @param swap_in 1 if the page was just swapped in, 0 if it is newly allocated
 * @return the result of the manager's map_swappable
 */
int swap_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in) {
    return sm->map_swappable(mm, addr, page, swap_in);
}

/**
 * @brief Tell the swap manager that a page must not be evicted (calls swap_manager::set_unswappable).
 * @param mm   address space the page belongs to
 * @param addr virtual address of the page
 * @return the result of the manager's set_unswappable
 */
int swap_set_unswappable(struct mm_struct *mm, uintptr_t addr) {
    return sm->set_unswappable(mm, addr);
}

/** @brief Number of swap-outs; declared here but never updated or read in this lab. */
volatile unsigned int swap_out_num = 0;

/**
 * @brief Evict up to n pages to the swap device.
 *
 * For each page: ask the manager for a victim, write it to the swap device, put the swap entry in
 * its PTE, free the page and invalidate the TLB entry. If the write fails the page goes back to
 * the manager and the loop continues.
 *
 * The slot of a page is derived from its virtual address: entry = (pra_vaddr / PGSIZE + 1) << 8,
 * so every virtual page has a fixed slot and slot 0 is never used. This is enough for the
 * self-test (five virtual pages) but two pages with the same address in different address spaces
 * would collide, so it is not a general swap-slot allocator.
 *
 * @param mm      address space to evict from
 * @param n       number of pages to evict
 * @param in_tick 1 if called from a timer tick, 0 otherwise (passed to swap_out_victim)
 * @return the number of loop passes that ran, which is n unless the manager had no victim
 */
int swap_out(struct mm_struct *mm, int n, int in_tick) {
    int i;
    for (i = 0; i != n; ++i) {
        uintptr_t v;
        // struct Page **ptr_page=NULL;
        struct Page *page;
        // cprintf("i %d, SWAP: call swap_out_victim\n",i);
        int r = sm->swap_out_victim(mm, &page, in_tick);
        if (r != 0) {
            cprintf("i %d, swap_out: call swap_out_victim failed\n", i);
            break;
        }
        // assert(!PageReserved(page));

        // cprintf("SWAP: choose victim page 0x%08x\n", page);

        v = page->pra_vaddr;
        pte_t *ptep = get_pte(mm->pgdir, v, 0);
        assert((*ptep & PTE_V) != 0);

        if (swapfs_write((page->pra_vaddr / PGSIZE + 1) << 8, page) != 0) {
            cprintf("SWAP: failed to save\n");
            sm->map_swappable(mm, v, page, 0);
            continue;
        } else {
            cprintf("swap_out: i %d, store page in vaddr 0x%x to disk swap entry %d\n", i, v,
                    page->pra_vaddr / PGSIZE + 1);
            *ptep = (page->pra_vaddr / PGSIZE + 1) << 8;
            free_page(page);
        }

        tlb_invalidate(mm->pgdir, v);
    }
    return i;
}

/**
 * @brief Read the page of a swap entry back into a newly allocated physical page.
 *
 * alloc_page() inside swap_in() may itself call swap_out() when memory is full. The caller maps
 * the page afterwards with page_insert().
 *
 * @param mm         address space whose PTE holds the swap entry
 * @param addr       virtual address of the page
 * @param ptr_result receives the new physical page
 * @return 0. Panics if no page can be allocated. A failed swapfs_read() is not reported: the
 *         assert(r != 0) sits in the branch where r is already non-zero
 */
int swap_in(struct mm_struct *mm, uintptr_t addr, struct Page **ptr_result) {
    struct Page *result = alloc_page();
    assert(result != NULL);

    pte_t *ptep = get_pte(mm->pgdir, addr, 0);
    // cprintf("SWAP: load ptep %x swap entry %d to vaddr 0x%08x, page %x, No %d\n", ptep, (*ptep)>>8, addr, result, (result-pages));

    int r;
    if ((r = swapfs_read((*ptep), result)) != 0) {
        assert(r != 0);
    }
    cprintf("swap_in: load disk swap entry %d with swap_page in vadr 0x%x\n", (*ptep) >> 8, addr);
    *ptr_result = result;
    return 0;
}

/**
 * @brief First phase of the self-test: write to the four pages 0x1000 .. 0x4000.
 *
 * Each page faults on its first write (pgfault_num goes from 1 to 4) and stays resident, and a
 * second write to the same page must not fault again.
 */
static inline void check_content_set(void) {
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 1);
    *(unsigned char *)0x1010 = 0x0a;
    assert(pgfault_num == 1);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 2);
    *(unsigned char *)0x2010 = 0x0b;
    assert(pgfault_num == 2);
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 3);
    *(unsigned char *)0x3010 = 0x0c;
    assert(pgfault_num == 3);
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 4);
    *(unsigned char *)0x4010 = 0x0d;
    assert(pgfault_num == 4);
}

/**
 * @brief Second phase of the self-test: the access sequence of the active manager.
 * @return the result of the manager's check_swap (0 if every expected page fault count matched)
 */
static inline int check_content_access(void) {
    int ret = sm->check_swap();
    return ret;
}

/** @brief The physical pages that check_swap() reserves as the only free memory during the test. */
struct Page *check_rp[CHECK_VALID_PHY_PAGE_NUM];
/** @brief The PTEs of the four test pages, used to check that they are resident. */
pte_t *check_ptep[CHECK_VALID_PHY_PAGE_NUM];
/** @brief Virtual addresses of the test pages; unused by the code of this lab. */
unsigned int check_swap_addr[CHECK_VALID_VIR_PAGE_NUM];

extern free_area_t free_area;

/** @brief Shorthand for the free list of the physical memory manager. */
#define free_list (free_area.free_list)
/** @brief Shorthand for the number of free pages of the physical memory manager. */
#define nr_free (free_area.nr_free)

/**
 * @brief Self-test of the page replacement mechanism and of the active swap manager.
 *
 * Steps: (1) build an mm over the boot page table (check_mm_struct) with one VMA
 * [0x1000, 0x6000); (2) take four physical pages as the only free memory, so that the fifth
 * page of the VMA forces a swap-out; (3) write to the four pages (check_content_set()); (4) run
 * the manager's access sequence (check_content_access()), which asserts the page fault count
 * after every access; (5) put the free list and the page tables back as they were and check that
 * no page leaked. Uses assert(), so a failure panics.
 */
static void check_swap(void) {
    // backup mem env
    int ret, count = 0, total = 0, i;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count++, total += p->property;
    }
    assert(total == nr_free_pages());
    cprintf("BEGIN check_swap: count %d, total %d\n", count, total);

    // now we set the phy pages env
    struct mm_struct *mm = mm_create();
    assert(mm != NULL);

    extern struct mm_struct *check_mm_struct;
    assert(check_mm_struct == NULL);

    check_mm_struct = mm;

    pde_t *pgdir = mm->pgdir = boot_pgdir_va;
    assert(pgdir[0] == 0);

    struct vma_struct *vma =
        vma_create(BEING_CHECK_VALID_VADDR, CHECK_VALID_VADDR, VM_WRITE | VM_READ);
    assert(vma != NULL);

    insert_vma_struct(mm, vma);

    // setup the temp Page Table vaddr 0~4MB
    cprintf("setup Page Table for vaddr 0X1000, so alloc a page\n");
    pte_t *temp_ptep = NULL;
    temp_ptep = get_pte(mm->pgdir, BEING_CHECK_VALID_VADDR, 1);
    assert(temp_ptep != NULL);
    cprintf("setup Page Table vaddr 0~4MB OVER!\n");

    for (i = 0; i < CHECK_VALID_PHY_PAGE_NUM; i++) {
        check_rp[i] = alloc_page();
        assert(check_rp[i] != NULL);
        assert(!PageProperty(check_rp[i]));
    }
    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    // assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;
    for (i = 0; i < CHECK_VALID_PHY_PAGE_NUM; i++) {
        free_pages(check_rp[i], 1);
    }
    assert(nr_free == CHECK_VALID_PHY_PAGE_NUM);

    cprintf("set up init env for check_swap begin!\n");
    // setup initial vir_page<->phy_page environment for page relpacement algorithm

    pgfault_num = 0;

    check_content_set();
    assert(nr_free == 0);
    for (i = 0; i < MAX_SEQ_NO; i++)
        swap_out_seq_no[i] = swap_in_seq_no[i] = -1;

    for (i = 0; i < CHECK_VALID_PHY_PAGE_NUM; i++) {
        check_ptep[i] = 0;
        check_ptep[i] = get_pte(pgdir, (i + 1) * 0x1000, 0);
        // cprintf("i %d, check_ptep addr %x, value %x\n", i, check_ptep[i], *check_ptep[i]);
        assert(check_ptep[i] != NULL);
        assert(pte2page(*check_ptep[i]) == check_rp[i]);
        assert((*check_ptep[i] & PTE_V));
    }
    cprintf("set up init env for check_swap over!\n");
    // now access the virt pages to test  page relpacement algorithm
    ret = check_content_access();
    assert(ret == 0);

    nr_free = nr_free_store;
    free_list = free_list_store;

    // restore kernel mem env
    for (i = 0; i < CHECK_VALID_PHY_PAGE_NUM; i++) {
        free_pages(check_rp[i], 1);
    }

    // free_page(pte2page(*temp_ptep));

    mm->pgdir = NULL;
    mm_destroy(mm);
    check_mm_struct = NULL;

    pde_t *pd1 = pgdir, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    pgdir[0] = 0;
    flush_tlb();

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count--, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);

    cprintf("check_swap() succeeded!\n");
}
