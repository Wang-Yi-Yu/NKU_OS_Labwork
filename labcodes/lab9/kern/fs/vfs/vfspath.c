/**
 * @file   vfspath.c
 * @brief  The current working directory of a process: reading and setting it, changing it by name,
 *         and printing its absolute name (vfs_get_curdir(), vfs_set_curdir(), vfs_chdir(),
 *         vfs_getcwd()).
 *
 * The current directory is stored in the file table of the process, current->filesp->pwd (see
 * struct files_struct in fs.h). The VFS holds one reference on it, which vfs_set_curdir() takes and
 * releases. Threads that share the file table (CLONE_FS) share the directory as well. The functions
 * here are called by the system calls chdir and getcwd (sysfile.c) and by vfs_set_bootfs() (vfs.c).
 */
#include <defs.h>
#include <string.h>
#include <vfs.h>
#include <inode.h>
#include <iobuf.h>
#include <stat.h>
#include <proc.h>
#include <error.h>
#include <assert.h>

/**
 * @brief Return the current directory of the current process, without taking any lock.
 * @return the current directory (no reference is taken), or NULL if there is none
 */
static struct inode *get_cwd_nolock(void) {
    return current->filesp->pwd;
}
/**
 * @brief Store @p pwd as the current directory of the current process, without taking any lock or
 *        reference.
 * @param pwd new current directory, or NULL
 */
static void set_cwd_nolock(struct inode *pwd) {
    current->filesp->pwd = pwd;
}

/**
 * @brief Take the files lock of the current process (files_struct::files_sem).
 * @note Blocking: the caller must be able to sleep.
 */
static void lock_cfs(void) {
    lock_files(current->filesp);
}
/**
 * @brief Release the files lock taken by lock_cfs().
 */
static void unlock_cfs(void) {
    unlock_files(current->filesp);
}

/**
 * @brief Get the current directory of the current process. Implements vfs_get_curdir() (see vfs.h).
 *
 * The directory is read and its reference count incremented. The read is not done under the files
 * lock, so this function does not wait for a concurrent vfs_set_curdir().
 * @param[out] dir_store receives the directory, with a reference the caller must release with
 *                       vop_ref_dec()
 * @return 0 on success; -E_NOENT if there is no current directory
 */
int vfs_get_curdir(struct inode **dir_store) {
    struct inode *node;
    if ((node = get_cwd_nolock()) != NULL) {
        vop_ref_inc(node);
        *dir_store = node;
        return 0;
    }
    return -E_NOENT;
}

/**
 * @brief Set the current directory of the current process. Implements vfs_set_curdir() (see vfs.h).
 *
 * Under the files lock: if @p dir is already the current directory, nothing changes. Otherwise
 * @p dir is checked to be a directory (vop_gettype() and S_ISDIR), a reference is taken on it, the
 * new directory is stored, and the reference on the old directory is released (which may free the
 * old inode if this was its last reference).
 * @param dir new current directory, or NULL to clear it. The caller keeps its own reference.
 * @return 0 on success; -E_NOTDIR if @p dir is not a directory; the error of vop_gettype()
 *         otherwise (the current directory is then unchanged)
 */
int vfs_set_curdir(struct inode *dir) {
    int ret = 0;
    lock_cfs();
    struct inode *old_dir;
    if ((old_dir = get_cwd_nolock()) != dir) {
        if (dir != NULL) {
            uint32_t type;
            if ((ret = vop_gettype(dir, &type)) != 0) {
                goto out;
            }
            if (!S_ISDIR(type)) {
                ret = -E_NOTDIR;
                goto out;
            }
            vop_ref_inc(dir);
        }
        set_cwd_nolock(dir);
        if (old_dir != NULL) {
            vop_ref_dec(old_dir);
        }
    }
out:
    unlock_cfs();
    return ret;
}

/**
 * @brief Change the current directory of the current process by name.
 *
 * Resolves @p path with vfs_lookup() and passes the result to vfs_set_curdir(). The reference from
 * vfs_lookup() is released afterwards, so only the reference held by the current directory remains.
 * @param path directory name; modified by vfs_lookup()
 * @return 0 on success; the error of vfs_lookup() or vfs_set_curdir() otherwise (for example
 *         -E_NOTDIR for a file)
 */
int vfs_chdir(char *path) {
    int ret;
    struct inode *node;
    if ((ret = vfs_lookup(path, &node)) == 0) {
        ret = vfs_set_curdir(node);
        vop_ref_dec(node);
    }
    return ret;
}

/**
 * @brief Write the absolute name of the current directory into an I/O buffer.
 *
 * The output is "<device>:" followed by the path of the directory relative to the root of its file
 * system, which vop_namefile() computes. The device name comes from vfs_get_devname().
 * @param iob buffer that receives the name; its offset is advanced by the bytes written
 * @return 0 on success; -E_NOENT if there is no current directory; the error of iobuf_move() or
 *         vop_namefile() otherwise
 * @note vfs_get_devname() may return NULL if the file system is not in the device list; in that
 *       case the strlen() below would be called with NULL. The code does not check this.
 */
int vfs_getcwd(struct iobuf *iob) {
    int ret;
    struct inode *node;
    if ((ret = vfs_get_curdir(&node)) != 0) {
        return ret;
    }
    assert(node->in_fs != NULL);

    const char *devname = vfs_get_devname(node->in_fs);
    if ((ret = iobuf_move(iob, (char *)devname, strlen(devname), 1, NULL)) != 0) {
        goto out;
    }
    char colon = ':';
    if ((ret = iobuf_move(iob, &colon, sizeof(colon), 1, NULL)) != 0) {
        goto out;
    }
    ret = vop_namefile(node, iob);

out:
    vop_ref_dec(node);
    return ret;
}
