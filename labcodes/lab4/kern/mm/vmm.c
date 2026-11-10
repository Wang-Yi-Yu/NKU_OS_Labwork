/**
 * @file   vmm.c
 * @brief  Virtual memory management: creation, destruction, lookup and ordered
 *         insertion of mm_struct and vma_struct, plus the VMA self test.
 *
 * The block comment below (the TA's design notes) describes the module. Only the
 * VMA functions are active in this lab; the page fault code is commented out.
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
#include <kmalloc.h>

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

// szx func : print_vma and print_mm
/**
 * @brief Print the fields of one VMA to the console (debugging aid).
 * @param name label printed in the header line
 * @param vma  VMA to print
 */
void print_vma(char *name, struct vma_struct *vma) {
    cprintf("-- %s print_vma --\n", name);
    cprintf("   mm_struct: %p\n", vma->vm_mm);
    cprintf("   vm_start,vm_end: %x,%x\n", vma->vm_start, vma->vm_end);
    cprintf("   vm_flags: %x\n", vma->vm_flags);
    cprintf("   list_entry_t: %p\n", &vma->list_link);
}

/**
 * @brief Print an address space and all its VMAs in list order (debugging aid).
 * @param name label printed in the header line
 * @param mm   address space to print
 */
void print_mm(char *name, struct mm_struct *mm) {
    cprintf("-- %s print_mm --\n", name);
    cprintf("   mmap_list: %p\n", &mm->mmap_list);
    cprintf("   map_count: %d\n", mm->map_count);
    list_entry_t *list = &mm->mmap_list;
    for (int i = 0; i < mm->map_count; i++) {
        list = list_next(list);
        print_vma(name, le2vma(list, list_link));
    }
}

static void check_vmm(void);
static void check_vma_struct(void);
static void check_pgfault(void);

/**
 * @brief Allocate and initialise an empty mm_struct.
 * @return the new mm_struct, or NULL if kmalloc() failed. The list is empty, map_count is 0,
 *         and pgdir, mmap_cache and sm_priv are NULL.
 */
// mm_create -  alloc a mm_struct & initialize it.
struct mm_struct *mm_create(void) {
    struct mm_struct *mm = kmalloc(sizeof(struct mm_struct));

    if (mm != NULL) {
        list_init(&(mm->mmap_list));
        mm->mmap_cache = NULL;
        mm->pgdir = NULL;
        mm->map_count = 0;
        mm->sm_priv = NULL;
    }
    return mm;
}

/**
 * @brief Allocate and initialise a VMA covering [vm_start, vm_end).
 * @param vm_start first address, inclusive
 * @param vm_end   first address after the area, exclusive
 * @param vm_flags VM_READ / VM_WRITE / VM_EXEC bits
 * @return the new VMA, or NULL if kmalloc() failed. Not yet linked into an mm.
 */
// vma_create - alloc a vma_struct & initialize it. (addr range: vm_start~vm_end)
struct vma_struct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags) {
    struct vma_struct *vma = kmalloc(sizeof(struct vma_struct));

    if (vma != NULL) {
        vma->vm_start = vm_start;
        vma->vm_end = vm_end;
        vma->vm_flags = vm_flags;
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
// find_vma - find a vma  (vma->vm_start <= addr <= vma_vm_end)
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
// check_vma_overlap - check if vma1 overlaps vma2 ?
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
// insert_vma_struct -insert vma in mm's list link
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
// mm_destroy - free mm and mm internal fields
void mm_destroy(struct mm_struct *mm) {
    list_entry_t *list = &(mm->mmap_list), *le;
    while ((le = list_next(list)) != list) {
        list_del(le);
        kfree(le2vma(le, list_link));  // kfree vma
    }
    kfree(mm);  // kfree mm
    mm = NULL;
}

/**
 * @brief Initialise virtual memory management; currently only runs check_vmm().
 */
// vmm_init - initialize virtual memory management
//          - now just call check_vmm to check correctness of vmm
void vmm_init(void) {
    check_vmm();
}

/**
 * @brief Self test of the VMA functions; prints a success message.
 * @note check_pgfault() is commented out, so the page fault tests are not run.
 */
// check_vmm - check correctness of vmm
static void check_vmm(void) {
    check_vma_struct();
    // check_pgfault();

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
