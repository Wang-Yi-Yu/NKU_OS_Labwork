/**
 * @file   sfs_lock.c
 * @brief  The two semaphores of an SFS volume: lock/unlock of the fs semaphore and of the io
 *         semaphore.
 *
 * sfs_fs has two locks that every SFS function uses (sfs.h describes them): fs_sem protects the
 * in-memory inode table and the loading and reclaiming of inodes, io_sem protects the device and
 * the shared scratch block sfs_buffer. These four functions are thin wrappers around down() and
 * up() (sem.h); the lock functions may sleep.
 */
#include <defs.h>
#include <sem.h>
#include <sfs.h>

/**
 * @brief Take the fs semaphore of a volume.
 *
 * Used by sfs_load_inode(), sfs_sync() and sfs_reclaim() (sfs_fs.c and sfs_inode.c).
 * @param sfs mounted volume
 */
void lock_sfs_fs(struct sfs_fs *sfs) {
    down(&(sfs->fs_sem));
}

/**
 * @brief Take the io semaphore of a volume.
 *
 * Taken by the block-level functions of sfs_io.c (sfs_rwblock(), sfs_rbuf(), sfs_wbuf(),
 * sfs_clear_block(), sfs_sync_super()). It is the innermost SFS lock.
 * @param sfs mounted volume
 */
void lock_sfs_io(struct sfs_fs *sfs) {
    down(&(sfs->io_sem));
}

/**
 * @brief Release the fs semaphore taken by lock_sfs_fs().
 * @param sfs mounted volume
 */
void unlock_sfs_fs(struct sfs_fs *sfs) {
    up(&(sfs->fs_sem));
}

/**
 * @brief Release the io semaphore taken by lock_sfs_io().
 * @param sfs mounted volume
 */
void unlock_sfs_io(struct sfs_fs *sfs) {
    up(&(sfs->io_sem));
}
