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
 *          The loop does not block: getchar() returns a negative value when no input is waiting,
 *          and readline() then returns NULL.
 *
 * @param[in] prompt  String printed before reading; NULL or "" prints nothing.
 * @return Pointer to the internal buffer holding the line (without the newline), or NULL on read error.
 * @warning The returned pointer refers to a static buffer that the next call overwrites; copy it first.
 */
/* *
 * readline - get a line from stdin
 * @prompt:     the string to be written to stdout
 *
 * The readline() function will write the input string @prompt to
 * stdout first. If the @prompt is NULL or the empty string,
 * no prompt is issued.
 *
 * This function will keep on reading characters and saving them to buffer
 * 'buf' until '\n' or '\r' is encountered.
 *
 * Note that, if the length of string that will be read is longer than
 * buffer size, the end of string will be discarded.
 *
 * The readline() function returns the text of the line read. If some errors
 * are happened, NULL is returned. The return value is a global variable,
 * thus it should be copied before it is used.
 * */
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
