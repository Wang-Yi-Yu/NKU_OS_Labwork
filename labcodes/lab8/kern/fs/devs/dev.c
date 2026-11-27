/**
 * @file   dev.c
 * @brief  Device inodes: the inode_ops table that adapts struct device to the VFS, and dev_init().
 *
 * A device is seen by the VFS as an inode of type device (in_type inode_type_device_info). The
 * functions here are the vop_* implementations of that inode type (dev_node_ops). Each one takes
 * the struct device out of the inode with vop_info() and passes the request to the matching
 * dop_* hook. Only lookups, open/close checks and the stat/seek rules are handled here; the actual
 * transfers are done by the device's d_io hook (dev_stdin.c, dev_stdout.c, dev_disk0.c).
 *
 * Call chain, for example read(fd) on stdin: sysfile_read() -> file_read() -> vop_read() =
 * dev_read() -> dop_io() = stdin_io(). dev_init() runs once at boot and registers the built-in
 * devices with the VFS through their dev_init_*() functions.
 */
#include <defs.h>
#include <string.h>
#include <stat.h>
#include <dev.h>
#include <inode.h>
#include <unistd.h>
#include <error.h>

/**
 * @brief vop_open of a device inode: reject creation-style flags, then call the device's open hook.
 *
 * O_CREAT, O_TRUNC, O_EXCL and O_APPEND are refused with -E_INVAL, because a device has no
 * contents to create, truncate or append to. The open count is kept by the VFS (vfs_open()), not here.
 *
 * @param node       the device inode being opened
 * @param open_flags O_* flags of the open() call
 * @return the result of the device's d_open hook (0 on success), or -E_INVAL
 */
static int dev_open(struct inode *node, uint32_t open_flags) {
    if (open_flags & (O_CREAT | O_TRUNC | O_EXCL | O_APPEND)) {
        return -E_INVAL;
    }
    struct device *dev = vop_info(node, device);
    return dop_open(dev, open_flags);
}

/**
 * @brief vop_close of a device inode: pass the call to the device's close hook.
 *
 * Called by the VFS on the last close() of the device (when the inode's open count drops to 0).
 *
 * @param node the device inode
 * @return the result of the device's d_close hook
 */
static int dev_close(struct inode *node) {
    struct device *dev = vop_info(node, device);
    return dop_close(dev);
}

/**
 * @brief vop_read of a device inode: hand the transfer to the device's I/O hook as a read.
 *
 * @param node the device inode
 * @param iob  the buffer to fill; the hook advances iob->io_offset and iob->io_resid
 * @return the result of the device's d_io hook (bytes moved are seen in iob)
 */
static int dev_read(struct inode *node, struct iobuf *iob) {
    struct device *dev = vop_info(node, device);
    return dop_io(dev, iob, 0);
}

/**
 * @brief vop_write of a device inode: hand the transfer to the device's I/O hook as a write.
 *
 * @param node the device inode
 * @param iob  the buffer to write from; the hook advances iob->io_offset and iob->io_resid
 * @return the result of the device's d_io hook (bytes moved are seen in iob)
 */
static int dev_write(struct inode *node, struct iobuf *iob) {
    struct device *dev = vop_info(node, device);
    return dop_io(dev, iob, 1);
}

/**
 * @brief vop_ioctl of a device inode: pass the control call to the device's ioctl hook.
 *
 * @param node the device inode
 * @param op   device specific operation code
 * @param data device specific argument
 * @return the result of the device's d_ioctl hook
 */
static int dev_ioctl(struct inode *node, int op, void *data) {
    struct device *dev = vop_info(node, device);
    return dop_ioctl(dev, op, data);
}

/**
 * @brief vop_fstat of a device inode, used by stat()/fstat().
 *
 * Fills the whole struct stat with zeros, then sets the type (via dev_gettype()), st_nlinks = 1,
 * st_blocks = d_blocks and st_size = d_blocks * d_blocksize. For a character device both are 0.
 *
 * @param node the device inode
 * @param stat output: the file status
 * @return 0 on success, or the error of vop_gettype()
 */
