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
#include <sync.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    { page = pmm_manager->alloc_pages(n); }
    local_intr_restore(intr_flag);
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    bool intr_flag;
    local_intr_save(intr_flag);
    { pmm_manager->free_pages(base, n); }
    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    { ret = pmm_manager->nr_free_pages(); }
    local_intr_restore(intr_flag);
    return ret;
}

//lab2 请补充你在lab2中的代码

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


/* pmm_init - initialize the physical memory management */
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

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
