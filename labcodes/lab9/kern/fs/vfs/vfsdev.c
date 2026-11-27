/**
 * @file   vfsdev.c
 * @brief  The VFS device list: the named devices and file systems that vfs_get_root() and
 *         vfs_lookup() resolve "name:" against, and the mount/unmount operations on them.
 *
 * Every device or file system the kernel exposes to the VFS is one vfs_dev_t entry in vdev_list.
 * Device files are added by the device drivers (dev_disk0, dev_stdin, dev_stdout) with
 * vfs_add_dev(); built-in file systems are added with vfs_add_fs(); a mountable device (the disk)
 * gets its file system with vfs_mount() when the kernel starts, and loses it with vfs_unmount() or
 * vfs_unmount_all().
 * All entries are protected by vdev_list_sem.
 */
#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <vfs.h>
#include <dev.h>
#include <inode.h>
#include <sem.h>
#include <list.h>
#include <kmalloc.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>
#include <proc.h>
/**
 * @brief One entry of the VFS device list: a named device, or a named file system.
 *
 * An entry is either a device file (fs == NULL, devnode set) or a file system (fs set). A device
 * entry is mountable if a file system may be mounted on it later.
 * @var vfs_dev_t::devname     name without the colon, a private copy made with strdup()
 * @var vfs_dev_t::devnode     inode of the device file, NULL for a pure file system
 * @var vfs_dev_t::fs          file system mounted on or attached to this entry; NULL if none
 * @var vfs_dev_t::mountable   non-zero if a file system can be mounted on this device
 * @var vfs_dev_t::vdev_link   link into vdev_list
 */
typedef struct {
    const char *devname;
    struct inode *devnode;
    struct fs *fs;
    bool mountable;
    list_entry_t vdev_link;
} vfs_dev_t;

/**
 * @brief Get the vfs_dev_t that contains the list node @p le.
 * @param le     pointer to a list_entry_t
 * @param member name of the list_entry_t member inside vfs_dev_t (vdev_link)
 */
#define le2vdev(le, member) to_struct((le), vfs_dev_t, member)

/** @brief Head of the device list; all entries are linked through vfs_dev_t::vdev_link. */
static list_entry_t vdev_list;  // device info list in vfs layer
/** @brief Semaphore that protects vdev_list and every vfs_dev_t entry in it. */
static semaphore_t vdev_list_sem;

/**
 * @brief Take vdev_list_sem.
 *
 * down() may put the caller to sleep, so only process context may call it.
 */
static void lock_vdev_list(void) {
    down(&vdev_list_sem);
}

/**
 * @brief Release vdev_list_sem taken by lock_vdev_list().
 */
static void unlock_vdev_list(void) {
    up(&vdev_list_sem);
}

/**
 * @brief Initialise the empty device list and its semaphore. Called from vfs_init().
 */
void vfs_devlist_init(void) {
    list_init(&vdev_list);
    sem_init(&vdev_list_sem, 1);
}

/**
 * @brief Call fsop_cleanup() on every file system in the device list.
 *
 * Runs under vdev_list_sem. Entries without a file system are skipped. Called from fs_cleanup().
 */
void vfs_cleanup(void) {
    if (!list_empty(&vdev_list)) {
        lock_vdev_list();
        {
            list_entry_t *list = &vdev_list, *le = list;
            while ((le = list_next(le)) != list) {
                vfs_dev_t *vdev = le2vdev(le, vdev_link);
                if (vdev->fs != NULL) {
                    fsop_cleanup(vdev->fs);
                }
            }
        }
        unlock_vdev_list();
    }
}

/**
 * @brief Get the root inode of the device or file system called @p devname.
 *
 * Searches the list under vdev_list_sem. If the entry has a file system, its root is returned
 * through fsop_get_root(). Otherwise, if the entry is a non-mountable device, the device inode is
 * returned and its reference count is incremented. A mountable device with no file system is
 * reported as not available.
 * @param devname     device name without the colon, for example "stdin" or "disk0"; must not
 *                    be NULL
 * @param[out] node_store receives the inode, with a reference the caller must release with
 *                        vop_ref_dec()
 * @return 0 on success; -E_NO_DEV if no entry has this name (or the list is empty); -E_NA_DEV if
 *         the entry is mountable but not mounted, or if the file system returned no root
 */
int vfs_get_root(const char *devname, struct inode **node_store) {
    assert(devname != NULL);
    int ret = -E_NO_DEV;
    if (!list_empty(&vdev_list)) {
        lock_vdev_list();
        {
            list_entry_t *list = &vdev_list, *le = list;
            while ((le = list_next(le)) != list) {
                vfs_dev_t *vdev = le2vdev(le, vdev_link);
                if (strcmp(devname, vdev->devname) == 0) {
                    struct inode *found = NULL;
                    if (vdev->fs != NULL) {
                        found = fsop_get_root(vdev->fs);
                    } else if (!vdev->mountable) {
                        vop_ref_inc(vdev->devnode);
                        found = vdev->devnode;
                    }
                    if (found != NULL) {
                        ret = 0, *node_store = found;
                    } else {
                        ret = -E_NA_DEV;
                    }
                    break;
                }
            }
        }
        unlock_vdev_list();
    }
    return ret;
}

