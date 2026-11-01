/**
 * @file   mmu.h
 * @brief  Constants and macros for RISC-V Sv39 paging: page size, virtual
 *         address decomposition and page table entry (PTE) flag bits.
 *
 * Sv39 uses 39-bit virtual addresses, up to 56-bit physical addresses, three
 * levels of page tables and 4KiB pages. A virtual address is made of VPN2 /
 * VPN1 / VPN0 (9 bits each) and a 12-bit page offset. A PTE is 8 bytes, so each
 * page table occupies exactly one page (512 entries). The bit layouts of the
 * virtual address and of a PTE are shown in the diagrams below.
 */
#ifndef __KERN_MM_MMU_H__
#define __KERN_MM_MMU_H__

#ifndef __ASSEMBLER__
#include <defs.h>
#endif

/** @brief Size of a page in bytes (4KiB). */
#define PGSIZE 4096
/** @brief log2(PGSIZE), i.e. the number of page-offset bits. */
#define PGSHIFT 12

/**
 * @brief Get the page number (physical or virtual) of an address.
 * @param la address
 */
#define PPN(la) (((uintptr_t)(la)) >> PGSHIFT)

// Sv39 linear address structure
// +-------9--------+-------9--------+--------9---------+----------12----------+
// |      VPN2      |      VPN1      |       VPN0       |  Offset within Page  |
// +----------------+----------------+------------------+----------------------+

// Sv39 in RISC-V64 uses 39-bit virtual address to access 56-bit physical address!
// Sv39 page table entry:
// +-------10--------+--------26-------+--------9----------+--------9--------+---2----+-------8-------+
// |    Reserved     |      PPN[2]     |      PPN[1]       |      PPN[0]     |Reserved|D|A|G|U|X|W|R|V|
// +-----------------+-----------------+-------------------+-----------------+--------+---------------+

/** @name Page directory / page table constants
 *  @{ */
/** @brief Number of entries in one page table (directory): 4096 / 8 = 512. */
#define SV39_NENTRY 512

/** @brief Size of a page in bytes, same as ::PGSIZE. */
#define SV39_PGSIZE 4096
/** @brief log2(SV39_PGSIZE). */
#define SV39_PGSHIFT 12
/**
 * @brief Number of bytes mapped by one page directory entry.
 * @warning The macro body uses `SV39NENTRY`, which is missing an underscore
 *          (it should be SV39_NENTRY). The macro is currently unused; fix it
 *          before using it.
 */
#define SV39_PTSIZE (PGSIZE * SV39NENTRY)
/** @brief log2(bytes mapped by one page directory entry), i.e. the shift of a 2MiB megapage. */
#define SV39_PTSHIFT 21

/** @brief Bit position of VPN0 (lowest-level page number) in a virtual address. */
#define SV39_VPN0SHIFT 12
/** @brief Bit position of VPN1 in a virtual address. */
#define SV39_VPN1SHIFT 21
/** @brief Bit position of VPN2 (highest-level page number) in a virtual address. */
#define SV39_VPN2SHIFT 30
/** @brief Bit position of the PPN inside a PTE (the low 10 bits are flags). */
#define SV39_PTE_PPN_SHIFT 10
/** @} */

/** @name Extract a page-number level from a virtual address (9 bits, 0..511)
 *  The `la` argument is the virtual address.
 *  @{ */
/** @brief Get VPN0 (index into the level-0 page table). */
#define SV39_VPN0(la) ((((uintptr_t)(la)) >> SV39_VPN0SHIFT) & 0x1FF)
/** @brief Get VPN1 (index into the level-1 page table). */
#define SV39_VPN1(la) ((((uintptr_t)(la)) >> SV39_VPN1SHIFT) & 0x1FF)
/** @brief Get VPN2 (index into the root page table). */
#define SV39_VPN2(la) ((((uintptr_t)(la)) >> SV39_VPN2SHIFT) & 0x1FF)
/** @} */
/**
 * @brief Get the page number of level n; n = 0/1/2 selects VPN0/VPN1/VPN2.
 * @param la virtual address
 * @param n  level (not parenthesised in the macro body, so wrap expressions in
 *           parentheses yourself)
 */
#define SV39_VPN(la, n) ((((uintptr_t)(la)) >> 12 >> (9 * n)) & 0x1FF)

/**
 * @brief Build a virtual address from the three page numbers and the page
 *        offset (the inverse of SV39_VPN2/1/0).
 * @param v2 VPN2
 * @param v1 VPN1
 * @param v0 VPN0
 * @param o  offset within the page
 */
#define SV39_PGADDR(v2, v1, v0, o) \
    ((uintptr_t)((v2) << SV39_VPN2SHIFT | (v1) << SV39_VPN1SHIFT | (v0) << SV39_VPN0SHIFT | (o)))

/**
 * @brief Get the physical address a PTE points to.
 * @param pte page table entry
 * @warning This macro is inherited from 32-bit ucore and does not match the
 *          Sv39 PTE layout (PPN lives in bits [53:10]); the correct form is
 *          `((pte) >> 10) << 12`. It is currently unused; fix it before using it.
 */
#define SV39_PTE_ADDR(pte) (((uintptr_t)(pte) & ~0x1FF) << 3)

/** @name Page table level numbers
 *  @{ */
#define SV39_PT0 0 /**< Level-0 table (leaf, each entry maps 4KiB). */
#define SV39_PT1 1 /**< Level-1 table (each entry may map a 2MiB megapage). */
#define SV39_PT2 2 /**< Root table (each entry may map a 1GiB gigapage). */
/** @} */

/** @name PTE flag bits (the low 10 bits of a PTE)
 *  @{ */
#define PTE_V    0x001 /**< Valid: the entry is valid; if 0 the hardware ignores all other bits. */
#define PTE_R    0x002 /**< Read: readable. */
#define PTE_W    0x004 /**< Write: writable. */
#define PTE_X    0x008 /**< Execute: executable. R/W/X all 0 = pointer to next-level table. */
#define PTE_U    0x010 /**< User: accessible from U mode. */
#define PTE_G    0x020 /**< Global: global mapping, the TLB does not distinguish ASIDs for it. */
#define PTE_A    0x040 /**< Accessed: read, written or fetched since last cleared. */
#define PTE_D    0x080 /**< Dirty: written since last cleared. */
#define PTE_SOFT 0x300 /**< RSW: two bits reserved for S-mode software. */
/** @} */

/** @name Commonly used flag combinations
 *  @{ */
#define PAGE_TABLE_DIR  (PTE_V)         /**< Pointer to the next-level table (R/W/X all 0). */
#define READ_ONLY       (PTE_R | PTE_V) /**< Read-only page. */
#define READ_WRITE      (PTE_R | PTE_W | PTE_V)         /**< Read-write page. */
#define EXEC_ONLY       (PTE_X | PTE_V)                 /**< Execute-only page. */
#define READ_EXEC       (PTE_R | PTE_X | PTE_V)         /**< Read-execute page. */
#define READ_WRITE_EXEC (PTE_R | PTE_W | PTE_X | PTE_V) /**< Read-write-execute page. */

/** @brief User page: R/W/X/U/V all set. */
#define PTE_USER (PTE_R | PTE_W | PTE_X | PTE_U | PTE_V)
/** @} */

#endif /* !__KERN_MM_MMU_H__ */
