/**
 * @file   pmm.h
 * @brief  Unified interface of physical memory management (PMM): the
 *         pmm_manager abstraction, page allocation/free entry points, and
 *         conversions between virtual addresses, physical addresses, physical
 *         page numbers and struct Page.
 *
 * C has no classes, so a struct of function pointers (struct pmm_manager) plays
 * the role of an interface: any allocation algorithm (First-Fit, Best-Fit, Buddy
 * System, ...) only has to implement its methods and export an `XXX_pmm_manager`
 * instance; selecting it in init_pmm_manager() (pmm.c) makes the kernel use it.
 *
 * Keep the four kinds of "address" apart:
 *  - virtual address (va, uintptr_t): what kernel code really uses, at or above KERNBASE;
 *  - physical address (pa, uintptr_t): `va - va_pa_offset`; RAM starts at 0x80000000;
 *  - physical page number (ppn, ppn_t): `pa >> PGSHIFT`;
 *  - page descriptor (struct Page *): an element of ::pages, index = `ppn - nbase`.
 */
#ifndef __KERN_MM_PMM_H__
#define __KERN_MM_PMM_H__

#include <assert.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>

/**
 * @brief Physical memory manager "class": an interface made of function pointers.
 *
 * A concrete manager XXX_pmm_manager only needs to implement the methods below
 * to be usable by ucore for managing all physical memory.
 */
struct pmm_manager {
    const char *name; /**< Manager name, printed at boot, e.g. "default_pmm_manager". */

    /** Initialise the manager's internal data structures (e.g. free block list, free page count). */
    void (*init)(void);

    /**
     * Set up the management data structures for the initial free physical memory range.
     * @param base struct Page of the first physical page of the range
     * @param n    number of pages in the range
     */
    void (*init_memmap)(struct Page *base, size_t n);

    /**
     * Allocate at least n contiguous physical pages; which block is chosen depends on the algorithm.
     * @param n number of pages needed
     * @return struct Page of the first allocated page, or NULL if the request cannot be satisfied
     */
    struct Page *(*alloc_pages)(size_t n);

    /**
     * Free n contiguous physical pages starting at base.
     * @param base struct Page of the first page of the range to free
     * @param n    number of pages
     */
    void (*free_pages)(struct Page *base, size_t n);

    /** Return the total number of free physical pages. */
    size_t (*nr_free_pages)(void);

    /** Self test: check that allocation/free of this manager behaves correctly (assert/panic on failure). */
    void (*check)(void);
};

/** @brief The physical memory manager currently in use, set by init_pmm_manager() in pmm.c. */
extern const struct pmm_manager *pmm_manager;

/**
 * @brief Initialise physical memory management; one step of kern_init().
 *
 * In order: choose and initialise the manager, probe physical memory, build the
 * Page array and register the free area, run the manager's self test, and print
 * the virtual/physical address of the boot page table.
 */
void pmm_init(void);

/**
 * @brief Allocate n contiguous physical pages.
 * @param n number of pages
 * @return descriptor of the first page, or NULL if memory is insufficient
 */
struct Page *alloc_pages(size_t n);

/**
 * @brief Free n contiguous physical pages.
 * @param base descriptor of the first page (must be a value returned by alloc_pages)
 * @param n    number of pages (must match the allocation)
 */
void free_pages(struct Page *base, size_t n);

/**
 * @brief Query the total number of free physical pages.
 * @return number of free pages
 */
size_t nr_free_pages(void);  // number of free pages

/** @brief Allocate one physical page; equivalent to `alloc_pages(1)`. */
#define alloc_page() alloc_pages(1)
/**
 * @brief Free one physical page; equivalent to `free_pages(page, 1)`.
 * @param page page descriptor
 */
#define free_page(page) free_pages(page, 1)

/**
 * @brief Convert a kernel virtual address to a physical address.
 *
 * kva must be at or above KERNBASE (the kernel linear-mapping area), otherwise
 * the kernel panics. The conversion is `kva - va_pa_offset`.
 *
 * @param kva kernel virtual address
 * @return the corresponding physical address
 */
