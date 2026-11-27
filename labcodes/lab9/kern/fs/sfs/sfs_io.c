/**
 * @file   sfs_io.c
 * @brief  Block-level I/O of SFS: whole-block and partial-block reads and writes on the device,
 *         and the writing of the superblock, the free map and cleared blocks.
 *
 * This is the bottom of the SFS stack. Everything above it (sfs_inode.c, sfs_fs.c) reads and
 * writes disk blocks only through these functions. Each public function takes the io semaphore
 * (lock_sfs_io(), sfs_lock.c) for the whole operation, so one block transfer and the use of the
 * shared scratch block sfs->sfs_buffer cannot be interleaved with another. The `_nolock` function
 * sfs_rwblock_nolock() does the actual device call dop_io() and expects the caller to hold the
 * lock. Block 0 (the superblock) can be accessed only with the check disabled.
 */
#include <defs.h>
#include <string.h>
#include <dev.h>
#include <sfs.h>
#include <iobuf.h>
#include <bitmap.h>
#include <assert.h>

//Basic block-level I/O routines

/**
 * @brief Read or write one block on the device. Does not take the io lock.
 *
 * Builds an iobuf for SFS_BLKSIZE bytes at byte offset blkno * SFS_BLKSIZE and passes it to
 * dop_io() of the device.
 * @param sfs   mounted volume (its device is used)
 * @param buf   buffer of SFS_BLKSIZE bytes, read into it or written from it
 * @param blkno block number
 * @param write 0 to read the block, non-zero to write it
 * @param check if non-zero, blkno must be non-zero and below super.blocks (asserted); it is
 *              cleared to allow access to the superblock at block 0
 * @return the result of dop_io(): 0 on success, otherwise the device error code
 * @note The caller must hold the io lock (lock_sfs_io()).
 */
static int sfs_rwblock_nolock(struct sfs_fs *sfs, void *buf, uint32_t blkno, bool write,
                              bool check) {
    assert((blkno != 0 || !check) && blkno < sfs->super.blocks);
    struct iobuf __iob, *iob = iobuf_init(&__iob, buf, SFS_BLKSIZE, blkno * SFS_BLKSIZE);
    return dop_io(sfs->dev, iob, write);
}

/**
 * @brief Read or write nblks consecutive blocks, with the io lock held for the whole transfer.
 *
 * Stops at the first block that fails; the blocks before it are transferred.
 * @param sfs   mounted volume
 * @param buf   buffer of nblks * SFS_BLKSIZE bytes
 * @param blkno first block number
 * @param nblks number of blocks
 * @param write false to read, true to write
 * @return 0 on success, otherwise the device error code of the first failing block
 */
static int sfs_rwblock(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks, bool write) {
    int ret = 0;
    lock_sfs_io(sfs);
    {
        while (nblks != 0) {
            if ((ret = sfs_rwblock_nolock(sfs, buf, blkno, write, 1)) != 0) {
                break;
            }
            blkno++, nblks--;
            buf += SFS_BLKSIZE;
        }
    }
    unlock_sfs_io(sfs);
    return ret;
}

/**
 * @brief Read nblks consecutive blocks from the disk into buf (block-aligned, whole blocks).
 * @param sfs   mounted volume
 * @param buf   [out] buffer of nblks * SFS_BLKSIZE bytes
 * @param blkno first block to read
 * @param nblks number of blocks to read
 * @return 0 on success, otherwise the device error code
 */
int sfs_rblock(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks) {
    return sfs_rwblock(sfs, buf, blkno, nblks, 0);
}

/**
 * @brief Write nblks consecutive blocks from buf to the disk (whole blocks).
 * @param sfs   mounted volume
 * @param buf   [in] buffer of nblks * SFS_BLKSIZE bytes
 * @param blkno first block to write
 * @param nblks number of blocks to write
 * @return 0 on success, otherwise the device error code
 */
int sfs_wblock(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks) {
    return sfs_rwblock(sfs, buf, blkno, nblks, 1);
}

/**
 * @brief Read len bytes at offset inside one block, using sfs_buffer as the staging block.
 *
 * The whole block is read into sfs->sfs_buffer and the requested bytes are copied out. The io
 * lock is held for the whole call, so sfs_buffer is not used by anybody else meanwhile.
 * @param sfs    mounted volume
 * @param buf    [out] destination of len bytes
 * @param len    number of bytes to read
 * @param blkno  block to read
 * @param offset offset inside the block; asserted to satisfy offset + len <= SFS_BLKSIZE
 * @return 0 on success (buf is filled), otherwise the device error code (buf is untouched)
 */
