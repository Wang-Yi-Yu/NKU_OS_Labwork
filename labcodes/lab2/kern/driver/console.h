/**
 * @file   console.h
 * @brief  Console (character input/output) driver interface.
 *
 * Character output and input are both performed through SBI calls handled by
 * the M-mode firmware; this module does not touch UART registers directly.
 * Higher-level formatted output (cprintf etc.) is declared in libs/stdio.h.
 */

#ifndef __KERN_DRIVER_CONSOLE_H__
#define __KERN_DRIVER_CONSOLE_H__

/**
 * @brief Initialise the console device.
 *
 * The current implementation is a no-op: the SBI firmware has already
 * initialised the serial port, so the kernel needs no extra configuration.
 * @note May be called before any character is output, but this is not required.
 */
void cons_init(void);

/**
 * @brief Output one character to the console.
 *
 * The value is cast to unsigned char before being passed to SBI, so only the
 * low 8 bits are used; values outside 0..255 are truncated.
 *
 * @param[in] c  Character to output (ASCII code).
 */
void cons_putc(int c);

/**
 * @brief Read one character from the console, non-blocking.
 *
 * Returns the SBI call result directly. When a character is available it is
 * the ASCII code of that character. When none is available, the SBI legacy
 * convention is to return -1 (not 0).
 *
 * @return The character read (0..255), or the value SBI returns when no input is waiting (-1).
 * @warning Callers must follow the SBI convention when detecting "no input"; do not treat 0 as the
 *          only signal. libs/stdio.c getchar relies on 0 to mean "no input", which conflicts with this behaviour.
 */
int cons_getc(void);

/**
 * @brief Serial interrupt entry point: try to read input characters from the serial port (placeholder).
 *
 * The current version is an empty function. No serial interrupt is registered
 * and no data is written to an input buffer.
 */
void serial_intr(void);

/**
 * @brief Keyboard interrupt entry point: try to read input characters from the keyboard (placeholder).
 *
 * The current version is an empty function; the kernel has no keyboard driver.
 */
void kbd_intr(void);

#endif /* !__KERN_DRIVER_CONSOLE_H__ */
