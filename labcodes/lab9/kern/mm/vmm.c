/**
 * @file   vmm.c
 * @brief  Virtual memory management: creation, destruction, lookup and ordered
 *         insertion of mm_struct and vma_struct, plus the VMA self test.
 *
 * The block comment below (the TA's design notes) describes the module. It holds the VMA
 * functions, the lab5 additions (mm_map, dup_mmap, exit_mmap, user_mem_check and the
 * copy_*_user helpers) and, new in lab9, the page fault handler do_pgfault() and the mmap
 * functions do_mmap() and do_munmap() (exercises, at the end of the file).
 * The mm functions use kmalloc() from kmalloc.c for their descriptors.
 */
#include <vmm.h>
#include <sync.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <error.h>
#include <pmm.h>
#include <riscv.h>
#include <swap.h>
#include <kmalloc.h>
#include <file.h>
#include <iobuf.h>
#include <unistd.h>

/*
  vmm design include two parts: mm_struct (mm) & vma_struct (vma)
  mm is the memory manager for the set of continuous virtual memory
  area which have the same PDT. vma is a continuous virtual memory area.
  There a linear link list for vma & a redblack link list for vma in mm.
---------------
  mm related functions:
   golbal functions
     struct mm_struct * mm_create(void)
     void mm_destroy(struct mm_struct *mm)
     int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr)
--------------
  vma related functions:
   global functions
     struct vma_struct * vma_create (uintptr_t vm_start, uintptr_t vm_end,...)
     void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma)
     struct vma_struct * find_vma(struct mm_struct *mm, uintptr_t addr)
   local functions
     inline void check_vma_overlap(struct vma_struct *prev, struct vma_struct *next)
---------------
   check correctness functions
     void check_vmm(void);
     void check_vma_struct(void);
     void check_pgfault(void);
*/

static void check_vmm(void);
static void check_vma_struct(void);
static void check_pgfault(void);

/**
 * @brief Allocate and initialise an empty mm_struct.
 * @return the new mm_struct, or NULL if kmalloc() failed. The list is empty, map_count and
 *         mm_count are 0, mm_sem is initialised to 1 (unlocked), locked_by is 0 (the lock is not
 *         held), and pgdir and mmap_cache are NULL. sm_priv is set by swap_init_mm() once the swap
 *         manager is ready (swap_init_ok), and is NULL before that.
 */
struct mm_struct *mm_create(void) {
    struct mm_struct *mm = kmalloc(sizeof(struct mm_struct));

    if (mm != NULL) {
        list_init(&(mm->mmap_list));
        mm->mmap_cache = NULL;
        mm->pgdir = NULL;
        mm->map_count = 0;

        if (swap_init_ok)
            swap_init_mm(mm);
        else
            mm->sm_priv = NULL;

        set_mm_count(mm, 0);
        sem_init(&(mm->mm_sem), 1);
        mm->locked_by = 0;
    }
    return mm;
}

/**
 * @brief Allocate and initialise a VMA covering [vm_start, vm_end).
 * @param vm_start first address, inclusive
 * @param vm_end   first address after the area, exclusive
 * @param vm_flags VM_READ / VM_WRITE / VM_EXEC bits
 * @return the new VMA, or NULL if kmalloc() failed. Not yet linked into an mm. vm_file is NULL and
 *         vm_pgoff is 0 (an anonymous area); do_mmap() fills them in for a file mapping.
 */
struct vma_struct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags) {
    struct vma_struct *vma = kmalloc(sizeof(struct vma_struct));

    if (vma != NULL) {
        vma->vm_start = vm_start;
        vma->vm_end = vm_end;
        vma->vm_flags = vm_flags;
        vma->vm_file = NULL;  // not a file mapping until do_mmap() sets it
        vma->vm_pgoff = 0;    // page offset in the file
    }
    return vma;
}

