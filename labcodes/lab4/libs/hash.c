/**
 * @file   hash.c
 * @brief  Multiplicative hash function used for hash tables of fixed size 2^bits.
 */
#include <stdlib.h>

/**
 * @brief Golden-ratio-derived odd multiplier for 32-bit multiplicative hashing.
 *
 * The value is 2^31 + 2^29 - 2^25 + 2^22 - 2^19 - 2^16 + 1 (0x9e370001).
 */
/* 2^31 + 2^29 - 2^25 + 2^22 - 2^19 - 2^16 + 1 */
#define GOLDEN_RATIO_PRIME_32 0x9e370001UL

/**
 * @brief Hash a 32-bit value into the range [0, 2^bits - 1].
 *
 * @details Multiplies val by GOLDEN_RATIO_PRIME_32 (mod 2^32) and keeps the top @p bits bits.
 *          The high bits of the product depend on all input bits, so they are used.
 *
 * @param[in] val   The input value.
 * @param[in] bits  Number of bits in the return value, 1..32. 0 would shift by 32 (undefined).
 * @return A hash value in [0, 2^bits - 1].
 */
/* *
 * hash32 - generate a hash value in the range [0, 2^@bits - 1]
 * @val:    the input value
 * @bits:   the number of bits in a return value
 *
 * High bits are more random, so we use them.
 * */
uint32_t hash32(uint32_t val, unsigned int bits) {
    uint32_t hash = val * GOLDEN_RATIO_PRIME_32;
    return (hash >> (32 - bits));
}
