/**
 * @file   stdio.c
 * @brief  High-level kernel console I/O: cprintf, vcprintf, cputchar and getchar.
 *
 * All output goes through cons_putc in the console driver. cputs writes a string followed by a newline.
 */
#include <defs.h>
#include <stdio.h>
#include <console.h>


/**
 * @brief Output callback for vprintfmt: write one character and count it.
 *
 * @param[in]     c    Character to print.
 * @param[in,out] cnt  Pointer to the running count of characters printed; incremented by one.
 */
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
void cputchar(int c) {
    cons_putc(c);
}

/**
 * @brief Write a string followed by a newline to the console.
 *
 * @param[in] str  NUL-terminated string to print.
 * @return The number of characters printed, including the newline.
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
 * @brief Read one character from the console.
 *
 * @details Loops while cons_getc() returns 0 (no character). Any other value, including
 *          the -1 that the legacy SBI returns when no input is waiting, is returned at once,
 *          so in practice this function does not block.
 *
 * @return The character read (non-zero), or a negative value if no input is available.
 */
int getchar(void) {
    int c;
    while ((c = cons_getc()) == 0)
        /* do nothing */;
    return c;
}
