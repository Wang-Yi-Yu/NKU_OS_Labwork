/**
 * @file   pmm.h
 * @brief  Interface of physical memory management (PMM): the pmm_manager
 *         "class", the page allocation/free entry points, the page mapping
 *         helpers used by the lab4 exercises, and address/descriptor conversions.
 *
 * C has no classes, so struct pmm_manager (a table of function pointers) is the
 * interface. An allocation algorithm (First-Fit in default_pmm.c, Best-Fit, ...)
 * implements the methods and exports an XXX_pmm_manager instance; the instance
 * is selected in init_pmm_manager() (pmm.c).
 *
 * Keep the four kinds of "address" apart:
 *  - virtual address (va, uintptr_t): what kernel code dereferences, >= KERNBASE;
 *  - physical address (pa, uintptr_t): `va - va_pa_offset`; RAM starts at 0x80000000;
 *  - physical page number (ppn): `pa >> PGSHIFT`;
 *  - page descriptor (struct Page *): an element of ::pages, index = `ppn - nbase`.
 */
#ifndef __KERN_MM_PMM_H__
#define __KERN_MM_PMM_H__

#include <defs.h>
#include <mmu.h>
#include <memlayout.h>
#include <atomic.h>
#include <assert.h>

/** @name Clone flags for do_fork()
 *  @{ */
/** @brief Share the parent's address space (mm_struct / page directory) instead of copying it. */
/* fork flags used in do_fork*/
#define CLONE_VM     0x00000100  // set if VM shared between processes
#define CLONE_THREAD 0x00000200  // thread group

/** @} */

/**
 * @brief Physical memory manager "class": an interface made of function pointers.
 *
 * A concrete manager XXX_pmm_manager only needs to implement the methods below
 * to be usable by ucore for managing all physical memory. Callers never use the
 * methods directly: alloc_pages(), free_pages() and nr_free_pages() in pmm.c
 * dispatch to them with interrupts disabled.
 */
// pmm_manager is a physical memory management class. A special pmm manager - XXX_pmm_manager
// only needs to implement the methods in pmm_manager class, then XXX_pmm_manager can be used
// by ucore to manage the total physical memory space.
struct pmm_manager {
    const char *name;    // XXX_pmm_manager's name
    void (*init)(void);  // initialize internal description&management data structure
                         // (free block list, number of free block) of XXX_pmm_manager
    void (*init_memmap)(struct Page *base,
                        size_t n);  // setup description&management data structcure according to
                                    // the initial free physical memory space
    struct Page *(*alloc_pages)(
        size_t n);  // allocate >=n pages, depend on the allocation algorithm
    void (*free_pages)(
        struct Page *base,
        size_t n);  // free >=n pages with "base" addr of Page descriptor structures(memlayout.h)
    size_t (*nr_free_pages)(void);  // return the number of free pages
    void (*check)(void);            // check the correctness of XXX_pmm_manager
};

/** @brief The physical memory manager in use, set by init_pmm_manager() in pmm.c. */
extern const struct pmm_manager *pmm_manager;
/** @brief Virtual address of the boot page directory (root page table, boot_page_table_sv39). */
extern pde_t *boot_pgdir_va;
/** @brief Page number of the start of DRAM: DRAM_BASE / PGSIZE (0x80000 on RISC-V). */
extern const size_t nbase;
/** @brief Physical address of the boot page directory, i.e. PADDR(boot_pgdir_va). */
extern uintptr_t boot_pgdir_pa;

/**
 * @brief Initialise physical memory management; one step of kern_init().
 *
 * In order: choose and initialise the manager, probe physical memory and build
 * the Page array and free list, run the manager's self test, record the boot
 * page directory, run the page-table self tests and initialise kmalloc.
 */
void pmm_init(void);

/**
 * @brief Allocate n physically contiguous pages.
 * @param n number of pages (not bytes); n * PGSIZE bytes are allocated
 * @return descriptor of the first page, or NULL if no free block is large enough
 */
struct Page *alloc_pages(size_t n);
/**
 * @brief Free n physically contiguous pages.
 * @param base descriptor of the first page (a block previously returned by alloc_pages())
 * @param n    number of pages; must match the allocation
 */
void free_pages(struct Page *base, size_t n);
/**
 * @brief Query the total number of free physical pages.
 * @return number of free pages (multiply by PGSIZE for bytes)
 */
size_t nr_free_pages(void);

