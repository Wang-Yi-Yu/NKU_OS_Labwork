/**
 * @file   vfs.h
 * @brief  Virtual File System layer: the abstract file system (struct fs, fsop_* macros) and the
 *         VFS API that turns device:path names into inodes and operates on them.
 *
 * The VFS is the layer between the file system calls (sysfile_*, file_*) and the concrete file
 * systems (SFS) and device files. A struct fs describes one mounted or built-in file system and is
 * reached through the fsop_* macros; a struct inode (inode.h) describes one file and is reached
 * through
 * the vop_* macros. The functions declared here resolve names: vfs_lookup() maps "dev:/a/b" (or a
 * path relative to the current directory or the boot file system) to an inode, and vfs_open() /
 * vfs_close() maintain the open count of that inode for the file table of the process.
 *
 * The VFS keeps a list of named devices (vdev_list, see vfsdev.c). Each entry is either a device
 * file (added with vfs_add_dev()) or a file system (added with vfs_add_fs() or vfs_mount()). The
 * boot file system (vfs_set_bootfs()) is the target of absolute paths such as "/sh".
 *
 * Reference counts of the inodes returned by the functions below are documented on each function.
 * Path arguments of the name-resolving functions may be destroyed (modified in place), as with the
 * BSD namei() that this layer is modelled on.
 */
#ifndef __KERN_FS_VFS_VFS_H__
#define __KERN_FS_VFS_VFS_H__

#include <defs.h>
#include <fs.h>
#include <sfs.h>

struct inode;   // abstract structure for an on-disk file (inode.h)
struct device;  // abstract structure for a device (dev.h)
struct iobuf;   // kernel or userspace I/O buffer (iobuf.h)

/**
 * @brief Abstract file system: the file-system-independent handle that the VFS keeps for one file
 *        system instance (for example one SFS volume).
 *
 * The file-system-specific state is stored in fs_info; the operations are the function pointers
 * below, called through the fsop_* macros. A struct fs is allocated with alloc_fs() and is owned
 * by the VFS device list (vdev_list) once it has been added with vfs_add_fs() or vfs_mount().
 *
 * @var fs::fs_info   union with the file-system-specific data (struct sfs_fs); select it with
 *                    fsop_info()
 * @var fs::fs_type   tag of fs_info (fs_type_sfs_info)
 * @var fs::fs_sync   flush all dirty buffers and metadata of the file system to disk
 * @var fs::fs_get_root  return the root inode of the file system, with its reference count
 *                    incremented; it must never return NULL
 * @var fs::fs_unmount   try to unmount the file system. On error the file system stays mounted and
 *                    the struct fs stays valid; on success all its storage is released
 * @var fs::fs_cleanup  clean up the file system at shutdown (called by vfs_cleanup())
 * @note The original text of fs_cleanup ends with "???", that is, the authors were not sure of its
 *       exact role. In this code it is called by vfs_cleanup() for every device with a file
 *       system; the SFS implementation prints the block usage of the volume.
 */
struct fs {
    union {
        struct sfs_fs __sfs_info;
    } fs_info;  // filesystem-specific data
    enum {
        fs_type_sfs_info,
    } fs_type;                                    // filesystem type
    int (*fs_sync)(struct fs *fs);                // Flush all dirty buffers to disk
    struct inode *(*fs_get_root)(struct fs *fs);  // Return root inode of filesystem.
    int (*fs_unmount)(struct fs *fs);             // Attempt unmount of filesystem.
    void (*fs_cleanup)(struct fs *fs);            // Cleanup of filesystem.???
};

/**
 * @brief Build the fs_type_<type>_info tag name.
 *
 * For example __fs_type(sfs) is fs_type_sfs_info.
 */
#define __fs_type(type) fs_type_##type##_info

/**
 * @brief Test whether @p fs has the file-system-specific data named by @p type.
 * @param fs   file system (struct fs *)
 * @param type type name without the _info suffix (sfs)
 * @return non-zero if fs->fs_type matches
 */
#define check_fs_type(fs, type) ((fs)->fs_type == __fs_type(type))

/**
 * @brief Return a pointer to the file-system-specific data of a struct fs, after checking the tag.
 *
 * Asserts that @p _fs is not NULL and that its fs_type matches @p type, then yields
 * &_fs->fs_info.__<type>_info.
 * @param _fs  file system (struct fs *), evaluated once
 * @param type type name without the _info suffix (sfs)
 */