/**
 * @brief Find the VMA that contains addr.
 *
 * Tries mm->mmap_cache first. Otherwise it scans the sorted list and stores the
 * result in mmap_cache.
 * @param mm   address space; NULL gives NULL
 * @param addr virtual address
 * @return the VMA with vm_start <= addr < vm_end, or NULL
 */
struct vma_struct *find_vma(struct mm_struct *mm, uintptr_t addr) {
    struct vma_struct *vma = NULL;
    if (mm != NULL) {
        vma = mm->mmap_cache;
        if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr)) {
            bool found = 0;
            list_entry_t *list = &(mm->mmap_list), *le = list;
            while ((le = list_next(le)) != list) {
                vma = le2vma(le, list_link);
                if (vma->vm_start <= addr && addr < vma->vm_end) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                vma = NULL;
            }
        }
        if (vma != NULL) {
            mm->mmap_cache = vma;
        }
    }
    return vma;
}

/**
 * @brief Check that prev is well formed, that next is well formed, and that they do not overlap.
 * @param prev VMA that should come first in the list
 * @param next VMA that should come after prev
 * @note Local helper of insert_vma_struct(). Uses assert(), so a violation panics.
 */
static inline void check_vma_overlap(struct vma_struct *prev, struct vma_struct *next) {
    assert(prev->vm_start < prev->vm_end);
    assert(prev->vm_end <= next->vm_start);
    assert(next->vm_start < next->vm_end);
}

/**
 * @brief Insert a VMA into an address space, keeping mmap_list sorted by vm_start.
 *
 * Finds the last VMA whose vm_start is not greater than the new vm_start, checks
 * overlap with it and with its successor, links the VMA after it, and increments
 * map_count. The VMA intervals are half-open, so touching intervals do not overlap.
 * @param mm  address space
 * @param vma VMA to insert; sets vma->vm_mm
 * @note Overlapping VMAs cause an assert failure.
 */
void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma) {
    assert(vma->vm_start < vma->vm_end);
    list_entry_t *list = &(mm->mmap_list);
    list_entry_t *le_prev = list, *le_next;

    list_entry_t *le = list;
    while ((le = list_next(le)) != list) {
        struct vma_struct *mmap_prev = le2vma(le, list_link);
        if (mmap_prev->vm_start > vma->vm_start) {
            break;
        }
        le_prev = le;
    }

    le_next = list_next(le_prev);

    /* check overlap */
    if (le_prev != list) {
        check_vma_overlap(le2vma(le_prev, list_link), vma);
    }
    if (le_next != list) {
        check_vma_overlap(vma, le2vma(le_next, list_link));
    }

    vma->vm_mm = mm;
    list_add_after(le_prev, &(vma->list_link));

    mm->map_count++;
}

/**
 * @brief Free all VMAs of an address space, then the mm_struct itself (via kfree()).
 * @param mm address space; must not be used after the call. The page directory and
 *           the pages mapped through the VMAs are not released here.
 * @note The `mm = NULL` at the end only clears the local parameter.
 */
void mm_destroy(struct mm_struct *mm) {
    assert(mm_count(mm) == 0);

    list_entry_t *list = &(mm->mmap_list), *le;
    while ((le = list_next(list)) != list) {
        list_del(le);
        kfree(le2vma(le, list_link));  // kfree vma
    }
    kfree(mm);  // kfree mm
    mm = NULL;
}

/**
 * @brief Create a VMA for [addr, addr + len) and insert it into the address space (lab5).
 *
 * @param mm        address space; must not be NULL
 * @param addr      start address, rounded down to a page boundary
 * @param len       length in bytes; addr + len is rounded up to a page boundary
 * @param vm_flags  VM_READ / VM_WRITE / VM_EXEC / VM_STACK bits
 * @param vma_store if not NULL, receives the new VMA on success
 * @return 0 on success; -E_INVAL if the range is outside the user address space or the existing
 *         VMA that find_vma() returns for the start address reaches into the range; -E_NO_MEM if
 *         the VMA could not be allocated
 * @note Provided by the lab framework. Used by load_icode() for the code, data and stack areas.
 */
