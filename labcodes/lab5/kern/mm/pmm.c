/**
 * @file   pmm.c
 * @brief  Framework of physical memory management (PMM): selects the physical
 *         memory manager, probes physical memory and builds the Page array,
 *         provides the entry points alloc_pages(), free_pages() and
 *         nr_free_pages(), and hosts the page table helpers get_pte(),
 *         get_page() and the page_insert()/page_remove() exercise area.
 *
 * The allocation algorithm is supplied by a struct pmm_manager instance
 * (First-Fit in default_pmm.c). The three entry points disable interrupts while
 * they call into the manager (local_intr_save()/local_intr_restore()).
 *
 * Initialisation call chain (from kern_init):
 * `pmm_init() -> init_pmm_manager() -> page_init() -> check_alloc_page()
 *  -> check_pgdir() -> check_boot_pgdir() -> kmalloc_init()`
 */
#include <default_pmm.h>
#include <defs.h>
#include <error.h>
#include <kmalloc.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <dtb.h>
#include <stdio.h>
#include <string.h>
#include <sync.h>
#include <vmm.h>
#include <riscv.h>

/** @brief Virtual address of the Page descriptor array; `pages[i]` describes page number `i + nbase`. Set by page_init(). */
struct Page *pages;
/** @brief Exclusive upper bound of the managed physical page numbers (end of usable DRAM / PGSIZE). Set by page_init(). */
size_t npage = 0;
/** @brief Offset of the kernel linear mapping (`va = pa + va_pa_offset`); set to PHYSICAL_MEMORY_OFFSET by page_init(). */
uint_t va_pa_offset;
/** @brief Page number of the start of DRAM (DRAM_BASE / PGSIZE, 0x80000 on RISC-V); the index base of ::pages. */
const size_t nbase = DRAM_BASE / PGSIZE;

/** @brief Virtual address of the boot page directory (boot_page_table_sv39 in entry.S); set in pmm_init(). */
pde_t *boot_pgdir_va = NULL;
/** @brief Physical address of the boot page directory, PADDR(boot_pgdir_va). */
uintptr_t boot_pgdir_pa;

/** @brief The physical memory manager in use, assigned by init_pmm_manager(). */
const struct pmm_manager *pmm_manager;

static void check_alloc_page(void);
static void check_pgdir(void);
static void check_boot_pgdir(void);

/**
 * @brief Select the physical memory manager and initialise it.
 *
 * The allocation algorithm is switched by assigning a different instance to
 * ::pmm_manager (e.g. &default_pmm_manager). The manager's name is printed and
 * its init method is called.
 */
static void init_pmm_manager(void) {
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

/**
 * @brief Register a range of initially free physical pages with the current manager.
 * @param base descriptor of the first page of the range
 * @param n    number of pages in the range
 */
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

/**
 * @brief Allocate n contiguous physical pages (forwards to pmm_manager->alloc_pages).
 * @details Interrupts are disabled while the manager works on its free list.
 * @param n number of pages (not bytes)
 * @return descriptor of the first page, or NULL if no free block is large enough
 */
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    { page = pmm_manager->alloc_pages(n); }
    local_intr_restore(intr_flag);
    return page;
}

/**
 * @brief Free n contiguous physical pages (forwards to pmm_manager->free_pages).
 * @details Interrupts are disabled while the manager works on its free list.
 * @param base descriptor of the first page
 * @param n    number of pages; must match the earlier allocation
 */
void free_pages(struct Page *base, size_t n) {
    bool intr_flag;
    local_intr_save(intr_flag);
    { pmm_manager->free_pages(base, n); }
    local_intr_restore(intr_flag);
}

/**
 * @brief Query the number of free physical pages (forwards to pmm_manager->nr_free_pages).
 * @details Free bytes = `nr_free_pages() * PGSIZE`. Interrupts are disabled during the query.
 * @return number of free pages
 */
size_t nr_free_pages(void) {
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    { ret = pmm_manager->nr_free_pages(); }
    local_intr_restore(intr_flag);
    return ret;
}

//lab2 function1 2414007

