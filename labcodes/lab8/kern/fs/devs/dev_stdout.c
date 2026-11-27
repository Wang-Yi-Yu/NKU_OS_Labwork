/**
 * @file   dev_stdout.c
 * @brief  The "stdout" device: a write-only character device that prints to the console.
 *
 * Every byte written to stdout is passed to cputchar(), so it appears on the console, which is
 * the serial line in QEMU. User programs get it as file descriptor 1: umain() opens "stdout:"
 * there before main() runs. stdout is a character device (d_blocks = 0) and does not support
 * seeks. Reads are refused.
 */
#include <defs.h>
#include <stdio.h>
#include <dev.h>
#include <vfs.h>
#include <iobuf.h>
#include <inode.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>

/**
 * @brief d_open hook of stdout: only O_WRONLY is accepted.
 *
 * @param dev        the stdout device (unused)
 * @param open_flags open flags; must be exactly O_WRONLY
 * @return 0 on success, -E_INVAL for any other flags
 */
static int stdout_open(struct device *dev, uint32_t open_flags) {
    if (open_flags != O_WRONLY) {
        return -E_INVAL;
    }
    return 0;
}

/**
 * @brief d_close hook of stdout: nothing to release.
 *
 * @param dev the stdout device (unused)
 * @return always 0
 */
static int stdout_close(struct device *dev) {
    return 0;
}

/**
 * @brief d_io hook of stdout: print the buffer on the console.
 *
 * For a write, each byte of iob->io_base is printed with cputchar() and io_resid is counted down
 * to 0. The whole buffer is always consumed, so io_offset does not move. A read returns -E_INVAL.
 *
 * @param dev   the stdout device (unused)
 * @param iob   the bytes to print; io_resid is set to 0 on success
 * @param write must be true; false is refused
 * @return 0 for a write, -E_INVAL for a read
 */
static int stdout_io(struct device *dev, struct iobuf *iob, bool write) {
    if (write) {
        char *data = iob->io_base;
        for (; iob->io_resid != 0; iob->io_resid--) {
            cputchar(*data++);
        }
        return 0;
    }
    return -E_INVAL;
}

/**
 * @brief d_ioctl hook of stdout: no control operations are defined.
 *
 * @param dev  the stdout device (unused)
 * @param op   operation code (unused)
 * @param data argument (unused)
 * @return always -E_INVAL
 */
static int stdout_ioctl(struct device *dev, int op, void *data) {
    return -E_INVAL;
}

/**
 * @brief Fill in the struct device of stdout: a character device with the four hooks above.
 *
 * @param dev the device inside the freshly created inode
 */
static void stdout_device_init(struct device *dev) {
    dev->d_blocks = 0;
    dev->d_blocksize = 1;
    dev->d_open = stdout_open;
    dev->d_close = stdout_close;
    dev->d_io = stdout_io;
    dev->d_ioctl = stdout_ioctl;
}

/**
 * @brief Create the stdout device inode and register it with the VFS under the name "stdout".
 *
 * Called by dev_init(). Panics if the inode cannot be allocated or vfs_add_dev() fails.
 */
void dev_init_stdout(void) {
    struct inode *node;
    if ((node = dev_create_inode()) == NULL) {
        panic("stdout: dev_create_node.\n");
    }
    stdout_device_init(vop_info(node, device));

    int ret;
    if ((ret = vfs_add_dev("stdout", node, 0)) != 0) {
        panic("stdout: vfs_add_dev: %e.\n", ret);
    }
}
