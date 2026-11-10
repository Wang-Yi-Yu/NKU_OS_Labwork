/**
 * @file   stdio.h
 * @brief  Standard I/O interface shared by the kernel and user-level libraries.
 *
 * Groups the console output and input functions (kern/libs/stdio.c), the line editor
 * (kern/libs/readline.c) and the formatted-output engine (libs/printfmt.c).
 */

#ifndef __LIBS_STDIO_H__
#define __LIBS_STDIO_H__

#include <defs.h>
#include <stdarg.h>

/* kern/libs/stdio.c */
/**
 * @brief Format and print to the console, printf style.
 * @param[in] fmt  Format string.
 * @param[in] ...  Arguments matching @p fmt.
 * @return The number of characters printed.
 */
int cprintf(const char *fmt, ...);
/**
 * @brief Format and print to the console from a va_list.
 * @param[in] fmt  Format string.
 * @param[in] ap   Argument list already started by the caller.
 * @return The number of characters printed.
 */
int vcprintf(const char *fmt, va_list ap);
/**
 * @brief Print one character to the console.
 * @param[in] c  Character to print.
 */
void cputchar(int c);
/**
 * @brief Read one character from the console.
 * @return The character read, or a negative value if none is available (does not block).
 */
int getchar(void);

/* kern/libs/readline.c */
/**
 * @brief Print a prompt and read one edited line from the console.
 * @param[in] prompt  String printed first; NULL prints nothing.
 * @return Pointer to a static buffer with the line, or NULL if input is unavailable.
 * @warning The buffer is overwritten by the next call; copy the result if it is needed later.
 */
char *readline(const char *prompt);

/* libs/printfmt.c */
/**
 * @brief Format a string and send each character to a caller-supplied output function.
 * @param[in] putch   Called once per output character as putch(ch, putdat).
 * @param[in] putdat  Opaque pointer passed as the second argument to putch.
 * @param[in] fmt     Format string.
 * @param[in] ...     Arguments matching @p fmt.
 */
void printfmt(void (*putch)(int, void *), void *putdat, const char *fmt, ...);
/**
 * @brief va_list version of printfmt().
 * @param[in] putch   Called once per output character as putch(ch, putdat).
 * @param[in] putdat  Opaque pointer passed to putch.
 * @param[in] fmt     Format string.
 * @param[in] ap      Argument list already started by the caller.
 */
void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap);
/**
 * @brief Format into a buffer of at most @p size bytes, always NUL-terminated.
 * @param[out] str   Destination buffer.
 * @param[in]  size  Size of @p str in bytes.
 * @param[in]  fmt   Format string.
 * @param[in]  ...   Arguments matching @p fmt.
 * @return The length the full output would have, excluding the NUL.
 */
int snprintf(char *str, size_t size, const char *fmt, ...);
/**
 * @brief va_list version of snprintf().
 * @param[out] str   Destination buffer.
 * @param[in]  size  Size of @p str in bytes.
 * @param[in]  fmt   Format string.
 * @param[in]  ap    Argument list already started by the caller.
 * @return The length the full output would have, excluding the NUL.
 */
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

#endif /* !__LIBS_STDIO_H__ */
