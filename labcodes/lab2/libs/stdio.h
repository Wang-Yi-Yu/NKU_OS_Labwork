/**
 * @file   stdio.h
 * @brief  Declarations of the kernel standard I/O interface: console output, line input and formatting.
 *
 * The console functions (cprintf, vcprintf, cputchar, cputs, getchar) are implemented in
 * kern/libs/stdio.c, readline() in libs/readline.c, and the formatting engine in libs/printfmt.c.
 * All formatting functions share the same conversion rules; the specification is given with vprintfmt().
 */
#ifndef __LIBS_STDIO_H__
#define __LIBS_STDIO_H__

#include <defs.h>
#include <stdarg.h>

/* kern/libs/stdio.c */

/**
 * @brief Formats a string and writes it to the console (variadic form).
 *
 * Calls vcprintf(); conversion rules are described in vprintfmt().
 *
 * @param[in] fmt  Format string.
 * @return Number of characters written, not counting the terminating '\0'.
 */
int cprintf(const char *fmt, ...);

/**
 * @brief Formats a string from a va_list and writes it to the console.
 *
 * Use this only when a va_list is already available; otherwise prefer cprintf().
 * Conversion rules are described in vprintfmt().
 *
 * @param[in] fmt  Format string.
 * @param[in] ap   Argument list.
 * @return Number of characters written, not counting the terminating '\0'.
 */
int vcprintf(const char *fmt, va_list ap);

/**
 * @brief Writes a single character to the console.
 *
 * The character is passed to the SBI console interface. '\n' is written as-is; no '\r' is added.
 *
 * @param[in] c  Character to write; only the low 8 bits are used.
 */
void cputchar(int c);

/**
 * @brief Writes a string to the console followed by a newline.
 *
 * @param[in] str  '\0'-terminated string; must not be NULL.
 * @return Number of characters written, including the appended newline.
 */
int cputs(const char *str);

/**
 * @brief Reads one character from the console.
 *
 * Calls cons_getc() and retries while it returns 0, so a 0 character is never returned.
 *
 * @return The character read, or a negative value when no input is currently available.
 * @warning The underlying SBI call returns -1 when no character is pending, so this function
 *          does not block waiting for input.
 */
int getchar(void);

/* kern/libs/readline.c */

/**
 * @brief Prints a prompt and reads one line of input with echo.
 *
 * Writes @p prompt (nothing is written when it is NULL or empty), then reads characters until
 * a newline or carriage return:
 *   - printable characters (>= ' ') are appended to the line buffer;
 *   - '\b' removes the last character from the line buffer, and is ignored when the buffer is empty;
 *   - '\n' or '\r' ends the line; it is echoed and not stored.
 * The returned string does not contain the line terminator; the buffer is '\0'-terminated.
 *
 * @param[in] prompt  Prompt string, may be NULL.
 * @return Pointer to a static buffer containing the line read; NULL if the underlying read
 *         returns a negative value.
 * @note The returned pointer refers to a single static buffer that the next call overwrites;
 *       copy the string if it must be kept.
 * @warning The buffer holds 1024 bytes. Once it is full, further characters are discarded and not
 *          echoed until a newline arrives.
 * @warning The read returns NULL immediately when no input is pending instead of waiting. Callers
 *          must check the return value.
 * @see getchar()
 */
char *readline(const char *prompt);

/* libs/printfmt.c */

/**
 * @brief Formats a string and emits it character by character through a callback (variadic form).
 *
 * Calls vprintfmt(). The caller chooses the output target, for example the console or a buffer.
 *
 * @param[in] putch   Single-character output callback, invoked once per character.
 * @param[in] putdat  User data passed to @p putch.
 * @param[in] fmt     Format string.
 * @see vprintfmt()
 */
void printfmt(void (*putch)(int, void *), void *putdat, const char *fmt, ...);

/**
 * @brief Formats a string from a va_list and emits it character by character through a callback.
 *
 * Supported conversions (the characters after '%'):
 *   - %c   a character (int argument);
 *   - %s   a string; NULL prints "(null)"; with the '#' flag, characters outside ' ' to '~' print as '?';
 *   - %d   signed decimal; %u unsigned decimal; %o unsigned octal; %x unsigned lowercase hexadecimal;
 *   - %p   "0x" followed by the pointer value in hexadecimal;
 *   - %e   description of an error code (e.g. "out of memory"); the argument may be positive or
 *          negative (its absolute value is used); codes outside the table print "error N";
 *   - %%   a literal '%'.
 *
 * Modifiers:
 *   - '-' left-justifies %s (padding goes to the right); '0' pads with '0' instead of spaces;
 *   - a decimal width, or '*' to take the width from the arguments; '.' followed by digits sets a
 *     precision, which limits the number of characters printed by %s and has no effect on integers;
 *   - 'l' reads a long argument, 'll' a long long argument;
 *   - an unknown conversion character prints '%' and the characters after it literally.
 *
 * @param[in] putch   Single-character output callback.
 * @param[in] putdat  User data passed to @p putch.
 * @param[in] fmt     Format string.
 * @param[in] ap      Argument list.
 * @note Width and padding apply to %s and the integer conversions; %p applies the width to its
 *       hexadecimal digits, not to the "0x" prefix. %c and %e ignore the width.
 * @warning With the '-' flag on an integer conversion the padding character is '-', not a space,
 *          so "%-5d" of 42 prints "---42" and is not left-justified.
 * @warning The argument types must match the conversions, otherwise va_arg reads the wrong type
 *          and the behaviour is undefined.
 */
void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap);

/**
 * @brief Formats a string into a buffer (variadic form).
 *
 * Calls vsnprintf(); conversion rules are described in vprintfmt().
 *
 * @param[out] str   Destination buffer.
 * @param[in]  size  Size of the buffer in bytes, including the terminating '\0'.
 * @param[in]  fmt   Format string.
 * @return Same as vsnprintf().
 * @see vsnprintf()
 */
int snprintf(char *str, size_t size, const char *fmt, ...);

/**
 * @brief Formats a string from a va_list into a buffer.
 *
 * At most @p size - 1 characters are stored, followed by '\0'. Characters beyond that are discarded.
 *
 * @param[out] str   Destination buffer; must not be NULL.
 * @param[in]  size  Size of the buffer in bytes, including the terminating '\0'.
 * @param[in]  fmt   Format string.
 * @param[in]  ap    Argument list.
 * @return If @p str is NULL or @p size is 0, returns -E_INVAL and writes nothing. Otherwise returns
 *         the number of characters that would be produced with an unlimited buffer, not counting
 *         the '\0', even when the output was truncated.
 * @warning The return value can exceed @p size - 1. To detect truncation, compare it with @p size.
 * @note See vprintfmt() for the conversion rules.
 */
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

#endif /* !__LIBS_STDIO_H__ */
