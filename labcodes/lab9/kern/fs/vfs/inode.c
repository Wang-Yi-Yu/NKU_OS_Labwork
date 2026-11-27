/**
 * @file   inode.c
 * @brief  Allocation, initialisation, reference counting and open counting of in-memory inodes.
 *
 * This file implements the generic half of struct inode (declared in inode.h). The file systems
 * allocate an inode with __alloc_inode() / alloc_inode(), initialise it with inode_init()
 * (vop_init)
 * and fill in its operation table. The VFS then manages its life through two counters:
 *   - ref_count is raised by vop_ref_inc() and lowered by vop_ref_dec(). When it reaches zero,
 *     inode_ref_dec() calls vop_reclaim(), and the file system frees the inode with inode_kill().
 *   - open_count is raised by vop_open_inc() (from vfs_open()) and lowered by vop_open_dec() (from
 *     vfs_close()). When it reaches zero, inode_open_dec() calls vop_close().
 * inode_check() runs before every vop_* call and validates both counters and the operation table.
 */
#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <atomic.h>
#include <vfs.h>
#include <inode.h>
#include <error.h>
#include <assert.h>
#include <kmalloc.h>

/**
 * @brief Allocate an inode structure and set its type tag.
 *
 * Only in_type is set. All other fields are left uninitialised (kmalloc() does not zero them), so
 * the inode must be passed to inode_init() before use.
 * @param type inode_type_* tag, normally produced by alloc_inode() (inode_type_device_info or
 *             inode_type_sfs_inode_info)
 * @return the new inode, or NULL if kmalloc() failed
 */
struct inode *__alloc_inode(int type) {
    struct inode *node;
    if ((node = kmalloc(sizeof(struct inode))) != NULL) {
        node->in_type = type;
    }
    return node;
}

/**
 * @brief Initialise an inode structure. Invoked through vop_init().
 *
 * Sets both counters to 0, stores the operation table and the file system, and then takes the
 * initial reference with vop_ref_inc(), so the creator owns one reference (ref_count == 1).
 * @param node inode returned by alloc_inode()
 * @param ops  operation table of the file system; its vop_magic is checked later by inode_check()
 * @param fs   file system that owns the inode, or NULL for a bare device file
 */
void inode_init(struct inode *node, const struct inode_ops *ops, struct fs *fs) {
    node->ref_count = 0;
    node->open_count = 0;
    node->in_ops = ops, node->in_fs = fs;
    vop_ref_inc(node);
}

/**
 * @brief Free an inode structure. Invoked through vop_kill(), normally from vop_reclaim().
 *
 * Both counters must already be zero; the function asserts this and then calls kfree().
 * @param node inode to free; it must not be used afterwards
 * @note The file system's resources (for example struct sfs_inode) must be released by the caller
 *       before this call, because this function only frees the struct inode itself.
 */
void inode_kill(struct inode *node) {
    assert(inode_ref_count(node) == 0);
    assert(inode_open_count(node) == 0);
    kfree(node);
}

/**
 * @brief Increment ref_count. Invoked through vop_ref_inc().
 * @param node inode
 * @return the new value of ref_count
 */
int inode_ref_inc(struct inode *node) {
    node->ref_count += 1;
    return node->ref_count;
}

/**
 * @brief Decrement ref_count. Invoked through vop_ref_dec().
 *
 * If the count reaches zero, calls vop_reclaim(node). Its result is checked: -E_BUSY is ignored
 * silently, and any other error is reported with cprintf() as a warning. The error is not returned.
 * @param node inode, with ref_count > 0 (asserted)
 * @return the new value of ref_count
 * @note If the returned value is 0, the inode may already have been freed by vop_reclaim(), so
 *       the caller must not use @p node any more.
 */
int inode_ref_dec(struct inode *node) {
    assert(inode_ref_count(node) > 0);
    int ref_count, ret;
    node->ref_count -= 1;
    ref_count = node->ref_count;
    if (ref_count == 0) {
        if ((ret = vop_reclaim(node)) != 0 && ret != -E_BUSY) {
            cprintf("vfs: warning: vop_reclaim: %e.\n", ret);
        }
    }
    return ref_count;
}

/**
 * @brief Increment open_count. Invoked through vop_open_inc(), called by vfs_open().
 * @param node inode
 * @return the new value of open_count
 */
int inode_open_inc(struct inode *node) {
    node->open_count += 1;
    return node->open_count;
}

/**
 * @brief Decrement open_count. Invoked through vop_open_dec(), called by vfs_close().
 *
 * If the count reaches zero, calls vop_close(node). A failure of vop_close() is reported with
 * cprintf() as a warning and is not returned.
 * @param node inode, with open_count > 0 (asserted)
 * @return the new value of open_count
 */
int inode_open_dec(struct inode *node) {
    assert(inode_open_count(node) > 0);
    int open_count, ret;
    node->open_count -= 1;
    open_count = node->open_count;
    if (open_count == 0) {
        if ((ret = vop_close(node)) != 0) {
            cprintf("vfs: warning: vop_close: %e.\n", ret);
        }
    }
    return open_count;
}

/**
 * @brief Consistency check, called before every vop_* call (through __vop_op()).
 *
 * Asserts that the inode and its operation table are not NULL, that vop_magic is VOP_MAGIC, that
 * ref_count >= open_count >= 0, and that both counts are below MAX_INODE_COUNT.
 * @param node  inode about to be used
 * @param opstr operation name (the sym of vop_<sym>), only for debugging; it is not printed here
 */
void inode_check(struct inode *node, const char *opstr) {
    assert(node != NULL && node->in_ops != NULL);
    assert(node->in_ops->vop_magic == VOP_MAGIC);
    int ref_count = inode_ref_count(node), open_count = inode_open_count(node);
    assert(ref_count >= open_count && open_count >= 0);
    assert(ref_count < MAX_INODE_COUNT && open_count < MAX_INODE_COUNT);
}
