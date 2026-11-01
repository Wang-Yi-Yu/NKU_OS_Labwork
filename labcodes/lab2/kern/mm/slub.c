/**
 * @file   slub.c
 * @brief  Simplified SLUB: size-class objects carved out of pages from alloc_pages().
 *
 * The page allocator underneath is whichever pmm_manager is currently selected.
 * This file never refers to a concrete First-Fit, Best-Fit or Buddy implementation.
 * @author 鲁昕宁(2414015)
 */
#include <assert.h>
#include <pmm.h>
#include <slub.h>
#include <stdio.h>
#include <string.h>

/** @brief Marker at the start of a page owned by an in-page size class. */
#define SLUB_MAGIC 0x534C5542u

/**
 * @brief Header stored at the start of a slab page. The rest of the page is objects.
 *
 * Management data stays in the page contents, not in struct Page fields used by
 * the page allocator.
 */
struct slab_hdr {
    uint32_t magic;    /**< Always ::SLUB_MAGIC for a slab page. */
    uint32_t obj_size; /**< Size in bytes of every slot in this slab. */
    uint32_t free_cnt; /**< Number of slots currently on the free list. */
    uint32_t capacity; /**< Total number of slots in the page. */
    void *freelist; /**< First free slot; each free slot stores the next one in its first word. */
    struct slab_hdr *next; /**< Next slab of the same cache. */
};

/** @brief One power-of-two size class and the slab pages currently held for it. */
struct kmem_cache {
    size_t obj_size;        /**< Slot size served by this cache, a power of two. */
    struct slab_hdr *slabs; /**< Slab pages of this cache, newest first. */
};

/**
 * @brief Descriptor of a live multi-page object.
 *
 * slub_free() is not told the size, so the page count of every multi-page object is
 * recorded here and found again through the object address (a hash table keyed by page).
 * The same `next` link chains the descriptors of one hash bucket and, while a descriptor
 * is unused, the free list of the static pool.
 */
struct large_obj {
    struct large_obj *next; /**< Next descriptor in the bucket, or next unused static descriptor. */
    void *kva;              /**< Kernel virtual address of the first page. */
    size_t npages;          /**< Number of pages of the object, as passed to alloc_pages(). */
};

/** @brief In-page size classes, 16 to 2048 bytes. 4096 cannot hold a header plus a 4096-byte object. */
static struct kmem_cache caches[] = {
    {16, NULL},  {32, NULL},  {64, NULL},   {128, NULL},
    {256, NULL}, {512, NULL}, {1024, NULL}, {2048, NULL},
};

/** @brief Number of hash buckets for live multi-page objects; only affects speed, not capacity. */
#define SLUB_LARGE_BUCKETS 128
/**
 * @brief Number of descriptors kept in static memory.
 *
 * While at most this many multi-page objects are alive, tracking them costs no page, so a
 * one-page request needs exactly one page. Beyond that descriptors come from the 32-byte
 * size class, which costs about one page per 127 objects. There is no upper limit other than
 * the memory of the page allocator.
 */
#define SLUB_LARGE_INLINE 64
/** @brief Live multi-page objects, hashed by the page number of their address. */
static struct large_obj *large_hash[SLUB_LARGE_BUCKETS];
/** @brief Static descriptor pool, used before falling back to the size class. */
static struct large_obj large_inline[SLUB_LARGE_INLINE];
/** @brief Unused descriptors of ::large_inline. */
static struct large_obj *large_inline_free;
/** @brief Non-zero once slub_init() has run; makes a second call release what SLUB still holds. */
static int slub_ready;

/**
 * @brief Kernel virtual address of a page descriptor.
 * @param page page returned by alloc_pages()
 * @return kernel virtual address of that page
 * @author 鲁昕宁(2414015)
 */
static void *page_to_kva(struct Page *page) {
    return (void *)(page2pa(page) + va_pa_offset);
}

/**
 * @brief Page descriptor for a kernel virtual address inside a slab.
 * @param kva kernel virtual address
 * @return the corresponding struct Page
 * @author 鲁昕宁(2414015)
 */
static struct Page *kva_to_page(void *kva) {
    return pa2page(PADDR(kva));
}

