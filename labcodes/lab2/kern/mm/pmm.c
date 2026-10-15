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

//lab2 function1 YOUR CODE

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