int mm_map(struct mm_struct *mm, uintptr_t addr, size_t len, uint32_t vm_flags,
           struct vma_struct **vma_store) {
    uintptr_t start = ROUNDDOWN(addr, PGSIZE), end = ROUNDUP(addr + len, PGSIZE);
    if (!USER_ACCESS(start, end)) {
        return -E_INVAL;
    }

    assert(mm != NULL);

    int ret = -E_INVAL;

    struct vma_struct *vma;
    if ((vma = find_vma(mm, start)) != NULL && end > vma->vm_start) {
        goto out;
    }
    ret = -E_NO_MEM;

    if ((vma = vma_create(start, end, vm_flags)) == NULL) {
        goto out;
    }
    insert_vma_struct(mm, vma);
    if (vma_store != NULL) {
        *vma_store = vma;
    }
    ret = 0;

out:
    return ret;
}

//lab5 function3 2414007
/**
 * @fn int dup_mmap(struct mm_struct *to, struct mm_struct *from)
 * @brief Copy the virtual memory areas and the mapped pages of one address space into another.
 *
 * Used by fork (through copy_mm()): the child gets a VMA with the same range and flags for every
 * VMA of the parent, and its own physical copy of every page that is mapped in that VMA, so parent
 * and child can no longer see each other's writes.
 *
 * The VMAs are visited in the order of from->mmap_list (ascending addresses). For each of them a
 * new VMA is created, inserted into @p to (insert_vma_struct() keeps the list sorted and
 * rejects overlaps) and then its pages are copied with copy_range() in copy mode (share = 0).
 *
 * @param to   destination address space; freshly created, with its page directory already set up
 *             and no VMA yet
 * @param from source address space (the parent's); the caller holds from->mm_sem
 * @return 0 on success; -E_NO_MEM if a VMA or a page (or page table) could not be allocated. On
 *         failure the part already copied stays in @p to; the caller undoes it with exit_mmap().
 */
int dup_mmap(struct mm_struct *to, struct mm_struct *from) {
    assert(to != NULL && from != NULL);
    list_entry_t *list = &(from->mmap_list), *le = list;
    while ((le = list_next(le)) != list) {
        struct vma_struct *vma = le2vma(le, list_link);
        struct vma_struct *nvma = vma_create(vma->vm_start, vma->vm_end, vma->vm_flags);
        if (nvma == NULL) {
            return -E_NO_MEM;
        }
        insert_vma_struct(to, nvma);

        // Plain fork: every page is copied, nothing is shared (copy-on-write is the challenge).
        bool share = 0;
        if (copy_range(to->pgdir, from->pgdir, vma->vm_start, vma->vm_end, share) != 0) {
            return -E_NO_MEM;
        }
    }
    return 0;
}

/**
 * @brief Release the pages and page tables behind every VMA of an address space (used by exit).
 *
 * @param mm address space; asserts mm != NULL and that mm_count(mm) is already 0
 * @note Two passes over the VMA list: unmap_range() frees the mapped pages, then exit_range()
 *       frees the page tables that became empty. The VMA structures themselves are freed later
 *       by mm_destroy().
 */
void exit_mmap(struct mm_struct *mm) {
    assert(mm != NULL && mm_count(mm) == 0);
    pde_t *pgdir = mm->pgdir;
    list_entry_t *list = &(mm->mmap_list), *le = list;
    while ((le = list_next(le)) != list) {
        struct vma_struct *vma = le2vma(le, list_link);
        unmap_range(pgdir, vma->vm_start, vma->vm_end);
    }
    while ((le = list_next(le)) != list) {
        struct vma_struct *vma = le2vma(le, list_link);
        exit_range(pgdir, vma->vm_start, vma->vm_end);
    }
}

/**
 * @brief Copy len bytes from a user buffer to a kernel buffer, after checking the source.
 *
 * @param mm       address space of the calling process
 * @param dst      kernel destination
 * @param src      user source
 * @param len      length in bytes
 * @param writable passed on as the write flag of user_mem_check() for the source (normally 0)
 * @return 1 on success, 0 if the source range failed the check (nothing is copied then)
 */
