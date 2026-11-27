/**
 * @file   sfs.c
 * @brief  Start-up entry of the Simple File System: sfs_init(), which mounts disk0 as SFS.
 *
 * It is the only function of this file. fs_init() calls it once during kernel start-up, after the
 * block devices have been registered, so that the root file system is ready before the first
 * process is created. The mount itself is done by sfs_mount() in sfs_fs.c.
 */
#include <defs.h>
#include <sfs.h>
#include <error.h>
#include <assert.h>

/**
 * @brief Mount the SFS volume on disk0. Called by fs_init() at kernel start-up.
 *
 * Call graph: kern_init -> fs_init -> sfs_init -> sfs_mount -> vfs_mount -> sfs_do_mount.
 * @note A failed mount is fatal: the function panics with the error code.
 */
void sfs_init(void) {
    int ret;
    if ((ret = sfs_mount("disk0")) != 0) {
        panic("failed: sfs: sfs_mount: %e.\n", ret);
    }
}
