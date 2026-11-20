/**
 * @file   rand.c
 * @brief  Pseudo-random number generator: rand() and srand().
 *
 * This is a 48-bit linear congruential generator (multiplier 0x5DEECE66D, increment 0xB, modulus
 * 2^48), the same recurrence as java.util.Random. It is not cryptographically secure. The state
 * is global, so all callers share one sequence.
 */

#include <riscv.h>
#include <stdlib.h>

/** @brief Internal 48-bit generator state; the initial value is 1, i.e. the default seed. */
static unsigned long long next = 1;

/**
 * @brief Return a pseudo-random integer in the range [0, RAND_MAX].
 *
 * Advances the internal state by one step and returns bits 12..47 of the state reduced modulo
 * RAND_MAX + 1.
 *
 * @return A pseudo-random value in [0, RAND_MAX].
 * @note The sequence is fully determined by the seed; call srand() first for a different one.
 */
int rand(void) {
    next = (next * 0x5DEECE66DLL + 0xBLL) & ((1LL << 48) - 1);
    unsigned long long result = (next >> 12);
    return (int)do_div(result, RAND_MAX + 1);
}

/**
 * @brief Seed the pseudo-random number generator used by rand().
 *
 * @param[in] seed  The new seed. The same seed always gives the same sequence from rand().
 */
void srand(unsigned int seed) {
    next = seed;
}
