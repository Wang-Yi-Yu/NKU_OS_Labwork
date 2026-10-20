/**
 * @file   pmm.c
 * @brief  Framework layer of physical memory management (PMM): selects the
 *         physical memory manager, probes physical memory and builds the Page
 *         descriptors, and provides the unified entry points alloc_pages(),
 *         free_pages() and nr_free_pages().
 *
 * This file only holds the "framework". The real allocation algorithm is
 * supplied by a struct pmm_manager instance (First-Fit in default_pmm.c,
 * Best-Fit in best_fit_pmm.c, ...); alloc_pages() and friends here merely
 * forward the call to the currently selected ::pmm_manager.
 *
 * Initialisation call chain:
 * `pmm_init() -> init_pmm_manager() -> page_init() -> init_memmap() -> check_alloc_page()`
 */
#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <buddy_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>

/** @brief Virtual start address of the Page descriptor array (right after the kernel image, page aligned). */
struct Page *pages;
/** @brief Exclusive upper bound of the physical page numbers the kernel manages, in pages. */
size_t npage = 0;
/** @brief Offset of the kernel linear mapping: the kernel image is mapped at VA=KERNBASE, PA=0x80200000, so the offset is PHYSICAL_MEMORY_OFFSET. */
uint64_t va_pa_offset;
/** @brief Page number of the start of DRAM. RAM starts at 0x80000000 on RISC-V; DRAM_BASE is defined in riscv.h. */
const size_t nbase = DRAM_BASE / PGSIZE;

/** @brief Virtual address of the boot page table (boot_page_table_sv39). */
uintptr_t *satp_virtual = NULL;
/** @brief Physical address of the boot page table, i.e. the base written into the satp register. */
uintptr_t satp_physical;

/** @brief The physical memory manager currently in use, assigned by init_pmm_manager(). */
const struct pmm_manager *pmm_manager;

static void check_alloc_page(void);

/**
 * @brief Select the physical memory manager and initialise it.
 *
 * The allocation algorithm is switched by assigning a different instance to
 * ::pmm_manager (e.g. &default_pmm_manager or &best_fit_pmm_manager). The
 * manager's name is then printed and its init method called.
 *
 * Best-Fit is the default. Building with `make PMM=buddy` defines PMM_BUDDY
 * and selects the Buddy System manager instead.
 */
static void init_pmm_manager(void) {
#ifdef PMM_BUDDY
    //lab2 function3 2414007
    pmm_manager = &buddy_pmm_manager;
#else
    pmm_manager = &best_fit_pmm_manager;
#endif
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
 * @details Allocates `n * PGSIZE` bytes of contiguous physical memory.
 */
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

/**
 * @brief Free n contiguous physical pages (forwards to pmm_manager->free_pages).
 */
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

/**
 * @brief Query the number of free physical pages (forwards to pmm_manager->nr_free_pages).
 * @details The amount of free memory in bytes is `nr_free_pages() * PGSIZE`.
 */
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
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
 * @brief Initialise physical memory management.
 *
 * 1. init_pmm_manager(): choose the allocation algorithm and initialise its internal data structures;
 * 2. page_init(): probe the physical memory range, reserve memory that is already in use, then build
 *    the free page list through pmm->init_memmap;
 * 3. check_alloc_page(): run the manager's check method to verify allocation/free;
 * 4. print the virtual/physical address of the boot page table boot_page_table_sv39.
 *
 * The granularity of every allocation/free is a 4KB page.
 */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t *)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual,
            satp_physical);
}

/**
 * @brief Run the current manager's self test and print a message on success.
 * @details A failed check triggers assert/panic and does not return.
 */
static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
