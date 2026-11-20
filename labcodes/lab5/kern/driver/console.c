/**
 * @file   console.c
 * @brief  Console driver: character output and input through the OpenSBI legacy console calls.
 *
 * Only cons_putc and cons_getc do real work; they call sbi_console_putchar / sbi_console_getchar
 * with interrupts disabled around the call. The keyboard and serial interrupt hooks and cons_init
 * are empty stubs, since input comes through SBI and not through a UART driver.
 */
#include <sbi.h>
#include <sync.h>
#include <defs.h>
#include <console.h>

/**
 * @brief Feed keyboard input into the console buffer. Empty stub.
 */
void kbd_intr(void) {}

/**
 * @brief Feed serial-port input into the console buffer. Empty stub.
 */
void serial_intr(void) {}

/**
 * @brief Initialise the console devices. Empty stub (the SBI console needs no setup).
 */
void cons_init(void) {}

/**
 * @brief Print one character on the console.
 *
 * Interrupts are disabled with local_intr_save around the SBI call, so the output is not
 * interleaved with an interrupt handler that also prints.
 *
 * @param[in] c  Character to print; only the low 8 bits are used.
 */
void cons_putc(int c) {
    bool intr_flag;
    local_intr_save(intr_flag);
    { sbi_console_putchar((unsigned char)c); }
    local_intr_restore(intr_flag);
}

/**
 * @brief Read the next input character from the console without blocking.
 *
 * The SBI call is made with interrupts disabled. Note that the legacy SBI getchar returns
 * -1 (not 0) when no input is waiting, and this function returns that value unchanged.
 *
 * @return The character read, or -1 if no input is waiting.
 */
int cons_getc(void) {
    int c = 0;
    bool intr_flag;
    local_intr_save(intr_flag);
    { c = sbi_console_getchar(); }
    local_intr_restore(intr_flag);
    return c;
}
