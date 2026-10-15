/**
 * @file   defs.h
 * @brief  Basic kernel types and generic macro definitions.
 *
 * Provides fixed-width integer types, address and page-number types, and
 * macros for rounding, member offset computation and recovering an enclosing
 * structure pointer from a member pointer. Almost every kernel source file
 * includes this header.
 */

#ifndef __LIBS_DEFS_H__
#define __LIBS_DEFS_H__

#ifndef NULL
/**
 * @brief Null pointer constant.
 */
#define NULL ((void *)0)
#endif

/**
 * @brief Force inline expansion of a function (applied to function definitions).
 */
#define __always_inline inline __attribute__((always_inline))

/**
 * @brief Prevent the compiler from inlining a function, e.g. for debugging or to control code size.
 */
#define __noinline __attribute__((noinline))

/**
 * @brief Mark a function as never returning (e.g. __panic), so the compiler can optimise and suppress related warnings.
 */
#define __noreturn __attribute__((noreturn))

/**
 * @brief Boolean type: 0 means false, any non-zero value means true.
 */
typedef int bool;

/*
 * Fixed-width integer types. The following typedefs share the names of the
 * C99 <stdint.h> types, with widths explicitly specified for RV64.
 */

/**
 * @brief 8-bit signed integer.
 */
typedef char int8_t;

/**
 * @brief 8-bit unsigned integer.
 */
typedef unsigned char uint8_t;

/**
 * @brief 16-bit signed integer.
 */
typedef short int16_t;

/**
 * @brief 16-bit unsigned integer.
 */
typedef unsigned short uint16_t;

/**
 * @brief 32-bit signed integer.
 */
typedef int int32_t;

/**
 * @brief 32-bit unsigned integer.
 */
typedef unsigned int uint32_t;

/**
 * @brief 64-bit signed integer.
 */
typedef long long int64_t;

/**
 * @brief 64-bit unsigned integer.
 */
typedef unsigned long long uint64_t;

/*
 * Fast integer types: at least the given width; the compiler picks the fastest
 * type that satisfies it.
 */

/**
 * @brief Fast signed integer of at least 8 bits.
 */
typedef signed char int_fast8_t;

/**
 * @brief Fast signed integer of at least 16 bits.
 */
typedef short int_fast16_t;

/**
 * @brief Fast signed integer of at least 32 bits (long on RV64, i.e. 64 bits).
 */
typedef long int_fast32_t;

/**
 * @brief Fast signed integer of at least 64 bits.
 */
typedef long long int_fast64_t;

/**
 * @brief Fast unsigned integer of at least 8 bits.
 */
typedef unsigned char uint_fast8_t;

/**
 * @brief Fast unsigned integer of at least 16 bits.
 */
typedef unsigned short uint_fast16_t;

/**
 * @brief Fast unsigned integer of at least 32 bits (unsigned long on RV64, i.e. 64 bits).
 */
typedef unsigned long uint_fast32_t;

/**
 * @brief Fast unsigned integer of at least 64 bits.
 */
typedef unsigned long long uint_fast64_t;

/**
 * @brief Signed integer as wide as a pointer, able to hold a pointer value without loss.
 *
 * Pointers and addresses are 64 bits wide. Addresses are represented with
 * pointer types; when arithmetic is needed, the numeric value is carried in
 * uintptr_t.
 */
typedef int64_t intptr_t;

/**
 * @brief Unsigned integer as wide as a pointer, used to represent addresses as numbers.
 */
typedef uint64_t uintptr_t;

/**
 * @brief Type for memory object sizes, same width as uintptr_t.
 */
typedef uintptr_t size_t;

/**
 * @brief Physical page number (PPN) type.
 *
 * Usually obtained by shifting a physical address right by the page offset
 * width (PGSHIFT).
 */
typedef size_t ppn_t;

/**
 * @brief Round @p a down to the nearest multiple of @p n.
 *
 * The result has the same type as @p a (including pointer types). When @p n
 * is a power of two, the modulo is optimised to a bitwise AND by the compiler.
 *
 * @param[in] a  Integer or pointer value to round.
 * @param[in] n  Rounding granularity, must be positive.
 * @return The largest multiple of @p n that is not greater than @p a.
 * @note Both @p a and @p n are evaluated exactly once, so side-effecting
 *       expressions are safe to pass.
 *
 * @code
 *   size_t base = ROUNDDOWN(0x80201234, 4096);  // yields 0x80201000
 * @endcode
 */
#define ROUNDDOWN(a, n)               \
    ({                                \
        size_t __a = (size_t)(a);     \
        (typeof(a))(__a - __a % (n)); \
    })

/**
 * @brief Round @p a up to the nearest multiple of @p n.
 *
 * Implemented as ROUNDDOWN(a + n - 1, n). The result has the same type as @p a.
 *
 * @param[in] a  Integer or pointer value to round.
 * @param[in] n  Rounding granularity, must be positive.
 * @return The smallest multiple of @p n that is not less than @p a.
 * @note Both @p a and @p n are evaluated exactly once. Adding @p n - 1 may
 *       overflow, so the caller must ensure @p a is well below the maximum
 *       value of size_t.
 *
 * @code
 *   size_t size = ROUNDUP(12345, 4096);  // yields 16384
 * @endcode
 */
#define ROUNDUP(a, n)                                       \
    ({                                                      \
        size_t __n = (size_t)(n);                           \
        (typeof(a))(ROUNDDOWN((size_t)(a) + __n - 1, __n)); \
    })

/**
 * @brief Byte offset of member @p member from the start of structure type @p type.
 *
 * Uses the null-pointer trick: only the address of the member is computed and
 * no memory is accessed.
 *
 * @param[in] type    Structure type name.
 * @param[in] member  Direct member name of the structure (nested paths are not supported).
 * @return The offset, of type size_t.
 * @note This is the kernel's own definition and is unrelated to offsetof from the
 *       standard <stddef.h>. It relies on GCC/Clang handling of the address
 *       computation through a null pointer.
 */
#define offsetof(type, member) ((size_t)(&((type *)0)->member))

/**
 * @brief Recover a pointer to the enclosing structure from a pointer to one of its members (container_of semantics).
 *
 * Used for intrusive data structures: for example, obtaining the structure
 * that contains a list node. It serves the same purpose as le2page in memlayout.h.
 *
 * @param[in] ptr     Pointer to member @p member inside the enclosing structure;
 *                    it must really point to that member.
 * @param[in] type    Name of the enclosing structure type.
 * @param[in] member  Name of the member within the enclosing structure.
 * @return Pointer to the enclosing structure, of type @p type *.
 * @note @p ptr is evaluated exactly once.
 *
 * @code
 *   list_entry_t *le = list_next(&free_list);
 *   struct Page *p = to_struct(le, struct Page, page_link);
 * @endcode
 */
#define to_struct(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

#endif /* !__LIBS_DEFS_H__ */
