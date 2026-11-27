/**
 * @file   vfs.c
 * @brief  VFS initialisation, allocation of struct fs, and the boot file system (vfs_set_bootfs()
 *         and vfs_get_bootfs()).
 *
 * The boot file system is the file system that absolute paths such as "/sh" are resolved on. It is
 * stored as an inode reference (bootfs_node) and protected by bootfs_sem. The device list and the
 * mount operations are in vfsdev.c; the current directory and the name lookup are in vfspath.c and
 * vfslookup.c.
 */
#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <vfs.h>
#include <inode.h>
#include <sem.h>
#include <kmalloc.h>
#include <error.h>
#include <proc.h>
/**
 * @brief Semaphore that serialises changes and reads of bootfs_node.
 *
 * It is initialised by vfs_init().
 */
static semaphore_t bootfs_sem;
/**
 * @brief Root inode of the boot file system, holding one reference.
 *
 * NULL when no boot file system is set.
 */
static struct inode *bootfs_node = NULL;

extern void vfs_devlist_init(void);

/**
 * @brief Allocate a struct fs and set its type tag.
 *
 * Only fs_type is set; the operation pointers and fs_info are left for the file system to fill in.
 * @param type fs_type_* tag value
 * @return the new struct fs, or NULL if kmalloc() failed
 */
struct fs *__alloc_fs(int type) {
    struct fs *fs;
    if ((fs = kmalloc(sizeof(struct fs))) != NULL) {
        fs->fs_type = type;
    }
    return fs;
}

/**
 * @brief Initialise the VFS: set up the boot file system semaphore and the device list.
 *
 * Called once from fs_init(). It calls vfs_devlist_init(), which creates the empty device list.
 */
void vfs_init(void) {
    sem_init(&bootfs_sem, 1);
    vfs_devlist_init();
}

/**
 * @brief Take bootfs_sem. down() may put the caller to sleep, so only process context may call it.
 */
static void lock_bootfs(void) {
    down(&bootfs_sem);
}
/**
 * @brief Release bootfs_sem taken by lock_bootfs().
 */
static void unlock_bootfs(void) {
    up(&bootfs_sem);
}

/**
 * @brief Replace the boot file system root with @p node and drop the reference to the old one.
 *
 * The swap is done under bootfs_sem; the old root is released after unlocking with vop_ref_dec().
 * @param node new root inode, which the boot file system takes over (its reference is kept), or
 *             NULL to clear the boot file system
 */
static void change_bootfs(struct inode *node) {
    struct inode *old;
    lock_bootfs();
    { old = bootfs_node, bootfs_node = node; }
    unlock_bootfs();
    if (old != NULL) {
        vop_ref_dec(old);
    }
}

/**
 * @brief Set the boot file system, which absolute paths ("/name") are resolved against.
 *
 * @p fsname must be a device name followed by exactly one colon, for example "disk0:". The function
 * calls vfs_chdir() on it, which also changes the current directory of the calling process to the
 * root of that device, and then keeps the resulting directory (with its own reference) as the boot
 * file system. The previous boot file system is released.
 * @note Side effects: the current directory of the calling process becomes the root of @p fsname,
 *       and the string @p fsname is modified in place (the colon is overwritten by NUL during the
 *       lookup).
 * @param fsname device name ending with a colon, or NULL to clear the boot file system
 * @return 0 on success; -E_INVAL if @p fsname has no colon or characters after it; the error of
 *         vfs_chdir() or vfs_get_curdir() otherwise (the old boot file system is kept then)
 */
int vfs_set_bootfs(char *fsname) {
    struct inode *node = NULL;
    if (fsname != NULL) {
        char *s;
        if ((s = strchr(fsname, ':')) == NULL || s[1] != '\0') {
            return -E_INVAL;
        }
        int ret;
        if ((ret = vfs_chdir(fsname)) != 0) {
            return ret;
        }
        if ((ret = vfs_get_curdir(&node)) != 0) {
            return ret;
        }
    }
    change_bootfs(node);
    return 0;
}

/**
 * @brief Get the root inode of the boot file system.
 *
 * The reference count is incremented under bootfs_sem.
 * @param[out] node_store receives the root inode, with a reference that the caller must release
 *                        with vop_ref_dec()
 * @return 0 on success; -E_NOENT if no boot file system is set
 * @note bootfs_node is tested for NULL before the semaphore is taken, so the test is not
 *       synchronised; the value is read again under the lock before it is used.
 */
int vfs_get_bootfs(struct inode **node_store) {
    struct inode *node = NULL;
    if (bootfs_node != NULL) {
        lock_bootfs();
        {
            if ((node = bootfs_node) != NULL) {
                vop_ref_inc(bootfs_node);
            }
        }
        unlock_bootfs();
    }
    if (node == NULL) {
        return -E_NOENT;
    }
    *node_store = node;
    return 0;
}
