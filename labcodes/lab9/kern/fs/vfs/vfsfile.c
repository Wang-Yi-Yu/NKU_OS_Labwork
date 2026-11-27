/**
 * @file   vfsfile.c
 * @brief  Opening and closing files by name at the VFS level (vfs_open(), vfs_close()), and the
 *         directory and link operations that are not implemented in this lab.
 *
 * vfs_open() is the VFS part of the open() system call: file.c (file_open()) calls it, and it
 * returns an inode that file.c stores in the file table of the process. It resolves the name with
 * vfs_lookup(), creates the file with vop_create() when O_CREAT is set, and then calls vop_open()
 * and vop_open_inc(). vfs_close() undoes this. The remaining functions in this file are stubs that
 * return -E_UNIMP.
 */
#include <defs.h>
#include <string.h>
#include <vfs.h>
#include <inode.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>

/**
 * @brief Open or create the file named @p path. Implements vfs_open() (see vfs.h).
 *
 * Steps:
 *   1. Check the access mode (O_RDONLY, O_WRONLY or O_RDWR); O_TRUNC needs write access.
 *   2. Resolve @p path with vfs_lookup(). If it is not found and O_CREAT is set, resolve the parent
 *      directory with vfs_lookup_parent() and create the file with vop_create(). If O_CREAT and
 *      O_EXCL are both set and the file exists, fail with -E_EXISTS.
 *   3. Call vop_open(node, open_flags) so that the file system can reject the mode.
 *   4. Increment the open count with vop_open_inc().
 *   5. With O_TRUNC or O_CREAT, truncate the file to length 0.
 * On success the caller owns one reference and one open count on the inode, which vfs_close()
 * releases.
 * @param path         file name; it may be modified (vfs_lookup() cuts a device prefix off it)
 * @param open_flags   O_RDONLY / O_WRONLY / O_RDWR, optionally combined with O_CREAT, O_EXCL,
 *                     O_TRUNC (see libs/unistd.h)
 * @param[out] node_store receives the inode of the file on success
 * @return 0 on success; -E_INVAL for an invalid access mode or O_TRUNC without write access;
 *         -E_EXISTS for O_CREAT | O_EXCL on an existing file; -E_NOENT (written as the literal -16
 *         in the code) when the file does not exist and O_CREAT is not set; otherwise the error of
 *         vfs_lookup_parent(), vop_create(), vop_open() or vop_truncate()
 * @note The file is created by vop_create(), but no inode operation table of this lab (SFS or
 *       device) implements vop_create, so O_CREAT on a missing file reaches the assertion in
 *       __vop_op(). This is a gap of the lab code, not something this comment fixes.
 * @note Reference leaks, reported and not fixed: the -E_EXISTS path (O_CREAT | O_EXCL on an
 *       existing file) returns without vop_ref_dec(node), and the parent directory obtained from
 *       vfs_lookup_parent() is never released with vop_ref_dec() after vop_create(), so each
 *       successful create keeps one reference on that directory.
 */
int vfs_open(char *path, uint32_t open_flags, struct inode **node_store) {
    bool can_write = 0;
    switch (open_flags & O_ACCMODE) {
        case O_RDONLY:
            break;
        case O_WRONLY:
        case O_RDWR:
            can_write = 1;
            break;
        default:
            return -E_INVAL;
    }

    if (open_flags & O_TRUNC) {
        if (!can_write) {
            return -E_INVAL;
        }
    }

    int ret;
    struct inode *node;
    bool excl = (open_flags & O_EXCL) != 0;
    bool create = (open_flags & O_CREAT) != 0;
    ret = vfs_lookup(path, &node);

    if (ret != 0) {
        if (ret == -16 && (create)) {
            char *name;
            struct inode *dir;
            if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0) {
                return ret;
            }
            ret = vop_create(dir, name, excl, &node);
        } else
            return ret;
    } else if (excl && create) {
        return -E_EXISTS;
    }
    assert(node != NULL);

    if ((ret = vop_open(node, open_flags)) != 0) {
        vop_ref_dec(node);
        return ret;
    }

    vop_open_inc(node);
    if (open_flags & O_TRUNC || create) {
        if ((ret = vop_truncate(node, 0)) != 0) {
            vop_open_dec(node);
            vop_ref_dec(node);
            return ret;
        }
    }
    *node_store = node;
    return 0;
}

/**
 * @brief Close a file opened with vfs_open(). Implements vfs_close() (see vfs.h).
 *
 * Decrements the open count (which calls vop_close() when it reaches zero) and then drops the
 * reference that vfs_open() took. The inode may be freed by this call, so the caller must not
 * use it afterwards.
 * @param node inode returned by vfs_open()
 * @return always 0; errors of vop_close() are only reported by inode_open_dec()
 */
int vfs_close(struct inode *node) {
    vop_open_dec(node);
    vop_ref_dec(node);
    return 0;
}

/**
 * @brief Delete a file or directory. Not implemented in this lab.
 * @param path path to delete (unused)
 * @return -E_UNIMP
 */
int vfs_unlink(char *path) {
    return -E_UNIMP;
}

/**
 * @brief Rename a file. Not implemented in this lab.
 * @param old_path existing name (unused)
 * @param new_path new name (unused)
 * @return -E_UNIMP
 */
int vfs_rename(char *old_path, char *new_path) {
    return -E_UNIMP;
}

/**
 * @brief Create a hard link. Not implemented in this lab.
 * @param old_path existing file (unused)
 * @param new_path name of the new link (unused)
 * @return -E_UNIMP
 */
int vfs_link(char *old_path, char *new_path) {
    return -E_UNIMP;
}

/**
 * @brief Create a symbolic link. Not implemented in this lab.
 * @param old_path contents of the link (unused)
 * @param new_path path of the link (unused)
 * @return -E_UNIMP
 */
int vfs_symlink(char *old_path, char *new_path) {
    return -E_UNIMP;
}

/**
 * @brief Read the contents of a symbolic link. Not implemented in this lab.
 * @param path path of the link (unused)
 * @param iob  output buffer (unused)
 * @return -E_UNIMP
 */
int vfs_readlink(char *path, struct iobuf *iob) {
    return -E_UNIMP;
}

/**
 * @brief Create a directory. Not implemented in this lab.
 * @param path path of the new directory (unused)
 * @return -E_UNIMP
 */
int vfs_mkdir(char *path) {
    return -E_UNIMP;
}
