#include <defs.h>
#include <list.h>
#include <memlayout.h>
#include <assert.h>
#include <kmalloc.h>
#include <sync.h>
#include <pmm.h>
#include <stdio.h>

/**
 * @file   kmalloc.c
 * @brief  Kernel small-object allocator (kmalloc / kfree) on top of the page
 *         allocator; a SLOB (Simple List Of Blocks) design adapted from Linux
 *         (Matt Mackall). The comment block below is the original description.
 *
 * Two allocation paths:
 *  - objects smaller than PAGE_SIZE - SLOB_UNIT are carved out of pages obtained
 *    from alloc_pages(). The heap is an address-ordered circular free list of
 *    slob_t units; allocation is first fit. Each object is preceded by a slob_t
 *    header that stores its size in units;
 *  - larger objects take whole pages from alloc_pages() directly and are recorded
 *    in the bigblocks list, so that kfree() can recognise them and find their order.
 *
 * Call chain: pmm_init() -> kmalloc_init() -> slob_init();
 * kmalloc() -> __kmalloc() -> slob_alloc() / __slob_get_free_pages();
 * kfree() -> slob_free() / __slob_free_pages().
 *
 * Interrupts are disabled while the lists are modified (spin_lock_irqsave is
 * a local_intr_save() wrapper), since the kernel is single-core.
 * This is the TA's implementation; only documentation is added here.
 */
/*
 * SLOB Allocator: Simple List Of Blocks
 *
 * Matt Mackall <mpm@selenic.com> 12/30/03
 *
 * How SLOB works:
 *
 * The core of SLOB is a traditional K&R style heap allocator, with
 * support for returning aligned objects. The granularity of this
 * allocator is 8 bytes on x86, though it's perhaps possible to reduce
 * this to 4 if it's deemed worth the effort. The slob heap is a
 * singly-linked list of pages from __get_free_page, grown on demand
 * and allocation from the heap is currently first-fit.
 *
 * Above this is an implementation of kmalloc/kfree. Blocks returned
 * from kmalloc are 8-byte aligned and prepended with a 8-byte header.
 * If kmalloc is asked for objects of PAGE_SIZE or larger, it calls
 * __get_free_pages directly so that it can return page-aligned blocks
 * and keeps a linked list of such pages and their orders. These
 * objects are detected in kfree() by their page alignment.
 *
 * SLAB is emulated on top of SLOB by simply calling constructors and
 * destructors for every SLAB allocation. Objects are returned with
 * the 8-byte alignment unless the SLAB_MUST_HWCACHE_ALIGN flag is
 * set, in which case the low-level allocator will fragment blocks to
 * create the proper alignment. Again, objects of page-size or greater
 * are allocated by calling __get_free_pages. As SLAB objects know
 * their size, no separate size bookkeeping is necessary and there is
 * essentially no allocation space overhead.
 */

/** @name Helper macros
 *  The "lock" is only an interrupt mask: spin_lock_irqsave(l, f) and
 *  spin_unlock_irqrestore(l, f) expand to local_intr_save(f) and
 *  local_intr_restore(f) from sync.h. The lock argument is discarded, so the
 *  names slob_lock and block_lock used below need not be defined.
 *  @{ */
// some helper
#define spin_lock_irqsave(l, f)      local_intr_save(f)
#define spin_unlock_irqrestore(l, f) local_intr_restore(f)
/** @brief Allocation flags type, kept for Linux-style signatures. Ignored: kmalloc() always passes 0. */
typedef unsigned int gfp_t;
/** @brief Size of the unit of page-granular allocations: PGSIZE (4096). */
#ifndef PAGE_SIZE
#define PAGE_SIZE PGSIZE
#endif

/** @brief Cache line size in bytes (64), used only for SLOB_ALIGN. */
#ifndef L1_CACHE_BYTES
#define L1_CACHE_BYTES 64
#endif

/**
 * @brief Round addr up to a multiple of size. size must be a power of two.
 * @param addr address or integer
 * @param size alignment in bytes
 */
#ifndef ALIGN
#define ALIGN(addr, size) (((addr) + (size) - 1) & (~((size) - 1)))
#endif

