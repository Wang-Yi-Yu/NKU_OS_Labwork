/**
 * @file   readline.c
 * @brief  Line input from stdin with echo and backspace handling.
 *
 * The prototype and full documentation of readline() are in libs/stdio.h. The result is stored
 * in a single static buffer shared by all calls, so each call overwrites the previous result.
 */
#include <stdio.h>

/** @brief Size in bytes of the static line buffer used by readline(), including the terminating '\0'. */
#define BUFSIZE 1024

/** @brief Static buffer that holds the line returned by readline(); shared by all calls. */
static char buf[BUFSIZE];

/**
 * @brief Reads one line from stdin, echoing characters as they are typed.
 *
 * Writes @p prompt first, then reads characters until '\n' or '\r'. Printable characters are
 * appended to buf (at most BUFSIZE - 1 of them); '\b' removes the last character when buf is
 * non-empty; the terminating newline character is echoed. Full description is in libs/stdio.h.
 *
 * @param prompt  Prompt to print first; may be NULL or empty.
 * @return Pointer to the static buffer holding the line, or NULL if the underlying read fails
 *         (returns a negative value).
 */
char *readline(const char *prompt) {
    if (prompt != NULL) {
        cprintf("%s", prompt);
    }
    int i = 0, c;
    while (1) {
        c = getchar();
        if (c < 0) {
            return NULL;
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
