/**
 * @file   vmm.h
 * @brief  Virtual memory management: descriptors of an address space (mm_struct)
 *         and of its virtual memory areas (vma_struct), the VMA API, and the
 *         user-memory helpers used by system calls.
 *
 * An mm_struct describes one address space: it owns a page directory (pgdir) and
 * a list of VMAs. A vma_struct is a half-open interval [vm_start, vm_end) of
 * virtual addresses that share the same flags. lab5 adds the sharing counter and
 * lock of an mm, mm_map(), dup_mmap() (fork), exit_mmap() (exit) and the checks
 * user_mem_check(), copy_from_user(), copy_to_user(). lab7 replaces the lock of an mm with a
 * semaphore (mm_sem), so a process that cannot get the lock sleeps instead of spinning. lab9 adds
 * the page fault handler do_pgfault(), the mmap support (vma_struct::vm_file and vm_pgoff,
 * do_mmap(), do_munmap()) and the swap manager's private data in mm_struct::sm_priv.
 */
#ifndef __KERN_MM_VMM_H__
#define __KERN_MM_VMM_H__

#include <defs.h>
#include <list.h>
#include <memlayout.h>
#include <sync.h>
#include <sem.h>
#include <proc.h>

/** @brief Forward declaration: vma_struct refers to mm_struct before it is defined. */
struct mm_struct;

/**
 * @brief A virtual memory area: the half-open interval [vm_start, vm_end) with common flags.
 * @var vma_struct::vm_mm      address space that owns this VMA
 * @var vma_struct::vm_start   first address of the area (inclusive)
 * @var vma_struct::vm_end     first address after the area (exclusive)
 * @var vma_struct::vm_flags   VM_READ / VM_WRITE / VM_EXEC bits
 * @var vma_struct::list_link  node of mm_struct::mmap_list; the list is sorted by vm_start
 * @var vma_struct::vm_pgoff    offset of the mapping in its file, counted in pages (lab9; 0 for
 *                              anonymous areas)
 * @var vma_struct::vm_file     file mapped into the area (lab9), or NULL for anonymous memory; the
 *                              VMA holds one reference on it (fopen_count)
 */
struct vma_struct {
    struct mm_struct *vm_mm;  // the set of vma using the same PDT
    uintptr_t vm_start;       // start addr of vma
    uintptr_t vm_end;         // end addr of vma, not include the vm_end itself
    uint32_t vm_flags;        // flags of vma
    list_entry_t list_link;   // linear list link which sorted by start addr of vma
    int vm_pgoff;             // file offset in pages
    struct file *vm_file;     // mapped file pointer
};

/**
 * @brief Get the vma_struct that contains a list node (container_of).
 * @param le     list node (list_entry_t *)
 * @param member name of the node inside vma_struct, normally `list_link`
 */
#define le2vma(le, member) to_struct((le), struct vma_struct, member)

/** @name VMA permission flags (bits of vma_struct::vm_flags)
 *  VM_READ: readable; VM_WRITE: writable; VM_EXEC: executable.
 *  @{ */
/** @brief The area is readable (bit 0). */
#define VM_READ 0x00000001
/** @brief The area is writable (bit 1). */
#define VM_WRITE 0x00000002
/** @brief The area is executable (bit 2). */
#define VM_EXEC 0x00000004
/** @brief The area is a user stack (bit 3, lab5); used by load_icode() for the user stack VMA. */
#define VM_STACK 0x00000008

/** @} */

/**
 * @brief Memory descriptor of an address space: the set of VMAs that share one page directory.
 * @var mm_struct::mmap_list  list of VMAs sorted by vm_start (non-overlapping)
 * @var mm_struct::mmap_cache last VMA returned by find_vma(), tried first on the next lookup
 * @var mm_struct::pgdir      root page directory of the address space (kernel virtual address)
 * @var mm_struct::map_count  number of VMAs in mmap_list
 * @var mm_struct::sm_priv    private data of the swap manager (lab9: set by swap_init_mm(); NULL
 *                            until the swap manager is ready)
 * @var mm_struct::mm_count   number of processes that share this mm (lab5; set by copy_mm/load_icode)
 * @var mm_struct::mm_sem     binary semaphore (initial value 1) taken around dup_mmap() while the mm
 *                            is copied (lab5); lab7 uses it in place of the lab5 spin lock mm_lock
 * @var mm_struct::locked_by  PID of the process that holds mm_sem, or 0 if nobody does (debug aid)
 */
