/**
 * @file   defs.h
 * @brief  Basic kernel-wide type definitions and helper macros (fixed-width integers, rounding, offsets).
 *
 * Included by almost every kernel and library source file. It defines the fixed-width integer types,
 * bool, size_t and the address types, plus macros for rounding, struct member offsets and
 * the container_of-style to_struct, together with attribute shorthands such as __noreturn.
 */

#ifndef __LIBS_DEFS_H__
#define __LIBS_DEFS_H__

/** @brief Null pointer constant, defined only if not already defined. */
#ifndef NULL
#define NULL ((void *)0)
#endif

/** @brief Force inlining of a function (GCC attribute). */
#define __always_inline inline __attribute__((always_inline))
/** @brief Forbid inlining of a function (GCC attribute). */
#define __noinline __attribute__((noinline))
/** @brief Declare that a function never returns (GCC attribute), e.g. __panic. */
#define __noreturn __attribute__((noreturn))

/** @brief Boolean type: 0 is false, non-zero is true. */
/* Represents true-or-false values */
typedef int bool;

/* Explicitly-sized versions of integer types */
/** @name Fixed-width integer types. @{ */
typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
/** @} */
/** @brief Native unsigned and signed integer types, chosen by the RISC-V XLEN (64 or 32 bits). */
#if __riscv_xlen == 64
typedef uint64_t uint_t;
typedef int64_t sint_t;
#elif __riscv_xlen == 32
typedef uint32_t uint_t;
typedef int32_t sint_t;
#endif

/* *
 * Pointers and addresses are 32 bits long.
 * We use pointer types to represent addresses,
 * uintptr_t to represent the numerical values of addresses.
 * */
/** @brief Signed integer big enough to hold a pointer value. */
typedef sint_t intptr_t;
/** @brief Unsigned integer big enough to hold a pointer value; used for address arithmetic. */
typedef uint_t uintptr_t;

/* size_t is used for memory object sizes */
/** @brief Unsigned type used for memory object sizes and counts. */
typedef uintptr_t size_t;

/* used for page numbers */
/** @brief Physical or virtual page number. */
typedef size_t ppn_t;

/* *
 * Rounding operations (efficient when n is a power of 2)
 * Round down to the nearest multiple of n
 * */
/**
 * @brief Round @p a down to the nearest multiple of @p n.
 *
 * Efficient when @p n is a power of two. @p a is evaluated once; the result has the same type as @p a.
 *
 * @param[in] a  Value to round.
 * @param[in] n  Alignment, normally a power of two.
 * @return The largest multiple of @p n that is not greater than @p a.
 */
#define ROUNDDOWN(a, n)               \
    ({                                \
        size_t __a = (size_t)(a);     \
        (typeof(a))(__a - __a % (n)); \
    })

/**
 * @brief Round @p a up to the nearest multiple of @p n.
 *
 * Efficient when @p n is a power of two. The result has the same type as @p a.
 *
 * @param[in] a  Value to round.
 * @param[in] n  Alignment, normally a power of two.
 * @return The smallest multiple of @p n that is not less than @p a.
 */
/* Round up to the nearest multiple of n */
#define ROUNDUP(a, n)                                       \
    ({                                                      \
        size_t __n = (size_t)(n);                           \
        (typeof(a))(ROUNDDOWN((size_t)(a) + __n - 1, __n)); \
    })

/**
 * @brief Byte offset of a member inside a struct type.
 * @param[in] type    Struct type name.
 * @param[in] member  Name of the member.
 * @return The offset in bytes, as size_t.
 */
/* Return the offset of 'member' relative to the beginning of a struct type */
#define offsetof(type, member) ((size_t)(&((type *)0)->member))

/* *
 * to_struct - get the struct from a ptr
 * @ptr:    a struct pointer of member
 * @type:   the type of the struct this is embedded in
 * @member: the name of the member within the struct
 * */
/**
 * @brief Recover the enclosing struct from a pointer to one of its members (container_of).
 *
 * @param[in] ptr     Pointer to the member.
 * @param[in] type    Type of the enclosing struct.
 * @param[in] member  Name of the member inside the struct.
 * @return Pointer to the enclosing struct of type @p type.
 */
#define to_struct(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

#endif /* !__LIBS_DEFS_H__ */
