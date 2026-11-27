/**
 * @file   iobuf.c
 * @brief  Helpers that move data through a struct iobuf: iobuf_init(), iobuf_move(),
 *         iobuf_move_zeros() and iobuf_skip().
 *
 * These functions are the only code that changes the position of an iobuf. Every transfer in the
 * file system (SFS inode I/O, the disk0 device, the VFS device-name parser and the getcwd copy)
 * goes through them, so the io_base/io_offset/io_resid bookkeeping is the same everywhere.
 * stdin_io() in dev_stdin.c updates io_resid by itself and does not use these helpers.
 */
#include <defs.h>
#include <string.h>
#include <iobuf.h>
#include <error.h>
#include <assert.h>

/**
 * @brief Initialise an iobuf for a new transfer.
 *
 * Sets io_base to base, io_offset to offset, and both io_len and io_resid to len. Nothing has
 * been transferred yet.
 *
 * @param iob    the iobuf to initialise
 * @param base   buffer the transfer reads from or writes to
 * @param len    length of the buffer, and the number of bytes that may be transferred
 * @param offset position in the file or device where the transfer starts
 * @return iob
 */
struct iobuf *iobuf_init(struct iobuf *iob, void *base, size_t len, off_t offset) {
    iob->io_base = base;
    iob->io_offset = offset;
    iob->io_len = iob->io_resid = len;
    return iob;
}

/**
 * @brief Copy data between an iobuf's buffer and a memory area, advancing the iobuf.
 *
 * alen is min(io_resid, len). When alen > 0 the copy is done with memmove() and the iobuf is
 * advanced by alen (iobuf_skip()). The direction is chosen by m2b: if true, data is the source and
 * the iobuf buffer the destination (data -> buffer); if false, the iobuf buffer is the source
 * (buffer -> data). The function may be called repeatedly on the same iobuf to move more data
 * until io_resid reaches 0.
 *
 * @param iob     the iobuf whose buffer is one side of the copy
 * @param data    the other side of the copy
 * @param len     maximum number of bytes to copy
 * @param m2b     true: data -> iobuf buffer ("memory to buffer"); false: iobuf buffer -> data
 * @param copiedp [out] if not NULL, receives the number of bytes copied in this call (alen)
 * @retval 0         all len bytes were copied
 * @retval -E_NO_MEM the iobuf had less room than len, so only alen bytes were copied. The error
 *                   name is historical; the caller checks only for non-zero.
 */
int iobuf_move(struct iobuf *iob, void *data, size_t len, bool m2b, size_t *copiedp) {
    size_t alen;
    if ((alen = iob->io_resid) > len) {
        alen = len;
    }
    if (alen > 0) {
        void *src = iob->io_base, *dst = data;
        if (m2b) {
            void *tmp = src;
            src = dst, dst = tmp;
        }
        memmove(dst, src, alen);
        iobuf_skip(iob, alen), len -= alen;
    }
    if (copiedp != NULL) {
        *copiedp = alen;
    }
    return (len == 0) ? 0 : -E_NO_MEM;
}

/**
 * @brief Zero min(io_resid, len) bytes of the iobuf buffer, advancing the iobuf.
 *
 * Kept from the original ucore. No code in lab9 calls it.
 *
 * @param iob     the iobuf to zero
 * @param len     maximum number of bytes to zero
 * @param copiedp [out] if not NULL, receives the number of bytes zeroed in this call (alen)
 * @retval 0         all len bytes were zeroed
 * @retval -E_NO_MEM the iobuf had less room than len, so only alen bytes were zeroed
 */
int iobuf_move_zeros(struct iobuf *iob, size_t len, size_t *copiedp) {
    size_t alen;
    if ((alen = iob->io_resid) > len) {
        alen = len;
    }
    if (alen > 0) {
        memset(iob->io_base, 0, alen);
        iobuf_skip(iob, alen), len -= alen;
    }
    if (copiedp != NULL) {
        *copiedp = alen;
    }
    return (len == 0) ? 0 : -E_NO_MEM;
}

/**
 * @brief Skip n bytes of an iobuf: advance io_base and io_offset by n, decrease io_resid by n.
 *
 * Asserts that n is not larger than io_resid.
 *
 * @param iob the iobuf
 * @param n   number of bytes to skip
 */
void iobuf_skip(struct iobuf *iob, size_t n) {
    assert(iob->io_resid >= n);
    iob->io_base += n, iob->io_offset += n, iob->io_resid -= n;
}