/**
 * @brief Size class that can hold @p size, or NULL when @p size needs whole pages.
 * @param size requested bytes
 * @return cache, or NULL for a multi-page allocation
 * @author 鲁昕宁(2414015)
 */
static struct kmem_cache *cache_for(size_t size) {
    for (size_t i = 0; i < sizeof(caches) / sizeof(caches[0]); i++) {
        if (size <= caches[i].obj_size) {
            return &caches[i];
        }
    }
    return NULL;
}

/**
 * @brief Offset of the first slot in a slab page: the header size rounded up to the slot size.
 * @param obj_size power-of-two object size
 * @return byte offset of slot 0, so every slot is aligned to its own size
 * @author Yiyu Wang(2414007)
 */
static size_t slab_start(size_t obj_size) {
    return ROUNDUP(sizeof(struct slab_hdr), obj_size);
}

/**
 * @brief Cut a freshly allocated page into free slots of @p obj_size.
 * @param obj_size power-of-two object size
 * @return slab header, or NULL if the page allocator cannot supply a page
 * @author 鲁昕宁(2414015)
 */
static struct slab_hdr *slab_create(size_t obj_size) {
    struct Page *page = alloc_pages(1);
    if (page == NULL) {
        return NULL;
    }
    struct slab_hdr *hdr = page_to_kva(page);
    size_t start = slab_start(obj_size);
    size_t capacity = (PGSIZE - start) / obj_size;
    char *base = (char *)hdr + start;
    void *next = NULL;
    for (size_t i = capacity; i > 0; i--) {
        void **slot = (void **)(base + (i - 1) * obj_size);
        *slot = next;
        next = slot;
    }
    hdr->magic = SLUB_MAGIC;
    hdr->obj_size = obj_size;
    hdr->capacity = capacity;
    hdr->free_cnt = capacity;
    hdr->freelist = next;
    hdr->next = NULL;
    return hdr;
}

/**
 * @brief Take one free slot from a slab that still has one.
 * @param hdr slab header
 * @return object address
 * @author 鲁昕宁(2414015)
 */
static void *slab_pop(struct slab_hdr *hdr) {
    void *obj = hdr->freelist;
    hdr->freelist = *(void **)obj;
    hdr->free_cnt--;
    return obj;
}

/**
 * @brief Release every slab page and multi-page object still held by SLUB.
 * @note The multi-page objects go first, because their overflow descriptors live in slab pages.
 * @author 鲁昕宁(2414015)
 */
static void slub_release_all(void) {
    for (size_t i = 0; i < SLUB_LARGE_BUCKETS; i++) {
        for (struct large_obj *desc = large_hash[i]; desc != NULL; desc = desc->next) {
            free_pages(kva_to_page(desc->kva), desc->npages);
        }
        large_hash[i] = NULL;
    }
    for (size_t i = 0; i < sizeof(caches) / sizeof(caches[0]); i++) {
        struct slab_hdr *hdr = caches[i].slabs;
        caches[i].slabs = NULL;
        while (hdr != NULL) {
            struct slab_hdr *next = hdr->next;
            hdr->magic = 0;
            free_pages(kva_to_page(hdr), 1);
            hdr = next;
        }
    }
}

/**
 * @brief Reset all caches and the multi-page bookkeeping; no page is taken until the first allocation.
 * @note A repeated call first returns every page still held by SLUB to the page allocator.
 */
void slub_init(void) {
    if (slub_ready) {
        slub_release_all();
    }
    for (size_t i = 0; i < sizeof(caches) / sizeof(caches[0]); i++) {
        caches[i].slabs = NULL;
    }
    memset(large_hash, 0, sizeof(large_hash));
    large_inline_free = NULL;
    for (size_t i = SLUB_LARGE_INLINE; i > 0; i--) {
        large_inline[i - 1].next = large_inline_free;
        large_inline_free = &large_inline[i - 1];
    }
    slub_ready = 1;
}

/**
 * @brief Hash bucket of a multi-page object.
 * @param kva page-aligned address of the object
 * @return index into ::large_hash
 * @author Yiyu Wang(2414007)
 */
