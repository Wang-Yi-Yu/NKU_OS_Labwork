/**
 * @file   stdio.c
 * @brief  User-level formatted output: cputch(), vcprintf(), cprintf() and cputs().
 *
 * All output goes through the sys_putc() system call, one character at a time.
 */
#include <defs.h>
#include <stdio.h>
#include <syscall.h>

/**
 * @brief Write one character to the console and count it.
 *
 * Used as the output callback of vprintfmt(). It is static and not part of the API.
 * @param[in] c  Character to write.
 * @param[in] cnt  Pointer to the counter to increase by one.
 */
static void cputch(int c, int *cnt) {
    sys_putc(c);
    (*cnt)++;
}

/**
 * @brief Format a string and write it to the console.
 *
 * Call this function if you are already dealing with a va_list. Otherwise use cprintf().
 * @param[in] fmt  printf-style format string.
 * @param[in] ap  Arguments for fmt, as a va_list.
 * @return The number of characters that were written.
 */
int vcprintf(const char *fmt, va_list ap) {
    int cnt = 0;
    vprintfmt((void *)cputch, &cnt, fmt, ap);
    return cnt;
}

/**
 * @brief Format a string and write it to the console.
 * @param[in] fmt  printf-style format string.
 * @param[in] ...  Arguments for fmt.
 * @return The number of characters that were written.
 */
int cprintf(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    int cnt = vcprintf(fmt, ap);
    va_end(ap);

    return cnt;
}

/**
 * @brief Write a string and a newline to the console.
 * @param[in] str  NUL-terminated string to write; the terminating NUL is not written.
 * @return The number of characters that were written, including the newline.
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
