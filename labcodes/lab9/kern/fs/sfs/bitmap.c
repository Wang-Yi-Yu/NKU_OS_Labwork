/**
 * @file   bitmap.c
 * @brief  Implementation of the bit arrays of bitmap.h (the SFS free-block map).
 *
 * Bits are stored in uint32_t words, bit i being bit (i % 32) of word (i / 32). Set bit = free,
 * clear bit = in use. The functions do no locking and no I/O: sfs_init_freemap() and
 * sfs_sync_freemap() in the SFS code copy the words to and from disk through bitmap_getdata().
 */
#include <defs.h>
#include <string.h>
#include <bitmap.h>
#include <kmalloc.h>
#include <error.h>
#include <assert.h>

/** @brief Type of one word of the bit array. */
#define WORD_TYPE uint32_t
/** @brief Number of bits in one WORD_TYPE. */
#define WORD_BITS (sizeof(WORD_TYPE) * CHAR_BIT)

/**
 * @brief The bit array (definition of the opaque struct bitmap).
 * @var bitmap::nbits  number of valid bits; indices must be below this value
 * @var bitmap::nwords number of words in map, ROUNDUP_DIV(nbits, WORD_BITS)
 * @var bitmap::map    the words; bit i is bit (i % WORD_BITS) of map[i / WORD_BITS]
 */
struct bitmap {
    uint32_t nbits;
    uint32_t nwords;
    WORD_TYPE *map;
};

/**
 * @brief Allocate a bitmap of nbits bits with all bits set (free).
 *
 * The bitmap struct and its word array are allocated separately. Bits past nbits in the last
 * word are cleared so that bitmap_alloc() can never return them.
 *
 * @param nbits number of bits; asserted to be non-zero and not to overflow when rounded up
 * @return the new bitmap, or NULL if kmalloc() fails (nothing is leaked in that case)
 */
struct bitmap *bitmap_create(uint32_t nbits) {
    static_assert(WORD_BITS != 0);
    assert(nbits != 0 && nbits + WORD_BITS > nbits);

    struct bitmap *bitmap;
    if ((bitmap = kmalloc(sizeof(struct bitmap))) == NULL) {
        return NULL;
    }

    uint32_t nwords = ROUNDUP_DIV(nbits, WORD_BITS);
    WORD_TYPE *map;
    if ((map = kmalloc(sizeof(WORD_TYPE) * nwords)) == NULL) {
        kfree(bitmap);
        return NULL;
    }

    bitmap->nbits = nbits, bitmap->nwords = nwords;
    bitmap->map = memset(map, 0xFF, sizeof(WORD_TYPE) * nwords);

    /* mark any leftover bits at the end in use(0) */
    if (nbits != nwords * WORD_BITS) {
        uint32_t ix = nwords - 1, overbits = nbits - ix * WORD_BITS;

        assert(nbits / WORD_BITS == ix);
        assert(overbits > 0 && overbits < WORD_BITS);

        for (; overbits < WORD_BITS; overbits++) {
            bitmap->map[ix] ^= (1 << overbits);
        }
    }
    return bitmap;
}

/**
 * @brief Take the lowest set bit: clear it and report its index.
 *
 * Words are scanned from the first one; inside a non-zero word the lowest set bit is cleared.
 * The word must contain a set bit when map[ix] != 0, otherwise the assertion fires.
 *
 * @param bitmap      bitmap to allocate from
 * @param index_store [out] receives the index of the bit that was cleared
 * @return 0 on success, -E_NO_MEM if no bit is set
 */
int bitmap_alloc(struct bitmap *bitmap, uint32_t *index_store) {
    WORD_TYPE *map = bitmap->map;
    uint32_t ix, offset, nwords = bitmap->nwords;
    for (ix = 0; ix < nwords; ix++) {
        if (map[ix] != 0) {
            for (offset = 0; offset < WORD_BITS; offset++) {
                WORD_TYPE mask = (1 << offset);
                if (map[ix] & mask) {
                    map[ix] ^= mask;
                    *index_store = ix * WORD_BITS + offset;
                    return 0;
                }
            }
            assert(0);
        }
    }
    return -E_NO_MEM;
}

/**
 * @brief Compute the word and the mask that address bit index.
 *
 * @param bitmap bitmap the index refers to (index is asserted to be below nbits)
 * @param index  bit index
 * @param word   [out] pointer to the word that holds the bit
 * @param mask   [out] mask with only the bit of index set inside *word
 */
static void bitmap_translate(struct bitmap *bitmap, uint32_t index, WORD_TYPE **word,
                             WORD_TYPE *mask) {
    assert(index < bitmap->nbits);
    uint32_t ix = index / WORD_BITS, offset = index % WORD_BITS;
    *word = bitmap->map + ix;
    *mask = (1 << offset);
}

/**
 * @brief Return whether the bit at index is set.
 * @param bitmap bitmap to query
 * @param index  bit index, below the nbits given to bitmap_create()
 * @return true if the bit is set (free), false if it is clear (in use)
 */
bool bitmap_test(struct bitmap *bitmap, uint32_t index) {
    WORD_TYPE *word, mask;
    bitmap_translate(bitmap, index, &word, &mask);
    return (*word & mask);
}

/**
 * @brief Set a clear bit again (mark the index free).
 *
 * The bit must be clear before the call (asserted), so freeing an index twice is caught.
 * @param bitmap bitmap to update
 * @param index  bit index, below the nbits given to bitmap_create()
 */
void bitmap_free(struct bitmap *bitmap, uint32_t index) {
    WORD_TYPE *word, mask;
    bitmap_translate(bitmap, index, &word, &mask);
    assert(!(*word & mask));
    *word |= mask;
}

/**
 * @brief Free the word array and the struct of a bitmap.
 * @param bitmap bitmap created by bitmap_create(); it must not be used afterwards
 */
void bitmap_destroy(struct bitmap *bitmap) {
    kfree(bitmap->map);
    kfree(bitmap);
}

/**
 * @brief Return the raw word array of a bitmap (used to read and write it to disk).
 * @param bitmap    bitmap whose data is wanted
 * @param len_store [out] if not NULL, receives the length of the data in bytes
 * @return pointer to bitmap->map; the length is nwords * sizeof(uint32_t), i.e. whole words,
 *         which can cover more bits than nbits
 */
void *bitmap_getdata(struct bitmap *bitmap, size_t *len_store) {
    if (len_store != NULL) {
        *len_store = sizeof(WORD_TYPE) * bitmap->nwords;
    }
    return bitmap->map;
}
