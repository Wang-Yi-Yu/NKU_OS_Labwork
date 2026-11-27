/**
 * @file   dev_disk0.c
 * @brief  The "disk0" device: a block device of 4 KB blocks on the SFS image (ramdisk.c).
 *
 * disk0 is the device the SFS file system is mounted on (sfs_mount() asks vfs_mount() for "disk0").
 * Its sectors come from ide.c, and in lab8 that means the ramdisk holding sfs.img. This file turns
 * the byte-oriented iobuf of the VFS into whole-block transfers: an I/O must start and end on block
 * boundaries (DISK0_BLKSIZE = PGSIZE = 4096 bytes, 8 sectors). Large transfers go through one
 * shared buffer of DISK0_BUFSIZE bytes (4 blocks), which is protected by disk0_sem.
 *
 * Call chain for a read of the SFS: sfs_io() -> sfs_rbuf()/sfs_rblock() -> sfs_rwblock_nolock()
 * (sfs_io.c), which calls dop_io() on disk0 -> disk0_io() here -> ide_read_secs() -> ramdisk_read().
 *
 * disk0 is registered as mountable (vfs_add_dev(..., 1)), so the file system can be mounted on
 * the name "disk0".
 */
#include <defs.h>
#include <mmu.h>
#include <sem.h>
#include <ide.h>
#include <inode.h>
#include <kmalloc.h>
#include <dev.h>
#include <vfs.h>
#include <iobuf.h>
#include <error.h>
#include <assert.h>

/** @brief Block size of disk0 in bytes: one page, 4096. */
#define DISK0_BLKSIZE PGSIZE
/** @brief Size of the shared transfer buffer in bytes: 4 blocks (16 KB). */
#define DISK0_BUFSIZE (4 * DISK0_BLKSIZE)
/** @brief Sectors per block: 4096 / SECTSIZE = 8. */
#define DISK0_BLK_NSECT (DISK0_BLKSIZE / SECTSIZE)

/** @brief Transfer buffer shared by all disk0 I/O; allocated in disk0_device_init(). */
static char *disk0_buffer;
/** @brief Mutex (initial value 1) that serialises the use of disk0_buffer and the disk. */
static semaphore_t disk0_sem;

/**
 * @brief Take disk0_sem. May sleep, so it must not be called with interrupts disabled.
 */
static void lock_disk0(void) {
    down(&(disk0_sem));
}

/**
 * @brief Release disk0_sem taken by lock_disk0().
 */
static void unlock_disk0(void) {
    up(&(disk0_sem));
}

/**
 * @brief d_open hook of disk0: accepts every open. Nothing is kept per open.
 *
 * @param dev        the disk0 device (unused)
 * @param open_flags open flags (unused)
 * @return always 0
 */
static int disk0_open(struct device *dev, uint32_t open_flags) {
    return 0;
}

/**
 * @brief d_close hook of disk0: nothing to release.
 *
 * @param dev the disk0 device (unused)
 * @return always 0
 */
static int disk0_close(struct device *dev) {
    return 0;
}

/**
 * @brief Read nblks blocks from the disk into disk0_buffer, without taking disk0_sem.
 *
 * The caller must hold disk0_sem. Block blkno becomes sector blkno * DISK0_BLK_NSECT. A failed
 * read panics, so there is no error return.
 *
 * @param blkno first block to read
 * @param nblks number of blocks; at most DISK0_BUFSIZE / DISK0_BLKSIZE
 */
static void disk0_read_blks_nolock(uint32_t blkno, uint32_t nblks) {
    int ret;
    uint32_t sectno = blkno * DISK0_BLK_NSECT, nsecs = nblks * DISK0_BLK_NSECT;
    if ((ret = ide_read_secs(DISK0_DEV_NO, sectno, disk0_buffer, nsecs)) != 0) {
        panic("disk0: read blkno = %d (sectno = %d), nblks = %d (nsecs = %d): 0x%08x.\n", blkno,
              sectno, nblks, nsecs, ret);
    }
}

/**
 * @brief Write nblks blocks from disk0_buffer to the disk, without taking disk0_sem.
 *
 * The caller must hold disk0_sem. A failed write panics, so there is no error return.
 *
 * @param blkno first block to write
 * @param nblks number of blocks; at most DISK0_BUFSIZE / DISK0_BLKSIZE
 */
static void disk0_write_blks_nolock(uint32_t blkno, uint32_t nblks) {
    int ret;
    uint32_t sectno = blkno * DISK0_BLK_NSECT, nsecs = nblks * DISK0_BLK_NSECT;
    if ((ret = ide_write_secs(DISK0_DEV_NO, sectno, disk0_buffer, nsecs)) != 0) {
        panic("disk0: write blkno = %d (sectno = %d), nblks = %d (nsecs = %d): 0x%08x.\n", blkno,
              sectno, nblks, nsecs, ret);
    }
}