/**
 * @brief Probe physical memory and build the Page descriptors.
 * @author Yiyu Wang(2414007)
 *
 * Memory layout built by this function (physical addresses grow to the right):
 * @code
 * | OpenSBI | kernel image | gap | Page array | gap | free pages ...  | tail |
 * 0x80000000  0x80200000   end   pages        mem_begin             mem_end
 * @endcode
 * Only the free pages between `mem_begin` and `mem_end` are handed to the
 * current ::pmm_manager; everything else stays reserved.
 *
 * Steps:
 *  1. set ::va_pa_offset to PHYSICAL_MEMORY_OFFSET (the kernel linear mapping);
 *  2. read the DRAM base and size found in the device tree and print them;
 *  3. set ::npage to the page number of the (capped) end of DRAM;
 *  4. place the Page array, page aligned, right after the kernel symbol `end`,
 *     and mark every descriptor reserved so no page is allocatable early;
 *  5. compute the first physical address behind the Page array, round it up and
 *     the end of DRAM down to page boundaries, and register the pages in between
 *     through init_memmap().
 *
 * @note The `end` symbol and ::pages are virtual addresses; they are converted
 *       with PADDR() before being compared with physical addresses.
 * @note The partial page at the end of DRAM (if any) is never given to the allocator.
 * @warning Panics if the device tree did not provide a memory range. init_memmap()
 *          is not called when no complete free page is left.
 */
static void page_init(void) {
    extern char end[];

    va_pa_offset = PHYSICAL_MEMORY_OFFSET;

    uint64_t mem_begin = get_memory_base();
    uint64_t mem_size = get_memory_size();
    if (mem_begin == 0 || mem_size == 0) {
        panic("DTB memory info not available");
    }
    uint64_t mem_end = mem_begin + mem_size;

    cprintf("physical memory map:\n");
    cprintf("  memory: 0x%016lx, [0x%016lx, 0x%016lx].\n", mem_size, mem_begin, mem_end - 1);

    // The kernel only manages memory below PHYSICAL_MEMORY_END.
    if (mem_end > PHYSICAL_MEMORY_END) {
        mem_end = PHYSICAL_MEMORY_END;
    }
    npage = mem_end / PGSIZE;

    // The Page array starts at the first page boundary behind the kernel image and
    // has one entry for each page in [nbase, npage). Clear it first: the memory
    // behind `end` is not guaranteed to be zero.
    pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE);
    memset(pages, 0, sizeof(struct Page) * (npage - nbase));
    for (size_t i = 0; i < npage - nbase; i++) {
        SetPageReserved(pages + i);
    }

    // The first physical address behind the Page array; everything below it is
    // occupied by OpenSBI, the kernel image or the Page array itself.
    uintptr_t freemem = PADDR((uintptr_t)pages + sizeof(struct Page) * (npage - nbase));
    mem_begin = ROUNDUP(freemem, PGSIZE);
    mem_end = ROUNDDOWN(mem_end, PGSIZE);
    if (mem_begin < mem_end) {
        init_memmap(pa2page(mem_begin), (mem_end - mem_begin) / PGSIZE);
    }
}

/**
 * @brief Map a physical range linearly (la -> pa) into a page directory.
 *
 * Every page of [la, la + size) gets a leaf PTE with `pa + (va - la)` and
 * permission perm; missing intermediate tables are created by get_pte().
 * @param pgdir root page directory (kernel virtual address)
 * @param la    linear (virtual) start address
 * @param size  number of bytes; the range is rounded out to whole pages
 * @param pa    physical start address
 * @param perm  permission bits of the leaf PTEs, e.g. READ_WRITE
 * @note Requires `PGOFF(la) == PGOFF(pa)`. Currently unused in this lab; intended
 *       for building the kernel mapping with per-section permissions.
 */
static void boot_map_segment(pde_t *pgdir, uintptr_t la, size_t size, uintptr_t pa, uint32_t perm) {
    assert(PGOFF(la) == PGOFF(pa));
    size_t n = ROUNDUP(size + PGOFF(la), PGSIZE) / PGSIZE;
    la = ROUNDDOWN(la, PGSIZE);
    pa = ROUNDDOWN(pa, PGSIZE);
    for (; n > 0; n--, la += PGSIZE, pa += PGSIZE) {
        pte_t *ptep = get_pte(pgdir, la, 1);
        assert(ptep != NULL);
        *ptep = pte_create(pa >> PGSHIFT, PTE_V | perm);
    }
}

/**
 * @brief Allocate one physical page for a page table and return its kernel virtual address.
 * @return kernel virtual address of the new (not zeroed) page; panics if no memory is left
 * @note Currently unused in this lab.
 */
static void *boot_alloc_page(void) {
    struct Page *p = alloc_page();
    if (p == NULL) {
        panic("boot_alloc_page failed.\n");
    }
    return page2kva(p);
}

