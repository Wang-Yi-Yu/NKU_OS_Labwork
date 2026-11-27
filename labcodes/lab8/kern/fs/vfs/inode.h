/**
 * @file   inode.h
 * @brief  In-memory inode (struct inode), its reference and open counts, and the VOP_* operation
 *         interface (struct inode_ops and the vop_* macros) through which the VFS reaches SFS and
 *         the device files.
 *
 * An inode is the file-system-independent handle of one file or directory. Its type-specific part
 * (struct sfs_inode for SFS files, struct device for device files) is stored in the union
 * inode::in_info, and the operations on it are a table of function pointers, inode_ops, supplied by
 * the file system. Upper layers call the operations through the vop_* macros, for example
 * vop_read(node, iob), which expands to node->in_ops->vop_read(node, iob). The call chain for a
 * read is sysfile_read() -> file_read() -> vop_read() -> sfs_read() -> sfs_io() -> sfs_io_nolock().
 *
 * Two independent counters manage an inode's life. ref_count (vop_ref_inc() / vop_ref_dec()) keeps
 * the structure alive; when it reaches zero, vop_reclaim() releases the file system's resources.
 * open_count (vop_open_inc() / vop_open_dec(), used by vfs_open() / vfs_close()) counts open
 * descriptors; when it reaches zero, vop_close() is called. Code above the VFS layer should never
 * touch open_count directly.
 *
 * The header is included by every VFS source file and by the file-system implementations (SFS,
 * devices), which fill in inode_ops and call inode_init() / vop_kill().
 */
#ifndef __KERN_FS_VFS_INODE_H__
#define __KERN_FS_VFS_INODE_H__

#include <defs.h>
#include <dev.h>
#include <sfs.h>
#include <atomic.h>
#include <assert.h>

struct stat;
struct iobuf;

/**
 * @brief Abstract low-level file: the in-memory inode shared by the VFS and a concrete file system.
 *
 * in_info is file-system-specific data and in_type is the tag that says which member of in_info is
 * valid. open_count is managed with vop_open_inc() / vop_open_dec() by vfs_open() and vfs_close().
 * Code above the VFS layer should not need to worry about it.
 *
 * @var inode::in_info     union with the type-specific inode: struct device (device files) or
 *                         struct sfs_inode (SFS files and directories); select it with vop_info()
 * @var inode::in_type     tag of in_info: inode_type_device_info or inode_type_sfs_inode_info
 * @var inode::ref_count   number of references to this inode (kernel structures and descriptors
 *                         holding it); inode_init() starts it at 1
 * @var inode::open_count  number of open files on this inode; always <= ref_count
 * @var inode::in_fs       file system this inode belongs to (NULL for a bare device file)
 * @var inode::in_ops      table of the operations of this inode, see struct inode_ops
 */
struct inode {
    union {
        struct device __device_info;
        struct sfs_inode __sfs_inode_info;
    } in_info;
    enum {
        inode_type_device_info = 0x1234,
        inode_type_sfs_inode_info,
    } in_type;
    int ref_count;
    int open_count;
    struct fs *in_fs;
    const struct inode_ops *in_ops;
};

/**
 * @brief Build the inode_type_<type>_info tag name.
 *
 * For example __in_type(sfs_inode) is inode_type_sfs_inode_info.
 */
#define __in_type(type) inode_type_##type##_info

/**
 * @brief Test whether @p node holds the type-specific data named @p type.
 * @param node inode to test
 * @param type type name without the _info suffix (device or sfs_inode)
 * @return non-zero if node->in_type matches
 */
#define check_inode_type(node, type) ((node)->in_type == __in_type(type))

/**
 * @brief Return the type-specific part of an inode, checking the type tag first.
 *
 * Asserts that @p node is not NULL and that it has the requested type, then yields a pointer to
 * node->in_info.__<type>_info. The result is an lvalue of type struct device or struct sfs_inode.
 * @param node inode (struct inode *)
 * @param type type name without the _info suffix (device or sfs_inode)
 */
