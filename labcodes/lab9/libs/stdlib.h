/**
 * @file   stdlib.h
 * @brief  General utility declarations: the RAND_MAX limit, rand/srand and hash32.
 *
 * rand and srand are implemented in libs/rand.c, hash32 in libs/hash.c.
 */

#ifndef __LIBS_STDLIB_H__
#define __LIBS_STDLIB_H__

#include <defs.h>

/** @brief The largest number rand() will return. */
#define RAND_MAX 2147483647UL

/**
 * @brief Return a pseudo-random integer in [0, RAND_MAX].
 * @return A pseudo-random value.
 * @note Defined in libs/rand.c.
 */
int rand(void);
/**
 * @brief Seed the pseudo-random number generator used by rand().
 * @param[in] seed  New seed value.
 * @note Defined in libs/rand.c.
 */
void srand(unsigned int seed);

/**
 * @brief Hash a 32-bit value into a range of 2^bits values; see libs/hash.c.
 * @param[in] val   Input value.
 * @param[in] bits  Number of bits in the result, 1..32.
 * @return A value in [0, 2^bits - 1].
 */
uint32_t hash32(uint32_t val, unsigned int bits);

#endif /* !__LIBS_RAND_H__ */
