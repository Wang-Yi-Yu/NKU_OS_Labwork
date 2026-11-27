/**
 * @file   sfs_fs.c
 * @brief  Mounting and the file-system level operations of SFS (struct fs): sfs_mount(), sync,
 *         get-root, unmount and cleanup.
 *
 * sfs_mount() hands sfs_do_mount() to vfs_mount(), which opens the device and creates a struct fs.
 * sfs_do_mount() reads the superblock and the free map from disk, checks them, builds the in-memory
 * struct sfs_fs and stores the four fsop_* functions of this file in the fs. The VFS then calls
 * them through the fs: fsop_sync() (sfs_sync), fsop_get_root() (sfs_get_root), fsop_unmount()
 * (sfs_unmount) and fsop_cleanup() (sfs_cleanup). The per-file operations (vop_*) are in
 * sfs_inode.c, and the block I/O they use is in sfs_io.c.
 *
 * Mounting reads the disk directly with dop_io() and does not take the SFS locks: it runs during
 * start-up, before any other code can reach the volume.
 */
#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <kmalloc.h>
#include <list.h>
#include <fs.h>
#include <vfs.h>
#include <dev.h>
#include <sfs.h>
#include <inode.h>
#include <iobuf.h>
#include <bitmap.h>
#include <error.h>
#include <assert.h>
#include <proc.h>

/**
 * @brief Sync the file system: write the dirty inodes, then the superblock and the free map.
 *
 * First every inode in memory is synchronised with vop_fsync() (which writes its din when it is
 * dirty), with the fs semaphore held. Then, if super_dirty is set, the superblock and the free map
 * are written. On a write error super_dirty is set again, so the next sync retries.
 * @param fs mounted file system (the fs_sync operation of struct fs)
 * @return 0 on success, otherwise the error of sfs_sync_super() or sfs_sync_freemap()
 * @note The inode writes are not checked: the errors of vop_fsync() are ignored by this loop.
 */
static int sfs_sync(struct fs *fs) {
    struct sfs_fs *sfs = fsop_info(fs, sfs);
    lock_sfs_fs(sfs);
    {
        list_entry_t *list = &(sfs->inode_list), *le = list;
        while ((le = list_next(le)) != list) {
            struct sfs_inode *sin = le2sin(le, inode_link);
            vop_fsync(info2node(sin, sfs_inode));
        }
    }
    unlock_sfs_fs(sfs);

    int ret;
    if (sfs->super_dirty) {
        sfs->super_dirty = 0;
        if ((ret = sfs_sync_super(sfs)) != 0) {
            sfs->super_dirty = 1;
            return ret;
        }
        if ((ret = sfs_sync_freemap(sfs)) != 0) {
            sfs->super_dirty = 1;
            return ret;
        }
    }
    return 0;
}

/**
 * @brief Return the root directory of the volume (fs_get_root operation of struct fs).
 *
 * The root inode is always at block SFS_BLKN_ROOT. The function panics if it cannot be loaded,
 * because the file system is unusable without it.
 * @param fs mounted file system
 * @return the root inode, with one reference held by the caller (release with vop_ref_dec())
 */
static struct inode *sfs_get_root(struct fs *fs) {
    struct inode *node;
    int ret;
    if ((ret = sfs_load_inode(fsop_info(fs, sfs), &node, SFS_BLKN_ROOT)) != 0) {
        panic("load sfs root failed: %e", ret);
    }
    return node;
}

/**
 * @brief Unmount the file system and free its memory (fs_unmount operation of struct fs).
 *
 * Frees the free map, sfs_buffer, the hash table and the struct sfs_fs itself. The volume can be
 * unmounted only when no inode of it is still in memory.
 * @param fs mounted file system; it must not be used after a successful call
 * @return 0 on success, -E_BUSY if some inode of the volume is still in memory
 * @note The superblock and free map must already be synchronised: super_dirty is asserted clear.
 */
static int sfs_unmount(struct fs *fs) {
    struct sfs_fs *sfs = fsop_info(fs, sfs);
    if (!list_empty(&(sfs->inode_list))) {
        return -E_BUSY;
    }
    assert(!sfs->super_dirty);
    bitmap_destroy(sfs->freemap);
    kfree(sfs->sfs_buffer);
    kfree(sfs->hash_list);
    kfree(sfs);
    return 0;
}