bool copy_from_user(struct mm_struct *mm, void *dst, const void *src, size_t len, bool writable) {
    if (!user_mem_check(mm, (uintptr_t)src, len, writable)) {
        return 0;
    }
    memcpy(dst, src, len);
    return 1;
}

/**
 * @brief Copy len bytes from a kernel buffer to a user buffer, after checking the destination.
 *
 * @param mm  address space of the calling process
 * @param dst user destination, checked for write access
 * @param src kernel source
 * @param len length in bytes
 * @return 1 on success, 0 if the destination range failed the check (nothing is copied then)
 */
bool copy_to_user(struct mm_struct *mm, void *dst, const void *src, size_t len) {
    if (!user_mem_check(mm, (uintptr_t)dst, len, 1)) {
        return 0;
    }
    memcpy(dst, src, len);
    return 1;
}

/**
 * @brief Initialise virtual memory management; currently only runs check_vmm().
 * @note Called before swap_init(), so the mm structures made by the self-tests have sm_priv == NULL.
 */
void vmm_init(void) {
    check_vmm();
}

/**
 * @brief Self test of the VMA functions and of the page fault handler; prints a success message.
 * @note Runs check_vma_struct() and then check_pgfault() (lab9), which needs do_pgfault().
 */
static void check_vmm(void) {
    check_vma_struct();
    check_pgfault();

    cprintf("check_vmm() succeeded.\n");
}

/**
 * @brief Test of mm_create(), vma_create(), insert_vma_struct(), find_vma() and mm_destroy().
 *
 * Inserts 100 VMAs [5i, 5i + 2) for i = 1..100, first in descending and then in
 * ascending order, and checks that the list comes out sorted. find_vma() must find
 * the VMA at its start and just before its end, and must return NULL in the gap
 * after it and for addresses below 5.
 * @note Uses assert(); a failure panics.
 */
static void check_vma_struct(void) {
    // size_t nr_free_pages_store = nr_free_pages();

    struct mm_struct *mm = mm_create();
    assert(mm != NULL);

    int step1 = 10, step2 = step1 * 10;

    int i;
    for (i = step1; i >= 1; i--) {
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        insert_vma_struct(mm, vma);
    }

    for (i = step1 + 1; i <= step2; i++) {
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        insert_vma_struct(mm, vma);
    }

    list_entry_t *le = list_next(&(mm->mmap_list));

    for (i = 1; i <= step2; i++) {
        assert(le != &(mm->mmap_list));
        struct vma_struct *mmap = le2vma(le, list_link);
        assert(mmap->vm_start == i * 5 && mmap->vm_end == i * 5 + 2);
        le = list_next(le);
    }

    for (i = 5; i <= 5 * step2; i += 5) {
        struct vma_struct *vma1 = find_vma(mm, i);
        assert(vma1 != NULL);
        struct vma_struct *vma2 = find_vma(mm, i + 1);
        assert(vma2 != NULL);
        struct vma_struct *vma3 = find_vma(mm, i + 2);
        assert(vma3 == NULL);
        struct vma_struct *vma4 = find_vma(mm, i + 3);
        assert(vma4 == NULL);
        struct vma_struct *vma5 = find_vma(mm, i + 4);
        assert(vma5 == NULL);

        assert(vma1->vm_start == i && vma1->vm_end == i + 2);
        assert(vma2->vm_start == i && vma2->vm_end == i + 2);
    }

    for (i = 4; i >= 0; i--) {
        struct vma_struct *vma_below_5 = find_vma(mm, i);
        if (vma_below_5 != NULL) {
            cprintf("vma_below_5: i %x, start %x, end %x\n", i, vma_below_5->vm_start,
                    vma_below_5->vm_end);
        }
        assert(vma_below_5 == NULL);
    }

    mm_destroy(mm);

    cprintf("check_vma_struct() succeeded!\n");
}

