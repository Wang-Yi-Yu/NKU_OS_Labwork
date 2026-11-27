/**
 * @file   dev_stdin.c
 * @brief  The "stdin" device: a read-only character device fed from the console by the timer.
 *
 * Input goes into a ring buffer (stdin_buffer, STDIN_BUFSIZE bytes) by dev_stdin_write(), which the
 * timer interrupt calls once per tick with the result of cons_getc(). A read takes bytes from the
 * buffer. When the buffer is empty the reader goes to sleep on wait_queue with WT_KBD, and
 * dev_stdin_write() wakes it up when the next character arrives. A user program gets the device as
 * file descriptor 0.
 *
 * The ring buffer uses two free-running positions: p_rpos (next byte to read) and p_wpos (next
 * byte to write). Both are used modulo STDIN_BUFSIZE, and the buffer is empty when p_rpos == p_wpos.
 */
#include <defs.h>
#include <stdio.h>
#include <wait.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <dev.h>
#include <vfs.h>
#include <iobuf.h>
#include <inode.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>

/** @brief Size of the stdin ring buffer in bytes. */
#define STDIN_BUFSIZE 4096

/** @brief Ring buffer of console characters not yet read. Index is position % STDIN_BUFSIZE. */
static char stdin_buffer[STDIN_BUFSIZE];
/**
 * @brief Read position (p_rpos) and write position (p_wpos) of the ring buffer.
 *
 * Both are byte counts that only grow; they index stdin_buffer modulo STDIN_BUFSIZE. p_rpos is the
 * next byte dev_stdin_read() returns, and p_wpos is one past the last byte stored.
 */
static off_t p_rpos, p_wpos;
/**
 * @brief The wait queue of readers blocked on empty input (__wait_queue), and a pointer to it (wait_queue).
 */
static wait_queue_t __wait_queue, *wait_queue = &__wait_queue;

/**
 * @brief Store one console character and wake up a blocked reader.
 *
 * Called from the timer interrupt (trap.c) with cons_getc(). The character '\0' is ignored, since
 * cons_getc() returns it when no character is waiting. Otherwise the character is written at
 * p_wpos. p_wpos advances only while the buffer is not full. If a reader sleeps on wait_queue, one
 * of them is woken up with WT_KBD. Interrupts are disabled while the buffer is changed.
 *
 * @param c the character typed on the console, or '\0' for none
 * @note When the buffer is full the character is still written, to the slot at p_wpos, which is
 *       the same slot as the oldest unread byte at p_rpos. So the oldest unread byte is replaced by
 *       the new one and p_wpos does not move. Whether the newest byte should be dropped instead is
 *       not decided in the code; it is noted here, not changed.
 */
void dev_stdin_write(char c) {
    bool intr_flag;
    if (c != '\0') {
        local_intr_save(intr_flag);
        {
            stdin_buffer[p_wpos % STDIN_BUFSIZE] = c;
            if (p_wpos - p_rpos < STDIN_BUFSIZE) {
                p_wpos++;
            }
            if (!wait_queue_empty(wait_queue)) {
                wakeup_queue(wait_queue, WT_KBD, 1);
            }
        }
        local_intr_restore(intr_flag);
    }
}

/**
 * @brief Copy up to len bytes from the stdin buffer to buf, sleeping while there are none.
 *
 * Bytes are copied one by one, and p_rpos advances with each. When the buffer is empty, the reader
 * sleeps on wait_queue (state WT_KBD) and is woken up by dev_stdin_write(); it then tries again
 * for the same byte. The call returns when len bytes have been copied, or when the reader is woken
 * for another reason, in which case the bytes copied so far are returned (0 if there are none).
 *
 * Interrupts are disabled while the buffer is inspected and enabled while the process sleeps.
 *
 * @param buf destination buffer, at least len bytes
 * @param len maximum number of bytes to read
 * @return number of bytes copied, 0 .. len
 * @note The call does not return after the first character when the buffer runs dry: it keeps
 *       sleeping until len bytes are copied (or another kind of wake-up arrives). This is how the
 *       original ucore code behaves; it is not changed here.
 */