/**
 * @brief Initialise memory management; called once from kern_init().
 *
 * Order: init_pmm_manager(), page_init(), check_alloc_page(), record
 * boot_pgdir_va / boot_pgdir_pa, check_pgdir(), a static_assert that KERNBASE
 * and KERNTOP are PTSIZE aligned, check_boot_pgdir(), kmalloc_init().
 */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other
    // size).
    // So a framework of physical memory manager (struct pmm_manager)is defined
    // in pmm.h
    // First we should init a physical memory manager(pmm) based on the
    // framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a
    // pmm
    check_alloc_page();

    // create boot_pgdir, an initial page directory(Page Directory Table, PDT)
    extern char boot_page_table_sv39[];
    boot_pgdir_va = (pte_t *)boot_page_table_sv39;
    boot_pgdir_pa = PADDR(boot_pgdir_va);

    check_pgdir();

    static_assert(KERNBASE % PTSIZE == 0 && KERNTOP % PTSIZE == 0);

    // now the basic virtual memory map(see memalyout.h) is established.
    // check the correctness of the basic virtual memory map.
    check_boot_pgdir();

    kmalloc_init();
}

/**
 * @brief Find the leaf PTE of linear address la, creating page tables if asked.
 *
 * Walks the three-level Sv39 table from the root `pgdir`: PDX1(la) -> PDX0(la)
 * -> PTX(la). A missing intermediate table is allocated (one zeroed page, ref = 1)
 * only when `create` is non-zero. Intermediate entries are pointers with
 * only PTE_V set (R = W = X = 0; no U/A/D bits, which RISC-V reserves in non-leaf entries and
 * QEMU 8.x rejects), so the permissions of the leaf PTE alone decide the access.
 *
 * @param pgdir  kernel virtual address of the root page directory
 * @param la     linear address to look up; need not be page aligned
 * @param create non-zero: allocate missing page tables; zero: never allocate
 * @return kernel virtual address of the leaf PTE, or NULL if a table is missing and
 *         was not created (or allocation failed). The leaf PTE itself is not created,
 *         so *ptep may be 0 (PTE_V clear); the caller checks it.
 */
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create) {
    pde_t *pdep1 = &pgdir[PDX1(la)];
    if (!(*pdep1 & PTE_V)) {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL) {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        // QEMU 8.x adaptation: non-leaf entries must not carry U/A/D (was PTE_U | PTE_V).
        *pdep1 = ptd_create(page2ppn(page));
    }

    pde_t *pdep0 = &((pde_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
    if (!(*pdep0 & PTE_V)) {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL) {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        // QEMU 8.x adaptation: non-leaf entries must not carry U/A/D (was PTE_U | PTE_V).
        *pdep0 = ptd_create(page2ppn(page));
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
}

/**
 * @brief Return the page mapped at linear address la, if any.
 * @param pgdir      root page directory (kernel virtual address)
 * @param la         linear address
 * @param ptep_store if non-NULL, receives the leaf PTE pointer (NULL if the table is missing)
 * @return page descriptor, or NULL if la has no valid mapping. Never allocates tables.
 */
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store) {
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep_store != NULL) {
        *ptep_store = ptep;
    }
    if (ptep != NULL && *ptep & PTE_V) {
        return pte2page(*ptep);
    }
    return NULL;
}

/**
 * @brief Drop one mapping whose leaf PTE is already known (provided by the lab framework).
 *
 * If the PTE is valid, the reference count of the mapped page is decreased, the page is freed
 * when the count reaches 0, the PTE is cleared and the TLB entry of la is dropped.
 * @param pgdir root page directory
 * @param la    linear address of the mapping
 * @param ptep  leaf PTE of la
 */
static inline void page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep) {
    if (*ptep & PTE_V) {
        struct Page *page = pte2page(*ptep);
        page_ref_dec(page);
        if (page_ref(page) == 0) {
            free_page(page);
        }
        *ptep = 0;
        tlb_invalidate(pgdir, la);
    }
}

/**
 * @brief Unmap every page in [start, end) of a user address space (provided by the lab framework).
 *
 * Walks the range with get_pte() and calls page_remove_pte() for each non-empty PTE; a missing
 * page table lets the walk skip to the next PTSIZE boundary. The page tables themselves stay;
 * exit_range() frees them.
 * @param pgdir root page directory
 * @param start page-aligned start address, must be a user address
 * @param end   page-aligned end address (exclusive), must be a user address
 */
