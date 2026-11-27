/**
 * @file   swapfs.h
 * @brief  Interface of the swap file system: page-sized reads and writes on the SWAP device.
 *
 * A swap entry (swap_entry_t, a PTE-format value from memlayout.h) names a page slot on the swap
 * device. The swap_offset() macro extracts the slot number from an entry; swapfs_read() and
 * swapfs_write() move one 4 KB page between that slot and a physical page, through ide.c.
 *
 * max_swap_offset and swap_offset() are defined in swap.h (swap.c); this header includes swap.h.
 */
#ifndef __KERN_FS_SWAP_SWAPFS_H__
#define __KERN_FS_SWAP_SWAPFS_H__

#include <memlayout.h>
#include <swap.h>

/**
 * @brief Check the swap device and compute max_swap_offset.
 *
 * Panics if the SWAP device is not valid. Called once by swap_init().
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

#endif /* !__KERN_FS_SWAP_SWAPFS_H__ */