/** @} */

/**
 * @brief Free-block header of the SLOB heap (one "unit" of SLOB_UNIT bytes).
 *
 * A free block is a slob_t whose units give its length in SLOB_UNIT units
 * (header included) and whose next points to the next free block in address
 * order. Allocated objects reuse the same header to store their size.
 * @var slob_block::units  size of the block in SLOB_UNIT units
 * @var slob_block::next   next free block (address-ordered, circular)
 */
struct slob_block {
    int units;
    struct slob_block *next;
};
/** @brief One heap unit / block header of the SLOB allocator. */
typedef struct slob_block slob_t;

/**
 * @brief Heap granularity: sizeof(slob_t), i.e. 16 bytes on RV64 (int + padding + pointer).
 * @note The original comment at the top of this file says 8 bytes (x86). The
 *       effective unit here is 16 bytes.
 */
#define SLOB_UNIT sizeof(slob_t)
/**
 * @brief Number of SLOB units needed for size bytes, rounded up.
 * @param size bytes
 */
#define SLOB_UNITS(size) (((size) + SLOB_UNIT - 1) / SLOB_UNIT)
/** @brief Alignment used for objects that must be cache-line aligned; not used by kmalloc() here. */
#define SLOB_ALIGN L1_CACHE_BYTES

/**
 * @brief Record of one page-granular allocation (object size >= PAGE_SIZE - SLOB_UNIT).
 * @var bigblock::order  log2 of the number of pages allocated
 * @var bigblock::pages  kernel virtual address of the first page (the pointer returned to the caller)
 * @var bigblock::next   next record in bigblocks
 * The records themselves are small objects taken from the slob heap.
 */
struct bigblock {
    int order;
    void *pages;
    struct bigblock *next;
};
/** @brief Type of a page-granular allocation record. */
typedef struct bigblock bigblock_t;

/**
 * @brief Global state of the SLOB heap.
 *
 * - arena:     static one-unit free block pointing to itself. It makes the free list
 *              non-empty from the start; the first allocation grows the heap by a page.
 * - slobfree:  the free-list position where the next search starts (rover).
 * - bigblocks: head of the list of page-granular allocations (bigblock_t records).
 */
static slob_t arena = {.next = &arena, .units = 1};
static slob_t *slobfree = &arena;
static bigblock_t *bigblocks;

/**
 * @brief Allocate 2^order contiguous physical pages from the page allocator.
 * @param gfp   ignored
 * @param order log2 of the number of pages
 * @return kernel virtual address of the first page, or NULL if out of memory
 */
static void *__slob_get_free_pages(gfp_t gfp, int order) {
    struct Page *page = alloc_pages(1 << order);
    if (!page)
        return NULL;
    return page2kva(page);
}

/**
 * @brief Allocate one page for the slob heap (order 0).
 * @param gfp ignored
 */
#define __slob_get_free_page(gfp) __slob_get_free_pages(gfp, 0)

/**
 * @brief Free 2^order contiguous pages that start at kernel virtual address kva.
 * @param kva   kernel virtual address of the first page
 * @param order log2 of the number of pages
 */
static inline void __slob_free_pages(unsigned long kva, int order) {
    free_pages(kva2page(kva), 1 << order);
}

static void slob_free(void *b, int size);

/**
 * @brief First-fit allocation of size bytes from the slob heap.
 *
 * If no free block is large enough, one more page is requested and added to
 * the heap (except when size == PAGE_SIZE, which must not grow the heap).
 * When align is non-zero the returned block starts on an address that is a
 * multiple of align; the unaligned head is split off as a separate free block.
 *
 * @param size  bytes, including the slob_t header added by the caller;
 *              asserted to satisfy size + SLOB_UNIT < PAGE_SIZE
 * @param gfp   ignored
 * @param align required alignment in bytes, or 0 for none
 * @return the block (its units field is already set), or 0 if out of memory
 */