void unmap_range(pde_t *pgdir, uintptr_t start, uintptr_t end) {
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));

    do {
        pte_t *ptep = get_pte(pgdir, start, 0);
        if (ptep == NULL) {
            start = ROUNDDOWN(start + PTSIZE, PTSIZE);
            continue;
        }
        if (*ptep != 0) {
            page_remove_pte(pgdir, start, ptep);
        }
        start += PGSIZE;
    } while (start != 0 && start < end);
}

/**
 * @brief Free the page tables of [start, end) that no longer hold a valid entry (provided by the
 *        lab framework).
 *
 * For every level-0 table in the range that has no valid PTE the table page is freed and its entry
 * cleared; a level-1 directory whose entries are all gone is freed as well. Call it after
 * unmap_range() of the same range.
 * @param pgdir root page directory
 * @param start page-aligned start address, must be a user address
 * @param end   page-aligned end address (exclusive), must be a user address
 */
void exit_range(pde_t *pgdir, uintptr_t start, uintptr_t end) {
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));

    uintptr_t d1start, d0start;
    int free_pt, free_pd0;
    pde_t *pd0, *pt, pde1, pde0;
    d1start = ROUNDDOWN(start, PDSIZE);
    d0start = ROUNDDOWN(start, PTSIZE);
    do {
        // level 1 page directory entry
        pde1 = pgdir[PDX1(d1start)];
        // if there is a valid entry, get into level 0
        // and try to free all page tables pointed to by
        // all valid entries in level 0 page directory,
        // then try to free this level 0 page directory
        // and update level 1 entry
        if (pde1 & PTE_V) {
            pd0 = page2kva(pde2page(pde1));
            // try to free all page tables
            free_pd0 = 1;
            do {
                pde0 = pd0[PDX0(d0start)];
                if (pde0 & PTE_V) {
                    pt = page2kva(pde2page(pde0));
                    // try to free page table
                    free_pt = 1;
                    for (int i = 0; i < NPTEENTRY; i++)
                        if (pt[i] & PTE_V) {
                            free_pt = 0;
                            break;
                        }
                    // free it only when all entry are already invalid
                    if (free_pt) {
                        free_page(pde2page(pde0));
                        pd0[PDX0(d0start)] = 0;
                    }
                } else
                    free_pd0 = 0;
                d0start += PTSIZE;
            } while (d0start != 0 && d0start < d1start + PDSIZE && d0start < end);
            // free level 0 page directory only when all pde0s in it are already invalid
            if (free_pd0) {
                free_page(pde2page(pde1));
                pgdir[PDX1(d1start)] = 0;
            }
        }
        d1start += PDSIZE;
        d0start = d1start;
    } while (d1start != 0 && d1start < end);
}

//lab5 function3 YOUR CODE
// Contract of int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share):
// copy the user pages of [start, end) (page aligned, user addresses) from the page table `from`
// into the page table `to`. For every valid PTE of `from` in the range, allocate a page, copy the
// 4 KiB content from the source page, and map the new page in `to` at the same address with the
// permissions of the source PTE (PTE_U included); create missing page tables in `to` with
// get_pte(). `share` selects sharing instead of copying and is 0 for the plain fork of this lab.
// Return 0 on success, -E_NO_MEM if an allocation fails. Called by dup_mmap() (vmm.c).

//LAB4 function1 2414015
/**
 * @brief Remove the mapping of la, if one exists.
 *
 * A missing page table or a clear PTE is not an error: the call returns without changing
 * anything else.
 * @param pgdir root page directory (kernel virtual address)
 * @param la    linear address; need not be page aligned
 */
void page_remove(pde_t *pgdir, uintptr_t la) {
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep != NULL && (*ptep & PTE_V)) {
        page_remove_pte(pgdir, la, ptep);
    }
}

