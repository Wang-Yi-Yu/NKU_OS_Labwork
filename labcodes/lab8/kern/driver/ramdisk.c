/**
 * @file   ramdisk.c
 * @brief  Ramdisk driver: serves the swap and SFS disks from the images linked into the kernel.
 *
 * The Makefile builds two images, bin/swap.img (a few KB of zeros, the swap device) and bin/sfs.img
 * (2 MB, the SFS file system with the user programs copied in by tools/mksfs.c). Both are linked
 * into the kernel with "ld --format=binary" after the kernel objects (Makefile, the kernel rule).
 * The linker turns each file into a pair of symbols, _binary_bin_<name>_img_start and
 * _binary_bin_<name>_img_end, and places the bytes in the kernel image. tools/kernel.ld does not
 * name them; they are ordinary data placed by the linker after the kernel sections.
 *
 * So the disks exist in memory before any file system code runs. ramdisk_init() describes an image
 * by its base address and size in sectors and installs ramdisk_read()/ramdisk_write() as the
 * read_secs/write_secs hooks of an ide_devices[] slot (ide.h). ide_read_secs() and
 * ide_write_secs() then reach the memory through these hooks. Writes change only the in-memory
 * copy: the host image files are never touched, and everything written is lost at reboot.
 */
#include <assert.h>
#include <defs.h>
#include <fs.h>
#include <ramdisk.h>
#include <stdio.h>
#include <string.h>

/** @brief Smaller of two values. Evaluates its arguments twice. */
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

/**
 * @brief Linker-generated symbols that bound the two images in the kernel.
 *
 * _start is the first byte of the image and _end is one past its last byte. The swap pair comes
 * from bin/swap.img and the SFS pair from bin/sfs.img. The linker creates them, so no C definition
 * exists.
 */
extern char _binary_bin_swap_img_start[], _binary_bin_swap_img_end[];
extern char _binary_bin_sfs_img_start[], _binary_bin_sfs_img_end[];

/**
 * @brief Check that a linked image is not empty and print its address range.
 *
 * Prints a warning if the two symbols are equal, which means the image was not linked in.
 * This function is not static and has no prototype in ramdisk.h.
 *
 * @param _initrd_begin first byte of the image
 * @param _initrd_end   one past the last byte of the image
 * @return 1 if the image has at least one byte, 0 if it is empty
 */
bool check_initrd(const char _initrd_begin[], const char _initrd_end[]) {
    if (_initrd_begin == _initrd_end) {
        cprintf("Warning: No Initrd!\n");
        return 0;
    }
    cprintf("Initrd: 0x%08x - 0x%08x, size: 0x%08x\n", _initrd_begin, _initrd_end - 1,
            _initrd_end - _initrd_begin);
    return 1;
}

/**
 * @brief read_secs hook of a ramdisk: copy sectors out of the in-memory image.
 *
 * The request is cut short at the end of the image: only min(nsecs, dev->size - secno) sectors
 * are copied, and the function still returns 0. The caller is not told about the short copy.
 *
 * @param dev   the ramdisk slot; its iobase is the image start, its size the image length in sectors
 * @param secno first sector to read, counted from the start of the image
 * @param dst   destination buffer, at least nsecs * SECTSIZE bytes
 * @param nsecs number of sectors to read
 * @return always 0
 * @note The "nsecs < 0" test can never be true because nsecs is size_t. If secno is past the end
 *       of the image, dev->size - secno wraps around and the copy is not limited at all.
 *       disk0_io() in dev_disk0.c never asks for such a sector, so this is latent.
 */
static int ramdisk_read(struct ide_device *dev, size_t secno, void *dst, size_t nsecs) {
    nsecs = MIN(nsecs, dev->size - secno);
    if (nsecs < 0)
        return -1;
    memcpy(dst, (void *)(dev->iobase + secno * SECTSIZE), nsecs * SECTSIZE);
    return 0;
}

/**
 * @brief write_secs hook of a ramdisk: copy sectors into the in-memory image.
 *
 * Same clipping and same caveats as ramdisk_read(): writes past the end of the image are silently
 * shortened, and the function returns 0. The image on the host is not changed.
 *
 * @param dev   the ramdisk slot
 * @param secno first sector to write, counted from the start of the image
 * @param src   source buffer, at least nsecs * SECTSIZE bytes
 * @param nsecs number of sectors to write
 * @return always 0
 * @note Same unsigned-comparison and wrap-around issue as ramdisk_read().
 */
static int ramdisk_write(struct ide_device *dev, size_t secno, const void *src, size_t nsecs) {
    nsecs = MIN(nsecs, dev->size - secno);
    if (nsecs < 0)
        return -1;
    memcpy((void *)(dev->iobase + secno * SECTSIZE), src, nsecs * SECTSIZE);
    return 0;
}

/**
 * @brief Describe one ide_devices[] slot as a ramdisk backed by a linked image.
 *
 * The slot is zeroed. For SWAP_DEV_NO the image is the swap image; for DISK0_DEV_NO it is the SFS
 * image. If the image is not empty the slot gets valid = 1, size = image length / SECTSIZE (a
 * partial last sector is dropped), iobase = image start, model "KERN_INITRD", and the two hooks.
 * The sets field is set to ~0, which is meaningless for memory. dev_data stays 0.
 *
 * @param devno SWAP_DEV_NO or DISK0_DEV_NO; any other value panics ("Device Not Found")
 * @param dev   the ide_devices[] slot to fill in
 * @note If the image is empty the slot stays invalid. ide_init() then fails its assert.
 */
void ramdisk_init(int devno, struct ide_device *dev) {
    memset(dev, 0, sizeof(struct ide_device));
    char *_initrd_begin;
    char *_initrd_end;
    if (devno == SWAP_DEV_NO) {
        _initrd_begin = _binary_bin_swap_img_start;
        _initrd_end = _binary_bin_swap_img_end;
    } else if (devno == DISK0_DEV_NO) {
        _initrd_begin = _binary_bin_sfs_img_start;
        _initrd_end = _binary_bin_sfs_img_end;
    } else {
        panic("Device Not Found");
    }

    if (check_initrd(_initrd_begin, _initrd_end)) {
        dev->valid = 1;
        dev->sets = ~0;
        dev->size = (unsigned int)(_initrd_end - _initrd_begin) / SECTSIZE;
        dev->iobase = (uintptr_t)_initrd_begin;
        strcpy(dev->model, "KERN_INITRD");
        dev->read_secs = ramdisk_read;
        dev->write_secs = ramdisk_write;
    }
}