/**
 * @brief The mm_struct of the page fault and swap self-tests; NULL while no test runs.
 *
 * Set by check_pgfault() and by check_swap() (swap.c). pgfault_handler() in trap.c resolves the
 * faults of the test code against this mm, and alloc_pages() swaps pages out of it.
 */
struct mm_struct *check_mm_struct;

/**
 * @brief Self-test of the page fault handler: write to an unmapped range and read it back.
 *
 * Builds an mm over the boot page table with one writable VMA [0, PTSIZE). Writing 100 bytes at
 * 0x100 faults on the first access, do_pgfault() maps a page, and the bytes read back must equal
 * the bytes written. Afterwards the page and the two page tables it needed are freed and the
 * number of free pages must be the same as before (no leak). Prints "check_pgfault() succeeded!".
 * An assertion failure panics.
 */
static void check_pgfault(void) {
    size_t nr_free_pages_store = nr_free_pages();

    check_mm_struct = mm_create();
    assert(check_mm_struct != NULL);

    struct mm_struct *mm = check_mm_struct;
    pde_t *pgdir = mm->pgdir = boot_pgdir_va;
    assert(pgdir[0] == 0);

    struct vma_struct *vma = vma_create(0, PTSIZE, VM_WRITE);
    assert(vma != NULL);

    insert_vma_struct(mm, vma);

    uintptr_t addr = 0x100;
    assert(find_vma(mm, addr) == vma);

    int i, sum = 0;

    for (i = 0; i < 100; i++) {
        *(char *)(addr + i) = i;
        sum += i;
    }
    for (i = 0; i < 100; i++) {
        sum -= *(char *)(addr + i);
    }

    assert(sum == 0);

    pde_t *pd1 = pgdir, *pd0 = page2kva(pde2page(pgdir[0]));
    page_remove(pgdir, ROUNDDOWN(addr, PGSIZE));
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    pgdir[0] = 0;
    flush_tlb();

    mm->pgdir = NULL;
    mm_destroy(mm);
    check_mm_struct = NULL;

    assert(nr_free_pages_store == nr_free_pages());

    cprintf("check_pgfault() succeeded!\n");
}
/**
 * @brief Number of page faults handled by do_pgfault() so far.
 *
 * The page replacement self-tests (check_swap) compare it with the number of faults they expect.
 */
volatile unsigned int pgfault_num = 0;

/**
 * @brief Repair a page fault: back the faulting page with memory (demand paging).
 *
 * Called through pgfault_handler() (trap.c) for a load or store page fault. The handler:
 *  1. looks up the VMA of addr; no VMA means an invalid access (-E_INVAL);
 *  2. refuses a write (error_code == CAUSE_STORE_PAGE_FAULT) into a VMA without VM_WRITE;
 *  3. derives the PTE permissions from the VMA flags (PTE_U | PTE_R, plus PTE_W for VM_WRITE);
 *  4. finds or creates the PTE of the page;
 *  5. for a file mapping (vma->vm_file != NULL) allocates a zeroed page and fills it from the file
 *     at the page offset vm_pgoff + (page index in the VMA); the file position is saved and
 *     restored around the read;
 *  6. for anonymous memory with an empty PTE allocates a zeroed page; for a PTE that holds a
 *     swap entry it swaps the page in, maps it and registers it with the swap manager.
 * After a successful return the faulting instruction is executed again and now succeeds.
 *
 * @param mm         address space of the faulting code
 * @param error_code scause of the fault; 15 (CAUSE_STORE_PAGE_FAULT) means a write access
 * @param addr       faulting virtual address (stval); rounded down to a page boundary inside
 * @return 0 on success; -E_INVAL if addr is in no VMA or the access is not allowed; -E_NO_MEM if
 *         no page table or page could be allocated; or the error of swap_in()
 * @note A failure to seek in or read the file is not reported: the page is kept (zero-filled or
 *       partly filled) and the function still returns 0.
 */
