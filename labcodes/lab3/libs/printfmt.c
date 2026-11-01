/**
 * @file   printfmt.c
 * @brief  Implementation of formatted output: printfmt/vprintfmt and snprintf/vsnprintf.
 *
 * The core function vprintfmt() emits characters one by one through a putch callback,
 * so the same formatting logic serves both console output (cprintf, see kern/libs/stdio.c)
 * and writing into a string buffer (snprintf). The public interface is documented in libs/stdio.h.
 */
#include <defs.h>
#include <riscv.h>
#include <error.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Table mapping error codes to description strings, used by the %e conversion.
 *
 * Indexed by the error code (defined in libs/error.h). Index 0 and undefined codes map to NULL.
 * vprintfmt() takes the absolute value of the argument before the lookup.
 */
static const char *const error_string[MAXERROR + 1] = {
    [0] NULL,
    [E_UNSPECIFIED] "unspecified error",
    [E_BAD_PROC] "bad process",
    [E_INVAL] "invalid parameter",
    [E_NO_MEM] "out of memory",
    [E_NO_FREE_PROC] "out of processes",
    [E_FAULT] "segmentation fault",
};

/**
 * @brief Prints an integer in reverse digit order, for bases up to 16.
 *
 * Recursive: higher-order digits are printed first, then the lowest digit. If the number has
 * fewer digits than @p width, the missing positions are filled with @p padc on the left.
 * Digits above 9 use lowercase letters.
 *
 * @details width is decremented at each recursion level and the padding is emitted only at the
 * innermost (least significant) level, so the total number of pad characters is width minus the
 * number of digits, and they always appear to the left of the digits.
 *
 * @param[in] putch   Single-character output callback.
 * @param[in] putdat  User data passed to @p putch.
 * @param[in] num     Unsigned value to print.
 * @param[in] base    Numeric base, in [1, 16].
 * @param[in] width   Minimum number of digit positions.
 * @param[in] padc    Padding character, e.g. ' ' or '0'.
 */
static void printnum(void (*putch)(int, void *), void *putdat, unsigned long long num,
                     unsigned base, int width, int padc) {
    unsigned long long result = num;
    unsigned mod = do_div(result, base);

    // first recursively print all preceding (more significant) digits
    if (num >= base) {
        printnum(putch, putdat, result, base, width - 1, padc);
    } else {
        // print any needed pad characters before first digit
        while (--width > 0)
            putch(padc, putdat);
    }
    // then print this (the least significant) digit
    putch("0123456789abcdef"[mod], putdat);
}

/**
 * @brief Reads an unsigned integer of the size selected by a length modifier from a va_list.
 *
 * @param[in,out] ap     Pointer to the variadic argument list; advanced by the read.
 * @param[in]     lflag  Length modifier: 0 for unsigned int, 1 for unsigned long,
 *                       2 or more for unsigned long long.
 * @return The value read, widened to unsigned long long.
 */
static unsigned long long getuint(va_list *ap, int lflag) {
    if (lflag >= 2) {
        return va_arg(*ap, unsigned long long);
    } else if (lflag) {
        return va_arg(*ap, unsigned long);
    } else {
        return va_arg(*ap, unsigned int);
    }
}

/**
 * @brief Signed counterpart of getuint().
 *
 * Kept separate from getuint() so that the value is sign-extended correctly when widened.
 *
 * @param[in,out] ap     Pointer to the variadic argument list; advanced by the read.
 * @param[in]     lflag  Length modifier: 0 for int, 1 for long, 2 or more for long long.
 * @return The value read, widened to long long.
 */
static long long getint(va_list *ap, int lflag) {
    if (lflag >= 2) {
        return va_arg(*ap, long long);
    } else if (lflag) {
        return va_arg(*ap, long);
    } else {
        return va_arg(*ap, int);
    }
}

/**
 * @brief Formats a string and emits it character by character through @p putch.
 *
 * Variadic wrapper around vprintfmt(). Full description is in libs/stdio.h.
 *
 * @param putch    Single-character output callback.
 * @param putdat   User data passed to @p putch.
 * @param fmt      Format string.
 */
void printfmt(void (*putch)(int, void *), void *putdat, const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vprintfmt(putch, putdat, fmt, ap);
    va_end(ap);
}

/**
 * @brief Formats a string from a va_list and emits it through @p putch.
 *
 * Core formatting engine. It scans @p fmt, copying ordinary characters directly and parsing
 * the flags, width, precision, length modifiers and conversion character after each '%'.
 * The full specification of the supported conversions is in libs/stdio.h.
 *
 * @param putch    Single-character output callback.
 * @param putdat   User data passed to @p putch.
 * @param fmt      Format string.
 * @param ap       Argument list consumed by the conversions.
 */
