/*
 * Copyright (C) 2015 Regents of the University of California
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

/**
 * @file   sbi.h
 * @brief  Inline wrappers for the legacy RISC-V SBI (Supervisor Binary Interface) calls served by OpenSBI.
 *
 * The kernel runs in S-mode and asks the M-mode firmware (OpenSBI) for services such as console I/O,
 * timer programming, IPIs, TLB shootdowns and shutdown by executing `ecall`. Each wrapper below places
 * the function ID in a7 and the arguments in a0..a2, then executes ecall. The IDs are the legacy (v0.1) ones.
 * Unlike a full libsbi, this header only contains static inline wrappers; there is no separate sbi.c.
 */

#ifndef __SBI_H__
#define __SBI_H__

#include <defs.h>

/**
 * @name Legacy SBI function IDs
 * @brief Values loaded into register a7 to select the SBI service requested by ecall.
 * @{
 */
#define SBI_SET_TIMER              0
#define SBI_CONSOLE_PUTCHAR        1
#define SBI_CONSOLE_GETCHAR        2
#define SBI_CLEAR_IPI              3
#define SBI_SEND_IPI               4
#define SBI_REMOTE_FENCE_I         5
#define SBI_REMOTE_SFENCE_VMA      6
#define SBI_REMOTE_SFENCE_VMA_ASID 7
#define SBI_SHUTDOWN               8
/** @} */

/**
 * @brief Generic SBI call with three arguments; returns the value the firmware leaves in a0.
 *
 * @param[in] which  SBI function ID, loaded into a7.
 * @param[in] arg0   First argument, loaded into a0 (a0 also carries the return value).
 * @param[in] arg1   Second argument, loaded into a1.
 * @param[in] arg2   Third argument, loaded into a2.
 * @return The value returned by the SBI implementation in a0.
 * @note A GNU statement expression. The "memory" clobber stops the compiler from moving memory
 *       accesses across the ecall.
 */
#define SBI_CALL(which, arg0, arg1, arg2)                                        \
    ({                                                                           \
        register uintptr_t a0 asm("a0") = (uintptr_t)(arg0);                     \
        register uintptr_t a1 asm("a1") = (uintptr_t)(arg1);                     \
        register uintptr_t a2 asm("a2") = (uintptr_t)(arg2);                     \
        register uintptr_t a7 asm("a7") = (uintptr_t)(which);                    \
        asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory"); \
        a0;                                                                      \
    })

/**
 * @name SBI_CALL shorthands
 * @brief SBI_CALL_n passes n arguments; unused argument registers are set to 0.
 * @param[in] which  SBI function ID.
 * @param[in] arg0   First argument (for _1 and _2).
 * @param[in] arg1   Second argument (for _2).
 * @{
 */
/* Lazy implementations until SBI is finalized */
#define SBI_CALL_0(which)             SBI_CALL(which, 0, 0, 0)
#define SBI_CALL_1(which, arg0)       SBI_CALL(which, arg0, 0, 0)
#define SBI_CALL_2(which, arg0, arg1) SBI_CALL(which, arg0, arg1, 0)
/** @} */

/**
 * @brief Print one character on the firmware debug console (legacy SBI_CONSOLE_PUTCHAR).
 *
 * @param[in] ch  Character to print; the firmware uses only its low 8 bits.
 */
static inline void sbi_console_putchar(int ch) {
    SBI_CALL_1(SBI_CONSOLE_PUTCHAR, ch);
}

/**
 * @brief Read one character from the firmware debug console without blocking (legacy SBI_CONSOLE_GETCHAR).
 *
 * @return The character read, or -1 if no input is waiting (legacy SBI convention; it is not 0).
 */
static inline int sbi_console_getchar(void) {
    return SBI_CALL_0(SBI_CONSOLE_GETCHAR);
}

/**
 * @brief Ask the firmware to raise a supervisor timer interrupt once the time CSR reaches a value
 *        (legacy SBI_SET_TIMER). The firmware also clears the pending supervisor timer bit (STIP).
 *
 * @param[in] stime_value  Absolute time CSR value at which the interrupt should fire. On RV32 it is
 *                         passed as two 32-bit halves (low word in a0, high word in a1).
 * @note Only one timer event can be pending at a time, so the next one must be armed from the interrupt handler.
 */
static inline void sbi_set_timer(uint64_t stime_value) {
#if __riscv_xlen == 32
    SBI_CALL_2(SBI_SET_TIMER, stime_value, stime_value >> 32);
#else
    SBI_CALL_1(SBI_SET_TIMER, stime_value);
#endif
}

/**
 * @brief Ask the firmware to shut the machine down (legacy SBI_SHUTDOWN). Used by the kernel monitor.
 *
 * @note Normally does not return. If the firmware ignores the request, the caller continues.
 */
static inline void sbi_shutdown(void) {
    SBI_CALL_0(SBI_SHUTDOWN);
}

/**
 * @brief Clear the pending supervisor software interrupt (IPI) of the calling hart (legacy SBI_CLEAR_IPI).
 */
static inline void sbi_clear_ipi(void) {
    SBI_CALL_0(SBI_CLEAR_IPI);
}

/**
 * @brief Send a supervisor software interrupt (IPI) to the harts selected by a mask (legacy SBI_SEND_IPI).
 *
 * @param[in] hart_mask  Pointer to a bit mask of hart IDs, passed to the firmware as a pointer.
 */
static inline void sbi_send_ipi(const unsigned long *hart_mask) {
    SBI_CALL_1(SBI_SEND_IPI, hart_mask);
}

/**
 * @brief Make the selected remote harts execute FENCE.I (legacy SBI_REMOTE_FENCE_I).
 *
 * @param[in] hart_mask  Pointer to a bit mask of hart IDs.
 */
static inline void sbi_remote_fence_i(const unsigned long *hart_mask) {
    SBI_CALL_1(SBI_REMOTE_FENCE_I, hart_mask);
}

/**
 * @brief Make the selected remote harts execute SFENCE.VMA (legacy SBI_REMOTE_SFENCE_VMA).
 *
 * @param[in] hart_mask  Pointer to a bit mask of hart IDs.
 * @param[in] start      Start address of the range to flush. Not forwarded by this wrapper.
 * @param[in] size       Size of the range to flush. Not forwarded by this wrapper.
 * @warning Only hart_mask is passed to the firmware (SBI_CALL_1), so start and size are ignored
 *          and the remote harts flush their whole TLB.
 */
static inline void sbi_remote_sfence_vma(const unsigned long *hart_mask, unsigned long start,
                                         unsigned long size) {
    SBI_CALL_1(SBI_REMOTE_SFENCE_VMA, hart_mask);
}

/**
 * @brief Make the selected remote harts execute SFENCE.VMA for one ASID (legacy SBI_REMOTE_SFENCE_VMA_ASID).
 *
 * @param[in] hart_mask  Pointer to a bit mask of hart IDs.
 * @param[in] start      Start address of the range to flush. Not forwarded by this wrapper.
 * @param[in] size       Size of the range to flush. Not forwarded by this wrapper.
 * @param[in] asid       Address space ID to flush. Not forwarded by this wrapper.
 * @warning Only hart_mask is passed to the firmware (SBI_CALL_1), so start, size and asid are ignored.
 */
static inline void sbi_remote_sfence_vma_asid(const unsigned long *hart_mask, unsigned long start,
                                              unsigned long size, unsigned long asid) {
    SBI_CALL_1(SBI_REMOTE_SFENCE_VMA_ASID, hart_mask);
}

#endif /* !__SBI_H__ */