static size_t large_bucket(void *kva) {
    return ((uintptr_t)kva >> PGSHIFT) % SLUB_LARGE_BUCKETS;
}

/**
 * @brief Get a descriptor for a multi-page object: from the static pool, else from the 32-byte class.
 * @return an unused descriptor, or NULL if the page allocator cannot supply a slab page
 * @author Yiyu Wang(2414007)
 */
static struct large_obj *large_desc_get(void) {
    struct large_obj *desc = large_inline_free;
    if (desc != NULL) {
        large_inline_free = desc->next;
        return desc;
    }
    return slub_alloc(sizeof(struct large_obj));
}

/**
 * @brief Give a descriptor back to where large_desc_get() took it from.
 * @param desc descriptor that is no longer linked into ::large_hash
 * @author Yiyu Wang(2414007)
 */
static void large_desc_put(struct large_obj *desc) {
    if (desc >= large_inline && desc < large_inline + SLUB_LARGE_INLINE) {
        desc->next = large_inline_free;
        large_inline_free = desc;
    } else {
        slub_free(desc);
    }
}

/**
 * @brief Allocate a multi-page object and remember its length for slub_free().
 * @param size requested bytes, greater than the largest in-page class
 * @return start address, or NULL if @p size is too large or the page allocator has no memory left
 * @author 鲁昕宁(2414015)
 */
static void *alloc_large(size_t size) {
    // Rounding up would wrap around to 0 for sizes within one page of SIZE_MAX.
    if (size > ~(size_t)0 - (PGSIZE - 1)) {
        return NULL;
    }
    size_t npages = ROUNDUP(size, PGSIZE) / PGSIZE;
    struct Page *page = alloc_pages(npages);
    if (page == NULL) {
        return NULL;
    }
    struct large_obj *desc = large_desc_get();
    if (desc == NULL) {
        free_pages(page, npages);
        return NULL;
    }
    desc->kva = page_to_kva(page);
    desc->npages = npages;
    size_t bucket = large_bucket(desc->kva);
    desc->next = large_hash[bucket];
    large_hash[bucket] = desc;
    return desc->kva;
}

/**
 * @brief Allocate an object of at least @p size bytes.
 * @details Sizes up to 2048 bytes are rounded up to a power-of-two class and served from
 * a slab page of that class (a new page is taken when every slab is full).
 * Larger sizes get whole pages from alloc_pages() and are recorded in a hash table.
 */
void *slub_alloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    struct kmem_cache *cache = cache_for(size);
    if (cache == NULL) {
        return alloc_large(size);
    }
    struct slab_hdr *hdr = cache->slabs;
    while (hdr != NULL && hdr->free_cnt == 0) {
        hdr = hdr->next;
    }
    if (hdr == NULL) {
        hdr = slab_create(cache->obj_size);
        if (hdr == NULL) {
            return NULL;
        }
        hdr->next = cache->slabs;
        cache->slabs = hdr;
    }
    return slab_pop(hdr);
}

/**
 * @brief Free a multi-page object.
 * @param obj page-aligned address passed to slub_free(); must be a live multi-page object
 * @author 鲁昕宁(2414015)
 */
static void free_large(void *obj) {
    struct large_obj **pp = &large_hash[large_bucket(obj)];
    while (*pp != NULL && (*pp)->kva != obj) {
        pp = &(*pp)->next;
    }
    assert(*pp != NULL);
    struct large_obj *desc = *pp;
    *pp = desc->next;
    free_pages(kva_to_page(obj), desc->npages);
    large_desc_put(desc);
}

/**
 * @brief Free an object returned by slub_alloc().
 * @details Slots of a slab never start at a page boundary (the header is there), while a
 * multi-page object always does. So a page-aligned address is looked up in the multi-page
 * hash table and its pages are freed; any other address goes back to the free list of its
 * slab, and a slab that becomes completely free is unlinked and its page is returned to
 * the page allocator.
 * @warning An address that was not returned by slub_alloc() is caught by an assert: a
 *          page-aligned one must be a live multi-page object, any other one must lie on a
 *          slot of a page that starts with ::SLUB_MAGIC. Freeing a multi-page object twice
 *          is caught, freeing a slab slot twice is not reliably detected.
 */