/**
 * @brief Get the name of the device entry that holds file system @p fs.
 *
 * The list is scanned without taking vdev_list_sem, so the result is only valid while no
 * concurrent change of the list is possible (the current code relies on this).
 * @param fs file system; must not be NULL
 * @return the name of the entry (its private copy, valid as long as the entry exists), or NULL if
 *         @p fs is not in the list
 */
const char *vfs_get_devname(struct fs *fs) {
    assert(fs != NULL);
    list_entry_t *list = &vdev_list, *le = list;
    while ((le = list_next(le)) != list) {
        vfs_dev_t *vdev = le2vdev(le, vdev_link);
        if (vdev->fs == fs) {
            return vdev->devname;
        }
    }
    return NULL;
}

/**
 * @brief Check whether a name is still free in the device list.
 *
 * Must be called with vdev_list_sem held. The return values are inverted with respect to the name:
 * 0 means a device with that name already exists, 1 means the name is free.
 * @param devname name to test
 * @retval 0 a device named @p devname is already in the list (conflict)
 * @retval 1 the name is free
 */
static bool check_devname_conflict(const char *devname) {
    list_entry_t *list = &vdev_list, *le = list;
    while ((le = list_next(le)) != list) {
        vfs_dev_t *vdev = le2vdev(le, vdev_link);
        if (strcmp(vdev->devname, devname) == 0) {
            return 0;
        }
    }
    return 1;
}


/**
 * @brief Add a new entry to the device list (common code of vfs_add_fs() and vfs_add_dev()).
 *
 * The name is copied with strdup(). The name is checked for length and for a clash with an
 * existing entry, and the entry is added at the head of the list under vdev_list_sem. The
 * "raw device" that the original comment mentions is not created by this function.
 * @param devname   name without the colon; at most FS_MAX_DNAME_LEN characters
 * @param devnode   device inode, or NULL for a file system; if not NULL it must be a device inode
 *                  (asserted with check_inode_type())
 * @param fs        file system, or NULL for a device
 * @param mountable non-zero if a file system may be mounted on the device; must be 0 when
 *                  @p devnode is NULL (asserted)
 * @return 0 on success; -E_TOO_BIG if the name is too long; -E_NO_MEM if memory is exhausted;
 *         -E_EXISTS if the name is already in the list
 * @note The list stores @p devnode and @p fs without taking references.
 */
static int vfs_do_add(const char *devname, struct inode *devnode, struct fs *fs, bool mountable) {
    assert(devname != NULL);
    assert((devnode == NULL && !mountable) ||
           (devnode != NULL && check_inode_type(devnode, device)));
    if (strlen(devname) > FS_MAX_DNAME_LEN) {
        return -E_TOO_BIG;
    }

    int ret = -E_NO_MEM;
    char *s_devname;
    if ((s_devname = strdup(devname)) == NULL) {
        return ret;
    }

    vfs_dev_t *vdev;
    if ((vdev = kmalloc(sizeof(vfs_dev_t))) == NULL) {
        goto failed_cleanup_name;
    }

    ret = -E_EXISTS;
    lock_vdev_list();
    if (!check_devname_conflict(s_devname)) {
        unlock_vdev_list();
        goto failed_cleanup_vdev;
    }
    vdev->devname = s_devname;
    vdev->devnode = devnode;
    vdev->mountable = mountable;
    vdev->fs = fs;

    list_add(&vdev_list, &(vdev->vdev_link));
    unlock_vdev_list();
    return 0;

failed_cleanup_vdev:
    kfree(vdev);
failed_cleanup_name:
    kfree(s_devname);
    return ret;
}

/**
 * @brief Add a built-in file system to the device list under the name "devname:".
 *
 * The entry has no device inode and is not mountable.
 * @param devname name of the file system without the colon
 * @param fs      file system to add
 * @return 0 on success, or an error of vfs_do_add() (-E_TOO_BIG, -E_NO_MEM, -E_EXISTS)
 */
int vfs_add_fs(const char *devname, struct fs *fs) {
    return vfs_do_add(devname, NULL, fs, 0);
}

/**
 * @brief Add a device file to the device list.
 *
 * A non-mountable device is reached as "devname:". A mountable one can later be given a file
 * system with vfs_mount(). The device list keeps the inode pointer without a reference.
 * @param devname   name of the device without the colon, for example "stdin" or "disk0"
 * @param devnode   device inode created by the driver (its in_type must be the device type)
 * @param mountable non-zero for a device that holds a file system (for example the disk)
 * @return 0 on success, or an error of vfs_do_add() (-E_TOO_BIG, -E_NO_MEM, -E_EXISTS)
 */
