/**
 * @file   intr.c
 * @brief  Global interrupt enable/disable in S mode, one CSR instruction each.
 *
 * sstatus.SIE is the global switch for S-mode interrupts. It is separate from the per-source
 * enable bits in the sie register (see clock_init() for the timer bit). On a trap the hardware
 * saves SIE into SPIE and clears SIE, and sret restores it.
 */
#include <intr.h>
#include <riscv.h>

/** @brief Enable interrupts in S mode (set sstatus.SIE). */
void intr_enable(void) {
    set_csr(sstatus, SSTATUS_SIE);
}

/** @brief Disable interrupts in S mode (clear sstatus.SIE). */
void intr_disable(void) {
    clear_csr(sstatus, SSTATUS_SIE);
}