static int dev_stdin_read(char *buf, size_t len) {
    int ret = 0;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        for (; ret < len; ret++, p_rpos++) {
        try_again:
            if (p_rpos < p_wpos) {
                *buf++ = stdin_buffer[p_rpos % STDIN_BUFSIZE];
            } else {
                wait_t __wait, *wait = &__wait;
                wait_current_set(wait_queue, wait, WT_KBD);
                local_intr_restore(intr_flag);

                schedule();

                local_intr_save(intr_flag);
                wait_current_del(wait_queue, wait);
                if (wait->wakeup_flags == WT_KBD) {
                    goto try_again;
                }
                break;
            }
        }
    }
    local_intr_restore(intr_flag);
    return ret;
}

/**
 * @brief d_open hook of stdin: only O_RDONLY is accepted.
 *
 * @param dev        the stdin device (unused)
 * @param open_flags open flags; must be exactly O_RDONLY
 * @return 0 on success, -E_INVAL for any other flags
 */
static int stdin_open(struct device *dev, uint32_t open_flags) {
    if (open_flags != O_RDONLY) {
        return -E_INVAL;
    }
    return 0;
}

/**
 * @brief d_close hook of stdin: nothing to release.
 *
 * @param dev the stdin device (unused)
 * @return always 0
 */
static int stdin_close(struct device *dev) {
    return 0;
}

/**
 * @brief d_io hook of stdin: fill the buffer of a read from the console input.
 *
 * The read copies up to io_resid bytes with dev_stdin_read(), so it may sleep. On success io_resid
 * is reduced by the count.
 *
 * @note Unlike the other devices, io_base and io_offset are not advanced; only io_resid is updated.
 *       file_read() calls this once per iobuf, so the difference is not visible there. A second
 *       call with the same iobuf would start writing again at the original io_base.
 *
 * @param dev   the stdin device (unused)
 * @param iob   the buffer to fill
 * @param write must be false; a write returns -E_INVAL
 * @return the number of bytes read (0 or more) for a read, -E_INVAL for a write
 */
static int stdin_io(struct device *dev, struct iobuf *iob, bool write) {
    if (!write) {
        int ret;
        if ((ret = dev_stdin_read(iob->io_base, iob->io_resid)) > 0) {
            iob->io_resid -= ret;
        }
        return ret;
    }
    return -E_INVAL;
}

/**
 * @brief d_ioctl hook of stdin: no control operations are defined.
 *
 * @param dev  the stdin device (unused)
 * @param op   operation code (unused)
 * @param data argument (unused)
 * @return always -E_INVAL
 */
static int stdin_ioctl(struct device *dev, int op, void *data) {
    return -E_INVAL;
}

/**
 * @brief Fill in the struct device of stdin and reset the ring buffer and its wait queue.
 *
 * stdin is a character device (d_blocks = 0, d_blocksize = 1). Both positions start at 0.
 *
 * @param dev the device inside the freshly created inode
 */
static void stdin_device_init(struct device *dev) {
    dev->d_blocks = 0;
    dev->d_blocksize = 1;
    dev->d_open = stdin_open;
    dev->d_close = stdin_close;
    dev->d_io = stdin_io;
    dev->d_ioctl = stdin_ioctl;

    p_rpos = p_wpos = 0;
    wait_queue_init(wait_queue);
}

/**
 * @brief Create the stdin device inode and register it with the VFS under the name "stdin".
 *
 * Called by dev_init(). Panics if the inode cannot be allocated or the registration fails.
 */
void dev_init_stdin(void) {
    struct inode *node;
    if ((node = dev_create_inode()) == NULL) {
        panic("stdin: dev_create_node.\n");
    }
    stdin_device_init(vop_info(node, device));

    int ret;
    if ((ret = vfs_add_dev("stdin", node, 0)) != 0) {
        panic("stdin: vfs_add_dev: %e.\n", ret);
    }
}
