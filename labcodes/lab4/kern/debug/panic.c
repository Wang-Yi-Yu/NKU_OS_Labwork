/**
 * @file   panic.c
 * @brief  Implementation of kernel fatal-error and warning output.
 *
 * Implements __panic and __warn, declared in assert.h, and maintains the global
 * panic state flag. Output is written to the console through cprintf and vcprintf.
 */

#include <defs.h>
#include <stdio.h>
#include <intr.h>
#include <kmonitor.h>

/**
 * @brief Flag recording whether the kernel has entered the panic state.
 *
 * Set to 1 by __panic on its first call, to prevent recursive panics.
 */
static bool is_panic = 0;

/**
 * @brief Print a fatal error, disable interrupts and enter the kernel monitor. Never returns.
 *
 * @details On the first call, prints "kernel panic at file:line:" and the formatted
 *          message. Interrupts are then disabled and the loop keeps running the kernel
 *          monitor. If called again while is_panic is already 1, it jumps straight to the
 *          loop, so the message is not printed twice.
 *
 * @param[in] file  Source file name where the panic occurred (normally __FILE__).
 * @param[in] line  Line number where the panic occurred (normally __LINE__).
 * @param[in] fmt   printf-style format string for the message.
 * @param[in] ...   Arguments matching @p fmt.
 */
/* *
 * __panic - __panic is called on unresolvable fatal errors. it prints
 * "panic: 'message'", and then enters the kernel monitor.
 * */
void __panic(const char *file, int line, const char *fmt, ...) {
    if (is_panic) {
        goto panic_dead;
    }
    is_panic = 1;

    // print the 'message'
    va_list ap;
    va_start(ap, fmt);
    cprintf("kernel panic at %s:%d:\n    ", file, line);
    vcprintf(fmt, ap);
    cprintf("\n");
    va_end(ap);

panic_dead:
    intr_disable();
    while (1) {
        kmonitor(NULL);
    }
}

/**
 * @brief Print a kernel warning without halting the kernel.
 *
 * @details Prints "kernel warning at file:line:" and the formatted message, then
 *          returns. The is_panic state is not modified.
 *
 * @param[in] file  Source file name of the warning (normally __FILE__).
 * @param[in] line  Line number of the warning (normally __LINE__).
 * @param[in] fmt   printf-style format string for the message.
 * @param[in] ...   Arguments matching @p fmt.
 */
/* __warn - like panic, but don't */
void __warn(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    cprintf("kernel warning at %s:%d:\n    ", file, line);
    vcprintf(fmt, ap);
    cprintf("\n");
    va_end(ap);
}

/**
 * @brief Query whether the kernel has entered the panic state.
 *
 * This function is not declared in any header, so callers must provide their own prototype.
 *
 * @return Non-zero if __panic has been called; 0 if no fatal error has occurred yet.
 */
bool is_kernel_panic(void) {
    return is_panic;
}