void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap) {
    register const char *p;
    register int ch, err;
    unsigned long long num;
    int base, width, precision, lflag, altflag;

    while (1) {
        while ((ch = *(unsigned char *)fmt++) != '%') {
            if (ch == '\0') {
                return;
            }
            putch(ch, putdat);
        }

        // Process a %-escape sequence
        char padc = ' ';
        width = precision = -1;
        lflag = altflag = 0;

    reswitch:
        switch (ch = *(unsigned char *)fmt++) {
            // flag to pad on the right
            case '-':
                padc = '-';
                goto reswitch;

            // flag to pad with 0's instead of spaces
            case '0':
                padc = '0';
                goto reswitch;

            // width field
            case '1' ... '9':
                for (precision = 0;; ++fmt) {
                    precision = precision * 10 + ch - '0';
                    ch = *fmt;
                    if (ch < '0' || ch > '9') {
                        break;
                    }
                }
                goto process_precision;

            case '*':
                precision = va_arg(ap, int);
                goto process_precision;

            case '.':
                if (width < 0)
                    width = 0;
                goto reswitch;

            case '#':
                altflag = 1;
                goto reswitch;

            process_precision:
                if (width < 0)
                    width = precision, precision = -1;
                goto reswitch;

            // long flag (doubled for long long)
            case 'l':
                lflag++;
                goto reswitch;

            // character
            case 'c':
                putch(va_arg(ap, int), putdat);
                break;

            // error message
            case 'e':
                err = va_arg(ap, int);
                if (err < 0) {
                    err = -err;
                }
                if (err > MAXERROR || (p = error_string[err]) == NULL) {
                    printfmt(putch, putdat, "error %d", err);
                } else {
                    printfmt(putch, putdat, "%s", p);
                }
                break;

            // string
            case 's':
                if ((p = va_arg(ap, char *)) == NULL) {
                    p = "(null)";
                }
                if (width > 0 && padc != '-') {
                    for (width -= strnlen(p, precision); width > 0; width--) {
                        putch(padc, putdat);
                    }
                }
                for (; (ch = *p++) != '\0' && (precision < 0 || --precision >= 0); width--) {
                    if (altflag && (ch < ' ' || ch > '~')) {
                        putch('?', putdat);
                    } else {
                        putch(ch, putdat);
                    }
                }
                for (; width > 0; width--) {
                    putch(' ', putdat);
                }
                break;

            // (signed) decimal
            case 'd':
                num = getint(&ap, lflag);
                if ((long long)num < 0) {
                    putch('-', putdat);
                    num = -(long long)num;
                }
                base = 10;
                goto number;

            // unsigned decimal
            case 'u':
                num = getuint(&ap, lflag);
                base = 10;
                goto number;

            // (unsigned) octal
            case 'o':
                num = getuint(&ap, lflag);
                base = 8;
                goto number;

            // pointer
            case 'p':
                putch('0', putdat);
                putch('x', putdat);
                num = (unsigned long long)(uintptr_t)va_arg(ap, void *);
                base = 16;
                goto number;

            // (unsigned) hexadecimal
            case 'x':
                num = getuint(&ap, lflag);
                base = 16;
            number:
                printnum(putch, putdat, num, base, width, padc);
                break;

            // escaped '%' character
            case '%':
                putch(ch, putdat);
                break;

            // unrecognized escape sequence - just print it literally
            default:
                putch('%', putdat);
                for (fmt--; fmt[-1] != '%'; fmt--)
                    /* do nothing */;
                break;
        }
    }
}

/**
 * @brief State of a string buffer during vsnprintf().
 *
 * Built by vsnprintf() and passed as the user data of sprintputch().
 */
struct sprintbuf {
    char *buf;  /**< Next writable position (first unused byte). */
    char *ebuf; /**< Position of the last writable byte, i.e. str + size - 1. */
    int cnt;    /**< Total number of characters produced, including those dropped on overflow. */
};

/**
 * @brief Writes one character into a bounded string buffer, dropping it when the buffer is full.
 *
 * cnt is incremented for every character even when it is not stored, so the final cnt can exceed
 * the buffer capacity.
 *
 * @param[in]     ch  Character to write.
 * @param[in,out] b   Buffer state; b->buf is advanced when the character is stored.
 */
static void sprintputch(int ch, struct sprintbuf *b) {
    b->cnt++;
    if (b->buf < b->ebuf) {
        *b->buf++ = ch;
    }
}

/**
 * @brief Formats a string into a buffer (variadic wrapper).
 *
 * Full description is in libs/stdio.h.
 *
 * @param str   Destination buffer.
 * @param size  Size of the buffer in bytes, including the terminating '\0'.
 * @param fmt   Format string.
 * @return Same as vsnprintf().
 */
int snprintf(char *str, size_t size, const char *fmt, ...) {
    va_list ap;
    int cnt;
    va_start(ap, fmt);
    cnt = vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return cnt;
}

/**
 * @brief Formats a string from a va_list into a buffer.
 *
 * Calls vprintfmt() with sprintputch() as the callback. At most size - 1 characters are stored,
 * followed by '\0'. Full description and return-value semantics are in libs/stdio.h.
 *
 * @param str   Destination buffer.
 * @param size  Size of the buffer in bytes, including the terminating '\0'.
 * @param fmt   Format string.
 * @param ap    Argument list consumed by the conversions.
 * @return Same as vsnprintf().
 */
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
    struct sprintbuf b = {str, str + size - 1, 0};
    if (str == NULL || b.buf > b.ebuf) {
        return -E_INVAL;
    }
    // print the string to the buffer
    vprintfmt((void *)sprintputch, &b, fmt, ap);
    // null terminate the buffer
    *b.buf = '\0';
    return b.cnt;
}
