/**
 * @file   mmu.h
 * @brief  Constants and macros for RISC-V Sv39 paging: page size, linear address
 *         decomposition, PTE/PDE address extraction and PTE flag bits.
 *
 * Sv39 uses 39-bit virtual addresses and 4 KiB pages, with three levels of page
 * tables of 512 eight-byte entries each, so every table is exactly one page.
 * The names PDX1 / PDX0 / PTX (inherited from 32-bit ucore) denote the level-2,
 * level-1 and level-0 indices, i.e. VPN2 / VPN1 / VPN0 of Sv39.
 */
#ifndef __KERN_MM_MMU_H__
#define __KERN_MM_MMU_H__

#ifndef __ASSEMBLER__
#include <defs.h>
#endif /* !__ASSEMBLER__ */

// A linear address 'la' has a three-part structure as follows:
//
// +--------10------+-------10-------+---------12----------+
// | Page Directory |   Page Table   | Offset within Page  |
// |      Index     |     Index      |                     |
// +----------------+----------------+---------------------+
//  \--- PDX(la) --/ \--- PTX(la) --/ \---- PGOFF(la) ----/
//  \----------- PPN(la) -----------/
//
// The PDX, PTX, PGOFF, and PPN macros decompose linear addresses as shown.
// To construct a linear address la from PDX(la), PTX(la), and PGOFF(la),
// use PGADDR(PDX(la), PTX(la), PGOFF(la)).

/**
 * @note The two diagrams above and below are inherited from 32-bit ucore (Sv32:
 *       10/10/12 bits, 34-bit physical address). The code is for RV64 Sv39:
 *       VPN2 = bits [38:30] (PDX1), VPN1 = bits [29:21] (PDX0), VPN0 = bits
 *       [20:12] (PTX), page offset = bits [11:0]; each index is 9 bits (mask 0x1FF).
 *       A PTE holds the PPN in bits [53:10] and the flags in bits [9:0].
 */
// RISC-V uses 32-bit virtual address to access 34-bit physical address!
// Sv32 page table entry:
// +---------12----------+--------10-------+---2----+-------8-------+
// |       PPN[1]        |      PPN[0]     |Reserved|D|A|G|U|X|W|R|V|
// +---------12----------+-----------------+--------+---------------+

/** @name Linear address index extraction (9-bit indices, see the note above)
 *  @{ */
/**
 * @brief Index of the root page directory (level 2, VPN2) of a virtual address.
 * @param la virtual address
 */
// page directory index
#define PDX1(la) ((((uintptr_t)(la)) >> PDX1SHIFT) & 0x1FF)
#define PDX0(la) ((((uintptr_t)(la)) >> PDX0SHIFT) & 0x1FF)

/**
 * @brief Index into the leaf page table (level 0, VPN0) of a virtual address.
 * @param la virtual address
 */
// page table index
#define PTX(la) ((((uintptr_t)(la)) >> PTXSHIFT) & 0x1FF)

/**
 * @brief Page number of an address: the bits above the 12-bit page offset (`la >> 12`).
 *
 * Works for virtual and physical addresses. The macro uses PTXSHIFT, which has
 * the same value as PGSHIFT (12).
 * @param la address
 */
// page number field of address
#define PPN(la) (((uintptr_t)(la)) >> PTXSHIFT)

/**
 * @brief Offset of an address inside its 4 KiB page (low 12 bits).
 * @param la address
 */
// offset in page
#define PGOFF(la) (((uintptr_t)(la)) & 0xFFF)

/**
 * @brief Rebuild a virtual address from PDX1, PDX0, PTX and offset (inverse of PDX1/PDX0/PTX/PGOFF).
 * @param d1 root directory index (PDX1)
 * @param d0 second level index (PDX0)
 * @param t  leaf table index (PTX)
 * @param o  offset within the page
 * @return the virtual address
 */
// construct linear address from indexes and offset
#define PGADDR(d1, d0, t, o) \
    ((uintptr_t)((d1) << PDX1SHIFT | (d0) << PDX0SHIFT | (t) << PTXSHIFT | (o)))

/**
 * @brief Physical address of the page (or of the next-level table) that a PTE/PDE points to.
 *
 * The PPN field occupies PTE bits [53:10]; `(pte & ~0x3FF) << 2` moves it to
 * physical bit 12, which is the same as `(pte >> 10) << 12`. The flag bits are
 * cleared. PDE_ADDR is an alias for readability when the entry is a pointer.
 * @param pte PTE or PDE value
 * @return physical address, page aligned
 */
// address in page table or page directory entry
#define PTE_ADDR(pte) (((uintptr_t)(pte) & ~0x3FF) << (PTXSHIFT - PTE_PPN_SHIFT))
#define PDE_ADDR(pde) PTE_ADDR(pde)

/** @} */

/** @name Page directory / page table geometry
 *  @{ */
/* page directory and page table constants */
#define NPDEENTRY 512  // page directory entries per page directory
#define NPTEENTRY 512  // page table entries per page table

/** @brief Bytes mapped by one page, 4096 (4 KiB). */
#define PGSIZE  4096  // bytes mapped by a page
#define PGSHIFT 12    // log2(PGSIZE)
/**
 * @brief Bytes mapped by one leaf page table, i.e. by one page directory entry
 *        (PGSIZE * 512 = 2 MiB). Also the 2 MiB megapage size.
 */
#define PTSIZE  (PGSIZE * NPTEENTRY)  // bytes mapped by a page directory entry
#define PTSHIFT 21                    // log2(PTSIZE)

/** @} */

/**
 * @name Bit positions of the fields of a linear address and of a PTE.
 * @{ */
/** @brief Bit position of PTX (VPN0), 12. Also the page offset width. */
#define PTXSHIFT      12  // offset of PTX in a linear address
#define PDX0SHIFT     21  // offset of PDX in a linear address
#define PDX1SHIFT     30
#define PTE_PPN_SHIFT 10  // offset of PPN in a physical address

/** @} */

/** @name PTE flag bits (the low 10 bits of a PTE)
 *  V: valid (if 0 the hardware ignores all other bits); R/W/X: read/write/execute
 *  (R/W/X all 0 marks a pointer to the next-level table); U: user accessible;
 *  G: global; A: accessed; D: dirty; SOFT: reserved for software.
 *  @{ */
// page table entry (PTE) fields
#define PTE_V    0x001  // Valid
#define PTE_R    0x002  // Read
#define PTE_W    0x004  // Write
#define PTE_X    0x008  // Execute
#define PTE_U    0x010  // User
#define PTE_G    0x020  // Global
#define PTE_A    0x040  // Accessed
#define PTE_D    0x080  // Dirty
#define PTE_SOFT 0x300  // Reserved for Software

/** @} */

/** @name Commonly used flag combinations
 *  @{ */
/** @brief Pointer to the next-level table (R/W/X all 0). */
#define PAGE_TABLE_DIR  (PTE_V)
#define READ_ONLY       (PTE_R | PTE_V)
#define READ_WRITE      (PTE_R | PTE_W | PTE_V)
#define EXEC_ONLY       (PTE_X | PTE_V)
#define READ_EXEC       (PTE_R | PTE_X | PTE_V)
#define READ_WRITE_EXEC (PTE_R | PTE_W | PTE_X | PTE_V)

/** @} */

/**
 * @brief Mask of R/W/X/U/V permission bits; get_pgtable_items() compares the
 *        permissions of consecutive entries with it.
 */
#define PTE_USER (PTE_R | PTE_W | PTE_X | PTE_U | PTE_V)

#endif /* !__KERN_MM_MMU_H__ */
