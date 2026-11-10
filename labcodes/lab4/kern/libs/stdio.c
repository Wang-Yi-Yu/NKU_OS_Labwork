/**
 * @file   stdio.c
 * @brief  High-level kernel console I/O: cprintf, vcprintf, cputchar and getchar.
 *
 * All output goes through cons_putc in the console driver. There is no cputs in lab4.
 */
#include <defs.h>
#include <stdio.h>
#include <console.h>

/* HIGH level console I/O */

/**
 * @brief Output callback for vprintfmt: write one character and count it.
 *
 * @param[in]     c    Character to print.
 * @param[in,out] cnt  Pointer to the running count of characters printed; incremented by one.
 */
/* *
 * cputch - writes a single character @c to stdout, and it will
 * increace the value of counter pointed by @cnt.
 * */
static void cputch(int c, int *cnt) {
    cons_putc(c);
    (*cnt)++;
}

/**
 * @brief Format a string with a va_list and print it to the console.
 *
 * @param[in] fmt  printf-style format string.
 * @param[in] ap   Variable argument list already started by the caller.
 * @return The number of characters printed (the length the output would have).
 * @note Call this if you already hold a va_list; otherwise use cprintf().
 */
/* *
 * vcprintf - format a string and writes it to stdout
 *
 * The return value is the number of characters which would be
 * written to stdout.
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want cprintf() instead.
 * */
int vcprintf(const char *fmt, va_list ap) {
    int cnt = 0;
    vprintfmt((void *)cputch, &cnt, fmt, ap);
    return cnt;
}

/**
 * @brief Format a string with variable arguments and print it to the console.
 *
 * @param[in] fmt  printf-style format string (see vprintfmt for the supported conversions).
 * @param[in] ...  Arguments matching @p fmt.
 * @return The number of characters printed.
 */
/* *
 * cprintf - formats a string and writes it to stdout
 *
 * The return value is the number of characters which would be
 * written to stdout.
 * */
int cprintf(const char *fmt, ...) {
    va_list ap;
    int cnt;
    va_start(ap, fmt);
    cnt = vcprintf(fmt, ap);
    va_end(ap);
    return cnt;
}

/**
 * @brief Print one character to the console.
 *
 * @param[in] c  Character to print.
 */
/* cputchar - writes a single character to stdout */
void cputchar(int c) {
    cons_putc(c);
}

/**
 * @brief Read one character from the console.
 *
 * @details Loops while cons_getc() returns 0 (no character). Any other value, including
 *          the -1 that the legacy SBI returns when no input is waiting, is returned at once,
 *          so in practice this function does not block.
 *
 * @return The character read (non-zero), or a negative value if no input is available.
 */
/* getchar - reads a single non-zero character from stdin */
int getchar(void) {
    int c;
    while ((c = cons_getc()) == 0)
        /* do nothing */;
    return c;
}
