/**
 * @file   iobuf.h
 * @brief  I/O buffer descriptor: the state of one read or write request as it moves through the layers.
 *
 * A struct iobuf says where a transfer goes (io_base), where it is in the file or device (io_offset),
 * how long the buffer is (io_len) and how many bytes are still to be moved (io_resid). The same
 * iobuf is passed down the call chain file_read()/file_write() -> vop_read()/vop_write() ->
 * sfs_io() or dop_io(). Each layer moves data with the iobuf_* helpers and so advances the state;
 * the number of bytes done is what the caller gets back, computed by iobuf_used().
 *
 * Note that a few callers (for example stdin_io() in dev_stdin.c) update only some of the fields.
 */
#ifndef __KERN_FS_IOBUF_H__
#define __KERN_FS_IOBUF_H__

#include <defs.h>

/**
 * @brief Read/write state of one transfer.
 *
 * @var iobuf::io_base   base address of the buffer used for the transfer. Advanced by iobuf_skip()
 *                       and the iobuf_move* functions, so it points at the next free byte
 * @var iobuf::io_offset position in the file or device where the next byte goes. Advanced together
 *                       with io_base
 * @var iobuf::io_len    total length of the buffer in bytes. It does not change during the transfer
 * @var iobuf::io_resid  number of bytes still to be transferred. Decreased by every move; 0 when the
 *                       transfer is complete
 */
struct iobuf {
    void *io_base;
    off_t io_offset;
    size_t io_len;
    size_t io_resid;
};

/**
 * @brief Number of bytes already transferred through an iobuf: io_len - io_resid.
 * @param iob the iobuf (expression, evaluated more than once)
 */
#define iobuf_used(iob) ((size_t)((iob)->io_len - (iob)->io_resid))

/**
 * @brief Initialise an iobuf for a new transfer.
 *
 * Sets io_base to base, io_offset to offset, and io_len and io_resid to len (nothing has moved yet).
 *
 * @param iob    the iobuf to initialise (usually a local variable)
 * @param base   buffer the transfer reads from or writes to
 * @param len    length of the buffer and the number of bytes to transfer
 * @param offset position in the file or device where the transfer starts
 * @return iob
 */
struct iobuf *iobuf_init(struct iobuf *iob, void *base, size_t len, off_t offset);
/**
 * @brief Copy data between an iobuf's buffer and a memory area, advancing the iobuf.
 *
 * Copies min(io_resid, len) bytes. The copy is in one direction or the other, chosen by m2b:
 * if m2b is true, from data into the iobuf buffer; if false, from the iobuf buffer into data.
 * The copy uses memmove(), so the two areas may overlap. The iobuf is advanced by the number of
 * bytes copied (iobuf_skip()). It may be called repeatedly on the same iobuf until io_resid is 0.
 *
 * @param iob    the iobuf whose buffer is one side of the copy
 * @param data   the other side of the copy
 * @param len    maximum number of bytes to copy
 * @param m2b    true: copy data -> iobuf buffer; false: copy iobuf buffer -> data
 * @param copiedp [out] if not NULL, receives the number of bytes copied in this call
 * @retval 0           all len bytes were copied
 * @retval -E_NO_MEM   fewer than len bytes were copied because the iobuf had less left (the error
 *                     name is historical; it means "short transfer")
 */
int iobuf_move(struct iobuf *iob, void *data, size_t len, bool m2b, size_t *copiedp);
/**
 * @brief Fill the iobuf buffer with zero bytes, advancing the iobuf.
 *
 * Zeroes min(io_resid, len) bytes at io_base and advances the iobuf by that amount.
 *
 * @param iob     the iobuf to fill
 * @param len     maximum number of bytes to zero
 * @param copiedp [out] if not NULL, receives the number of bytes zeroed in this call
 * @retval 0           all len bytes were zeroed
 * @retval -E_NO_MEM   fewer than len bytes were zeroed because the iobuf had less left
 */
int iobuf_move_zeros(struct iobuf *iob, size_t len, size_t *copiedp);
/**
 * @brief Skip n bytes of the iobuf: advance io_base and io_offset by n and decrease io_resid by n.
 *
 * Asserts that at least n bytes are left.
 *
 * @param iob the iobuf
 * @param n   number of bytes to skip, at most io_resid
 */
void iobuf_skip(struct iobuf *iob, size_t n);

#endif /* !__KERN_FS_IOBUF_H__ */