/**
 * @brief Map page at la with permission perm, replacing any previous mapping.
 *
 * The new reference is counted before an old mapping of the same page is considered, so that
 * page is not freed and then reused. A leaf that grants R, W or X also gets A and D. That is
 * not required on QEMU 8.2.2 (it updates them itself), but an implementation may fault on
 * A = 0 or D = 0 instead, so they are set up front to stay portable.
 * @param pgdir root page directory (kernel virtual address)
 * @param page  physical page to map
 * @param la    linear address
 * @param perm  permission bits besides PTE_V (added by pte_create)
 * @return 0, or -E_NO_MEM if an intermediate page table cannot be allocated
 */
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm) {
    // Allocate missing intermediate tables first: on failure nothing has been modified yet.
    pte_t *ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL) {
        return -E_NO_MEM;
    }
    // Count the new reference before touching the old mapping, so that remapping the same page
    // never drops its count to 0.
    page_ref_inc(page);
    if (*ptep & PTE_V) {
        struct Page *mapped = pte2page(*ptep);
        if (mapped == page) {
            // Same page: only the permission changes, undo the extra reference.
            page_ref_dec(page);
        } else {
            // Different page: unmap the old one (frees it if this was its last reference).
            page_remove_pte(pgdir, la, ptep);
        }
    }
    // W without R is a reserved encoding in RISC-V.
    if (perm & PTE_W) {
        perm |= PTE_R;
    }
    if (perm & (PTE_R | PTE_W | PTE_X)) {
        perm |= PTE_A | PTE_D;
    }
    *ptep = pte_create(page2ppn(page), perm);
    tlb_invalidate(pgdir, la);
    return 0;
}


/**
 * @brief Invalidate the TLB entry of linear address la after its PTE was changed.
 *
 * Must be called after every change of a valid PTE. It issues `sfence.vma` with
 * rs1 = la, which drops translations for that address (all ASIDs) instead of the
 * whole TLB as flush_tlb() does.
 * @param pgdir page directory whose entries were changed (not used; kept for the interface)
 * @param la    linear address whose cached translation may be stale
 */
void tlb_invalidate(pde_t *pgdir, uintptr_t la) {
    asm volatile("sfence.vma %0" : : "r"(la));
}

/**
 * @brief Allocate one physical page and map it at la in pgdir (provided by the lab framework).
 *
 * @param pgdir root page directory
 * @param la    linear address to map
 * @param perm  permission bits of the mapping
 * @return the page, or NULL if no page is free or page_insert() fails (the page is freed again)
 * @note Used by load_icode() to back the program and the stack of a user process.
 */
struct Page *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm) {
    struct Page *page = alloc_page();
    if (page != NULL) {
        if (page_insert(pgdir, page, la, perm) != 0) {
            free_page(page);
            return NULL;
        }
        // swap_map_swappable(check_mm_struct, la, page, 0);
        page->pra_vaddr = la;
        assert(page_ref(page) == 1);
    }

    return page;
}

/**
 * @brief Run the current manager's self test and print a message on success.
 * @details A failed check triggers assert/panic and does not return.
 */
static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

/**
 * @brief Self test of the page mapping functions on the boot page directory.
 *
 * Checks get_page() on an unmapped address, that page_insert() creates the
 * intermediate tables, that mapping the same page at a second address gives
 * reference count 2, and that page_remove() drops the reference counts back.
 * All temporary tables and pages are freed afterwards and the free page count
 * must be restored. boot_pgdir_va[0] is cleared at the end.
 * @note Failures panic through assert(). Depends on the lab's page_insert() and page_remove().
 */
