/**
 * @file   console.c
 * @brief  Console driver implementation, based on SBI character I/O calls.
 *
 * See console.h for the full documentation of each function. The serial_intr
 * and kbd_intr functions in this file are placeholders.
 */

#include <sbi.h>
#include <console.h>

/**
 * @brief Keyboard interrupt entry point (placeholder, empty function).
 */
void kbd_intr(void) {}

/**
 * @brief Serial interrupt entry point (placeholder, empty function).
 */
void serial_intr(void) {}

/**
 * @brief Initialise the console device (no-op).
 *
 * @details Performs no hardware configuration and relies on the serial port
 *          already initialised by the SBI firmware.
 */
void cons_init(void) {}

/**
 * @brief Output one character to the console.
 *
 * @details Casts @p c to unsigned char and sends it with sbi_console_putchar;
 *          bits above the low 8 are discarded.
 */
void cons_putc(int c) {
    sbi_console_putchar((unsigned char)c);
}

/**
 * @brief Read one character from the console, non-blocking.
 *
 * @details Returns the result of sbi_console_getchar unchanged. When no input
 *          is waiting, SBI returns -1 under the legacy convention, not 0; the
 *          caller must follow that convention to detect "no input".
 */
int cons_getc(void) {
    int c = 0;
    c = sbi_console_getchar();
    return c;
}