#define PADDR(kva)                                                 \
    ({                                                             \
        uintptr_t __m_kva = (uintptr_t)(kva);                      \
        if (__m_kva < KERNBASE) {                                  \
            panic("PADDR called with invalid kva %08lx", __m_kva); \
        }                                                          \
        __m_kva - va_pa_offset;                                    \
    })

/*
 * KADDR - convert a physical address to a kernel virtual address
 * (`pa + va_pa_offset`); panics if pa is invalid. Disabled in this lab, kept for
 * later labs.
 */
/*
#define KADDR(pa)                                                \
    ({                                                           \
        uintptr_t __m_pa = (pa);                                 \
        size_t __m_ppn = PPN(__m_pa);                            \
        if (__m_ppn >= npage) {                                  \
            panic("KADDR called with invalid pa %08lx", __m_pa); \
        }                                                        \
        (void *)(__m_pa + va_pa_offset);                         \
    })
*/

/** @brief Virtual start address of the Page descriptor array; `pages[i]` describes the frame with page number `i + nbase`. */
extern struct Page *pages;
/** @brief Exclusive upper bound of the page numbers the kernel manages, i.e. the page number of the end of DRAM. */
extern size_t npage;
/** @brief Page number of the start of DRAM: `DRAM_BASE / PGSIZE` (0x80000 on RISC-V). */
extern const size_t nbase;
/** @brief Offset of the kernel linear mapping, `va = pa + va_pa_offset`; should equal PHYSICAL_MEMORY_OFFSET. */
extern uint64_t va_pa_offset;

/**
 * @brief Get the physical page number of a page descriptor.
 * @param page page descriptor (must lie inside the ::pages array)
 * @return physical page number `page - pages + nbase`
 */
static inline ppn_t page2ppn(struct Page *page) {
    return page - pages + nbase;
}

/**
 * @brief Get the starting physical address of a page.
 * @param page page descriptor
 * @return physical address `page2ppn(page) << PGSHIFT`
 */
static inline uintptr_t page2pa(struct Page *page) {
    return page2ppn(page) << PGSHIFT;
}

/**
 * @brief Read the reference count of a page.
 * @param page page descriptor
 */
static inline int page_ref(struct Page *page) {
    return page->ref;
}

/**
 * @brief Set the reference count of a page.
 * @param page page descriptor
 * @param val  new count
 */
static inline void set_page_ref(struct Page *page, int val) {
    page->ref = val;
}

/**
 * @brief Increment the reference count.
 * @param page page descriptor
 * @return the count after incrementing
 */
static inline int page_ref_inc(struct Page *page) {
    page->ref += 1;
    return page->ref;
}

/**
 * @brief Decrement the reference count.
 * @param page page descriptor
 * @return the count after decrementing
 */
static inline int page_ref_dec(struct Page *page) {
    page->ref -= 1;
    return page->ref;
}

/**
 * @brief Get the descriptor of the page containing a physical address.
 * @param pa physical address (need not be page aligned)
 * @return `&pages[PPN(pa) - nbase]`; panics if `PPN(pa) >= npage`
 * @warning `PPN(pa) < nbase` is not checked, so an address below the start of
 *          DRAM yields an out-of-range index.
 */
static inline struct Page *pa2page(uintptr_t pa) {
    if (PPN(pa) >= npage) {
        panic("pa2page called with invalid pa");
    }
    return &pages[PPN(pa) - nbase];
}

/**
 * @brief Flush the TLB.
 * @warning `sfence.vm` is the old mnemonic of privileged spec 1.9 and is rejected
 *          by recent binutils; the rest of the kernel uses `sfence.vma`. This
 *          function is currently never called.
 */
static inline void flush_tlb() {
    asm volatile("sfence.vm");
}

/** @brief Bottom / top of the boot kernel stack (KSTACKSIZE bytes), allocated in .data of entry.S. */
extern char bootstack[], bootstacktop[];

#endif /* !__KERN_MM_PMM_H__ */
