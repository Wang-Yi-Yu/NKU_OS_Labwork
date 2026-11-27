/**
 * @file   vfslookup.c
 * @brief  Resolution of path names: the device prefix of a path, the directory where the lookup
 *         starts, and the walk through the file system (vfs_lookup(), vfs_lookup_parent()).
 *
 * A path can have one of these forms: "device:path" (start at the root of the device's file
 * system), "/path" (start at the root of the boot file system, see vfs_set_bootfs()), ":path"
 * (start at the root of the file system of the current directory) or a plain relative "path" (start
 * at the current directory). get_device() handles the first part of every form; vfs_lookup() then
 * walks the rest with vop_lookup(), one directory level at a time inside the file system. The
 * callers are vfs_open() (vfsfile.c), vfs_chdir() (vfspath.c) and vfs_set_bootfs() (vfs.c, through
 * vfs_chdir()), and vfs_lookup_parent() is used by vfs_open() to create a file.
 */
#include <defs.h>
#include <string.h>
#include <vfs.h>
#include <inode.h>
#include <error.h>
#include <assert.h>

/**
 * @brief Split off the device prefix of a path and choose the inode where the lookup starts.
 *
 * The function scans @p path for the first ':' or '/'. A colon after a name ("disk0:/a") is cut in
 * place into a NUL, so the device name becomes a separate string; the function then returns the
 * root inode of that device. Without a device prefix, a path starting with '/' starts at the boot
 * file system and a path starting with ':' starts at the root of the current directory's file
 * system. A plain relative path starts at the current directory. Leading slashes of the remaining
 * path are skipped.
 * @param path         path to split; it is modified in place (a colon is replaced by NUL)
 * @param[out] subpath receives the part of @p path that is still to be looked up inside the file
 *                     system, which points into @p path (it may be an empty string)
 * @param[out] node_store receives the starting inode, with a reference the caller must release with
 *                        vop_ref_dec() (in every success case)
 * @return 0 on success; -E_NOENT if an absolute path is used and no boot file system is set, or
 *         the current directory is not set; -E_NO_DEV or -E_NA_DEV from vfs_get_root() for an
 *         unknown or unusable device name
 */
static int get_device(char *path, char **subpath, struct inode **node_store) {
    int i, slash = -1, colon = -1;
    for (i = 0; path[i] != '\0'; i++) {
        if (path[i] == ':') {
            colon = i;
            break;
        }
        if (path[i] == '/') {
            slash = i;
            break;
        }
    }
    if (colon < 0 && slash != 0) {
        /* *
         * No colon before a slash, so no device name specified, and the slash isn't leading
         * or is also absent, so this is a relative path or just a bare filename. Start from
         * the current directory, and use the whole thing as the subpath.
         * */
        *subpath = path;
        return vfs_get_curdir(node_store);
    }
    if (colon > 0) {
        /* device:path - get root of device's filesystem */
        path[colon] = '\0';

        /* device:/path - skip slash, treat as device:path */
        while (path[++colon] == '/')
            ;
        *subpath = path + colon;
        return vfs_get_root(path, node_store);
    }

    /* *
     * we have either /path or :path
     * /path is a path relative to the root of the "boot filesystem"
     * :path is a path relative to the root of the current filesystem
     * */
    int ret;
    if (*path == '/') {
        if ((ret = vfs_get_bootfs(node_store)) != 0) {
            return ret;
        }
    } else {
        assert(*path == ':');
        struct inode *node;
        if ((ret = vfs_get_curdir(&node)) != 0) {
            return ret;
        }
        /* The current directory may not be a device, so it must have a fs. */
        assert(node->in_fs != NULL);
        *node_store = fsop_get_root(node->in_fs);
        vop_ref_dec(node);
    }

    /* ///... or :/... */
    while (*(++path) == '/')
        ;
    *subpath = path;
    return 0;
}

/**
 * @brief Resolve a full path name to an inode. Implements vfs_lookup() (see vfs.h).
 *
 * get_device() finds the starting directory and the remaining path. If that remaining path is
 * not empty, vop_lookup() is called on the starting directory, and the starting directory's
 * reference is then dropped. An empty remainder returns the starting inode itself.
 * @param path         path name; modified in place by get_device() and vop_lookup()
 * @param[out] node_store receives the inode, with a reference the caller must release with
 *                        vop_ref_dec()
 * @return 0 on success; the error of get_device() or vop_lookup() otherwise (for example -E_NOENT
 *         for a missing name)
 */
int vfs_lookup(char *path, struct inode **node_store) {
    int ret;
    struct inode *node;
    if ((ret = get_device(path, &path, &node)) != 0) {
        return ret;
    }
    if (*path != '\0') {
        ret = vop_lookup(node, path, node_store);
        vop_ref_dec(node);
        return ret;
    }
    *node_store = node;
    return 0;
}

/**
 * @brief Resolve the starting directory of a path and return the part that is left to look up.
 *
 * This is the "parent" half of a path lookup, used when a file has to be created: the caller gets
 * the directory where the new file goes and the remaining name, and it creates the file with
 * vop_create(). The remaining part is not split further, so for a path with several levels the
 * returned name still contains the slashes ("dir/new"). Implements vfs_lookup_parent() (see vfs.h).
 * @param path         path name; modified in place by get_device()
 * @param[out] node_store receives the starting directory, with a reference the caller must release
 *                        with vop_ref_dec()
 * @param[out] endp    receives a pointer into @p path at the part that remains to be looked up
 * @return 0 on success, or the error of get_device()
 * @note Whether a multi-level remainder is meaningful for vop_create() depends on the file system;
 *       this function does not walk the intermediate directories.
 */
int vfs_lookup_parent(char *path, struct inode **node_store, char **endp) {
    int ret;
    struct inode *node;
    if ((ret = get_device(path, &path, &node)) != 0) {
        return ret;
    }
    *endp = path;
    *node_store = node;
    return 0;
}
