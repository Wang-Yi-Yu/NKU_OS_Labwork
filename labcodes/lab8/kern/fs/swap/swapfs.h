/**
 * @file   swapfs.h
 * @brief  Interface of the swap file system: page-sized reads and writes on the SWAP device.
 *
 * A swap entry (swap_entry_t, a PTE-format value from memlayout.h) names a page slot on the swap
 * device. The swap_offset() macro extracts the slot number from an entry; swapfs_read() and
 * swapfs_write() move one 4 KB page between that slot and a physical page, through ide.c.
 *
 * Note: lab8 does not use the swap file system. Nothing in lab8 calls swapfs_init(), swapfs_read()
 * or swapfs_write(), and no page is ever swapped out. The framework port added swap_offset() and
 * max_swap_offset below, which the original headers did not have, so that swapfs.c compiles.
 */
#ifndef __KERN_FS_SWAP_SWAPFS_H__
#define __KERN_FS_SWAP_SWAPFS_H__

#include <memlayout.h>

/**
 * @brief Check the swap device and compute max_swap_offset.
 *
 * Panics if the SWAP device is not valid. Not called in lab8.
 */
void swapfs_init(void);
/**
 * @brief Read one page from the swap device into a physical page.
 * @param entry swap entry naming the slot to read (see swap_offset())
 * @param page  physical page to fill; its kernel virtual address is used as the buffer
 * @return the result of ide_read_secs(), 0 on success
 */
int swapfs_read(swap_entry_t entry, struct Page *page);
/**
 * @brief Write one physical page to the swap device.
 * @param entry swap entry naming the slot to write (see swap_offset())
 * @param page  physical page to write out; its kernel virtual address is used as the buffer
 * @return the result of ide_write_secs(), 0 on success
 */
int swapfs_write(swap_entry_t entry, struct Page *page);

/**
 * @brief Number of page slots on the swap device, set by swapfs_init().
 *
 * It is 0 until swapfs_init() runs, so swap_offset() rejects every entry before that.
 */
extern size_t max_swap_offset;

/**
 * @brief Get the page slot number of a swap entry, and check that it is in range.
 *
 * Expands to a statement expression. The slot is the entry shifted right by 8 bits, so slot 0 is
 * never valid. The macro panics with "invalid swap_entry_t" if the slot is 0 or is not below
 * max_swap_offset.
 *
 * @param entry swap_entry_t to decode (an expression; evaluated more than once)
 * @return the slot number, of type size_t
 */
#define swap_offset(entry)                                    \
    ({                                                        \
        size_t __offset = ((entry) >> 8);                     \
        if (!(__offset > 0 && __offset < max_swap_offset)) {  \
            panic("invalid swap_entry_t = %08x.\n", (entry)); \
        }                                                     \
        __offset;                                             \
    })

#endif /* !__KERN_FS_SWAP_SWAPFS_H__ */