struct mm_struct {
    list_entry_t mmap_list;         // linear list link which sorted by start addr of vma
    struct vma_struct *mmap_cache;  // current accessed vma, used for speed purpose
    pde_t *pgdir;                   // the PDT of these vma
    int map_count;                  // the count of these vma
    void *sm_priv;                  // the private data for swap manager
    int mm_count;                   // the number ofprocess which shared the mm
    semaphore_t mm_sem;             // mutex for using dup_mmap fun to duplicat the mm
    int locked_by;                  // pid of the process that holds mm_sem, 0 if free
};

/**
 * @brief Find the VMA that contains addr.
 * @param mm   address space; NULL gives NULL
 * @param addr virtual address
 * @return the VMA with vm_start <= addr < vm_end, or NULL if none. Updates mm->mmap_cache.
 */
struct vma_struct *find_vma(struct mm_struct *mm, uintptr_t addr);
/**
 * @brief Allocate and initialise a VMA.
 * @param vm_start first address, inclusive
 * @param vm_end   first address after the area, exclusive
 * @param vm_flags VM_READ / VM_WRITE / VM_EXEC bits
 * @return the new VMA, or NULL if kmalloc() failed. It is not in any mm until insert_vma_struct().
 */
struct vma_struct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags);
/**
 * @brief Insert a VMA into the sorted list of an address space.
 * @param mm  address space
 * @param vma VMA to insert; must satisfy vm_start < vm_end; sets vma->vm_mm and increments map_count
 * @note Asserts that the VMA does not overlap its neighbours in the list.
 */
void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma);

/**
 * @brief Allocate an empty address space descriptor (no VMAs, pgdir NULL).
 * @return the new mm_struct, or NULL if kmalloc() failed
 */
struct mm_struct *mm_create(void);
/**
 * @brief Free all VMAs of an address space and then the mm_struct itself.
 * @param mm address space; must not be used after the call
 * @note The page directory and the pages mapped by the VMAs are not freed here.
 */
void mm_destroy(struct mm_struct *mm);

/**
 * @brief Initialise virtual memory management; called from kern_init(). Runs check_vmm().
 */
void vmm_init(void);

/**
 * @brief Create a VMA for [addr, addr + len) and insert it into the address space (lab5).
 * @param mm        address space; must not be NULL
 * @param addr      start address, rounded down to a page boundary
 * @param len       length in bytes; the end addr + len is rounded up to a page boundary
 * @param vm_flags  VM_READ / VM_WRITE / VM_EXEC / VM_STACK bits
 * @param vma_store if not NULL, receives the created VMA on success
 * @return 0 on success; -E_INVAL if the range is not inside the user address space (USER_ACCESS)
 *         or its start lies in an existing VMA that reaches into the range; -E_NO_MEM if the VMA
 *         could not be allocated
 */
int mm_map(struct mm_struct *mm, uintptr_t addr, size_t len, uint32_t vm_flags,
           struct vma_struct **vma_store);
/**
 * @brief Handle a page fault at addr (lab9).
 *
 * Finds the VMA of addr and checks the access against its permissions. Then it fills in the PTE:
 * for a file mapping the page is read from the file, for anonymous memory a zeroed page is
 * allocated, and a PTE that holds a swap entry is swapped in. Called by pgfault_handler() in trap.c.
 *
 * @param mm         address space of the faulting code
 * @param error_code scause of the fault; 15 (CAUSE_STORE_PAGE_FAULT) means a write access
 * @param addr       faulting virtual address (stval)
 * @return 0 if the fault was repaired and the instruction can be re-executed; -E_INVAL if addr is
 *         in no VMA or a write hits a read-only VMA; -E_NO_MEM if no page could be allocated
 */
int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr);