static void *slob_alloc(size_t size, gfp_t gfp, int align) {
    assert((size + SLOB_UNIT) < PAGE_SIZE);

    slob_t *prev, *cur, *aligned = 0;
    int delta = 0, units = SLOB_UNITS(size);
    unsigned long flags;

    spin_lock_irqsave(&slob_lock, flags);
    prev = slobfree;
    for (cur = prev->next;; prev = cur, cur = cur->next) {
        if (align) {
            aligned = (slob_t *)ALIGN((unsigned long)cur, align);
            delta = aligned - cur;
        }
        if (cur->units >= units + delta) { /* room enough? */
            if (delta) {                   /* need to fragment head to align? */
                aligned->units = cur->units - delta;
                aligned->next = cur->next;
                cur->next = aligned;
                cur->units = delta;
                prev = cur;
                cur = aligned;
            }

            if (cur->units == units)    /* exact fit? */
                prev->next = cur->next; /* unlink */
            else {                      /* fragment */
                prev->next = cur + units;
                prev->next->units = cur->units - units;
                prev->next->next = cur->next;
                cur->units = units;
            }

            slobfree = prev;
            spin_unlock_irqrestore(&slob_lock, flags);
            return cur;
        }
        if (cur == slobfree) {
            spin_unlock_irqrestore(&slob_lock, flags);

            if (size == PAGE_SIZE) /* trying to shrink arena? */
                return 0;

            cur = (slob_t *)__slob_get_free_page(gfp);
            if (!cur)
                return 0;

            slob_free(cur, PAGE_SIZE);
            spin_lock_irqsave(&slob_lock, flags);
            cur = slobfree;
        }
    }
}

/**
 * @brief Return a block to the free list, keeping the list in address order and merging neighbours.
 *
 * The insertion point is found by walking from slobfree. The block is merged with
 * its successor if they are adjacent and with its predecessor if they are adjacent.
 * @param block start of the block (its slob_t header)
 * @param size  bytes of the block. If non-zero, the block's units are set from it;
 *              if 0, the units already stored in the header are used.
 */
static void slob_free(void *block, int size) {
    slob_t *cur, *b = (slob_t *)block;
    unsigned long flags;

    if (!block)
        return;

    if (size)
        b->units = SLOB_UNITS(size);

    /* Find reinsertion point */
    spin_lock_irqsave(&slob_lock, flags);
    for (cur = slobfree; !(b > cur && b < cur->next); cur = cur->next)
        if (cur >= cur->next && (b > cur || b < cur->next))
            break;

    if (b + b->units == cur->next) {
        b->units += cur->next->units;
        b->next = cur->next->next;
    } else
        b->next = cur->next;

    if (cur + cur->units == b) {
        cur->units += b->units;
        cur->next = b->next;
    } else
        cur->next = b;

    slobfree = cur;

    spin_unlock_irqrestore(&slob_lock, flags);
}

/**
 * @brief Initialise the SLOB allocator. Only prints a message: the free list is
 *        already valid because arena is statically initialised.
 */
void slob_init(void) {
    cprintf("use SLOB allocator\n");
}

/**
 * @brief Initialise kmalloc(); called from pmm_init() after the page allocator is ready.
 */
inline void kmalloc_init(void) {
    slob_init();
    cprintf("kmalloc_init() succeeded!\n");
}

/**
 * @brief Number of bytes currently allocated by the small-object allocator.
 * @return always 0 in this implementation (usage is not tracked).
 */
size_t slob_allocated(void) {
    return 0;
}

/**
 * @brief Number of bytes currently allocated (forwards to slob_allocated()).
 * @return always 0 in this implementation.
 */
size_t kallocated(void) {
    return slob_allocated();
}

/**
 * @brief Compute the page order used for a page-granular allocation of size bytes.
 *
 * The loop shifts size right until it is at most PAGE_SIZE and counts the shifts.
 * @param size bytes requested
 * @return order such that `PAGE_SIZE << order` is (intended to be) at least size
 * @warning The right shift discards low bits, so the result is rounded down in some
 *          cases. For example find_order(8193) returns 1 (8 KiB) while 3 pages are
 *          needed, and find_order(16385) returns 2 (16 KiB). Such a request gets a
 *          block that is too small. Not fixed here.
 */