/**
 * @brief Try to repair a failed file system by syncing it (fs_cleanup operation of struct fs).
 *
 * It prints the block counts, calls fsop_sync() up to 32 times until it succeeds and prints a
 * warning if it never does. vfs_cleanup() (vfsdev.c) calls it for every mounted device.
 * @param fs mounted file system
 */
static void sfs_cleanup(struct fs *fs) {
    struct sfs_fs *sfs = fsop_info(fs, sfs);
    uint32_t blocks = sfs->super.blocks, unused_blocks = sfs->super.unused_blocks;
    cprintf("sfs: cleanup: '%s' (%d/%d/%d)\n", sfs->super.info, blocks - unused_blocks,
            unused_blocks, blocks);
    int i, ret;
    for (i = 0; i < 32; i++) {
        if ((ret = fsop_sync(fs)) == 0) {
            break;
        }
    }
    if (ret != 0) {
        warn("sfs: sync error: '%s': %e.\n", sfs->super.info, ret);
    }
}

/**
 * @brief Read one disk block directly from the device, without the SFS locks (used at mount time).
 *
 * (1) initialise an iobuf for SFS_BLKSIZE bytes at blkno * SFS_BLKSIZE;
 * (2) read the device into it with dop_io().
 * @param dev        block device to read
 * @param blkno      number of the block
 * @param blk_buffer [out] buffer of SFS_BLKSIZE bytes
 * @return 0 on success, otherwise the device error code
 */
static int sfs_init_read(struct device *dev, uint32_t blkno, void *blk_buffer) {
    struct iobuf __iob, *iob = iobuf_init(&__iob, blk_buffer, SFS_BLKSIZE, blkno * SFS_BLKSIZE);
    return dop_io(dev, iob, 0);
}

/**
 * @brief Read the free map from disk into the memory of a bitmap, at mount time.
 *
 * The bitmap's own word array is the destination: the blocks are read one after the other into
 * it, so blk_buffer is not used.
 * @param dev        block device to read
 * @param freemap    [out] bitmap whose size is nblks * SFS_BLKSIZE bytes (asserted)
 * @param blkno      first block of the free map (SFS_BLKN_FREEMAP)
 * @param nblks      number of blocks of the free map
 * @param blk_buffer unused
 * @return 0 on success, otherwise the device error code of the first failing read
 */
static int sfs_init_freemap(struct device *dev, struct bitmap *freemap, uint32_t blkno,
                            uint32_t nblks, void *blk_buffer) {
    size_t len;
    void *data = bitmap_getdata(freemap, &len);
    assert(data != NULL && len == nblks * SFS_BLKSIZE);
    while (nblks != 0) {
        int ret;
        if ((ret = sfs_init_read(dev, blkno, data)) != 0) {
            return ret;
        }
        blkno++, nblks--, data += SFS_BLKSIZE;
    }
    return 0;
}

/**
 * @brief Mount an SFS volume that is on dev and build its struct fs (the vfs_mount callback).
 *
 * Steps:
 *  1. check that the block size of dev is SFS_BLKSIZE (-E_NA_DEV otherwise) and allocate the fs;
 *  2. read the superblock (block 0) and check its magic (-E_INVAL) and block count
 *     (super.blocks must not exceed dev->d_blocks, -E_INVAL);
 *  3. allocate the scratch block, the hash table and the free map, and read the free map;
 *  4. count the free blocks in the map and assert that the count equals super.unused_blocks;
 *  5. initialise the semaphores, the inode lists and the fsop_* function pointers.
 * On any failure everything allocated so far is freed (goto chain at the end of the function).
 * @param dev      block device that contains the file system
 * @param fs_store [out] receives the new struct fs on success
 * @return 0 on success; -E_NA_DEV, -E_NO_MEM, -E_INVAL or a device error on failure
 * @note Mount-time problems are reported with cprintf(); only the check of unused_blocks is an
 *       assertion, so an inconsistent free map stops the kernel instead of failing the mount.
 */
