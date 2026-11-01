/**
 * @file   sbi.c
 * @brief  Implementation of SBI calls: traps into M-mode firmware via ecall, and wrappers for console and timer services.
 *
 * The interface is declared in libs/sbi.h. sbi_call() is the common low-level routine that all
 * wrappers in this file use.
 */
#include <sbi.h>
#include <defs.h>


/**
 * @brief SBI call number (legacy extension) for setting the timer; used by sbi_set_timer().
 *
 * These variables are not declared in a header and are only meant for this file. They are not
 * static and are writable, so other translation units could reach them through extern.
 * The call number is passed to the firmware in a7 (x17) by sbi_call().
 */
uint64_t SBI_SET_TIMER = 0;

/** @brief SBI call number for writing a console character; used by sbi_console_putchar(). */
uint64_t SBI_CONSOLE_PUTCHAR = 1;

/** @brief SBI call number for reading a console character; used by sbi_console_getchar(). */
uint64_t SBI_CONSOLE_GETCHAR = 2;

/** @brief SBI call number for clearing a pending IPI. */
uint64_t SBI_CLEAR_IPI = 3;

/** @brief SBI call number for sending an IPI. */
uint64_t SBI_SEND_IPI = 4;

/** @brief SBI call number for a remote fence.i. */
uint64_t SBI_REMOTE_FENCE_I = 5;

/** @brief SBI call number for a remote sfence.vma. */
uint64_t SBI_REMOTE_SFENCE_VMA = 6;

/** @brief SBI call number for a remote sfence.vma restricted to an ASID. */
uint64_t SBI_REMOTE_SFENCE_VMA_ASID = 7;

/** @brief SBI call number for system shutdown. */
uint64_t SBI_SHUTDOWN = 8;

/**
 * @brief Performs one SBI call (ecall) and returns the firmware's result.
 *
 * Register convention: a7 (x17) holds the call number, a0 to a2 (x10 to x12) hold the
 * arguments, and the result is read from a0 (x10). Implemented as inline assembly with a
 * "memory" clobber, so the compiler will not move memory accesses across the call.
 *
 * @param[in] sbi_type  SBI call number, one of the SBI_* variables above.
 * @param[in] arg0      First argument, passed in a0.
 * @param[in] arg1      Second argument, passed in a1.
 * @param[in] arg2      Third argument, passed in a2.
 * @return The value the firmware left in a0; its meaning depends on the call.
 */
uint64_t sbi_call(uint64_t sbi_type, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
    uint64_t ret_val;
    __asm__ volatile(
        "mv x17, %[sbi_type]\n"
        "mv x10, %[arg0]\n"
        "mv x11, %[arg1]\n"
        "mv x12, %[arg2]\n"
        "ecall\n"
        "mv %[ret_val], x10"
        : [ret_val] "=r"(ret_val)
        : [sbi_type] "r"(sbi_type), [arg0] "r"(arg0), [arg1] "r"(arg1), [arg2] "r"(arg2)
        : "memory");
    return ret_val;
}

/**
 * @brief Writes one character to the SBI console.
 *
 * @details The character is passed as the first argument of SBI_CONSOLE_PUTCHAR; the other
 * arguments are set to 0.
 */
void sbi_console_putchar(unsigned char ch) {
    sbi_call(SBI_CONSOLE_PUTCHAR, ch, 0, 0);
}

/**
 * @brief Schedules the next timer interrupt.
 *
 * @details @p stime_value is passed as the argument of SBI_SET_TIMER; the other arguments are set to 0.
 */
void sbi_set_timer(unsigned long long stime_value) {
    sbi_call(SBI_SET_TIMER, stime_value, 0, 0);
}

/**
 * @brief Reads one character from the SBI console.
 *
 * @details The firmware's return value is returned directly. When no character is pending, the
 * firmware returns -1 and the call does not block.
 */
int sbi_console_getchar(void) {
    return sbi_call(SBI_CONSOLE_GETCHAR, 0, 0, 0);
}

/**
 * @brief Shuts the machine down through the firmware.
 *
 * @details Uses the legacy SBI_SHUTDOWN call. It does not return when the firmware
 * supports it; QEMU then exits.
 */
void sbi_shutdown(void) {
    sbi_call(SBI_SHUTDOWN, 0, 0, 0);
}
