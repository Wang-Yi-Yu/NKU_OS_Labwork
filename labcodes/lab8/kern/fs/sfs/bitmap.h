/**
 * @file   bitmap.h
 * @brief  Fixed-size bit arrays used by SFS to keep the free-block map.
 *
 * A bitmap stores nbits bits in an array of 32-bit words. The SFS free map uses it with the
 * convention "1 = block is free, 0 = block is in use": bitmap_create() starts with every bit set
 * and clears the padding bits past nbits, bitmap_alloc() takes a set bit and clears it, and
 * bitmap_free() sets it again. The bitmap has no locking of its own; the SFS code calls it while
 * holding the fs semaphore (see sfs.h).
 */
#ifndef __KERN_FS_SFS_BITMAP_H__
#define __KERN_FS_SFS_BITMAP_H__

#include <defs.h>

/**
 * @brief Opaque bit array. Its fields are private to bitmap.c.
 */
struct bitmap;

/**
 * @brief Allocate a bitmap with nbits bits, all of them set (free).
 *
 * The bits past nbits in the last word are cleared, so they can never be allocated.
 * @param nbits number of bits; must not be 0
 * @return the new bitmap, or NULL if memory for it is not available
 */
struct bitmap *bitmap_create(uint32_t nbits);

/**
 * @brief Find a set bit, clear it and return its index.
 *
 * The search goes from index 0 upwards, so the lowest free index is returned.
 * @param bitmap      bitmap to allocate from
 * @param index_store [out] index of the bit that was taken
 * @return 0 on success, -E_NO_MEM if every bit is already clear (no free bit)
 */
int bitmap_alloc(struct bitmap *bitmap, uint32_t *index_store);

/**
 * @brief Return whether the bit at index is set.
 * @param bitmap bitmap to query
 * @param index  bit index, must be less than the nbits given to bitmap_create()
 * @return true if the bit is set (free in the SFS convention), false if it is clear (in use)
 */
bool bitmap_test(struct bitmap *bitmap, uint32_t index);

/**
 * @brief Set the bit at index again (give the block back).
 *
 * The bit must be clear before the call: a double free trips an assertion.
 * @param bitmap bitmap to update
 * @param index  bit index, must be less than the nbits given to bitmap_create()
 */
void bitmap_free(struct bitmap *bitmap, uint32_t index);

/**
 * @brief Free a bitmap and its word array.
 * @param bitmap bitmap to destroy; it must not be used afterwards
 */
void bitmap_destroy(struct bitmap *bitmap);

/**
 * @brief Return the raw word array of the bitmap, for writing it to disk.
 * @param bitmap    bitmap whose data is wanted
 * @param len_store [out] if not NULL, receives the size of the data in bytes (a multiple of 4)
 * @return pointer to the first word; it stays valid until bitmap_destroy()
 */
void *bitmap_getdata(struct bitmap *bitmap, size_t *len_store);

#endif /* !__KERN_FS_SFS_BITMAP_H__ */