static void check_pgdir(void) {
    // assert(npage <= KMEMSIZE / PGSIZE);
    // The memory starts at 2GB in RISC-V
    // so npage is always larger than KMEMSIZE / PGSIZE
    size_t nr_free_store;

    nr_free_store = nr_free_pages();

    assert(npage <= KERNTOP / PGSIZE);
    assert(boot_pgdir_va != NULL && (uint32_t)PGOFF(boot_pgdir_va) == 0);
    assert(get_page(boot_pgdir_va, 0x0, NULL) == NULL);

    struct Page *p1, *p2;
    p1 = alloc_page();
    assert(page_insert(boot_pgdir_va, p1, 0x0, 0) == 0);

    pte_t *ptep;
    assert((ptep = get_pte(boot_pgdir_va, 0x0, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert(page_ref(p1) == 1);

    ptep = (pte_t *)KADDR(PDE_ADDR(boot_pgdir_va[0]));
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;
    assert(get_pte(boot_pgdir_va, PGSIZE, 0) == ptep);

    p2 = alloc_page();
    assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(*ptep & PTE_U);
    assert(*ptep & PTE_W);
    // QEMU 8.x adaptation: the original `assert(boot_pgdir_va[0] & PTE_U)` is removed, because the
    // non-leaf entries created by get_pte() no longer carry PTE_U (QEMU 8.x rejects such entries).
    assert(page_ref(p2) == 1);

    assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2);
    assert(page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert((*ptep & PTE_U) == 0);

    page_remove(boot_pgdir_va, 0x0);
    assert(page_ref(p1) == 1);
    assert(page_ref(p2) == 0);

    page_remove(boot_pgdir_va, PGSIZE);
    assert(page_ref(p1) == 0);
    assert(page_ref(p2) == 0);

    assert(page_ref(pde2page(boot_pgdir_va[0])) == 1);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_pgdir() succeeded!\n");
}

/**
 * @brief Check the kernel mapping and a user-style mapping on the boot page directory.
 *
 * The intended checks walk the physical pages of [KERNBASE, npage * PGSIZE) and
 * assert that get_pte(boot_pgdir_va, KADDR(pa)) maps each one back to pa. Then a
 * page is mapped at 0x100 and 0x100 + PGSIZE, a string is written through one alias
 * and read through the other, and the temporary tables are freed.
 * @warning The loop variable `i` is declared as `int`, but KERNBASE is a 64-bit
 *          value. The initial value is truncated to a negative int, so the condition
 *          `i < npage * PGSIZE` is false at once and the kernel-mapping loop runs
 *          zero times on RV64. The other checks still run. Fixing this means
 *          changing `i` to a 64-bit type (not done here).
 */
static void check_boot_pgdir(void) {
    size_t nr_free_store;
    pte_t *ptep;
    int i;

    nr_free_store = nr_free_pages();

    for (i = ROUNDDOWN(KERNBASE, PGSIZE); i < npage * PGSIZE; i += PGSIZE) {
        assert((ptep = get_pte(boot_pgdir_va, (uintptr_t)KADDR(i), 0)) != NULL);
        assert(PTE_ADDR(*ptep) == i);
    }

    assert(boot_pgdir_va[0] == 0);

    struct Page *p;
    p = alloc_page();
    assert(page_insert(boot_pgdir_va, p, 0x100, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 1);
    assert(page_insert(boot_pgdir_va, p, 0x100 + PGSIZE, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 2);

    const char *str = "ucore: Hello world!!";
    strcpy((void *)0x100, str);
    assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);

    *(char *)(page2kva(p) + 0x100) = '\0';
    assert(strlen((const char *)0x100) == 0);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(p);
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_boot_pgdir() succeeded!\n");
}

/**
 * @brief Format the permission bits of a PTE as a 3-character string.
 *
 * Output format: `u` if PTE_U is set else `-`, then `r` (always), then `w` if
 * PTE_W is set else `-`, e.g. "ur-" or "-rw"; this is a string such as "ur-".
 * @param perm PTE permission bits (PTE_U and PTE_W are examined)
 * @return pointer to a static buffer; overwritten by the next call (not reentrant)
 * @note Currently unused in this lab (static helper for the page table dump).
 */
static const char *perm2str(int perm) {
    static char str[4];
    str[0] = (perm & PTE_U) ? 'u' : '-';
    str[1] = 'r';
    str[2] = (perm & PTE_W) ? 'w' : '-';
    str[3] = '\0';
    return str;
}

/**
 * @brief Find the next run of valid entries with the same permissions in a table.
 *
 * Scans table[start .. right) for the first valid entry (PTE_V set) and extends
 * the run while the permission bits (PTE_USER mask) stay the same. The run is
 * reported as the half-open index range [*left_store, *right_store).
 *
 * @param left        unused (kept from the original interface)
 * @param right       exclusive end index of the scan (NPDEENTRY or NPTEENTRY)
 * @param start       first index to examine
 * @param table       the page table (array of PTE/PDE values)
 * @param left_store  out: first index of the run; may be NULL
 * @param right_store out: index just after the run; may be NULL
 * @return permission bits of the run (non-zero, includes PTE_V), or 0 if there
 *         is no valid entry in [start, right)
 * @note The original comments give X_SIZE as 4 MiB (32-bit ucore). On Sv39 a page
 *       directory entry covers PTSIZE = 2 MiB and a PTE covers PGSIZE = 4 KiB.
 *       Currently unused in this lab.
 */
static int get_pgtable_items(size_t left, size_t right, size_t start, uintptr_t *table,
                             size_t *left_store, size_t *right_store) {
    if (start >= right) {
        return 0;
    }
    while (start < right && !(table[start] & PTE_V)) {
        start++;
    }
    if (start < right) {
        if (left_store != NULL) {
            *left_store = start;
        }
        int perm = (table[start++] & PTE_USER);
        while (start < right && (table[start] & PTE_USER) == perm) {
            start++;
        }
        if (right_store != NULL) {
            *right_store = start;
        }
        return perm;
    }
    return 0;
}