/** @brief Allocate one physical page; equivalent to `alloc_pages(1)`. */
#define alloc_page() alloc_pages(1)
/**
 * @brief Free one physical page; equivalent to `free_pages(page, 1)`.
 * @param page page descriptor
 */
#define free_page(page) free_pages(page, 1)

/**
 * @brief Return the kernel virtual address of the leaf PTE for linear address la.
 * @param pgdir  root page directory (kernel virtual address)
 * @param la     linear address
 * @param create non-zero: allocate missing page tables on the way
 * @return pointer to the PTE, or NULL if a table is missing and not created.
 *         See get_pte() in pmm.c for details.
 */
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create);
/**
 * @brief Return the page mapped at linear address la, if any.
 * @param pgdir      root page directory (kernel virtual address)
 * @param la         linear address
 * @param ptep_store if non-NULL, receives the leaf PTE pointer (or NULL)
 * @return page descriptor, or NULL if la has no valid mapping
 */
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store);
/**
 * @brief Remove the mapping of linear address la, if there is one (lab4 exercise, see pmm.c).
 * @param pgdir root page directory (kernel virtual address)
 * @param la    linear address whose mapping is removed; no-op if unmapped
 */
void page_remove(pde_t *pgdir, uintptr_t la);
/**
 * @brief Map physical page `page` at linear address la with permission perm (lab4 exercise, see pmm.c).
 * @param pgdir root page directory (kernel virtual address)
 * @param page  page descriptor to map
 * @param la    linear address; its previous mapping, if any, is replaced
 * @param perm  permission bits, e.g. PTE_U | PTE_W | PTE_R
 * @return 0 on success, -E_NO_MEM if a page table could not be allocated
 */
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);

/**
 * @brief Invalidate the TLB entry of linear address la after a PTE change.
 * @param pgdir root page directory whose entries were changed
 * @param la    linear address whose cached translation may be stale
 */
void tlb_invalidate(pde_t *pgdir, uintptr_t la);
/**
 * @brief Allocate a page and map it at la (alloc_page() + page_insert()).
 * @param pgdir root page directory
 * @param la    linear address to map
 * @param perm  permission bits of the mapping
 * @return the new page, or NULL if memory is insufficient
 * @warning Declared here but not defined in this lab's pmm.c; it is not called yet.
 */
struct Page *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm);

/**
 * @brief Print the page directory and its mappings (debugging aid).
 * @warning Declared here but not defined in this lab's pmm.c.
 */
void print_pgdir(void);

/**
 * @brief Convert a kernel virtual address to a physical address.
 *
 * kva must be at or above KERNBASE (the kernel linear-mapping area), otherwise
 * the kernel panics. The result is `kva - va_pa_offset`.
 * @param kva kernel virtual address
 * @return the corresponding physical address
 */
/* *
 * PADDR - takes a kernel virtual address (an address that points above KERNBASE),
 * where the machine's maximum 256MB of physical memory is mapped and returns the
 * corresponding physical address.  It panics if you pass it a non-kernel virtual address.
 * */
#define PADDR(kva)                                                 \
    ({                                                             \
        uintptr_t __m_kva = (uintptr_t)(kva);                      \
        if (__m_kva < KERNBASE) {                                  \
            panic("PADDR called with invalid kva %08lx", __m_kva); \
        }                                                          \
        __m_kva - va_pa_offset;                                    \
    })

/**
 * @brief Convert a physical address to a kernel virtual address (`pa + va_pa_offset`).
 *
 * Panics if the page number of pa is not below npage. Used to reach page tables
 * and pages through the kernel linear mapping, e.g. by page2kva() and get_pte().
 * @param pa physical address
 * @return the kernel virtual address
 */
/* *
 * KADDR - takes a physical address and returns the corresponding kernel virtual
 * address. It panics if you pass an invalid physical address.
 * */
#define KADDR(pa)                                                \
    ({                                                           \
        uintptr_t __m_pa = (pa);                                 \
        size_t __m_ppn = PPN(__m_pa);                            \
        if (__m_ppn >= npage) {                                  \
            panic("KADDR called with invalid pa %08lx", __m_pa); \
        }                                                        \
        (void *)(__m_pa + va_pa_offset);                         \
    })

/** @brief Virtual address of the Page descriptor array; `pages[i]` describes page number `i + nbase`. */
extern struct Page *pages;
/** @brief Exclusive upper bound of the managed physical page numbers (end of usable DRAM, in pages). */
extern size_t npage;
/** @brief Offset of the kernel linear mapping: `va = pa + va_pa_offset`; equals PHYSICAL_MEMORY_OFFSET. */
extern uint_t va_pa_offset;

