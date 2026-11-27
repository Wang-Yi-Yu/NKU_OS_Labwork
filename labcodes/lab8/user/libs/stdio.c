/**
 * @file   stdio.c
 * @brief  User-level formatted output: cputch(), vcprintf(), cprintf(), cputs() and, since lab8,
 *         fputch(), vfprintf() and fprintf().
 *
 * The cprintf family goes through the sys_putc() system call, one character at a time. The
 * fprintf family writes every character to a file descriptor with write(), so the output can be
 * redirected like any other file (the shell prints through fprintf(1, ...)).
 */
#include <defs.h>
#include <stdio.h>
#include <syscall.h>
#include <file.h>
#include <ulib.h>
#include <unistd.h>

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
    vprintfmt((void *)cputch, NO_FD, &cnt, fmt, ap);
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

/**
 * @brief Write one character to a file descriptor and count it (lab8).
 *
 * Used as the output callback of vprintfmt() for vfprintf(). It is static and not part of the API.
 * @param[in]     c    Character to write.
 * @param[in,out] cnt  Pointer to the counter to increase by one.
 * @param[in]     fd   File descriptor to write to.
 */
static void fputch(char c, int *cnt, int fd) {
    write(fd, &c, sizeof(char));
    (*cnt)++;
}

/**
 * @brief Format a string with a va_list and write it to a file descriptor (lab8).
 * @param[in] fd   File descriptor to write to.
 * @param[in] fmt  printf-style format string.
 * @param[in] ap   Variable argument list already started by the caller.
 * @return The number of characters written.
 */
int vfprintf(int fd, const char *fmt, va_list ap) {
    int cnt = 0;
    vprintfmt((void *)fputch, fd, &cnt, fmt, ap);
    return cnt;
}

/**
 * @brief Format a string and write it to a file descriptor, printf style (lab8).
 * @param[in] fd   File descriptor to write to.
 * @param[in] fmt  Format string.
 * @param[in] ...  Arguments matching @p fmt.
 * @return The number of characters written.
 */
int fprintf(int fd, const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    int cnt = vfprintf(fd, fmt, ap);
    va_end(ap);

    return cnt;
}