int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr) {
    int ret = -E_INVAL;
    struct vma_struct *vma = find_vma(mm, addr);

    pgfault_num++;

    // 1. Check that the address lies in a VMA.
    if (vma == NULL || vma->vm_start > addr) {
        cprintf("not valid addr %x, and can not find it in vma\n", addr);
        return -E_INVAL;
    }

    // 2. Permission check, for writes only. RISC-V: cause 15 (CAUSE_STORE_PAGE_FAULT) is a store.
    bool is_write = (error_code == 15);
    if (is_write && !(vma->vm_flags & VM_WRITE)) {
        cprintf("do_pgfault: write to non-writable vma\n");
        return -E_INVAL;
    }

    // 3. Page permissions. RISC-V needs PTE_R before a page can be accessed at all.
    uint32_t perm = PTE_U | PTE_R;  // user accessible + readable
    if (vma->vm_flags & VM_WRITE) {
        perm |= PTE_W;
    }

    addr = ROUNDDOWN(addr, PGSIZE);

    // 4. Get the PTE, creating the page table if necessary.
    pte_t *ptep = get_pte(mm->pgdir, addr, 1);
    if (ptep == NULL) {
        cprintf("get_pte in do_pgfault failed\n");
        return -E_NO_MEM;
    }

    // 5. Page fault in a file mapping, handled like Linux: the page is read from the file on
    //    demand, with file_seek() + file_read() at the right offset.
    if (vma->vm_file != NULL) {
        // File mapping: only the "page not yet present" case is handled here.
        if (*ptep == 0) {
            // Allocate the physical page.
            struct Page *page = pgdir_alloc_page(mm->pgdir, addr, perm);
            if (page == NULL) {
                cprintf("pgdir_alloc_page in mmap pgfault failed\n");
                return -E_NO_MEM;
            }

            void *kva = page2kva(page);
            // Zero it first, so that a file that ends inside the page leaves no stale data.
            memset(kva, 0, PGSIZE);

            // File offset: vm_pgoff is the page offset of the start of the mapping, plus the
            // index of this page inside the mapping.
            size_t page_index = (addr - vma->vm_start) / PGSIZE;
            off_t file_offset = (vma->vm_pgoff + page_index) * PGSIZE;

            // Seek to the right position first, then read.
            struct file *file = vma->vm_file;
            int fd = file->fd;
            off_t saved_pos = file->pos;  // save the current file position

            ret = file_seek(fd, file_offset, LSEEK_SET);
            if (ret != 0) {
                cprintf("file_seek in mmap pgfault failed: %e\n", ret);
                // Even if the seek fails the page is already allocated: report success and let
                // the process go on with a zero-filled page.
                return 0;
            }

            size_t read_size = 0;
            ret = file_read(fd, kva, PGSIZE, &read_size);

            // Restore the file position.
            file_seek(fd, saved_pos, LSEEK_SET);

            if (ret != 0 && ret != -E_EOF) {
                cprintf("file_read in mmap pgfault failed: %e\n", ret);
                // The read failed, but the page is already allocated: report success.
                return 0;
            }
            // The read succeeded (it may have returned anywhere from 0 to PGSIZE bytes).
        }
        // If *ptep != 0 the page already exists: return success.
        return 0;
    }

    // 6. Anonymous page / swap page.
    if (*ptep == 0) {
        // Anonymous page: allocate a new zeroed page.
        if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL) {
            cprintf("pgdir_alloc_page in do_pgfault failed\n");
            return -E_NO_MEM;
        }
    } else {
        // The PTE is not 0: either a swap entry or a page that is already present.
        if (swap_init_ok) {
            // Swap page: bring it back from the swap device.
            struct Page *page = NULL;
            if ((ret = swap_in(mm, addr, &page)) != 0) {
                cprintf("swap_in in do_pgfault failed\n");
                return ret;
            }
            page_insert(mm->pgdir, page, addr, perm);
            swap_map_swappable(mm, addr, page, 1);
            page->pra_vaddr = addr;
        } else {
            // The swap manager is not ready but the PTE is not 0: check whether it is a valid
            // page (PTE_V bit).
            if (*ptep & PTE_V) {
                // The page is already there (probably a permission problem): return success.
                return 0;
            }
            // Not a valid page: a leftover swap entry or garbage. Clear the PTE and allocate a
            // new page.
            *ptep = 0;
            if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL) {
                cprintf("pgdir_alloc_page in do_pgfault failed (swap not ready)\n");
                return -E_NO_MEM;
            }
        }
    }

    return 0;
}