void slub_free(void *obj) {
    if (obj == NULL) {
        return;
    }
    if (((uintptr_t)obj & (PGSIZE - 1)) == 0) {
        free_large(obj);
        return;
    }
    struct slab_hdr *hdr = (struct slab_hdr *)((uintptr_t)obj & ~(uintptr_t)(PGSIZE - 1));
    assert(hdr->magic == SLUB_MAGIC);
    size_t offset = (uintptr_t)obj - (uintptr_t)hdr;
    assert(offset >= slab_start(hdr->obj_size) && offset % hdr->obj_size == 0);
    struct kmem_cache *cache = cache_for(hdr->obj_size);
    *(void **)obj = hdr->freelist;
    hdr->freelist = obj;
    hdr->free_cnt++;
    if (hdr->free_cnt == hdr->capacity) {
        struct slab_hdr **pp = &cache->slabs;
        while (*pp != hdr) {
            pp = &(*pp)->next;
        }
        *pp = hdr->next;
        hdr->magic = 0;
        free_pages(kva_to_page(hdr), 1);
    }
}

/** @brief Sizes of the in-page classes exercised by slub_check(). */
static const size_t check_sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};

/**
 * @brief Page containing an object address.
 * @param obj object pointer
 * @return page-aligned kernel virtual address
 * @author 鲁昕宁(2414015)
 */
static uintptr_t obj_page(void *obj) {
    return (uintptr_t)obj & ~(uintptr_t)(PGSIZE - 1);
}

/**
 * @brief Fill an object with a repeating byte and require the same bytes to come back.
 * @param obj object address
 * @param size number of bytes that must be writable
 * @param mark byte stored throughout the object
 * @author 鲁昕宁(2414015)
 */
static void check_pattern(void *obj, size_t size, unsigned char mark) {
    unsigned char *bytes = obj;
    for (size_t i = 0; i < size; i++) {
        bytes[i] = mark;
    }
    for (size_t i = 0; i < size; i++) {
        assert(bytes[i] == mark);
    }
}

/**
 * @brief True when two ranges share any byte.
 * @param a start of the first range
 * @param asize length of the first range
 * @param b start of the second range
 * @param bsize length of the second range
 * @return 1 if the ranges overlap
 * @author 鲁昕宁(2414015)
 */
static int ranges_overlap(void *a, size_t asize, void *b, size_t bsize) {
    uintptr_t a0 = (uintptr_t)a;
    uintptr_t b0 = (uintptr_t)b;
    return a0 < b0 + bsize && b0 < a0 + asize;
}

/**
 * @brief Pages the page allocator really consumes for an n-page request.
 *
 * The Buddy System rounds the request up to a power of two (3 pages cost 4), First-Fit and
 * Best-Fit do not. Measuring it keeps the page-count assertions exact for every page
 * allocator without naming one.
 *
 * @param n number of pages
 * @return drop of nr_free_pages() caused by alloc_pages(n); the pages are given back before returning
 * @author Yiyu Wang(2414007)
 */
static size_t page_cost(size_t n) {
    size_t before = nr_free_pages();
    struct Page *page = alloc_pages(n);
    assert(page != NULL);
    size_t cost = before - nr_free_pages();
    free_pages(page, n);
    assert(nr_free_pages() == before);
    return cost;
}

/**
 * @brief Take single pages from the page allocator until only @p keep pages are left.
 *
 * Every taken page stores the pointer to the previously taken one in its first word.
 *
 * @param keep number of free pages that must remain
 * @return the most recently taken page, or NULL if nothing was taken
 * @author Yiyu Wang(2414007)
 */
static struct Page *hog_pages(size_t keep) {
    struct Page *head = NULL;
    while (nr_free_pages() > keep) {
        struct Page *page = alloc_pages(1);
        assert(page != NULL);
        *(struct Page **)page_to_kva(page) = head;
        head = page;
    }
    return head;
}

/**
 * @brief Give back all pages taken by hog_pages().
 * @param head value returned by hog_pages()
 * @author Yiyu Wang(2414007)
 */
static void unhog_pages(struct Page *head) {
    while (head != NULL) {
        struct Page *page = head;
        head = *(struct Page **)page_to_kva(page);
        free_pages(page, 1);
    }
}

