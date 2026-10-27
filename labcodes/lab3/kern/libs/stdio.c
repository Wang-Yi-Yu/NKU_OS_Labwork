/**
 * @file   stdio.c
 * @brief  Implementation of the kernel's high-level standard I/O functions (cprintf, cputs, getchar, etc.).
 *
 * Sits on top of the console driver (console.h). Formatting is done by vprintfmt
 * in libs/printfmt.c, and each character is emitted through cons_putc. The
 * function prototypes are declared in libs/stdio.h.
 */

#include <defs.h>
#include <stdio.h>
#include <console.h>

/**
 * @brief Write one character to standard output and increment a counter.
 *
 * Used as the output callback of vprintfmt. Each call outputs one character and adds 1 to *cnt.
 *
 * @param[in]     c    Character to output.
 * @param[in,out] cnt  Pointer to the character counter; must not be NULL. Incremented once per character.
 */
static void cputch(int c, int *cnt) {
    cons_putc(c);
    (*cnt)++;
}

/**
 * @brief Format output to standard output, taking a va_list argument.
 *
 * Use this when the caller already holds a va_list; otherwise use cprintf.
 *
 * @param[in] fmt  printf-style format string; must not be NULL.
 * @param[in] ap   Variable argument list matching @p fmt.
 * @return The number of characters output (not counting the terminating '\0').
 */
int vcprintf(const char *fmt, va_list ap) {
    int cnt = 0;
    vprintfmt((void *)cputch, &cnt, fmt, ap);
    return cnt;
}

/**
 * @brief Format output to standard output.
 *
 * Calls vcprintf internally. Argument handling is similar to the standard
 * printf, supporting the conversions implemented by vprintfmt.
 *
 * @param[in] fmt  printf-style format string; must not be NULL.
 * @param[in] ...  Arguments matching @p fmt.
 * @return The number of characters output (not counting the terminating '\0').
 * @note The count depends on the expansion of conversions such as %s, so it reflects the actual output length.
 *
 * @code
 *   cprintf("heap at 0x%016lx, %d pages\n", base, n);
 * @endcode
 */
int cprintf(const char *fmt, ...) {
    va_list ap;
    int cnt;
    va_start(ap, fmt);
    cnt = vcprintf(fmt, ap);
    va_end(ap);
    return cnt;
}

/**
 * @brief Write one character to standard output.
 *
 * Calls cons_putc directly. No newline translation is performed and nothing is counted.
 *
 * @param[in] c  Character to output (ASCII code).
 */
void cputchar(int c) {
    cons_putc(c);
}

/**
 * @brief Write a string to standard output, followed by a newline.
 *
 * Outputs the characters of @p str one by one until '\0', then outputs '\n'.
 *
 * @param[in] str  NUL-terminated string; must not be NULL.
 * @return The number of characters output, including the appended newline.
 */
int cputs(const char *str) {
    int cnt = 0;
    char c;
    while ((c = *str++) != '\0') {
        cputch(c, &cnt);
    }
    cputch('\n', &cnt);
    return cnt;
}

/**
 * @brief Read one character from standard input, busy-waiting until a non-zero character arrives.
 *
 * Calls cons_getc in a loop until it returns a value other than 0. The function
 * does not yield the CPU.
 *
 * @return A non-zero character that was read.
 * @warning Relies on cons_getc returning 0 when no input is available. The SBI
 *          legacy interface returns -1 in that case, which makes the loop
 *          condition false, so this function returns -1 immediately instead of
 *          waiting. This inconsistency has not been fixed yet.
 */
int getchar(void) {
    int c;
    while ((c = cons_getc()) == 0)
        /* do nothing */;
    return c;
}
