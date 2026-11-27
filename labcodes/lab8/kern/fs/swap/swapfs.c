/**
 * @file   swapfs.c
 * @brief  Swap file system: whole-page reads and writes on the SWAP device.
 *
 * The swap device is the ramdisk of the swap image (ramdisk.c, reached through ide.c). A page is
 * PGSIZE (4096) bytes, which is PAGE_NSECT (8) sectors, so swap slot n occupies sectors
 * n*8 .. n*8+7. swapfs_init() works out how many slots exist. The swap_offset() macro (swapfs.h)
 * maps a swap entry to a slot and checks it.
 *
 * lab8 does not use this file: no code calls swapfs_init(), swapfs_read() or swapfs_write(), so
 * max_swap_offset stays 0 and the swap device is never touched.
 */
#include <swapfs.h>
#include <mmu.h>
#include <fs.h>
#include <ide.h>
#include <pmm.h>
#include <assert.h>

/** @brief Number of swap slots (one per page) on the swap device; 0 until swapfs_init() runs. */
size_t max_swap_offset;

/**
 * @brief Check the swap device and compute max_swap_offset.
 *
 * max_swap_offset is the device size in sectors divided by the sectors per page (PAGE_NSECT).
 * Panics if the SWAP device is not valid. Not called in lab8.
 */
void swapfs_init(void) {
    static_assert((PGSIZE % SECTSIZE) == 0);
    if (!ide_device_valid(SWAP_DEV_NO)) {
        panic("swap fs isn't available.\n");
    }
    max_swap_offset = ide_device_size(SWAP_DEV_NO) / (PGSIZE / SECTSIZE);
}

/**
 * @brief Read the page stored in a swap slot into a physical page.
 *
 * Reads PAGE_NSECT sectors starting at the first sector of the slot into the kernel virtual
 * address of page. Not called in lab8.
 *
 * @param entry swap entry of the slot; swap_offset() panics if it is out of range
 * @param page  physical page to fill
 * @return the result of ide_read_secs(), 0 on success
 */
int swapfs_read(swap_entry_t entry, struct Page *page) {
    return ide_read_secs(SWAP_DEV_NO, swap_offset(entry) * PAGE_NSECT, page2kva(page), PAGE_NSECT);
}

/**
 * @brief Write a physical page into a swap slot.
 *
 * Writes PAGE_NSECT sectors from the kernel virtual address of page to the slot. Not called in lab8.
 *
 * @param entry swap entry of the slot; swap_offset() panics if it is out of range
 * @param page  physical page to write out
 * @return the result of ide_write_secs(), 0 on success
 */
int swapfs_write(swap_entry_t entry, struct Page *page) {
    return ide_write_secs(SWAP_DEV_NO, swap_offset(entry) * PAGE_NSECT, page2kva(page), PAGE_NSECT);
}