/** @brief Most objects of one class that check_multi_slab() holds at once: 2 pages worth plus one, of 16 bytes. */
#define CHECK_MAX_OBJS (2 * (PGSIZE / 16) + 1)

/** @brief Pointers held by check_multi_slab(); static to keep the kernel stack small. */
static void *check_objs[CHECK_MAX_OBJS];

/**
 * @brief Fill every class with more than two slabs worth of objects and release them out of order.
 *
 * For each class it takes `2 * (PGSIZE / size) + 1` objects, which cannot fit in
 * two pages, so slub_alloc() has to create further slabs while earlier ones are
 * full. It then checks the number of pages used against the slot count of the
 * class, that every object is aligned and distinct and keeps its own contents,
 * and frees them evens first and odds last so that slabs become empty in the
 * middle of the slab list. Every page must come back.
 *
 * @author Yiyu Wang(2414007)
 */
static void check_multi_slab(void) {
    size_t base = nr_free_pages();
    for (size_t c = 0; c < sizeof(check_sizes) / sizeof(check_sizes[0]); c++) {
        size_t size = check_sizes[c];
        size_t n = 2 * (PGSIZE / size) + 1;
        size_t capacity = (PGSIZE - ROUNDUP(sizeof(struct slab_hdr), size)) / size;
        assert(n <= CHECK_MAX_OBJS);

        for (size_t i = 0; i < n; i++) {
            check_objs[i] = slub_alloc(size);
            assert(check_objs[i] != NULL);
            assert((uintptr_t)check_objs[i] % size == 0);
            check_pattern(check_objs[i], size, (unsigned char)(i % 251 + 1));
        }
        assert(base - nr_free_pages() == (n + capacity - 1) / capacity);
        assert(base - nr_free_pages() >= 3);

        for (size_t i = 0; i < n; i++) {
            unsigned char *bytes = check_objs[i];
            for (size_t k = 0; k < size; k++) {
                assert(bytes[k] == (unsigned char)(i % 251 + 1));
            }
            for (size_t j = i + 1; j < n; j++) {
                assert(check_objs[i] != check_objs[j]);
            }
        }
        for (size_t i = 0; i < n; i += 2) {
            slub_free(check_objs[i]);
        }
        for (size_t i = 1; i < n; i += 2) {
            slub_free(check_objs[i]);
        }
        assert(nr_free_pages() == base);
    }
    cprintf("slub_check: multi-slab OK\n");
}

/**
 * @brief Check the page counts of multi-page objects around the page-size boundaries.
 *
 * Sizes 2049 and 4096 need one page, 4097 and 8192 two, 8193 three. The page
 * allocator may hand out more than requested (the Buddy System rounds 3 pages
 * up to 4), so the pages taken must equal what page_cost() measures for the
 * needed page count: not less, and not more because SLUB asked for too many
 * pages. All of them must come back when the object is freed, because
 * slub_free() must pass the same page count that slub_alloc() asked for.
 *
 * @author Yiyu Wang(2414007)
 */
static void check_large_sizes(void) {
    static const size_t sizes[] = {2049, 4096, 4097, 2 * PGSIZE, 2 * PGSIZE + 1};
    static const size_t need[] = {1, 1, 2, 2, 3};
    size_t base = nr_free_pages();
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        void *obj = slub_alloc(sizes[i]);
        assert(obj != NULL);
        assert(obj_page(obj) == (uintptr_t)obj);
        assert(base - nr_free_pages() == page_cost(need[i]));
        check_pattern(obj, sizes[i], (unsigned char)(0x30 + i));
        slub_free(obj);
        assert(nr_free_pages() == base);
    }

    /* A large and a small object alive together must not disturb each other. */
    void *big = slub_alloc(2 * PGSIZE + 1);
    void *small = slub_alloc(48);
    assert(big != NULL && small != NULL);
    check_pattern(big, 2 * PGSIZE + 1, 0x77);
    check_pattern(small, 48, 0x88);
    slub_free(small);
    check_pattern(big, 2 * PGSIZE + 1, 0x77);
    slub_free(big);
    assert(nr_free_pages() == base);
    cprintf("slub_check: large sizes OK\n");
}

