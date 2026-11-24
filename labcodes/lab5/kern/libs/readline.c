/**
 * @file   readline.c
 * @brief  Line editor for the kernel monitor: reads one line from the console into a static buffer.
 */
#include <stdio.h>

/** @brief Size in bytes of the static line buffer, including the terminating NUL. */
#define BUFSIZE 1024
/** @brief Storage for the most recently read line; overwritten by each readline() call. */
static char buf[BUFSIZE];

/**
 * @brief Read one line of input from the console, echoing it, and return it.
 *
 * @details Prints @p prompt if it is not NULL. Printable characters are echoed and stored.
 *          Backspace ('\b') removes the last character. Enter ('\n' or '\r') echoes a newline,
 *          terminates the buffer and returns it. Characters beyond BUFSIZE - 1 are dropped.
 *          getchar() returns a negative value when no input is waiting. Keep polling in that
 *          case, preserving the partial line until Enter is received.
 *
 * @param[in] prompt  String printed before reading; NULL or "" prints nothing.
 * @return Pointer to the internal buffer holding the completed line (without the newline).
 * @warning The returned pointer refers to a static buffer that the next call overwrites; copy it first.
 */
char *readline(const char *prompt) {
    if (prompt != NULL) {
        cprintf("%s", prompt);
    }
    int i = 0, c;
    while (1) {
        c = getchar();
        if (c < 0) {
            continue;
        } else if (c >= ' ' && i < BUFSIZE - 1) {
            cputchar(c);
            buf[i++] = c;
        } else if (c == '\b' && i > 0) {
            cputchar(c);
            i--;
        } else if (c == '\n' || c == '\r') {
            cputchar(c);
            buf[i] = '\0';
            return buf;
        }
    }
}
