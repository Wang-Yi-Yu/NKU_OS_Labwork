/**
 * @file   dev.h
 * @brief  Device layer: the struct device descriptor, the dop_* operations and the device inodes.
 *
 * A device is a file-system-visible object such as stdin, stdout or disk0. Each one has a
 * struct device that holds its geometry and four function pointers (open, close, io, ioctl). The
 * device is wrapped in a VFS inode whose inode_ops (dev.c) forward vop_* calls to these pointers,
 * so user-level file calls reach the device through the usual chain sysfile_* -> file_* -> vop_*
 * -> dop_*. The concrete devices are set up by dev_init() (dev.c), which calls one dev_init_*()
 * per device; each of those registers its inode with vfs_add_dev().
 */
#ifndef __KERN_FS_DEVS_DEV_H__
#define __KERN_FS_DEVS_DEV_H__

#include <defs.h>

struct inode;
struct iobuf;

/**
 * @brief A device that is accessible through the file system namespace.
 *
 * d_io serves both reads and writes; the iobuf says which direction is wanted (its io_base and
 * io_resid describe the buffer and the amount still to move). A device with d_blocks > 0 is a
 * block device (S_IFBLK) that supports seeks in whole blocks. A device with d_blocks == 0 is a
 * character device (S_IFCHR) and does not support seeks.
 *
 * @var device::d_blocks    number of blocks on the device; 0 for a character device
 * @var device::d_blocksize size of one block in bytes; the unit of seek and of I/O on a block device
 * @var device::d_open      called on each open() of the device; returns 0 or a negative error
 * @var device::d_close     called on the last close() of the device; returns 0 or a negative error
 * @var device::d_io        performs one transfer described by the iobuf; write is true for a write
 * @var device::d_ioctl     device control call; op and data are device specific
 */
struct device {
    size_t d_blocks;
    size_t d_blocksize;
    int (*d_open)(struct device *dev, uint32_t open_flags);
    int (*d_close)(struct device *dev);
    int (*d_io)(struct device *dev, struct iobuf *iob, bool write);
    int (*d_ioctl)(struct device *dev, int op, void *data);
};

/**
 * @brief Call the open hook of a device.
 * @param dev        the device
 * @param open_flags O_* flags of the open() call (access mode and modifiers)
 */
#define dop_open(dev, open_flags) ((dev)->d_open(dev, open_flags))
/**
 * @brief Call the close hook of a device.
 * @param dev the device
 */
#define dop_close(dev) ((dev)->d_close(dev))
/**
 * @brief Call the I/O hook of a device: one read or write transfer described by an iobuf.
 * @param dev   the device
 * @param iob   the transfer; the hook advances its io_offset and io_resid
 * @param write true for a write, false for a read
 */
#define dop_io(dev, iob, write) ((dev)->d_io(dev, iob, write))
/**
 * @brief Call the control hook of a device.
 * @param dev  the device
 * @param op   device specific operation code
 * @param data device specific argument
 */
#define dop_ioctl(dev, op, data) ((dev)->d_ioctl(dev, op, data))

/**
 * @brief Register all built-in devices (stdin, stdout, disk0) in the VFS.
 *
 * Called once, by fs_init() during kern_init(), before interrupts are enabled.
 */
void dev_init(void);
/**
 * @brief Allocate a VFS inode of device type and bind it to the device operations (dev.c).
 *
 * The caller then fills in the struct device inside the inode (vop_info(node, device)).
 *
 * @return the new inode, or NULL if no memory is available
 */
struct inode *dev_create_inode(void);
/**
 * @brief Deliver one character of keyboard or console input to the stdin device.
 *
 * Called by the timer interrupt handler in trap.c, once per tick, with the result of
 * cons_getc(). QEMU raises no keyboard interrupt in this setup, so the polling there is the only
 * source of input. Character '\0' (what cons_getc() returns when nothing is waiting) is ignored.
 * Otherwise the character is appended to the stdin ring buffer, and a process sleeping in
 * read() on stdin is woken up. Implemented in dev_stdin.c. Safe to call from interrupt context.
 *
 * @param c the character read from the console
 */
void dev_stdin_write(char c);

#endif /* !__KERN_FS_DEVS_DEV_H__ */