/**
 * @brief Physical page number of a page descriptor.
 * @param page page descriptor, must lie inside ::pages
 * @return `page - pages + nbase`
 */
static inline ppn_t page2ppn(struct Page *page) {
    return page - pages + nbase;
}

/**
 * @brief Physical start address of a page.
 * @param page page descriptor
 * @return `page2ppn(page) << PGSHIFT`
 */
static inline uintptr_t page2pa(struct Page *page) {
    return page2ppn(page) << PGSHIFT;
}

/**
 * @brief Descriptor of the page that contains physical address pa.
 * @param pa physical address (need not be page aligned)
 * @return `&pages[PPN(pa) - nbase]`; panics if `PPN(pa) >= npage`
 * @warning `PPN(pa) < nbase` is not checked, so an address below DRAM yields a
 *          negative index.
 */
static inline struct Page *pa2page(uintptr_t pa) {
    if (PPN(pa) >= npage) {
        panic("pa2page called with invalid pa");
    }
    return &pages[PPN(pa) - nbase];
}

/**
 * @brief Kernel virtual address of a page's memory.
 * @param page page descriptor
 * @return `KADDR(page2pa(page))`
 */
static inline void *page2kva(struct Page *page) {
    return KADDR(page2pa(page));
}

/**
 * @brief Descriptor of the page that contains a kernel virtual address.
 * @param kva kernel virtual address (>= KERNBASE)
 * @return `pa2page(PADDR(kva))`
 */
static inline struct Page *kva2page(void *kva) {
    return pa2page(PADDR(kva));
}

/**
 * @brief Page referenced by a valid PTE.
 * @param pte page table entry; must have PTE_V set
 * @return the page descriptor of PTE_ADDR(pte); panics if PTE_V is clear
 */
static inline struct Page *pte2page(pte_t pte) {
    if (!(pte & PTE_V)) {
        panic("pte2page called with invalid pte");
    }
    return pa2page(PTE_ADDR(pte));
}

/**
 * @brief Page (e.g. a page table page) referenced by a page directory entry.
 * @param pde page directory entry; PTE_V is not checked
 * @return `pa2page(PDE_ADDR(pde))`
 */
static inline struct Page *pde2page(pde_t pde) {
    return pa2page(PDE_ADDR(pde));
}

/**
 * @brief Read the reference count of a page.
 * @param page page descriptor
 * @return current reference count
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
 * @brief Increment the reference count of a page (one more mapping).
 * @param page page descriptor
 * @return the count after incrementing
 */
static inline int page_ref_inc(struct Page *page) {
    page->ref += 1;
    return page->ref;
}

/**
 * @brief Decrement the reference count of a page (one mapping removed).
 * @param page page descriptor
 * @return the count after decrementing; 0 means the page can be freed
 */
static inline int page_ref_dec(struct Page *page) {
    page->ref -= 1;
    return page->ref;
}

/**
 * @brief Flush the whole TLB on this hart (`sfence.vma` with no operands).
 *
 * Use tlb_invalidate() for a single address; this variant is heavier.
 */
static inline void flush_tlb() {
    asm volatile("sfence.vma");
}

/**
 * @brief Build a leaf PTE from a physical page number and flag bits.
 * @param ppn  physical page number to store in the PPN field
 * @param type permission/flag bits, e.g. PTE_R | PTE_W | PTE_U
 * @return the PTE with PTE_V set
 */
// construct PTE from a page and permission bits
static inline pte_t pte_create(uintptr_t ppn, int type) {
    return (ppn << PTE_PPN_SHIFT) | PTE_V | type;
}

/**
 * @brief Build a non-leaf PTE (pointer to the next-level page table).
 * @param ppn physical page number of the next-level table
 * @return the PTE with only PTE_V set (R/W/X all 0)
 */
static inline pte_t ptd_create(uintptr_t ppn) {
    return pte_create(ppn, PTE_V);
}

/**
 * @brief Bounds of the boot kernel stack allocated in entry.S.
 *
 * bootstack is the lowest address and bootstacktop the highest (initial sp);
 * the stack is KSTACKSIZE bytes.
 */
extern char bootstack[], bootstacktop[];

#endif /* !__KERN_MM_PMM_H__ */