int vfs_add_dev(const char *devname, struct inode *devnode, bool mountable) {
    return vfs_do_add(devname, devnode, NULL, mountable);
}

/**
 * @brief Find a mountable device by name.
 *
 * Must be called with vdev_list_sem held.
 * @param devname     device name, must not be NULL
 * @param[out] vdev_store receives the entry when found
 * @retval 0 the mountable device was found; *vdev_store is set
 * @retval -E_NO_DEV there is no mountable device with this name
 */
static int find_mount(const char *devname, vfs_dev_t **vdev_store) {
    assert(devname != NULL);
    list_entry_t *list = &vdev_list, *le = list;
    while ((le = list_next(le)) != list) {
        vfs_dev_t *vdev = le2vdev(le, vdev_link);
        if (vdev->mountable && strcmp(vdev->devname, devname) == 0) {
            *vdev_store = vdev;
            return 0;
        }
    }
    return -E_NO_DEV;
}

/**
 * @brief Mount a file system on a mountable device.
 *
 * Under vdev_list_sem, finds the device and calls @p mountfunc with the device's inode data. The
 * function must create a struct fs and store it in the pointer it is given; on success the entry
 * then refers to that file system.
 * @param devname   name of the mountable device, for example "disk0"
 * @param mountfunc function that mounts the file system on the device, for example sfs_do_mount
 * @return 0 on success; -E_NO_DEV if there is no mountable device called @p devname; -E_BUSY if the
 *         device already has a file system; otherwise the error returned by @p mountfunc
 * @note The lock is held while @p mountfunc runs, so it must not call VFS functions that take
 *       vdev_list_sem. If @p mountfunc fails after storing a pointer, that pointer is not cleared
 *       here.
 */
int vfs_mount(const char *devname, int (*mountfunc)(struct device *dev, struct fs **fs_store)) {
    int ret;
    lock_vdev_list();
    vfs_dev_t *vdev;
    if ((ret = find_mount(devname, &vdev)) != 0) {
        goto out;
    }
    if (vdev->fs != NULL) {
        ret = -E_BUSY;
        goto out;
    }
    assert(vdev->devname != NULL && vdev->mountable);

    struct device *dev = vop_info(vdev->devnode, device);
    if ((ret = mountfunc(dev, &(vdev->fs))) == 0) {
        assert(vdev->fs != NULL);
        cprintf("vfs: mount %s.\n", vdev->devname);
    }

out:
    unlock_vdev_list();
    return ret;
}

/**
 * @brief Unmount the file system on a mountable device.
 *
 * Under vdev_list_sem: fsop_sync() is called first, then fsop_unmount(). If the unmount succeeds,
 * the entry's file system pointer is cleared. If either step fails, the file system stays mounted.
 * @param devname name of the mountable device
 * @return 0 on success; -E_NO_DEV if there is no mountable device called @p devname; -E_INVAL if
 *         nothing is mounted on it; the error of fsop_sync() or fsop_unmount() otherwise
 */
int vfs_unmount(const char *devname) {
    int ret;
    lock_vdev_list();
    vfs_dev_t *vdev;
    if ((ret = find_mount(devname, &vdev)) != 0) {
        goto out;
    }
    if (vdev->fs == NULL) {
        ret = -E_INVAL;
        goto out;
    }
    assert(vdev->devname != NULL && vdev->mountable);

    if ((ret = fsop_sync(vdev->fs)) != 0) {
        goto out;
    }
    if ((ret = fsop_unmount(vdev->fs)) == 0) {
        vdev->fs = NULL;
        cprintf("vfs: unmount %s.\n", vdev->devname);
    }

out:
    unlock_vdev_list();
    return ret;
}

/**
 * @brief Sync and unmount every file system on a mountable device.
 *
 * Best effort, under vdev_list_sem. For each mounted device, fsop_sync() then fsop_unmount() is
 * tried. A failure is printed with cprintf() and the loop moves on to the next device, leaving that
 * file system mounted. No caller in this lab's code; it is provided for a shutdown path.
 * @return always 0; errors are only reported as warnings
 */
int vfs_unmount_all(void) {
    if (!list_empty(&vdev_list)) {
        lock_vdev_list();
        {
            list_entry_t *list = &vdev_list, *le = list;
            while ((le = list_next(le)) != list) {
                vfs_dev_t *vdev = le2vdev(le, vdev_link);
                if (vdev->mountable && vdev->fs != NULL) {
                    int ret;
                    if ((ret = fsop_sync(vdev->fs)) != 0) {
                        cprintf("vfs: warning: sync failed for %s: %e.\n", vdev->devname, ret);
                        continue;
                    }
                    if ((ret = fsop_unmount(vdev->fs)) != 0) {
                        cprintf("vfs: warning: unmount failed for %s: %e.\n", vdev->devname, ret);
                        continue;
                    }
                    vdev->fs = NULL;
                    cprintf("vfs: unmount %s.\n", vdev->devname);
                }
            }
        }
        unlock_vdev_list();
    }
    return 0;
}