#define __vop_info(node, type)                                    \
    ({                                                            \
        struct inode *__node = (node);                            \
        assert(__node != NULL && check_inode_type(__node, type)); \
        &(__node->in_info.__##type##_info);                       \
    })

/**
 * @brief Public form of __vop_info(); see there.
 * @param node inode (struct inode *)
 * @param type type name without the _info suffix (device or sfs_inode)
 */
#define vop_info(node, type) __vop_info(node, type)

/**
 * @brief Recover the struct inode that contains a type-specific info structure.
 * @param info pointer to a struct device or struct sfs_inode that is embedded in an inode
 * @param type type name without the _info suffix (device or sfs_inode)
 */
#define info2node(info, type) to_struct((info), struct inode, in_info.__##type##_info)

/**
 * @brief Allocate an inode with its in_type set; the other fields are not initialised.
 * @param type inode_type_* tag value
 * @return the new inode, or NULL if kmalloc() failed
 */
struct inode *__alloc_inode(int type);

/**
 * @brief Allocate an inode of the given type name (device or sfs_inode); see __alloc_inode().
 * @param type type name without the _info suffix
 */
#define alloc_inode(type) __alloc_inode(__in_type(type))

/** @brief Upper bound (exclusive) that inode_check() enforces for ref_count and open_count. */
#define MAX_INODE_COUNT 0x10000

/**
 * @brief Increment the reference count of @p node. Use the vop_ref_inc() macro.
 * @param node inode
 * @return the new reference count
 */
int inode_ref_inc(struct inode *node);
/**
 * @brief Decrement the reference count of @p node. Use the vop_ref_dec() macro.
 *
 * When the count reaches zero, calls vop_reclaim(node) so that the file system can release the
 * inode. A result of -E_BUSY from vop_reclaim() is ignored silently; any other error is only
 * reported with cprintf(), it is not returned.
 * @param node inode, with ref_count > 0 (asserted)
 * @return the new reference count
 * @note The caller must not touch @p node after the count reached zero and it was reclaimed.
 */
int inode_ref_dec(struct inode *node);
/**
 * @brief Increment the open count of @p node. Use the vop_open_inc() macro, called by vfs_open().
 * @param node inode
 * @return the new open count
 */
int inode_open_inc(struct inode *node);
/**
 * @brief Decrement the open count of @p node. Use the vop_open_dec() macro, called by vfs_close().
 *
 * When the count reaches zero, calls vop_close(node). A failure of vop_close() is only reported
 * with cprintf(); it is not returned.
 * @param node inode, with open_count > 0 (asserted)
 * @return the new open count
 */
int inode_open_dec(struct inode *node);

/**
 * @brief Initialise a freshly allocated inode. Use the vop_init() macro.
 *
 * Sets ref_count and open_count to 0, stores the operation table and the file system, and then
 * takes the initial reference with vop_ref_inc(), so ref_count is 1 on return.
 * @param node inode allocated with alloc_inode() (or __alloc_inode())
 * @param ops  operation table, whose vop_magic must be VOP_MAGIC
 * @param fs   file system the inode belongs to, or NULL for a bare device file
 */
void inode_init(struct inode *node, const struct inode_ops *ops, struct fs *fs);
/**
 * @brief Free an inode. Use the vop_kill() macro, normally from the file system's vop_reclaim().
 * @param node inode whose ref_count and open_count are both 0 (asserted); it is freed with kfree()
 */
void inode_kill(struct inode *node);

/**
 * @brief Magic number that every inode_ops table must carry in vop_magic.
 *
 * inode_check() asserts it before each operation.
 */
#define VOP_MAGIC 0x8c4ba476

/**
 * @brief Table of the operations of an inode, filled in by each file system (SFS, device files).
 *
 * Every member is called through the vop_* macro of the same name, for example vop_read(node, iob)
 * calls node->in_ops->vop_read(node, iob). Unless stated otherwise, a member is only called on an
 * inode that is referenced (ref_count > 0), the caller holds no lock of its own, and the
 * implementation takes the locks it needs. The operations are described below, with the vop_* macro
 * that calls each one.
 *
 * Open and close:
 *   - vop_open: called by vfs_open() when a file is opened. It can reject illegal or undesired
 *     open modes. An operation may run without the file actually being opened. The implementation
 *     need not look at O_CREAT, O_EXCL or O_TRUNC, because the VFS layer handles them.
 *     It must not be called directly from above the VFS layer; use vfs_open(), which keeps the open
 *     count consistent.
 *   - vop_close: called on the last close of a file, that is, when open_count drops to 0 in
 *     inode_open_dec(). It must not be called directly from above the VFS layer; use vfs_close().
 *   - vop_reclaim: called when the inode is no longer referenced (ref_count reached 0). It may run
 *     long after vop_close(). It must release the inode, normally with vop_kill(); it may return
 *     -E_BUSY, which the caller ignores.
 *
 * Data and attributes:
 *   - vop_read: read data from the file into the iobuf, at the offset stored in the iobuf, and
 *     update the iobuf's residual length and offset to match the amount read. Not allowed on
 *     directories or symlinks.
 *   - vop_write: write data from the iobuf to the file, at the offset stored in the iobuf, and
 *     update the residual length and offset. Not allowed on directories or symlinks.
 *   - vop_getdirentry: read one file name from a directory into the iobuf. The name is chosen from
 *     the offset field of the iobuf, which the operation updates. Unlike regular-file I/O, that
 *     offset is not a byte count and is only meaningful to the file system. The residual length
 *     is handled as in vop_read(). On a non-directory the operation returns -E_NOTDIR.
 *   - vop_namefile: compute the pathname of the file relative to the root of its file system and
 *     copy it into the iobuf. It need not work on objects other than directories.
 *   - vop_fstat: fill the struct stat that the pointer refers to with information about the file.
 *   - vop_gettype: store the file type in *type_store. The type values (SFS_TYPE_*) are defined in
 *     sfs.h.
 *   - vop_tryseek: check whether seeking to position @p pos is legal. Some files refuse all seeks
 *     (serial or console devices), and fixed-size files may refuse seeks past their end.
 *   - vop_fsync: force all dirty buffers of this file to stable storage.
 *   - vop_truncate: force the size of the file to @p len, discarding any excess blocks.
 *   - vop_ioctl: perform the control operation @p op on the file; @p data is interpreted per
 *     operation.
 *
 * Directory operations:
 *   - vop_create: create a regular file called @p name in the directory @p node. If @p excl is
 *     true, fail when the file already exists; otherwise use the existing file. The inode of the
 *     file is returned as vop_lookup() does.
 *   - vop_lookup: parse @p path relative to the directory @p node and return the inode of the file
 *     it names. The operation may destroy @p path. It must increment the reference count of the
 *     inode it returns.
 *
 * @note The original design text talks about "uio" and ENOTDIR; in this code the buffer type is
 *       struct iobuf and the error values are the negative codes -E_NOTDIR, -E_INVAL, and so on.
 * @note vop_magic must be VOP_MAGIC; inode_check() asserts it before every operation.
 */
struct inode_ops {
    unsigned long vop_magic;
    int (*vop_open)(struct inode *node, uint32_t open_flags);
    int (*vop_close)(struct inode *node);
    int (*vop_read)(struct inode *node, struct iobuf *iob);
    int (*vop_write)(struct inode *node, struct iobuf *iob);
    int (*vop_fstat)(struct inode *node, struct stat *stat);
    int (*vop_fsync)(struct inode *node);
    int (*vop_namefile)(struct inode *node, struct iobuf *iob);
    int (*vop_getdirentry)(struct inode *node, struct iobuf *iob);
    int (*vop_reclaim)(struct inode *node);
    int (*vop_gettype)(struct inode *node, uint32_t *type_store);
    int (*vop_tryseek)(struct inode *node, off_t pos);
    int (*vop_truncate)(struct inode *node, off_t len);
    int (*vop_create)(struct inode *node, const char *name, bool excl, struct inode **node_store);
    int (*vop_lookup)(struct inode *node, char *path, struct inode **node_store);
    int (*vop_ioctl)(struct inode *node, int op, void *data);
};

/**
 * @brief Consistency check, run before every vop_* call.
 *
 * Asserts that @p node and its operation table are valid, that vop_magic equals VOP_MAGIC, and that
 * ref_count >= open_count >= 0 and both counts are below MAX_INODE_COUNT.
 * @param node  inode about to be used
 * @param opstr name of the operation, used only for debugging (the vop_* macro passes #sym)
 */
void inode_check(struct inode *node, const char *opstr);

/**
 * @brief Common body of the vop_* macros: check the inode, then return the function pointer.
 *
 * Asserts that @p node, its operation table and the requested member are not NULL, runs
 * inode_check(), and yields node->in_ops->vop_<sym>. The caller then applies the pointer to the
 * arguments.
 * @param node inode (evaluated once)
 * @param sym  operation name without the vop_ prefix
 */
#define __vop_op(node, sym)                                                                    \
    ({                                                                                         \
        struct inode *__node = (node);                                                         \
        assert(__node != NULL && __node->in_ops != NULL && __node->in_ops->vop_##sym != NULL); \
        inode_check(__node, #sym);                                                             \
        __node->in_ops->vop_##sym;                                                             \
    })

/**
 * @brief Call inode_ops::vop_open. See struct inode_ops for the contract.
 * @param node       inode
 * @param open_flags open flags (O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, ...)
 * @return 0 on success, a negative error code otherwise
 */
#define vop_open(node, open_flags) (__vop_op(node, open)(node, open_flags))
/**
 * @brief Call inode_ops::vop_close, the last close of the file. See struct inode_ops.
 * @param node inode
 * @return 0 on success, a negative error code otherwise
 */
#define vop_close(node) (__vop_op(node, close)(node))
/**
 * @brief Call inode_ops::vop_read. See struct inode_ops.
 * @param node inode
 * @param iob  destination buffer; its offset and residual length are updated
 * @return 0 on success, a negative error code otherwise
 */
#define vop_read(node, iob) (__vop_op(node, read)(node, iob))
/**
 * @brief Call inode_ops::vop_write. See struct inode_ops.
 * @param node inode
 * @param iob  source buffer; its offset and residual length are updated
 * @return 0 on success, a negative error code otherwise
 */
#define vop_write(node, iob) (__vop_op(node, write)(node, iob))
/**
 * @brief Call inode_ops::vop_fstat. See struct inode_ops.
 * @param node inode
 * @param stat struct stat to fill in
 * @return 0 on success, a negative error code otherwise
 */
#define vop_fstat(node, stat) (__vop_op(node, fstat)(node, stat))
/**
 * @brief Call inode_ops::vop_fsync. See struct inode_ops.
 * @param node inode
 * @return 0 on success, a negative error code otherwise
 */
#define vop_fsync(node) (__vop_op(node, fsync)(node))
/**
 * @brief Call inode_ops::vop_namefile. See struct inode_ops.
 * @param node inode, normally a directory
 * @param iob  buffer that receives the path relative to the file system root
 * @return 0 on success, a negative error code otherwise
 */
#define vop_namefile(node, iob) (__vop_op(node, namefile)(node, iob))
/**
 * @brief Call inode_ops::vop_getdirentry. See struct inode_ops.
 * @param node inode, must be a directory
 * @param iob  buffer that receives the name; its offset is updated
 * @return 0 on success, a negative error code otherwise
 */
#define vop_getdirentry(node, iob) (__vop_op(node, getdirentry)(node, iob))
/**
 * @brief Call inode_ops::vop_reclaim, called when ref_count reaches 0. See struct inode_ops.
 * @param node inode
 * @return 0 on success, a negative error code otherwise (-E_BUSY is ignored by the caller)
 */
#define vop_reclaim(node) (__vop_op(node, reclaim)(node))
/**
 * @brief Call inode_ops::vop_ioctl. See struct inode_ops.
 * @param node inode
 * @param op   control operation code
 * @param data operation-specific argument
 * @return 0 on success, a negative error code otherwise
 */
#define vop_ioctl(node, op, data) (__vop_op(node, ioctl)(node, op, data))
/**
 * @brief Call inode_ops::vop_gettype. See struct inode_ops.
 * @param node       inode
 * @param type_store receives the file type (SFS_TYPE_FILE, SFS_TYPE_DIR, ...)
 * @return 0 on success, a negative error code otherwise
 */
#define vop_gettype(node, type_store) (__vop_op(node, gettype)(node, type_store))
/**
 * @brief Call inode_ops::vop_tryseek. See struct inode_ops.
 * @param node inode
 * @param pos  position to seek to
 * @return 0 if the seek is allowed, a negative error code otherwise
 */
#define vop_tryseek(node, pos) (__vop_op(node, tryseek)(node, pos))
/**
 * @brief Call inode_ops::vop_truncate. See struct inode_ops.
 * @param node inode
 * @param len  new size of the file
 * @return 0 on success, a negative error code otherwise
 */
#define vop_truncate(node, len) (__vop_op(node, truncate)(node, len))
/**
 * @brief Call inode_ops::vop_create. See struct inode_ops.
 * @param node       directory in which the file is created
 * @param name       name of the new file
 * @param excl       fail with an error if the file already exists
 * @param node_store receives the inode of the file (with a reference)
 * @return 0 on success, a negative error code otherwise
 */
#define vop_create(node, name, excl, node_store) \
    (__vop_op(node, create)(node, name, excl, node_store))
/**
 * @brief Call inode_ops::vop_lookup. See struct inode_ops.
 * @param node       directory from which @p path is resolved
 * @param path       path relative to @p node; may be modified by the callee
 * @param node_store receives the inode the path names (with a reference)
 * @return 0 on success, a negative error code otherwise
 */
#define vop_lookup(node, path, node_store) (__vop_op(node, lookup)(node, path, node_store))


/**
 * @brief Return the file system an inode belongs to (in_fs).
 * @param node inode
 */
#define vop_fs(node) ((node)->in_fs)
/**
 * @brief Initialise an inode; see inode_init().
 * @param node inode
 * @param ops  operation table
 * @param fs   file system, or NULL
 */
#define vop_init(node, ops, fs) inode_init(node, ops, fs)
/**
 * @brief Free an inode; see inode_kill(). Usually called from vop_reclaim().
 * @param node inode with no references and no open files
 */
#define vop_kill(node) inode_kill(node)

/*
 * Reference count manipulation (handled above filesystem level)
 */
/**
 * @brief Take a reference on an inode; see inode_ref_inc().
 * @param node inode
 */
#define vop_ref_inc(node) inode_ref_inc(node)
/**
 * @brief Drop a reference on an inode; when it was the last one the inode is reclaimed.
 * @param node inode; must not be used after the call if it was the last reference
 * @see inode_ref_dec()
 */
#define vop_ref_dec(node) inode_ref_dec(node)
/**
 * @brief Count one more open file on an inode; see inode_open_inc(). Called by vfs_open().
 *
 * Like vop_open_dec(), it should not be called from above the VFS layer.
 * @param node inode
 */
#define vop_open_inc(node) inode_open_inc(node)
/**
 * @brief Count one less open file on an inode; at zero vop_close() runs. Called by vfs_close().
 * @param node inode
 * @see inode_open_dec()
 */
#define vop_open_dec(node) inode_open_dec(node)


/**
 * @brief Current reference count of an inode.
 * @param node inode
 * @return node->ref_count
 */
static inline int inode_ref_count(struct inode *node) {
    return node->ref_count;
}

/**
 * @brief Current open count of an inode.
 * @param node inode
 * @return node->open_count
 */
static inline int inode_open_count(struct inode *node) {
    return node->open_count;
}

#endif /* !__KERN_FS_VFS_INODE_H__ */