/**
 * @brief Check that requests too large to be served fail without side effects.
 *
 * The sizes are chosen around the point where rounding the size up to a whole
 * number of pages would wrap around to a small value (or 0). Each request must
 * return NULL, leave the free page count unchanged and not corrupt the
 * multi-page table.
 *
 * @author Yiyu Wang(2414007)
 */
static void check_huge_requests(void) {
    static const size_t sizes[] = {
        ~(size_t)0,      ~(size_t)0 - 1,  ~(size_t)0 - PGSIZE + 1, ~(size_t)0 - PGSIZE + 2,
        (size_t)1 << 63, (size_t)1 << 40,
    };
    size_t base = nr_free_pages();
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        assert(slub_alloc(sizes[i]) == NULL);
        assert(nr_free_pages() == base);
    }
    void *obj = slub_alloc(PGSIZE + 1);
    assert(obj != NULL);
    slub_free(obj);
    assert(nr_free_pages() == base);
    cprintf("slub_check: huge requests OK\n");
}

/**
 * @brief Check the behaviour when the page allocator has no page left.
 *
 * Takes every free page from the page allocator, then requires small and
 * multi-page requests to fail without changing any state. After one page is
 * returned, a one-page request has to succeed again while a two-page request
 * still fails. Finally all pages are given back.
 *
 * @author Yiyu Wang(2414007)
 */
static void check_page_exhaustion(void) {
    size_t base = nr_free_pages();
    struct Page *head = hog_pages(0);
    assert(nr_free_pages() == 0);
    assert(alloc_pages(1) == NULL);

    assert(slub_alloc(16) == NULL);
    assert(slub_alloc(2048) == NULL);
    assert(slub_alloc(PGSIZE) == NULL);
    assert(slub_alloc(PGSIZE + 1) == NULL);
    assert(nr_free_pages() == 0);

    struct Page *page = head;
    head = *(struct Page **)page_to_kva(page);
    free_pages(page, 1);
    assert(nr_free_pages() == 1);
    assert(slub_alloc(PGSIZE + 1) == NULL);
    void *obj = slub_alloc(16);
    assert(obj != NULL);
    assert(nr_free_pages() == 0);
    slub_free(obj);
    assert(nr_free_pages() == 1);
    obj = slub_alloc(PGSIZE);
    assert(obj != NULL);
    assert(nr_free_pages() == 0);
    slub_free(obj);
    assert(nr_free_pages() == 1);

    unhog_pages(head);
    assert(nr_free_pages() == base);
    cprintf("slub_check: page exhaustion OK\n");
}

/** @brief Number of live multi-page objects in check_many_large(): more than the old 256-entry table and the static pool. */
#define CHECK_MANY_LARGE 300

/**
 * @brief Check that the number of live multi-page objects is not limited by a table size.
 *
 * Holds CHECK_MANY_LARGE one-page objects at once, more than ::SLUB_LARGE_INLINE
 * descriptors of the static pool (and more than the 256 entries of the table
 * that earlier versions had). The first ::SLUB_LARGE_INLINE objects must cost
 * exactly their own pages; every further descriptor comes from the 32-byte size
 * class and adds one slab page per `capacity` descriptors. Contents and
 * addresses must stay intact, and freeing in a scrambled order must return
 * every page, including the descriptor slabs.
 *
 * @author Yiyu Wang(2414007)
 */