static int dev_fstat(struct inode *node, struct stat *stat) {
    int ret;
    memset(stat, 0, sizeof(struct stat));
    if ((ret = vop_gettype(node, &(stat->st_mode))) != 0) {
        return ret;
    }
    struct device *dev = vop_info(node, device);
    stat->st_nlinks = 1;
    stat->st_blocks = dev->d_blocks;
    stat->st_size = stat->st_blocks * dev->d_blocksize;
    return 0;
}

/**
 * @brief vop_gettype of a device inode: block device or character device.
 *
 * A device with d_blocks > 0 is reported as S_IFBLK, every other device as S_IFCHR.
 *
 * @param node       the device inode
 * @param type_store output: S_IFBLK or S_IFCHR
 * @return always 0
 */
static int dev_gettype(struct inode *node, uint32_t *type_store) {
    struct device *dev = vop_info(node, device);
    *type_store = (dev->d_blocks > 0) ? S_IFBLK : S_IFCHR;
    return 0;
}

/**
 * @brief vop_tryseek of a device inode: check that a seek to pos is allowed.
 *
 * For a block device, pos must be a multiple of d_blocksize and lie in [0, d_blocks * d_blocksize).
 * A character device never allows a seek.
 *
 * @param node the device inode
 * @param pos  the target byte offset
 * @return 0 if the seek is allowed, -E_INVAL otherwise
 */
static int dev_tryseek(struct inode *node, off_t pos) {
    struct device *dev = vop_info(node, device);
    if (dev->d_blocks > 0) {
        if ((pos % dev->d_blocksize) == 0) {
            if (pos >= 0 && pos < dev->d_blocks * dev->d_blocksize) {
                return 0;
            }
        }
    }
    return -E_INVAL;
}

/**
 * @brief vop_lookup of a device inode: only the empty path names the device itself.
 *
 * Devices have no sub-namespace. The VFS passes the part of the path after "name:"; if that part
 * is not empty the lookup fails. On success the device inode is returned with its reference
 * count increased (vop_ref_inc).
 *
 * @param node       the device inode the name refers to
 * @param path       the remaining path; must be the empty string
 * @param node_store output: the device inode, with one more reference
 * @return 0 on success, -E_NOENT if path is not empty
 * @note The caller owns the extra reference and must drop it with vop_ref_dec().
 */
static int dev_lookup(struct inode *node, char *path, struct inode **node_store) {
    if (*path != '\0') {
        return -E_NOENT;
    }
    vop_ref_inc(node);
    *node_store = node;
    return 0;
}

/**
 * @brief The operations of a device inode; the VFS calls them through in_ops.
 *
 * Only the vop_* entries listed here are implemented. Entries that are not listed stay NULL.
 */
static const struct inode_ops dev_node_ops = {
    .vop_magic = VOP_MAGIC,
    .vop_open = dev_open,
    .vop_close = dev_close,
    .vop_read = dev_read,
    .vop_write = dev_write,
    .vop_fstat = dev_fstat,
    .vop_ioctl = dev_ioctl,
    .vop_gettype = dev_gettype,
    .vop_tryseek = dev_tryseek,
    .vop_lookup = dev_lookup,
};

/**
 * @brief Declare and call dev_init_<x>() for one built-in device.
 *
 * Expands to a block that declares the function and calls it. For example init_device(stdin)
 * calls dev_init_stdin().
 *
 * @param x device name, without the dev_init_ prefix
 */
#define init_device(x)                  \
    do {                                \
        extern void dev_init_##x(void); \
        dev_init_##x();                 \
    } while (0)

/**
 * @brief Register the built-in devices with the VFS: stdin, stdout and disk0.
 *
 * Calls dev_init_stdin(), dev_init_stdout() and dev_init_disk0(). The null device is commented
 * out. Panics if one of the devices cannot be set up.
 */
void dev_init(void) {
    // init_device(null);
    init_device(stdin);
    init_device(stdout);
    init_device(disk0);
}

/**
 * @brief Create an inode of device type and bind it to dev_node_ops.
 *
 * The struct device inside the inode is not initialised here; the caller fills it in.
 *
 * @return the new inode, or NULL if allocation failed
 */
struct inode *dev_create_inode(void) {
    struct inode *node;
    if ((node = alloc_inode(device)) != NULL) {
        vop_init(node, &dev_node_ops, NULL);
    }
    return node;
}
