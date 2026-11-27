/**
 * @file   ide.h
 * @brief  Block device table and sector I/O interface of the disk drivers (IDE slot abstraction).
 *
 * The table ide_devices[] has one slot per device number. Only two slots are used in lab9, and both
 * are ramdisks (ramdisk.c): SWAP_DEV_NO for the swap image and DISK0_DEV_NO for the SFS image. Each
 * slot carries a read and a write hook, so the callers do not care whether the sectors come from
 * memory or from hardware. The file system drivers use this layer: dev_disk0.c calls
 * ide_read_secs()/ide_write_secs() for the disk0 device, and swapfs.c calls them for the swap device.
 *
 * Sectors are SECTSIZE (512) bytes long; see fs.h.
 */
#ifndef __KERN_DRIVER_IDE_H__
#define __KERN_DRIVER_IDE_H__

#include <defs.h>

/** @brief Number of slots in ide_devices[]. */
#define MAX_IDE 4
/** @brief Largest number of sectors one ide_read_secs()/ide_write_secs() call may transfer (64 KB). */
#define MAX_NSECS 128
/** @brief Number of sectors that can be addressed (2^28 sectors, 128 GB). */
#define MAX_DISK_NSECS 0x10000000U
/**
 * @brief Non-zero if ideno names a slot that has been set up (see ide_init()).
 * @param ideno device number, 0 .. MAX_IDE - 1
 */
#define VALID_IDE(ideno) (((ideno) >= 0) && ((ideno) < MAX_IDE) && (ide_devices[ideno].valid))

/**
 * @brief One slot of the device table: a disk described by its geometry and its I/O hooks.
 *
 * The table is declared static in this header, so every file that includes it gets a private
 * copy. Only ide.c fills the copy in, and only ide.c reads it through VALID_IDE, so the copies in
 * the other files are never used.
 *
 * @var ide_device::valid     1 if the slot describes a real device (set by ide_init()), 0 otherwise
 * @var ide_device::sets      command sets supported; set to ~0 for the ramdisk, where it has no meaning
 * @var ide_device::size      device size in sectors
 * @var ide_device::iobase    for a ramdisk, the address in memory of the first byte of the image
 * @var ide_device::dev_data  driver private data; unused by the ramdisk (stays 0)
 * @var ide_device::model     model name as a string, "KERN_INITRD" for a ramdisk
 * @var ide_device::read_secs read hook: copy nsecs sectors from the device starting at secno into dst
 *                            (dev is this slot); returns 0 on success
 * @var ide_device::write_secs write hook: copy nsecs sectors from src to the device starting at
 *                            secno (dev is this slot); returns 0 on success
 */
static struct ide_device {
    unsigned int valid;
    unsigned int sets;
    unsigned int size;
    uintptr_t iobase;
    void *dev_data;
    char model[32];

    int (*read_secs)(struct ide_device *dev, size_t secno, void *dst, size_t nsecs);
    int (*write_secs)(struct ide_device *dev, size_t secno, const void *src, size_t nsecs);
} ide_devices[MAX_IDE];

/**
 * @brief Set up the device table: the swap and disk0 slots are described by ramdisk_init().
 *
 * Marks every slot invalid, then fills in SWAP_DEV_NO and DISK0_DEV_NO. Asserts that both are
 * valid afterwards, so a missing image stops the kernel here. Called once by the kernel init,
 * before fs_init().
 */
void ide_init(void);
/**
 * @brief Check whether a device number names a usable device.
 * @param ideno device number to test
 * @return true if the slot is valid, false otherwise
 */
bool ide_device_valid(unsigned short ideno);
/**
 * @brief Get the size of a device.
 * @param ideno device number
 * @return size in sectors, or 0 if the slot is not valid
 */
size_t ide_device_size(unsigned short ideno);

/**
 * @brief Read sectors from a device into memory.
 *
 * Asserts that the device is valid and that the request is within MAX_NSECS sectors and
 * MAX_DISK_NSECS addresses. Then calls the read hook of the slot.
 *
 * @param ideno device number (SWAP_DEV_NO or DISK0_DEV_NO in lab9)
 * @param secno first sector to read
 * @param dst   destination buffer, at least nsecs * SECTSIZE bytes
 * @param nsecs number of sectors, 1 .. MAX_NSECS
 * @return the result of the read hook: 0 on success
 */
int ide_read_secs(unsigned short ideno, uint32_t secno, void *dst, size_t nsecs);
/**
 * @brief Write sectors from memory to a device.
 *
 * Same checks as ide_read_secs(), then calls the write hook of the slot.
 *
 * @param ideno device number (SWAP_DEV_NO or DISK0_DEV_NO in lab9)
 * @param secno first sector to write
 * @param src   source buffer, at least nsecs * SECTSIZE bytes
 * @param nsecs number of sectors, 1 .. MAX_NSECS
 * @return the result of the write hook: 0 on success
 */
int ide_write_secs(unsigned short ideno, uint32_t secno, const void *src, size_t nsecs);

#endif /* !__KERN_DRIVER_IDE_H__ */