static void check_many_large(void) {
    size_t base = nr_free_pages();
    size_t desc_size = cache_for(sizeof(struct large_obj))->obj_size;
    size_t desc_capacity = (PGSIZE - slab_start(desc_size)) / desc_size;
    size_t per_obj = page_cost(1);
    assert(CHECK_MANY_LARGE <= CHECK_MAX_OBJS && CHECK_MANY_LARGE > 256);
    assert(CHECK_MANY_LARGE > SLUB_LARGE_INLINE);

    for (size_t i = 0; i < SLUB_LARGE_INLINE; i++) {
        check_objs[i] = slub_alloc(PGSIZE);
        assert(check_objs[i] != NULL);
        assert(obj_page(check_objs[i]) == (uintptr_t)check_objs[i]);
        check_pattern(check_objs[i], PGSIZE, (unsigned char)(i % 251 + 1));
    }
    assert(base - nr_free_pages() == SLUB_LARGE_INLINE * per_obj);

    for (size_t i = SLUB_LARGE_INLINE; i < CHECK_MANY_LARGE; i++) {
        check_objs[i] = slub_alloc(PGSIZE);
        assert(check_objs[i] != NULL);
        assert(obj_page(check_objs[i]) == (uintptr_t)check_objs[i]);
        check_pattern(check_objs[i], PGSIZE, (unsigned char)(i % 251 + 1));
    }
    size_t overflow = CHECK_MANY_LARGE - SLUB_LARGE_INLINE;
    assert(base - nr_free_pages() ==
           CHECK_MANY_LARGE * per_obj + (overflow + desc_capacity - 1) / desc_capacity);

    for (size_t i = 0; i < CHECK_MANY_LARGE; i++) {
        unsigned char *bytes = check_objs[i];
        for (size_t k = 0; k < PGSIZE; k++) {
            assert(bytes[k] == (unsigned char)(i % 251 + 1));
        }
        for (size_t j = i + 1; j < CHECK_MANY_LARGE; j++) {
            assert(check_objs[i] != check_objs[j]);
        }
    }
    for (size_t i = 0; i < CHECK_MANY_LARGE; i += 3) {
        slub_free(check_objs[i]);
    }
    for (size_t i = 1; i < CHECK_MANY_LARGE; i += 3) {
        slub_free(check_objs[i]);
    }
    for (size_t i = 2; i < CHECK_MANY_LARGE; i += 3) {
        slub_free(check_objs[i]);
    }
    assert(nr_free_pages() == base);
    cprintf("slub_check: many large objects OK\n");
}

/**
 * @brief Check that a failing descriptor allocation gives the pages of a multi-page object back.
 *
 * With the static pool used up, a multi-page object needs a descriptor from the
 * 32-byte class, i.e. one more page. When exactly one page is left, the object's
 * own page can be taken but the descriptor cannot, so slub_alloc() has to return
 * NULL and undo the first step. A freed object must also hand its descriptor back
 * so that the static pool can be used again.
 *
 * @author Yiyu Wang(2414007)
 */
static void check_large_rollback(void) {
    size_t base = nr_free_pages();
    for (size_t i = 0; i < SLUB_LARGE_INLINE; i++) {
        check_objs[i] = slub_alloc(PGSIZE);
        assert(check_objs[i] != NULL);
    }
    size_t used = base - nr_free_pages();

    struct Page *head = hog_pages(1);
    assert(nr_free_pages() == 1);
    assert(slub_alloc(PGSIZE) == NULL);
    assert(nr_free_pages() == 1);
    assert(slub_alloc(2 * PGSIZE) == NULL);
    assert(nr_free_pages() == 1);

    // Returning one object frees its descriptor to the static pool: a one-page object needs one page again.
    slub_free(check_objs[0]);
    assert(nr_free_pages() == 1 + page_cost(1));
    check_objs[0] = slub_alloc(PGSIZE);
    assert(check_objs[0] != NULL);
    assert(nr_free_pages() == 1);

    unhog_pages(head);
    assert(base - nr_free_pages() == used);
    for (size_t i = 0; i < SLUB_LARGE_INLINE; i++) {
        slub_free(check_objs[i]);
    }
    assert(nr_free_pages() == base);
    cprintf("slub_check: descriptor rollback OK\n");
}

/**
 * @brief Self test of SLUB: size classes, slot reuse, page return and multi-page objects.
 *
 * Every step compares nr_free_pages() with the expected value, so a leak or a
 * premature free shows up as a failed assert. Prints "slub_check() succeeded"
 * when all cases pass.
 *
 * @note The loop over multi-page objects near the end stops when the page allocator runs
 *       out of pages, not at a fixed count; the exact exhaustion cases are in
 *       check_page_exhaustion().
 */
