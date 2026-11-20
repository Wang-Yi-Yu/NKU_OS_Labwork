/**
 * @file   stdarg.h
 * @brief  Kernel version of the variadic argument macros, built directly on compiler built-ins.
 *
 * Does not depend on the libc <stdarg.h>. Usage is the same as standard C: va_start to initialize,
 * va_arg to read each argument, va_end when done.
 */
#ifndef __LIBS_STDARG_H__
#define __LIBS_STDARG_H__

/**
 * @brief Type of a variadic argument list.
 *
 * The size of the save area is provided by the compiler.
 */
typedef __builtin_va_list va_list;

/**
 * @brief Initializes the argument list @p ap so that it points past @p last.
 *
 * @param ap    The va_list variable to initialize.
 * @param last  The last named parameter of the enclosing function.
 */
#define va_start(ap, last) (__builtin_va_start(ap, last))

/**
 * @brief Reads the next argument of type @p type from @p ap and advances @p ap.
 *
 * @param ap    An initialized va_list.
 * @param type  Type of the next argument.
 * @return The value of the argument.
 */
#define va_arg(ap, type) (__builtin_va_arg(ap, type))

/**
 * @brief Ends use of the argument list. This implementation expands to nothing.
 *
 * @param ap  The va_list variable.
 */
#define va_end(ap) /*nothing*/

#endif /* !__LIBS_STDARG_H__ */
