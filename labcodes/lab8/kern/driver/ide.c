/**
 * @file   ide.c
 * @brief  Device table set-up and sector I/O dispatch for the swap and disk0 devices.
 *
 * ide_init() fills the two slots that lab8 uses (SWAP_DEV_NO and DISK0_DEV_NO) with ramdisks
 * (ramdisk.c), then the other functions check a device number and forward the request to the
 * read or write hook of its slot. The name is a leftover of the IDE driver of the original
 * ucore; no real IDE hardware is driven in this kernel. Callers are swapfs.c (swap device) and
 * dev_disk0.c (the disk0 device used by the SFS file system).
 */
#include <assert.h>
#include <defs.h>
#include <fs.h>
#include <ide.h>
#include <ramdisk.h>
#include <stdio.h>

/**
 * @brief Stub of the IDE "wait until the controller is ready" routine. It does nothing.
 *
 * Kept from the original driver; no code calls it in lab8, and the parameters are ignored.
 *
 * @param iobase      I/O base port (ignored)
 * @param check_error whether to check the error bit (ignored)
 * @return always 0
 */
static int ide_wait_ready(unsigned short iobase, bool check_error) {
    return 0;
}

/**
 * @brief Initialise the device table: mark all slots invalid, then set up the two ramdisks.
 *
 * SWAP_DEV_NO gets the swap image and DISK0_DEV_NO gets the sfs.img image, both through
 * ramdisk_init(). Each one is asserted to be valid afterwards. Also checks at compile time that
 * SECTSIZE is a multiple of 4.
 */
void ide_init(void) {
    static_assert((SECTSIZE % 4) == 0);
    for (int ideno = 0; ideno < MAX_IDE; ++ideno) {
        ide_devices[ideno].valid = 0;
    }

    ramdisk_init(SWAP_DEV_NO, &ide_devices[SWAP_DEV_NO]);
    assert(VALID_IDE(SWAP_DEV_NO));

    ramdisk_init(DISK0_DEV_NO, &ide_devices[DISK0_DEV_NO]);
    assert(VALID_IDE(DISK0_DEV_NO));
}

/**
 * @brief Check whether a device number names a usable device.
 * @param ideno device number to test
 * @return true if the slot is valid, false otherwise
 */
bool ide_device_valid(unsigned short ideno) {
    return VALID_IDE(ideno);
}

/**
 * @brief Get the size of a device in sectors.
 * @param ideno device number
 * @return size in sectors, or 0 if the slot is not valid
 */
size_t ide_device_size(unsigned short ideno) {
    if (ide_device_valid(ideno)) {
        return ide_devices[ideno].size;
    }
    return 0;
}

/**
 * @brief Read sectors from a device into memory, through the read hook of its slot.
 *
 * Asserts that the device is valid, that nsecs is at most MAX_NSECS, and that the sector range
 * ends below MAX_DISK_NSECS. It does not check the range against the device size; the hook
 * does that (see ramdisk_read()).
 *
 * @param ideno device number
 * @param secno first sector to read
 * @param dst   destination buffer, at least nsecs * SECTSIZE bytes
 * @param nsecs number of sectors
 * @return the result of the read hook, 0 on success
 */
int ide_read_secs(unsigned short ideno, uint32_t secno, void *dst, size_t nsecs) {
    assert(nsecs <= MAX_NSECS && VALID_IDE(ideno));
    assert(secno < MAX_DISK_NSECS && secno + nsecs <= MAX_DISK_NSECS);
    return ide_devices[ideno].read_secs(&ide_devices[ideno], secno, dst, nsecs);
}

/**
 * @brief Write sectors from memory to a device, through the write hook of its slot.
 *
 * Same checks as ide_read_secs().
 *
 * @param ideno device number
 * @param secno first sector to write
 * @param src   source buffer, at least nsecs * SECTSIZE bytes
 * @param nsecs number of sectors
 * @return the result of the write hook, 0 on success
 */
int ide_write_secs(unsigned short ideno, uint32_t secno, const void *src, size_t nsecs) {
    assert(nsecs <= MAX_NSECS && VALID_IDE(ideno));
    assert(secno < MAX_DISK_NSECS && secno + nsecs <= MAX_DISK_NSECS);
    return ide_devices[ideno].write_secs(&ide_devices[ideno], secno, src, nsecs);
}