int sfs_rbuf(struct sfs_fs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset) {
    assert(offset >= 0 && offset < SFS_BLKSIZE && offset + len <= SFS_BLKSIZE);
    int ret;
    lock_sfs_io(sfs);
    {
        if ((ret = sfs_rwblock_nolock(sfs, sfs->sfs_buffer, blkno, 0, 1)) == 0) {
            memcpy(buf, sfs->sfs_buffer + offset, len);
        }
    }
    unlock_sfs_io(sfs);
    return ret;
}

/**
 * @brief Write len bytes at offset inside one block (read-modify-write through sfs_buffer).
 *
 * The block is read, the len bytes at offset are replaced by buf and the whole block is written
 * back, all with the io lock held. The other bytes of the block keep their old values.
 * @param sfs    mounted volume
 * @param buf    [in] source of len bytes
 * @param len    number of bytes to write
 * @param blkno  block to change
 * @param offset offset inside the block; asserted to satisfy offset + len <= SFS_BLKSIZE
 * @return 0 on success; otherwise the error of the read or of the write-back
 * @note If the write-back fails, the block on disk may be left unchanged or partly updated by
 *       the device; the caller cannot tell which.
 */
int sfs_wbuf(struct sfs_fs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset) {
    assert(offset >= 0 && offset < SFS_BLKSIZE && offset + len <= SFS_BLKSIZE);
    int ret;
    lock_sfs_io(sfs);
    {
        if ((ret = sfs_rwblock_nolock(sfs, sfs->sfs_buffer, blkno, 0, 1)) == 0) {
            memcpy(sfs->sfs_buffer + offset, buf, len);
            ret = sfs_rwblock_nolock(sfs, sfs->sfs_buffer, blkno, 1, 1);
        }
    }
    unlock_sfs_io(sfs);
    return ret;
}

/**
 * @brief Write the in-memory superblock to block SFS_BLKN_SUPER.
 *
 * The superblock struct is copied into sfs_buffer (the rest of the block is zeroed) and written
 * with check disabled, because block 0 is not accepted by sfs_rwblock_nolock() otherwise.
 * @param sfs mounted volume
 * @return 0 on success, otherwise the device error code
 */
int sfs_sync_super(struct sfs_fs *sfs) {
    int ret;
    lock_sfs_io(sfs);
    {
        memset(sfs->sfs_buffer, 0, SFS_BLKSIZE);
        memcpy(sfs->sfs_buffer, &(sfs->super), sizeof(sfs->super));
        ret = sfs_rwblock_nolock(sfs, sfs->sfs_buffer, SFS_BLKN_SUPER, 1, 0);
    }
    unlock_sfs_io(sfs);
    return ret;
}

/**
 * @brief Write the whole in-memory free map to its blocks on disk.
 *
 * The number of blocks is sfs_freemap_blocks() of the superblock. The write is done by
 * sfs_wblock(), which takes the io lock itself, so this function does not need it.
 * @param sfs mounted volume
 * @return 0 on success, otherwise the device error code
 */
int sfs_sync_freemap(struct sfs_fs *sfs) {
    uint32_t nblks = sfs_freemap_blocks(&(sfs->super));
    return sfs_wblock(sfs, bitmap_getdata(sfs->freemap, NULL), SFS_BLKN_FREEMAP, nblks);
}

/**
 * @brief Fill nblks disk blocks with zeros, starting at blkno.
 *
 * Used when a block is allocated, so that a new inode, indirect block or directory entry never
 * starts with stale data. The io lock is held for the whole call.
 * @param sfs   mounted volume
 * @param blkno first block to clear
 * @param nblks number of blocks to clear
 * @return 0 on success, otherwise the device error code of the first failing block
 * @note If nblks is 0 the loop does not run and the function returns the uninitialised ret. All
 *       callers in this tree pass nblks = 1.
 */
int sfs_clear_block(struct sfs_fs *sfs, uint32_t blkno, uint32_t nblks) {
    int ret;
    lock_sfs_io(sfs);
    {
        memset(sfs->sfs_buffer, 0, SFS_BLKSIZE);
        while (nblks != 0) {
            if ((ret = sfs_rwblock_nolock(sfs, sfs->sfs_buffer, blkno, 1, 1)) != 0) {
                break;
            }
            blkno++, nblks--;
        }
    }
    unlock_sfs_io(sfs);
    return ret;
}
