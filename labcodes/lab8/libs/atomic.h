/**
 * @file   atomic.h
 * @brief  Atomic single-bit operations built on the RISC-V AMO instructions (A extension).
 *
 * Used for the flags word of struct Page (see memlayout.h), where a read-modify-write must not
 * be torn by a trap handler. The bit number may exceed the word size: the word holding the bit
 * is addr[BIT_WORD(nr)] and the bit inside it is BIT_MASK(nr). bool and the other basic types come
 * from defs.h, so include that first.
 */
#ifndef __LIBS_ATOMIC_H__
#define __LIBS_ATOMIC_H__

static inline void set_bit(int nr, volatile void *addr) __attribute__((always_inline));
static inline void clear_bit(int nr, volatile void *addr) __attribute__((always_inline));
static inline void change_bit(int nr, volatile void *addr) __attribute__((always_inline));
static inline bool test_bit(int nr, volatile void *addr) __attribute__((always_inline));
static inline bool test_and_set_bit(int nr, volatile void *addr) __attribute__((always_inline));
static inline bool test_and_clear_bit(int nr, volatile void *addr) __attribute__((always_inline));

/** @brief Number of bits of a machine word (XLEN). */
#define BITS_PER_LONG __riscv_xlen

#if (BITS_PER_LONG == 64)
/** @brief Name of the 64-bit AMO instruction `amo<op>.d`. */
#define __AMO(op) "amo" #op ".d"
#elif (BITS_PER_LONG == 32)
#define __AMO(op) "amo" #op ".w"
#else
#error "Unexpected BITS_PER_LONG"
#endif

/** @brief Mask of bit @p nr inside its word. */
#define BIT_MASK(nr) (1UL << ((nr) % BITS_PER_LONG))
/** @brief Index of the word that contains bit @p nr. */
#define BIT_WORD(nr) ((nr) / BITS_PER_LONG)

/**
 * @brief Atomically apply `amo<op>` with the mask modified by @p mod and return the old value of
 *        the bit (statement expression).
 */
#define __test_and_op_bit(op, mod, nr, addr)                         \
    ({                                                               \
        unsigned long __res, __mask;                                 \
        __mask = BIT_MASK(nr);                                       \
        __asm__ __volatile__(__AMO(op) " %0, %2, %1"                 \
                             : "=r"(__res), "+A"(addr[BIT_WORD(nr)]) \
                             : "r"(mod(__mask)));                    \
        ((__res & __mask) != 0);                                     \
    })

/** @brief Atomically apply `amo<op>` to the word of bit @p nr, discarding the old value (rd = zero). */
#define __op_bit(op, mod, nr, addr)                 \
    __asm__ __volatile__(__AMO(op) " zero, %1, %0"  \
                         : "+A"(addr[BIT_WORD(nr)]) \
                         : "r"(mod(BIT_MASK(nr))))

/** @name Bitmask modifiers
 *  @{ */
/** @brief Use the mask as is (for or/xor). */
#define __NOP(x) (x)
/** @brief Invert the mask (for and, to clear a bit). */
#define __NOT(x) (~(x))
/** @} */

/**
 * @brief Atomically set a bit in memory (amoor).
 *
 * @p nr may be almost arbitrarily large; the function is not restricted to a single word.
 *
 * @param nr   the bit to set
 * @param addr the address to start counting from
 */
static inline void set_bit(int nr, volatile void *addr) {
    __op_bit(or, __NOP, nr, ((volatile unsigned long *)addr));
}

/**
 * @brief Atomically clear a bit in memory (amoand with the inverted mask).
 * @param nr   the bit to clear
 * @param addr the address to start counting from
 */
static inline void clear_bit(int nr, volatile void *addr) {
    __op_bit(and, __NOT, nr, ((volatile unsigned long *)addr));
}

/**
 * @brief Atomically toggle a bit in memory (amoxor).
 * @param nr   the bit to change
 * @param addr the address to start counting from
 */
static inline void change_bit(int nr, volatile void *addr) {
    __op_bit(xor, __NOP, nr, ((volatile unsigned long *)addr));
}

/**
 * @brief Determine whether a bit is set.
 * @note A plain load, which is already atomic for an aligned word.
 * @param nr   the bit to test
 * @param addr the address to count from
 * @return 1 if the bit is set, 0 otherwise
 */
static inline bool test_bit(int nr, volatile void *addr) {
    return (((*(volatile unsigned long *)addr) >> nr) & 1);
}

/**
 * @brief Atomically set a bit and return its old value.
 * @param nr   the bit to set
 * @param addr the address to count from
 * @return the old value of the bit
 */
static inline bool test_and_set_bit(int nr, volatile void *addr) {
    return __test_and_op_bit(or, __NOP, nr, ((volatile unsigned long *)addr));
}

/**
 * @brief Atomically clear a bit and return its old value.
 * @param nr   the bit to clear
 * @param addr the address to count from
 * @return the old value of the bit
 */
static inline bool test_and_clear_bit(int nr, volatile void *addr) {
    return __test_and_op_bit(and, __NOT, nr, ((volatile unsigned long *)addr));
}

#endif /* !__LIBS_ATOMIC_H__ */
