/**
 * @file   clock.c
 * @brief  Timer driver: reads the time CSR and schedules timer interrupts through OpenSBI.
 *
 * OpenSBI's sbi_set_timer() can only arm one timer event at a time, so the first event is armed
 * by clock_init() and the trap handler arms the next one (clock_set_next_event()) every time a
 * timer interrupt arrives. On QEMU the time CSR runs at 10 MHz, so one timebase period of
 * 100000 cycles is 10 ms, i.e. 100 interrupts per second.
 */
#include <clock.h>
#include <defs.h>
#include <sbi.h>
#include <stdio.h>
#include <riscv.h>

/** @brief Number of timer interrupts handled since clock_init(); incremented by the trap handler. */
volatile size_t ticks;

/**
 * @brief Read the 64-bit time CSR.
 *
 * On RV64 a single `rdtime` is enough. On RV32 the value is read as two halves, and the high
 * half is read twice to detect a carry between the two reads.
 *
 * @return the current value of the time CSR
 */
static inline uint64_t get_cycles(void) {
#if __riscv_xlen == 64
    uint64_t n;
    __asm__ __volatile__("rdtime %0" : "=r"(n));
    return n;
#else
    uint32_t lo, hi, tmp;
    __asm__ __volatile__("1:\n"
                         "rdtimeh %0\n"
                         "rdtime %1\n"
                         "rdtimeh %2\n"
                         "bne %0, %2, 1b"
                         : "=&r"(hi), "=&r"(lo), "=&r"(tmp));
    return ((uint64_t)hi << 32) | lo;
#endif
}

/** @brief Timer period in time CSR cycles (hard-coded): 100000 cycles = 10 ms at 10 MHz. */
static uint64_t timebase = 100000;

/**
 * @brief Initialise the timer: enable the supervisor timer interrupt and arm the first event.
 *
 * Sets sie.STIE (MIP_STIP), arms the first event with clock_set_next_event(), resets ::ticks to
 * 0 and prints "++ setup timer interrupts". The global enable bit sstatus.SIE is set separately
 * by intr_enable().
 */
void clock_init(void) {
    // enable timer interrupt in sie
    set_csr(sie, MIP_STIP);
    // divided by 500 when using Spike(2MHz)
    // divided by 100 when using QEMU(10MHz)
    // timebase = sbi_timebase() / 500;
    clock_set_next_event();

    // initialize time counter 'ticks' to zero
    ticks = 0;

    cprintf("++ setup timer interrupts\n");
}

/**
 * @brief Arm the next timer interrupt, `timebase` cycles after the current time.
 *
 * Must be called once per timer interrupt, otherwise no further timer interrupt occurs.
 */
void clock_set_next_event(void) {
    sbi_set_timer(get_cycles() + timebase);
}