/**
 * @brief d_io hook of disk0: transfer whole blocks between the iobuf and the disk.
 *
 * The transfer is described by iob: it starts at iob->io_offset and covers iob->io_resid bytes.
 * Both must be multiples of DISK0_BLKSIZE, and the range must end within the device. Otherwise the
 * request is refused and nothing is done. A zero-length transfer succeeds at once.
 *
 * Otherwise disk0_sem is taken and the transfer runs in chunks of at most DISK0_BUFSIZE bytes.
 * A write first moves the data from iob into disk0_buffer, then writes it out. A read reads the
 * blocks into disk0_buffer, then moves them into iob. iobuf_move() advances io_offset and
 * io_resid in iob, so iob reflects the bytes done when the function returns. The disk I/O itself
 * cannot fail here: ide_read_secs()/ide_write_secs() errors make the kernel panic.
 *
 * @param dev   the disk0 device (unused; the global state is used)
 * @param iob   the transfer; io_offset and io_resid must be block aligned, see above
 * @param write true to write iob to the disk, false to read from the disk into iob
 * @return 0 on success, -E_INVAL if the request is not block aligned or goes past the end of disk0
 * @note May sleep on disk0_sem, so it must be called in process context.
 */
static int disk0_io(struct device *dev, struct iobuf *iob, bool write) {
    off_t offset = iob->io_offset;
    size_t resid = iob->io_resid;
    uint32_t blkno = offset / DISK0_BLKSIZE;
    uint32_t nblks = resid / DISK0_BLKSIZE;

    /* don't allow I/O that isn't block-aligned */
    if ((offset % DISK0_BLKSIZE) != 0 || (resid % DISK0_BLKSIZE) != 0) {
        return -E_INVAL;
    }

    /* don't allow I/O past the end of disk0 */
    if (blkno + nblks > dev->d_blocks) {
        return -E_INVAL;
    }

    /* read/write nothing ? */
    if (nblks == 0) {
        return 0;
    }

    lock_disk0();
    while (resid != 0) {
        size_t copied, alen = DISK0_BUFSIZE;
        if (write) {
            iobuf_move(iob, disk0_buffer, alen, 0, &copied);
            assert(copied != 0 && copied <= resid && copied % DISK0_BLKSIZE == 0);
            nblks = copied / DISK0_BLKSIZE;
            disk0_write_blks_nolock(blkno, nblks);
        } else {
            if (alen > resid) {
                alen = resid;
            }
            nblks = alen / DISK0_BLKSIZE;
            disk0_read_blks_nolock(blkno, nblks);
            iobuf_move(iob, disk0_buffer, alen, 1, &copied);
            assert(copied == alen && copied % DISK0_BLKSIZE == 0);
        }
        resid -= copied, blkno += nblks;
    }
    unlock_disk0();
    return 0;
}

/**
 * @brief d_ioctl hook of disk0: no control operations are implemented.
 *
 * @param dev  the disk0 device (unused)
 * @param op   operation code (unused)
 * @param data argument (unused)
 * @return always -E_UNIMP
 */
static int disk0_ioctl(struct device *dev, int op, void *data) {
    return -E_UNIMP;
}

/**
 * @brief Fill in the struct device of disk0 and set up its lock and transfer buffer.
 *
 * Sets d_blocks to the number of whole 4 KB blocks on the IDE device (sectors / 8), d_blocksize to
 * DISK0_BLKSIZE, installs the hooks, initialises disk0_sem with value 1, and allocates the
 * DISK0_BUFSIZE transfer buffer. Panics if the device is not valid or the buffer cannot be allocated.
 *
 * @param dev the device inside the freshly created inode
 */
static void disk0_device_init(struct device *dev) {
    static_assert(DISK0_BLKSIZE % SECTSIZE == 0);
    if (!ide_device_valid(DISK0_DEV_NO)) {
        panic("disk0 device isn't available.\n");
    }
    dev->d_blocks = ide_device_size(DISK0_DEV_NO) / DISK0_BLK_NSECT;
    dev->d_blocksize = DISK0_BLKSIZE;
    dev->d_open = disk0_open;
    dev->d_close = disk0_close;
    dev->d_io = disk0_io;
    dev->d_ioctl = disk0_ioctl;
    sem_init(&(disk0_sem), 1);

    static_assert(DISK0_BUFSIZE % DISK0_BLKSIZE == 0);
    if ((disk0_buffer = kmalloc(DISK0_BUFSIZE)) == NULL) {
        panic("disk0 alloc buffer failed.\n");
    }
}

/**
 * @brief Create the disk0 device inode and register it with the VFS as a mountable device.
 *
 * Called by dev_init(). Panics if the inode cannot be allocated or vfs_add_dev() fails. The SFS
 * file system is mounted on this device later, in sfs_init().
 */
void dev_init_disk0(void) {
    struct inode *node;
    if ((node = dev_create_inode()) == NULL) {
        panic("disk0: dev_create_node.\n");
    }
    disk0_device_init(vop_info(node, device));

    int ret;
    if ((ret = vfs_add_dev("disk0", node, 1)) != 0) {
        panic("disk0: vfs_add_dev: %e.\n", ret);
    }
}