void slub_check(void) {
    size_t base = nr_free_pages();
    size_t pages = base;

    assert(slub_alloc(0) == NULL);
    assert(nr_free_pages() == pages);
    slub_free(NULL);
    assert(nr_free_pages() == pages);

    void *objs[sizeof(check_sizes) / sizeof(check_sizes[0])];
    for (size_t i = 0; i < sizeof(check_sizes) / sizeof(check_sizes[0]); i++) {
        objs[i] = slub_alloc(check_sizes[i]);
        assert(objs[i] != NULL);
        assert(((uintptr_t)objs[i] % check_sizes[i]) == 0);
        check_pattern(objs[i], check_sizes[i], (unsigned char)(i + 1));
    }
    for (size_t i = 0; i < sizeof(check_sizes) / sizeof(check_sizes[0]); i++) {
        for (size_t j = i + 1; j < sizeof(check_sizes) / sizeof(check_sizes[0]); j++) {
            assert(!ranges_overlap(objs[i], check_sizes[i], objs[j], check_sizes[j]));
        }
    }

    void *extra16 = slub_alloc(16);
    assert(extra16 != NULL);
    assert(obj_page(extra16) == obj_page(objs[0]));
    pages = nr_free_pages();
    slub_free(extra16);
    assert(nr_free_pages() == pages);
    slub_free(objs[0]);
    objs[0] = NULL;
    assert(nr_free_pages() == pages + 1);
    for (size_t i = 1; i < sizeof(check_sizes) / sizeof(check_sizes[0]); i++) {
        unsigned char *bytes = objs[i];
        for (size_t n = 0; n < check_sizes[i]; n++) {
            assert(bytes[n] == (unsigned char)(i + 1));
        }
    }
    for (size_t i = 1; i < sizeof(check_sizes) / sizeof(check_sizes[0]); i++) {
        slub_free(objs[i]);
    }
    assert(nr_free_pages() == base);

    pages = nr_free_pages();
    void *obj16 = slub_alloc(16);
    void *obj32 = slub_alloc(32);
    assert(obj16 != NULL && obj32 != NULL);
    assert(nr_free_pages() == pages - 2);
    slub_free(obj16);
    assert(nr_free_pages() == pages - 1);
    slub_free(obj32);
    assert(nr_free_pages() == pages);

    void *live = slub_alloc(16);
    void *gone = slub_alloc(16);
    assert(live != NULL && gone != NULL);
    check_pattern(live, 16, 0x11);
    slub_free(gone);
    pages = nr_free_pages();
    void *reused = slub_alloc(16);
    assert(reused != NULL);
    assert(nr_free_pages() == pages);
    assert(!ranges_overlap(reused, 16, live, 16));
    check_pattern(live, 16, 0x11);
    slub_free(live);
    slub_free(reused);
    assert(nr_free_pages() == base);

    pages = nr_free_pages();
    void *big = slub_alloc(PGSIZE + 1);
    assert(big != NULL);
    assert(nr_free_pages() == pages - 2);
    check_pattern(big, PGSIZE + 1, 0x5a);
    slub_free(big);
    assert(nr_free_pages() == pages);

    pages = nr_free_pages();
    struct large_node {
        struct large_node *next;
        unsigned char mark;
    };
    struct large_node *head = NULL;
    size_t nlarge = 0;
    for (;;) {
        size_t before = nr_free_pages();
        struct large_node *node = slub_alloc(PGSIZE + 1);
        if (node == NULL) {
            assert(nr_free_pages() == before);
            break;
        }
        node->next = head;
        node->mark = 0xa5;
        ((unsigned char *)node)[PGSIZE] = node->mark;
        head = node;
        nlarge++;
    }
    assert(nlarge > 0);
    for (struct large_node *node = head; node != NULL; node = node->next) {
        assert(node->mark != 0);
        assert(((unsigned char *)node)[PGSIZE] == node->mark);
    }
    while (head != NULL) {
        struct large_node *next = head->next;
        slub_free(head);
        head = next;
    }
    assert(nr_free_pages() == pages);
    assert(nr_free_pages() == base);

    check_multi_slab();
    check_large_sizes();
    check_huge_requests();
    check_page_exhaustion();
    check_many_large();
    check_large_rollback();
    cprintf("slub_check() succeeded\n");
}
