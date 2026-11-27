/**
 * @file   console.h
 * @brief  Console (character input/output) driver interface.
 *
 * Character output and input are both performed through SBI calls handled by
 * the M-mode firmware; this module does not touch UART registers directly.
 * Input is polled and kept in a circular buffer (see console.c); the "stdin"
 * device of the file system drains that buffer.
 * Higher-level formatted output (cprintf etc.) is declared in libs/stdio.h.
 */

#ifndef __KERN_DRIVER_CONSOLE_H__
#define __KERN_DRIVER_CONSOLE_H__

/**
 * @brief Initialise the console device.
 *
 * Reads and discards one character through SBI so that the console input path is
 * initialised; the firmware has already configured the serial port itself.
 * @note May be called before any character is output, but this is not required.
 */
void cons_init(void);

/**
 * @brief Output one character to the console.
 *
 * A backspace is expanded to "backspace, space, backspace" so that the terminal
 * erases the previous character.
 *
 * @param[in] c  Character to output (ASCII code).
 */
void cons_putc(int c);

/**
 * @brief Read one character from the console, non-blocking.
 *
 * Polls the serial port first, then returns the oldest character of the circular
 * input buffer.
 *
 * @return The character read, or 0 if no input is waiting.
 */
int cons_getc(void);

/**
 * @brief Serial interrupt entry point: poll the serial port and buffer every pending character.
 */
void serial_intr(void);

/**
 * @brief Keyboard interrupt entry point: try to read input characters from the keyboard.
 *
 * QEMU has no separate keyboard, so this just calls serial_intr().
 */
void kbd_intr(void);

#endif /* !__KERN_DRIVER_CONSOLE_H__ */
