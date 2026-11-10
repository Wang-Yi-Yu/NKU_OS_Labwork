/**
 * @file   assert.h
 * @brief  Kernel debugging macros: warn, panic, assert and static_assert.
 *
 * Declares the __warn and __panic functions and the macros that wrap them.
 * The macros pass __FILE__ and __LINE__ automatically so that every message
 * shows the call site.
 */

#ifndef __KERN_DEBUG_ASSERT_H__
#define __KERN_DEBUG_ASSERT_H__

#include <defs.h>

/**
 * @brief Print a kernel warning and continue execution (called by the warn macro).
 *
 * The output is "kernel warning at file:line:" followed by the formatted message.
 *
 * @param[in] file  Name of the source file, normally __FILE__.
 * @param[in] line  Line number, normally __LINE__.
 * @param[in] fmt   printf-style format string (formats supported by vcprintf).
 * @param[in] ...   Arguments matching @p fmt.
 * @note Unlike __panic, this function does not halt the kernel.
 */
void __warn(const char *file, int line, const char *fmt, ...);

/**
 * @brief Print a fatal error and enter the kernel monitor; never returns (called by the panic macro).
 *
 * The output is "kernel panic at file:line:" followed by the formatted message.
 *
 * @param[in] file  Name of the source file, normally __FILE__.
 * @param[in] line  Line number, normally __LINE__.
 * @param[in] fmt   printf-style format string.
 * @param[in] ...   Arguments matching @p fmt.
 * @note Declared noreturn: it never returns to the caller.
 * @warning Call it through the panic macro. A direct call must supply the file name and line number manually.
 */
void __noreturn __panic(const char *file, int line, const char *fmt, ...);

/**
 * @brief Print a kernel warning with the file name and line of the call site, then continue.
 *
 * The warn macro fills in __FILE__ and __LINE__; the caller only supplies the format string and arguments.
 *
 * @param[in] ...  Format string and arguments; at least the format string is required.
 *
 * @code
 *   warn("frame %d is not aligned", idx);
 * @endcode
 */
#define warn(...) __warn(__FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief Print a fatal error and stop the kernel; does not return.
 *
 * The panic macro fills in __FILE__ and __LINE__. Use it for unrecoverable errors.
 *
 * @param[in] ...  Format string and arguments; at least the format string is required.
 *
 * @code
 *   panic("out of memory: need %d pages", n);
 * @endcode
 */
#define panic(...) __panic(__FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief Run-time assertion: if @p x is false, call panic to stop the kernel.
 *
 * The failure message is "assertion failed: <expression text>". The macro is
 * wrapped in do { ... } while (0), so it is safe as a single statement in an
 * if/else branch.
 *
 * @param[in] x  Boolean expression that must be true. It is evaluated exactly once in the normal path.
 *
 * @code
 *   assert(ptr != NULL);
 * @endcode
 */
#define assert(x)                              \
    do {                                       \
        if (!(x)) {                            \
            panic("assertion failed: %s", #x); \
        }                                      \
    } while (0)

// static_assert(x) will generate a compile-time error if 'x' is false.
/**
 * @brief Compile-time assertion: compilation fails if @p x is 0.
 *
 * Expands to a switch whose case labels collide when @p x is 0, which the
 * compiler rejects. Therefore @p x must be an integer constant expression,
 * and the macro can only be used inside a function body (it is a statement,
 * not a file-scope declaration).
 *
 * @param[in] x  Integer constant expression evaluated at compile time. Non-zero values pass.
 * @note The switch also evaluates @p x at run time, so @p x should have no side effects.
 *
 * @code
 *   static_assert(sizeof(struct Page) == 64);
 * @endcode
 */
#define static_assert(x) \
    switch (x) {         \
        case 0:          \
        case (x):;       \
    }

#endif /* !__KERN_DEBUG_ASSERT_H__ */