static int sfs_do_mount(struct device *dev, struct fs **fs_store) {
    static_assert(SFS_BLKSIZE >= sizeof(struct sfs_super));
    static_assert(SFS_BLKSIZE >= sizeof(struct sfs_disk_inode));
    static_assert(SFS_BLKSIZE >= sizeof(struct sfs_disk_entry));

    if (dev->d_blocksize != SFS_BLKSIZE) {
        return -E_NA_DEV;
    }

    /* allocate fs structure */
    struct fs *fs;
    if ((fs = alloc_fs(sfs)) == NULL) {
        return -E_NO_MEM;
    }
    struct sfs_fs *sfs = fsop_info(fs, sfs);
    sfs->dev = dev;

    int ret = -E_NO_MEM;

    void *sfs_buffer;
    if ((sfs->sfs_buffer = sfs_buffer = kmalloc(SFS_BLKSIZE)) == NULL) {
        goto failed_cleanup_fs;
    }

    /* load and check superblock */
    if ((ret = sfs_init_read(dev, SFS_BLKN_SUPER, sfs_buffer)) != 0) {
        goto failed_cleanup_sfs_buffer;
    }

    ret = -E_INVAL;

    struct sfs_super *super = sfs_buffer;
    if (super->magic != SFS_MAGIC) {
        cprintf("sfs: wrong magic in superblock. (%08x should be %08x).\n", super->magic,
                SFS_MAGIC);
        goto failed_cleanup_sfs_buffer;
    }
    if (super->blocks > dev->d_blocks) {
        cprintf("sfs: fs has %u blocks, device has %u blocks.\n", super->blocks, dev->d_blocks);
        goto failed_cleanup_sfs_buffer;
    }
    super->info[SFS_MAX_INFO_LEN] = '\0';
    sfs->super = *super;

    ret = -E_NO_MEM;

    uint32_t i;

    /* alloc and initialize hash list */
    list_entry_t *hash_list;
    if ((sfs->hash_list = hash_list = kmalloc(sizeof(list_entry_t) * SFS_HLIST_SIZE)) == NULL) {
        goto failed_cleanup_sfs_buffer;
    }
    for (i = 0; i < SFS_HLIST_SIZE; i++) {
        list_init(hash_list + i);
    }

    /* load and check freemap */
    struct bitmap *freemap;
    uint32_t freemap_size_nbits = sfs_freemap_bits(super);
    if ((sfs->freemap = freemap = bitmap_create(freemap_size_nbits)) == NULL) {
        goto failed_cleanup_hash_list;
    }
    uint32_t freemap_size_nblks = sfs_freemap_blocks(super);
    if ((ret = sfs_init_freemap(dev, freemap, SFS_BLKN_FREEMAP, freemap_size_nblks, sfs_buffer)) !=
        0) {
        goto failed_cleanup_freemap;
    }

    uint32_t blocks = sfs->super.blocks, unused_blocks = 0;
    for (i = 0; i < freemap_size_nbits; i++) {
        if (bitmap_test(freemap, i)) {
            unused_blocks++;
        }
    }
    assert(unused_blocks == sfs->super.unused_blocks);

    /* and other fields */
    sfs->super_dirty = 0;
    sem_init(&(sfs->fs_sem), 1);
    sem_init(&(sfs->io_sem), 1);
    sem_init(&(sfs->mutex_sem), 1);
    list_init(&(sfs->inode_list));
    cprintf("sfs: mount: '%s' (%d/%d/%d)\n", sfs->super.info, blocks - unused_blocks, unused_blocks,
            blocks);

    /* link addr of sync/get_root/unmount/cleanup funciton  fs's function pointers*/
    fs->fs_sync = sfs_sync;
    fs->fs_get_root = sfs_get_root;
    fs->fs_unmount = sfs_unmount;
    fs->fs_cleanup = sfs_cleanup;
    *fs_store = fs;
    return 0;

failed_cleanup_freemap:
    bitmap_destroy(freemap);
failed_cleanup_hash_list:
    kfree(hash_list);
failed_cleanup_sfs_buffer:
    kfree(sfs_buffer);
failed_cleanup_fs:
    kfree(fs);
    return ret;
}

/**
 * @brief Mount the SFS volume on the named device (through vfs_mount()).
 * @param devname name of the block device, for example "disk0"
 * @return 0 on success, otherwise the error of vfs_mount() or sfs_do_mount()
 */
int sfs_mount(const char *devname) {
    return vfs_mount(devname, sfs_do_mount);
}