/**
 * @brief Remove a range from the address space.
 * @param mm   address space
 * @param addr start address of the range
 * @param len  length in bytes
 * @return 0 on success, a negative error code otherwise
 * @warning Declared only; vmm.c of this lab does not define it and no code calls it. The lab9
 *          munmap() path uses do_munmap() instead.
 */
int mm_unmap(struct mm_struct *mm, uintptr_t addr, size_t len);
/**
 * @brief Copy the VMAs and the mapped pages of one address space into another (used by fork).
 * @param to   destination address space; freshly created and empty
 * @param from source address space (the parent's)
 * @return 0 on success, -E_NO_MEM if memory is exhausted
 * @note lab5 exercise (function3), implemented in vmm.c: one VMA is created in @p to for every VMA
 *       of @p from, and the pages of that VMA are copied with copy_range().
 */
int dup_mmap(struct mm_struct *to, struct mm_struct *from);
/**
 * @brief Release the pages and the page tables behind every VMA of an address space (used by exit).
 * @param mm address space; asserts that mm_count(mm) is already 0. The VMA list itself stays
 *           intact until mm_destroy().
 * @note Calls unmap_range() for every VMA (frees the pages), then exit_range() for every VMA (frees
 *       the page tables that became empty).
 */
void exit_mmap(struct mm_struct *mm);
/**
 * @brief Find a free range of len bytes in the user address space.
 * @param mm  address space
 * @param len length in bytes
 * @return the start address of a free range
 * @warning Declared only; vmm.c of this lab does not define it and no code calls it.
 */
uintptr_t get_unmapped_area(struct mm_struct *mm, size_t len);
/**
 * @brief Change the program break of the address space.
 * @param mm   address space
 * @param addr start address of the range
 * @param len  length in bytes
 * @return 0 on success, a negative error code otherwise
 * @warning Declared only; vmm.c of this lab does not define it and no code calls it.
 */
int mm_brk(struct mm_struct *mm, uintptr_t addr, size_t len);

/**
 * @brief Number of page faults handled by do_pgfault() so far (defined in vmm.c).
 *
 * The page replacement self-tests (check_swap) compare it with the expected number of faults.
 */
extern volatile unsigned int pgfault_num;
/**
 * @brief The mm_struct used by the VMM and swap self-tests, NULL while no test runs.
 *
 * Defined in vmm.c, set by check_pgfault() and check_swap(). alloc_pages() swaps out of it and
 * pgdir_alloc_page() registers new pages with the swap manager only while it is not NULL.
 */
extern struct mm_struct *check_mm_struct;

/**
 * @brief Check that a buffer is accessible for the given kind of access (lab5).
 * @param mm    address space of the calling process. For NULL (a kernel thread) the buffer only has
 *              to lie in the kernel part of the address space (KERN_ACCESS).
 * @param start first address of the buffer
 * @param len   length in bytes
 * @param write true to check for write access (every VMA must have VM_WRITE), false for read
 *              access (VM_READ)
 * @return true if the whole buffer is inside the user range and covered by VMAs with the needed
 *         permission, false otherwise. A write to the lowest page of a VM_STACK VMA is rejected.
 * @note System calls must call this before dereferencing a pointer received from user mode.
 */
bool user_mem_check(struct mm_struct *mm, uintptr_t start, size_t len, bool write);
/**
 * @brief Copy len bytes from a user buffer into a kernel buffer after checking the source (lab5).
 * @param mm       address space of the calling process
 * @param dst      kernel destination
 * @param src      user source
 * @param len      length in bytes
 * @param writable passed as the write flag to user_mem_check() for the source range (normally false)
 * @return true on success, false if the source range is not valid
 */
bool copy_from_user(struct mm_struct *mm, void *dst, const void *src, size_t len, bool writable);
/**
 * @brief Copy len bytes from a kernel buffer into a user buffer after checking the destination (lab5).
 * @param mm  address space of the calling process
 * @param dst user destination, must be writable
 * @param src kernel source
 * @param len length in bytes
 * @return true on success, false if the destination range is not valid
 */
