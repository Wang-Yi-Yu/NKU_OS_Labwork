/**
 * @file   panic.c
 * @brief  Implementation of kernel fatal-error and warning output.
 *
 * Implements __panic and __warn, declared in assert.h, and maintains the global
 * panic state flag. Output is written to the console through cprintf and vcprintf.
 */

#include <defs.h>
#include <intr.h>
#include <kmonitor.h>
#include <stdio.h>

/**
 * @brief Flag recording whether the kernel has entered the panic state.
 *
 * Set to 1 by __panic on its first call, to prevent recursive panics.
 */
static bool is_panic = 0;

/**
 * @brief Print a fatal error, disable interrupts and enter the kernel monitor.
 *
 * @details On the first call, prints "kernel panic at file:line:" and the
 *          formatted message. Then interrupts are disabled (so that, for
 *          example, the timer cannot fire again while the kernel is dead) and
 *          the loop below keeps running the kernel monitor. If it is called
 *          again while is_panic is already 1, it jumps straight to the loop to
 *          avoid recursive printing.
 */
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
 * @details Prints "kernel warning at file:line:" and the formatted message,
 *          then returns. The is_panic state is not modified.
 */
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
