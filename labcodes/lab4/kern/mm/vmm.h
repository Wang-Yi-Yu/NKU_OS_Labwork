/**
 * @file   vmm.h
 * @brief  Virtual memory management: descriptors of an address space (mm_struct)
 *         and of its virtual memory areas (vma_struct), the VMA API, and the
 *         page fault entry point.
 *
 * An mm_struct describes one address space: it owns a page directory (pgdir) and
 * a list of VMAs. A vma_struct is a half-open interval [vm_start, vm_end) of
 * virtual addresses that share the same flags. In this lab only the VMA part is
 * used (vmm_init() runs the VMA self test); do_pgfault() is declared but not
 * defined in this lab.
 */
#ifndef __KERN_MM_VMM_H__
#define __KERN_MM_VMM_H__

#include <defs.h>
#include <list.h>
#include <memlayout.h>
#include <sync.h>

//pre define
struct mm_struct;

/**
 * @brief A virtual memory area: the half-open interval [vm_start, vm_end) with common flags.
 * @var vma_struct::vm_mm      address space that owns this VMA
 * @var vma_struct::vm_start   first address of the area (inclusive)
 * @var vma_struct::vm_end     first address after the area (exclusive)
 * @var vma_struct::vm_flags   VM_READ / VM_WRITE / VM_EXEC bits
 * @var vma_struct::list_link  node of mm_struct::mmap_list; the list is sorted by vm_start
 */
// the virtual continuous memory area(vma), [vm_start, vm_end),
// addr belong to a vma means  vma.vm_start<= addr <vma.vm_end
struct vma_struct {
    struct mm_struct *vm_mm;  // the set of vma using the same PDT
    uintptr_t vm_start;       // start addr of vma
    uintptr_t vm_end;         // end addr of vma, not include the vm_end itself
    uint32_t vm_flags;        // flags of vma
    list_entry_t list_link;   // linear list link which sorted by start addr of vma
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
#define VM_READ  0x00000001
#define VM_WRITE 0x00000002
#define VM_EXEC  0x00000004

/** @} */

/**
 * @brief Memory descriptor of an address space: the set of VMAs that share one page directory.
 * @var mm_struct::mmap_list  list of VMAs sorted by vm_start (non-overlapping)
 * @var mm_struct::mmap_cache last VMA returned by find_vma(), tried first on the next lookup
 * @var mm_struct::pgdir      root page directory of the address space (kernel virtual address)
 * @var mm_struct::map_count  number of VMAs in mmap_list
 * @var mm_struct::sm_priv    private data of the swap manager (NULL here)
 */
// the control struct for a set of vma using the same PDT
struct mm_struct {
    list_entry_t mmap_list;         // linear list link which sorted by start addr of vma
    struct vma_struct *mmap_cache;  // current accessed vma, used for speed purpose
    pde_t *pgdir;                   // the PDT of these vma
    int map_count;                  // the count of these vma
    void *sm_priv;                  // the private data for swap manager
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
 * @brief Page fault handler: check that addr belongs to mm and repair the mapping.
 * @param mm         address space of the faulting process
 * @param error_code page fault cause bits from the trap frame
 * @param addr       faulting virtual address
 * @return 0 if the fault was handled, non-zero if the access is invalid
 * @warning Declared here but not defined in this lab's vmm.c, and not called from trap.c.
 */
int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr);

/**
 * @brief Number of page faults handled so far (used by the page fault tests).
 * @warning Declared only; no definition exists in this lab's sources.
 */
extern volatile unsigned int pgfault_num;
/**
 * @brief mm_struct used by the page fault test code; declared here and in trap.c.
 * @warning No definition exists in this lab's sources.
 */
extern struct mm_struct *check_mm_struct;
#endif /* !__KERN_MM_VMM_H__ */