bool copy_to_user(struct mm_struct *mm, void *dst, const void *src, size_t len);
/**
 * @brief Copy a NUL-terminated string from a user buffer into a kernel buffer, page by page (lab8).
 * @param mm   address space of the calling process (NULL for a kernel thread)
 * @param dst  kernel destination of at least @p maxn bytes
 * @param src  user source string
 * @param maxn size of @p dst in bytes, including the terminating NUL
 * @return true if the whole string, including its NUL, was copied; false if a page of the source
 *         is not readable or the string does not fit into @p maxn bytes
 * @note Each piece is checked with user_mem_check() before it is read, so the string may cross
 *       page boundaries without trusting the user pointer.
 */
bool copy_string(struct mm_struct *mm, char *dst, const char *src, size_t maxn);

/**
 * @brief Map a file or anonymous memory into an address space (lab9 exercise, vmm.c).
 *
 * Reserves the virtual range [addr, addr + len) (page aligned) and records it as a VMA; no
 * physical page is allocated here, do_pgfault() does that on first access. With addr == 0 the
 * kernel picks a free range between the existing VMAs.
 *
 * @param mm       address space; the caller holds mm->mm_sem (sysfile_mmap() does)
 * @param addr     wanted start address, or 0 to let the kernel choose
 * @param len      length in bytes
 * @param vm_flags VM_READ / VM_WRITE / VM_EXEC bits of the new area
 * @param file     file to map, or NULL for an anonymous mapping
 * @param offset   byte offset into the file (ignored when file is NULL); stored in pages in
 *                 vma_struct::vm_pgoff
 * @return the start address of the new mapping, or a negative error code cast to uintptr_t
 *         (for example -E_INVAL for an invalid or overlapping range, -E_NO_MEM if the VMA cannot
 *         be allocated)
 */
uintptr_t do_mmap(struct mm_struct *mm, uintptr_t addr, size_t len, uint32_t vm_flags,
                  struct file *file, off_t offset);
/**
 * @brief Remove a mapping created by do_mmap() from an address space (lab9 exercise, vmm.c).
 *
 * Unmaps the pages that are mapped inside the range, releases the file reference of a file
 * mapping and deletes the VMA, so that the VMA list, the page table and the reference counts stay
 * consistent.
 *
 * @param mm   address space; the caller holds mm->mm_sem (sysfile_munmap() does)
 * @param addr start address of the range
 * @param len  length in bytes
 * @return 0 on success, or a negative error code (for example -E_INVAL for a range outside the
 *         user space or one that does not match an existing VMA)
 */
int do_munmap(struct mm_struct *mm, uintptr_t addr, size_t len);

/** @brief Number of processes that share the mm. */
static inline int mm_count(struct mm_struct *mm) {
    return mm->mm_count;
}

/** @brief Set the sharing counter of the mm to val. */
static inline void set_mm_count(struct mm_struct *mm, int val) {
    mm->mm_count = val;
}

/** @brief Increment the sharing counter of the mm and return the new value. */
static inline int mm_count_inc(struct mm_struct *mm) {
    mm->mm_count += 1;
    return mm->mm_count;
}

/** @brief Decrement the sharing counter of the mm and return the new value (0 means no sharer is left). */
static inline int mm_count_dec(struct mm_struct *mm) {
    mm->mm_count -= 1;
    return mm->mm_count;
}

/**
 * @brief Acquire mm->mm_sem with down() and record the owner; a NULL mm (kernel thread) is ignored.
 * @note Sleeps if another process holds the semaphore (lab7), unlike the lab5 spin lock.
 */
static inline void lock_mm(struct mm_struct *mm) {
    if (mm != NULL) {
        down(&(mm->mm_sem));
        if (current != NULL) {
            mm->locked_by = current->pid;
        }
    }
}

/** @brief Release mm->mm_sem with up() and clear the owner; a NULL mm (kernel thread) is ignored. */
static inline void unlock_mm(struct mm_struct *mm) {
    if (mm != NULL) {
        up(&(mm->mm_sem));
        mm->locked_by = 0;
    }
}

#endif /* !__KERN_MM_VMM_H__ */
