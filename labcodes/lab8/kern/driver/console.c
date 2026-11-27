/**
 * @file   console.c
 * @brief  Console driver: character output through the OpenSBI legacy console calls and a
 *         buffered, polled character input path.
 *
 * Output goes straight to sbi_console_putchar. Input cannot use a device interrupt under QEMU, so
 * cons_getc() polls the SBI console, appends every character it finds to a 512-byte circular
 * buffer (cons_intr) and then hands out the oldest buffered character. Lab 8 turns the console
 * into the "stdin" device, whose write side (dev_stdin_write) is fed from the timer interrupt
 * handler through this buffer.
 */
#include <console.h>
#include <defs.h>
#include <sbi.h>
#include <sync.h>

/** @brief Size in bytes of the circular console input buffer. */
#define CONSBUFSIZE 512

/**
 * @brief Circular input buffer of the console.
 * @var cons::buf   storage for characters that were read but not yet consumed
 * @var cons::rpos  index of the next character cons_getc() returns
 * @var cons::wpos  index where cons_intr() stores the next character
 * @note The buffer is empty when rpos == wpos. It has no overflow check, so more than
 *       CONSBUFSIZE unread characters overwrite the oldest ones.
 */
static struct {
    uint8_t buf[CONSBUFSIZE];
    uint32_t rpos;
    uint32_t wpos;
} cons;

/**
 * @brief Drain a character source into the circular input buffer.
 *
 * Calls proc repeatedly until it returns -1 (no more input). A return value of 0 is ignored and
 * every other value is stored as one character, wrapping wpos at CONSBUFSIZE.
 *
 * @param[in] proc  Function that returns the next character, 0 for "ignore", or -1 for "no data".
 */
void cons_intr(int (*proc)(void)) {
    int c;
    while ((c = (*proc)()) != -1) {
        if (c != 0) {
            cons.buf[cons.wpos++] = c;
            if (cons.wpos == CONSBUFSIZE) {
                cons.wpos = 0;
            }
        }
    }
}

/**
 * @brief Feed keyboard input into the console buffer.
 *
 * QEMU has no separate keyboard here, so this simply polls the serial port.
 */
void kbd_intr(void) {
    serial_intr();
}

/**
 * @brief Read one character from the serial port through SBI.
 * @return The character (the DEL code 127 is mapped to backspace), or -1 if no input is waiting.
 */
int serial_proc_data(void) {
    int c = sbi_console_getchar();
    if (c < 0) {
        return -1;
    }
    if (c == 127) {
        c = '\b';
    }
    return c;
}

/**
 * @brief Poll the serial port and feed every pending character into the console buffer.
 */
void serial_intr(void) {
    cons_intr(serial_proc_data);
}

/**
 * @brief Print one character on the serial port.
 *
 * A backspace is expanded to "backspace, space, backspace" so that the terminal erases the
 * previous character instead of only moving the cursor.
 *
 * @param[in] c  Character to print.
 */
void serial_putc(int c) {
    if (c != '\b') {
        sbi_console_putchar(c);
    } else {
        sbi_console_putchar('\b');
        sbi_console_putchar(' ');
        sbi_console_putchar('\b');
    }
}

/**
 * @brief Initialise the console devices.
 *
 * Reads and discards one character through SBI so that the console input path is initialised.
 */
void cons_init(void) {
    sbi_console_getchar();
}

/**
 * @brief Print one character on the console.
 *
 * Interrupts are disabled with local_intr_save around the output, so the text is not interleaved
 * with an interrupt handler that also prints.
 *
 * @param[in] c  Character to print.
 */
void cons_putc(int c) {
    bool intr_flag;
    local_intr_save(intr_flag);
    { serial_putc(c); }
    local_intr_restore(intr_flag);
}

/**
 * @brief Read the next input character from the console without blocking.
 *
 * First polls the serial port, so the function also works with interrupts disabled (for example
 * from the kernel monitor), then takes the oldest character out of the circular buffer.
 *
 * @return The next buffered character, or 0 if none is waiting.
 */
int cons_getc(void) {
    int c = 0;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        serial_intr();

        if (cons.rpos != cons.wpos) {
            c = cons.buf[cons.rpos++];
            if (cons.rpos == CONSBUFSIZE) {
                cons.rpos = 0;
            }
        }
    }
    local_intr_restore(intr_flag);
    return c;
}