/**
 * @brief Check that a buffer may be accessed in the requested way (see vmm.h).
 *
 * @param mm    address space of the caller. For a kernel thread (NULL) the buffer only has to lie in
 *              the kernel part of the address space (KERN_ACCESS).
 * @param addr  first address of the buffer
 * @param len   length in bytes
 * @param write 1 to check for write access (VM_WRITE), 0 for read access (VM_READ)
 * @return 1 if the whole buffer is in the user range and covered by VMAs with the needed
 *         permission, 0 otherwise. For a write into a VM_STACK VMA the lowest page of the VMA is
 *         refused as well.
 * @note Provided by the lab framework.
 */
bool user_mem_check(struct mm_struct *mm, uintptr_t addr, size_t len, bool write) {
    if (mm != NULL) {
        if (!USER_ACCESS(addr, addr + len)) {
            return 0;
        }
        struct vma_struct *vma;
        uintptr_t start = addr, end = addr + len;
        while (start < end) {
            if ((vma = find_vma(mm, start)) == NULL || start < vma->vm_start) {
                return 0;
            }
            if (!(vma->vm_flags & ((write) ? VM_WRITE : VM_READ))) {
                return 0;
            }
            if (write && (vma->vm_flags & VM_STACK)) {
                if (start < vma->vm_start + PGSIZE) {  // check stack start & size
                    return 0;
                }
            }
            start = vma->vm_end;
        }
        return 1;
    }
    return KERN_ACCESS(addr, addr + len);
}

/**
 * @brief Copy a NUL-terminated string from a user buffer into a kernel buffer, page by page (lab8).
 *
 * The source is read in pieces that end at page boundaries. Each piece is checked with
 * user_mem_check() before it is touched; if it contains the terminating NUL the string is copied
 * and the function returns, otherwise the piece is copied and the next page is examined.
 *
 * @param mm   address space of the calling process (NULL for a kernel thread)
 * @param dst  kernel destination of at least @p maxn bytes
 * @param src  user source string
 * @param maxn size of @p dst in bytes, including the terminating NUL
 * @return 1 if the whole string, including its NUL, was copied; 0 if a page of the source is not
 *         readable or the string does not fit into @p maxn bytes
 */
bool copy_string(struct mm_struct *mm, char *dst, const char *src, size_t maxn) {
    size_t alen, part = ROUNDDOWN((uintptr_t)src + PGSIZE, PGSIZE) - (uintptr_t)src;
    while (1) {
        if (part > maxn) {
            part = maxn;
        }
        if (!user_mem_check(mm, (uintptr_t)src, part, 0)) {
            return 0;
        }
        if ((alen = strnlen(src, part)) < part) {
            memcpy(dst, src, alen + 1);
            return 1;
        }
        if (part == maxn) {
            return 0;
        }
        memcpy(dst, src, part);
        dst += part, src += part, maxn -= part;
        part = PGSIZE;
    }
}

/*
 * Framework hint for do_mmap() (the function find_free_area() does not exist; it is only a hint):
 *   find_free_area(mm, len, start_hint) looks for a free range of len bytes (already page
 *   aligned) between the VMAs of the mm, starting at start_hint and, if nothing is found, again
 *   from the start of the user space. It returns the address found, or 0 on failure. The hint
 *   names First-Fit, but the lab9 specification of do_mmap() asks for Best-Fit.
 */

//lab9 function2 YOUR CODE
//lab9 function3 YOUR CODE
