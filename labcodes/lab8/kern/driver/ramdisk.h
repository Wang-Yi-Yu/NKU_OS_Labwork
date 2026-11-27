/**
 * @file   ramdisk.h
 * @brief  Interface of the ramdisk driver, which serves the swap and SFS disks from memory.
 *
 * The ramdisk has no hardware behind it. ramdisk_init() turns one slot of ide_devices[] (ide.h)
 * into a device whose sectors are the bytes of an image that the Makefile links into the kernel
 * (see ramdisk.c). ide_init() calls it at boot for SWAP_DEV_NO and DISK0_DEV_NO. This header has no
 * include guard; it is only included by ide.c and ramdisk.c.
 */
#include <ide.h>

/**
 * @brief Attach the ramdisk image of one device number to an ide_devices[] slot.
 *
 * The slot is cleared first. If the image is not empty, the slot becomes valid and its
 * read_secs/write_secs hooks are set to the ramdisk versions. Otherwise the slot stays invalid.
 * Any other devno than SWAP_DEV_NO or DISK0_DEV_NO panics.
 *
 * @param devno SWAP_DEV_NO (the swap image) or DISK0_DEV_NO (the sfs.img image)
 * @param dev   the ide_devices[] slot to fill in
 */
void ramdisk_init(int devno, struct ide_device *dev);