#define __fsop_info(_fs, type)                             \
    ({                                                     \
        struct fs *__fs = (_fs);                           \
        assert(__fs != NULL && check_fs_type(__fs, type)); \
        &(__fs->fs_info.__##type##_info);                  \
    })

/**
 * @brief Public form of __fsop_info(); see there.
 * @param fs   file system (struct fs *)
 * @param type type name without the _info suffix (sfs)
 */
#define fsop_info(fs, type) __fsop_info(fs, type)

/**
 * @brief Recover the struct fs that contains a file-system-specific info structure.
 * @param info pointer to a struct sfs_fs embedded in a struct fs
 * @param type type name without the _info suffix (sfs)
 */
#define info2fs(info, type) to_struct((info), struct fs, fs_info.__##type##_info)

/**
 * @brief Allocate a struct fs with its fs_type set; the other fields are not initialised.
 * @param type fs_type_* tag value
 * @return the new struct fs, or NULL if kmalloc() failed
 */
struct fs *__alloc_fs(int type);

/**
 * @brief Allocate a struct fs of the given type name (sfs); see __alloc_fs().
 * @param type type name without the _info suffix
 */
#define alloc_fs(type) __alloc_fs(__fs_type(type))

// Macros to shorten the calling sequences.
/**
 * @brief Call fs::fs_sync: flush all dirty data of the file system to disk.
 * @param fs file system
 * @return 0 on success, a negative error code otherwise
 */
#define fsop_sync(fs) ((fs)->fs_sync(fs))
/**
 * @brief Call fs::fs_get_root: get the root inode of the file system.
 * @param fs file system
 * @return the root inode with its reference count incremented; never NULL
 */
#define fsop_get_root(fs) ((fs)->fs_get_root(fs))
/**
 * @brief Call fs::fs_unmount: try to unmount the file system.
 * @param fs file system
 * @return 0 on success; -E_BUSY if inodes of the file system are still in use. On error the
 *         file system remains mounted.
 */
#define fsop_unmount(fs) ((fs)->fs_unmount(fs))
/**
 * @brief Call fs::fs_cleanup: release the file system at shutdown.
 * @param fs file system
 */
#define fsop_cleanup(fs) ((fs)->fs_cleanup(fs))

/**
 * @brief Initialise the VFS: create the semaphore that protects the boot file system and call
 *        vfs_devlist_init(). Called once from fs_init().
 */
void vfs_init(void);
/**
 * @brief Shut down the VFS: call fsop_cleanup() on every file system in the device list.
 *        Called from fs_cleanup().
 */
void vfs_cleanup(void);
/**
 * @brief Initialise the empty device list vdev_list and its semaphore. Called from vfs_init().
 */
void vfs_devlist_init(void);

/**
 * @brief Set the current directory of the current process to @p dir.
 *
 * @p dir must be a directory (checked with vop_gettype()). The function takes its own reference on
 * @p dir and releases the reference on the old directory; the caller keeps its own reference.
 * Passing NULL clears the current directory. The change is done under the files lock of the
 * current process.
 * @param dir new current directory, or NULL
 * @return 0 on success; -E_NOTDIR if @p dir is not a directory; the error of vop_gettype()
 *         otherwise
 */
int vfs_set_curdir(struct inode *dir);
/**
 * @brief Get the current directory of the current process.
 * @param[out] dir_store receives the current directory, with its reference count incremented
 * @return 0 on success; -E_NOENT if the process has no current directory
 * @note The caller must release the reference with vop_ref_dec().
 */
int vfs_get_curdir(struct inode **dir_store);
/**
 * @brief Get the root inode of the device or file system named @p devname.
 *
 * Works for file systems (returns fsop_get_root()) and for non-mountable devices (returns the
 * device inode). A mountable device without a mounted file system cannot be used this way.
 * @param devname    name of the device, without the colon, for example "stdin"
 * @param[out] root_store receives the root inode, with its reference count incremented
 * @return 0 on success; -E_NO_DEV if there is no device with this name; -E_NA_DEV if the device
 *         is not usable (mountable but not mounted)
 */
int vfs_get_root(const char *devname, struct inode **root_store);
/**
 * @brief Get the name of the device a file system is attached to.
 * @param fs file system, not NULL
 * @return the device name (for example "disk0"), or NULL if @p fs is not in the device list
 * @note The device list is read without taking vdev_list_sem.
 */
const char *vfs_get_devname(struct fs *fs);


/**
 * @brief Open or create a file by name (the VFS part of open()).
 *
 * Resolves @p path with vfs_lookup(). If the file does not exist and O_CREAT is set, the parent
 * directory is located with vfs_lookup_parent() and vop_create() makes the file. Then vop_open()
 * is called, the open count is incremented, and with O_TRUNC (or after O_CREAT) the file is
 * truncated to zero length. The access mode is O_RDONLY, O_WRONLY or O_RDWR.
 * @param path         file name; may be modified (see vfs_lookup())
 * @param open_flags   O_RDONLY / O_WRONLY / O_RDWR, optionally with O_CREAT, O_EXCL, O_TRUNC
 * @param[out] inode_store receives the inode on success, with a reference and an open count held
 *                     by the caller; release it with vfs_close()
 * @return 0 on success; -E_INVAL for a bad access mode or O_TRUNC without write access;
 *         -E_EXISTS for O_CREAT|O_EXCL on an existing file; -E_NOENT if the file does not exist and
 *         O_CREAT is not set; or the error of the lookup, vop_create(), vop_open() or
 *         vop_truncate()
 * @note  On failure no open count is kept. Two failure paths (-E_EXISTS, and the parent directory
 *        after vop_create()) do not release a reference; see the notes in vfsfile.c.
 */
int vfs_open(char *path, uint32_t open_flags, struct inode **inode_store);
/**
 * @brief Close an inode opened with vfs_open(). It does not fail.
 *
 * Decrements the open count (vop_close() runs when it reaches zero) and then drops the reference
 * taken by vfs_open(). The inode may be freed by this call.
 * @param node inode returned by vfs_open()
 * @return always 0
 */
int vfs_close(struct inode *node);
/**
 * @brief Create a hard link. Not implemented in this lab.
 * @param old_path existing file
 * @param new_path name of the new link
 * @return -E_UNIMP
 */
int vfs_link(char *old_path, char *new_path);
/**
 * @brief Create a symbolic link. Not implemented in this lab.
 * @param old_path contents of the link
 * @param new_path path of the link
 * @return -E_UNIMP
 */
int vfs_symlink(char *old_path, char *new_path);
/**
 * @brief Read the contents of a symbolic link. Not implemented in this lab.
 * @param path path of the link
 * @param iob  buffer that would receive the contents
 * @return -E_UNIMP
 */
int vfs_readlink(char *path, struct iobuf *iob);
/**
 * @brief Create a directory. Not implemented in this lab.
 * @param path path of the new directory
 * @return -E_UNIMP
 */
int vfs_mkdir(char *path);
/**
 * @brief Delete a file or directory. Not implemented in this lab.
 * @param path path to delete
 * @return -E_UNIMP
 */
int vfs_unlink(char *path);
/**
 * @brief Rename a file. Not implemented in this lab.
 * @param old_path existing name
 * @param new_path new name
 * @return -E_UNIMP
 */
int vfs_rename(char *old_path, char *new_path);
/**
 * @brief Change the current directory of the current process by name.
 *
 * Looks up @p path with vfs_lookup() and then calls vfs_set_curdir() on the result.
 * @param path directory name; may be modified (see vfs_lookup())
 * @return 0 on success; the error of vfs_lookup() or -E_NOTDIR if the path is not a directory
 */
int vfs_chdir(char *path);
/**
 * @brief Write the absolute name of the current directory into an I/O buffer.
 *
 * Appends "<device>:" followed by the path of the directory relative to the root of its file
 * system.
 * @param iob buffer that receives the name, advanced as data is written
 * @return 0 on success; -E_NOENT if there is no current directory; the error of iobuf_move() or
 *         vop_namefile() otherwise
 */
int vfs_getcwd(struct iobuf *iob);


/**
 * @brief Resolve a full path name to an inode.
 *
 * Accepts "device:path" (start at the root of that device), "/path" (start at the boot file
 * system), ":path" (start at the root of the current directory's file system) and a plain
 * relative path (start at the current directory). The path is cut in place and the lookup is then
 * done with vop_lookup() step by step on the file system. An empty remainder returns the start
 * inode itself.
 * @param path         path name; it is modified in place and must be writable
 * @param[out] node_store receives the inode, with a reference held by the caller; release it with
 *                     vop_ref_dec()
 * @return 0 on success; -E_NOENT if there is no boot file system for absolute paths; -E_NO_DEV or
 *         -E_NA_DEV for an unknown or unusable device; or the error of vop_lookup()
 */
int vfs_lookup(char *path, struct inode **node_store);
/**
 * @brief Resolve the directory part of a path, leaving the last component for the caller.
 *
 * The device prefix and the starting directory are resolved as in vfs_lookup(). The remaining
 * part of the path is returned through @p endp, and the caller decides what to do with it. This
 * is used by vfs_open() to create a file.
 * @param path          path name; it is modified in place
 * @param[out] node_store receives the starting directory, with a reference held by the caller
 * @param[out] endp      receives a pointer into @p path at the remaining, unresolved part
 * @return 0 on success, or the error of the device or current-directory lookup
 */
int vfs_lookup_parent(char *path, struct inode **node_store, char **endp);

/**
 * @brief Set the boot file system, the target of absolute paths such as "/sh".
 *
 * @p fsname must end with a colon, for example "disk0:". The function changes the current directory
 * of the calling process to the root of that device (vfs_chdir()) and then records the resulting
 * directory as the boot file system.
 * @note Side effect: the current directory of the calling process is changed to the root of
 *       @p fsname as well. Passing NULL clears the boot file system.
 * @param fsname device name with a trailing colon, or NULL
 * @return 0 on success; -E_INVAL if @p fsname does not end with a single colon; the error of
 *         vfs_chdir() or vfs_get_curdir() otherwise
 */
int vfs_set_bootfs(char *fsname);
/**
 * @brief Get the root directory of the boot file system.
 * @param[out] node_store receives the root inode, with its reference count incremented
 * @return 0 on success; -E_NOENT if no boot file system is set
 * @note The caller must release the reference with vop_ref_dec().
 */
int vfs_get_bootfs(struct inode **node_store);

/**
 * @brief Add a built-in file system to the device list under the name "devname:".
 *
 * Intended for file systems that are not backed by a disk (for example emufs or procfs). The
 * device is marked non-mountable.
 * @param devname device name, at most FS_MAX_DNAME_LEN characters, unique in the list
 * @param fs      file system to add
 * @return 0 on success; -E_TOO_BIG if the name is too long; -E_EXISTS if the name is already in
 *         use;
 *         -E_NO_MEM if memory is exhausted
 */
int vfs_add_fs(const char *devname, struct fs *fs);
/**
 * @brief Add a device inode to the device list.
 *
 * If @p mountable is false, the device is reached as "devname:". If it is true, it can later be
 * mounted with vfs_mount(). This function asserts that @p devnode is a device inode.
 * @note The header-level description of the "devnameraw:" name is not implemented in this code:
 *       a mountable device is looked up by its plain name, and vfs_get_root() then fails with
 *       -E_NA_DEV until a file system is mounted.
 * @param devname   device name, unique in the list
 * @param devnode   device inode, or NULL for a file system; the list stores the pointer without
 *                  taking a reference (vfs_get_root() takes one when it hands the device out)
 * @param mountable non-zero if a file system may be mounted on the device
 * @return 0 on success; -E_TOO_BIG, -E_EXISTS or -E_NO_MEM as for vfs_add_fs()
 */
int vfs_add_dev(const char *devname, struct inode *devnode, bool mountable);

/**
 * @brief Mount a file system on a mountable device.
 *
 * Finds the mountable device @p devname, then calls @p mountfunc with the device, which must create
 * the struct fs and store it in its fs_store argument. The device list lock is held during the
 * call, so @p mountfunc must not call VFS functions that take that lock (vfs_add_fs(),
 * vfs_add_dev(), vfs_get_root() and so on).
 * @param devname   name of the mountable device, for example "disk0"
 * @param mountfunc file-system-specific mount function, for example sfs_do_mount
 * @return 0 on success; -E_NO_DEV if there is no mountable device with this name; -E_BUSY if a file
 *         system is already mounted; otherwise the error returned by @p mountfunc
 */
int vfs_mount(const char *devname, int (*mountfunc)(struct device *dev, struct fs **fs_store));
/**
 * @brief Unmount the file system on a device: sync it, then call fsop_unmount().
 * @param devname name of the mountable device
 * @return 0 on success; -E_NO_DEV if there is no such device; -E_INVAL if nothing is mounted;
 *         the error of fsop_sync() or fsop_unmount() otherwise (for example -E_BUSY)
 */
int vfs_unmount(const char *devname);
/**
 * @brief Unmount every file system that is mounted on a mountable device.
 *
 * Best effort: a failure is reported with cprintf() and the loop goes on with the next device.
 * @return always 0
 */
int vfs_unmount_all(void);

#endif /* !__KERN_FS_VFS_VFS_H__ */