static int find_order(int size) {
    int order = 0;
    for (; size > 4096; size >>= 1)
        order++;
    return order;
}

/**
 * @brief Common allocation routine behind kmalloc().
 *
 * size < PAGE_SIZE - SLOB_UNIT: slob_alloc() of size + SLOB_UNIT bytes; the
 * header is filled in and the address after it is returned.
 * Otherwise: a bigblock_t record is allocated from the heap, the pages come from
 * __slob_get_free_pages(order = find_order(size)), and the record is pushed on bigblocks.
 * @param size bytes requested
 * @param gfp  ignored
 * @return kernel virtual address of the object, or NULL
 */
static void *__kmalloc(size_t size, gfp_t gfp) {
    slob_t *m;
    bigblock_t *bb;
    unsigned long flags;

    if (size < PAGE_SIZE - SLOB_UNIT) {
        m = slob_alloc(size + SLOB_UNIT, gfp, 0);
        return m ? (void *)(m + 1) : 0;
    }

    bb = slob_alloc(sizeof(bigblock_t), gfp, 0);
    if (!bb)
        return 0;

    bb->order = find_order(size);
    bb->pages = (void *)__slob_get_free_pages(gfp, bb->order);

    if (bb->pages) {
        spin_lock_irqsave(&block_lock, flags);
        bb->next = bigblocks;
        bigblocks = bb;
        spin_unlock_irqrestore(&block_lock, flags);
        return bb->pages;
    }

    slob_free(bb, sizeof(bigblock_t));
    return 0;
}

/**
 * @brief Allocate size bytes; the result is aligned to SLOB_UNIT (16 bytes).
 * @param size bytes requested
 * @return kernel virtual address, or NULL if out of memory. Free with kfree().
 */
void *kmalloc(size_t size) {
    return __kmalloc(size, 0);
}

/**
 * @brief Free an object returned by kmalloc(). NULL is ignored.
 *
 * A page-aligned pointer is first searched in bigblocks (page-granular allocations).
 * If it is not found, the pointer is a small object whose slob_t header sits just
 * before it, and slob_free() is called on that header. A small object can be
 * page-aligned too, but then the bigblocks search simply fails.
 * @param block pointer returned by kmalloc()
 */
void kfree(void *block) {
    bigblock_t *bb, **last = &bigblocks;
    unsigned long flags;

    if (!block)
        return;

    if (!((unsigned long)block & (PAGE_SIZE - 1))) {
        /* might be on the big block list */
        spin_lock_irqsave(&block_lock, flags);
        for (bb = bigblocks; bb; last = &bb->next, bb = bb->next) {
            if (bb->pages == block) {
                *last = bb->next;
                spin_unlock_irqrestore(&block_lock, flags);
                __slob_free_pages((unsigned long)block, bb->order);
                slob_free(bb, sizeof(bigblock_t));
                return;
            }
        }
        spin_unlock_irqrestore(&block_lock, flags);
    }

    slob_free((slob_t *)block - 1, 0);
    return;
}

/**
 * @brief Usable size of an allocated block.
 *
 * For a page-granular block: PAGE_SIZE << order. For a small object: the size
 * in its header in bytes, which includes the slob_t header.
 * @param block pointer returned by kmalloc(), or NULL
 * @return size in bytes, or 0 for NULL
 * @note The lookup path unlocks with slob_lock, not block_lock. The macro ignores
 *       the lock argument, so this is harmless here.
 */
unsigned int ksize(const void *block) {
    bigblock_t *bb;
    unsigned long flags;

    if (!block)
        return 0;

    if (!((unsigned long)block & (PAGE_SIZE - 1))) {
        spin_lock_irqsave(&block_lock, flags);
        for (bb = bigblocks; bb; bb = bb->next)
            if (bb->pages == block) {
                spin_unlock_irqrestore(&slob_lock, flags);
                return PAGE_SIZE << bb->order;
            }
        spin_unlock_irqrestore(&block_lock, flags);
    }

    return ((slob_t *)block - 1)->units * SLOB_UNIT;
}
