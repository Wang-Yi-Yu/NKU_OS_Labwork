/**
 * @file   panic.c
 * @brief  User-level panic and warning helpers: __panic() and __warn().
 *
 * They print a message with the file name and line number, and __panic() terminates the process
 * with exit(-E_PANIC). The macros panic() and warn() in ulib.h call these functions.
 */
#include <defs.h>
#include <stdarg.h>
#include <stdio.h>
#include <ulib.h>
#include <error.h>

/**
 * @brief Print a fatal error with its source location and terminate the process.
 *
 * Prints "user panic at <file>:<line>:" and then the message, then calls exit(-E_PANIC).
 * This function never returns.
 * @param[in] file  Source file name where the error was raised (__FILE__).
 * @param[in] line  Line number in file (__LINE__).
 * @param[in] fmt  printf-style format string for the message.
 * @note Use the panic() macro from ulib.h, which fills in file and line.
 */
void __panic(const char *file, int line, const char *fmt, ...) {
    // print the 'message'
    va_list ap;
    va_start(ap, fmt);
    cprintf("user panic at %s:%d:\n    ", file, line);
    vcprintf(fmt, ap);
    cprintf("\n");
    va_end(ap);
    exit(-E_PANIC);
}

/**
 * @brief Print a warning with its source location; the process continues.
 *
 * Prints "user warning at <file>:<line>:" and then the message. Use the warn() macro
 * from ulib.h instead of calling this function directly.
 * @param[in] file  Source file name where the warning was raised (__FILE__).
 * @param[in] line  Line number in file (__LINE__).
 * @param[in] fmt  printf-style format string for the message.
 */
void __warn(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    cprintf("user warning at %s:%d:\n    ", file, line);
    vcprintf(fmt, ap);
    cprintf("\n");
    va_end(ap);
}
