/**
 * @file   sh.c
 * @brief  The user shell: reads a command line, splits it into words and runs the program (lab8).
 *
 * By default user_main() in kern/process/proc.c starts sh as the first user process. sh runs in
 * the interactive mode when started without arguments, and reads its commands from a file when
 * given one. Each command line is processed by a child process created with fork(): runcmd()
 * parses the line with gettoken(), applies the redirections < and >, and calls __exec()
 * (user/libs/ulib.c), which issues SYS_exec. The kernel then loads the program from the file
 * system through do_execve(). The shell itself waits with waitpid() and prints the error code of a
 * failed command.
 *
 * Supported syntax: words separated by spaces, double quotes around words that contain spaces,
 * the redirections "<" and ">", and the separator ";". A word that does not name an existing file
 * is looked up in the root directory of the file system (a "/" is put in front of it). The pipe
 * symbol "|" is recognised by the parser but not supported (see the notes of runcmd()).
 */
#include <ulib.h>
#include <stdio.h>
#include <string.h>
#include <dir.h>
#include <file.h>
#include <error.h>
#include <unistd.h>

/**
 * @brief Print a formatted message on the standard output (descriptor 1).
 * @param ... format string and arguments, as for printf()
 */
#define printf(...) fprintf(1, __VA_ARGS__)
/** @brief Print one character on the standard output (descriptor 1). */
#define putc(c) printf("%c", c)

/** @brief Size in bytes of the line buffers of readline() and runcmd(), and of shcwd. */
#define BUFSIZE 4096
/** @brief Characters that separate two words of a command line. */
#define WHITESPACE " \t\r\n"
/**
 * @brief Characters that are tokens of their own: "<" input, ">" output, "|" pipe, "&" (not
 *        supported), ";" command separator.
 */
#define SYMBOLS "<|>&;"

/**
 * @brief Working directory set by the "cd" command, as typed by the user.
 * @note Only the child process that runs the command sets it, so the value never reaches the
 *       shell (see the note of main()).
 */
char shcwd[BUFSIZE];

/**
 * @brief Split off the next token of a command line, in place.
 *
 * The line is changed in place: the white space before a token and the end of each token are
 * overwritten with NUL characters, so the token is a normal C string. A double quote anywhere in
 * a word starts a quoted part that ends at the next double quote. The quote characters are
 * replaced by spaces (they are not removed), and white space and symbols inside the quoted part
 * do not end the word.
 * @param[in,out] p1  on entry, the rest of the line; on return, the rest after this token, or NULL
 *                    if the line has no more characters
 * @param[out]    p2  on return, the start of the token
 * @return 'w' for a word, the symbol character ('<', '>', '|', '&' or ';') for a symbol, or 0 when
 *         no token is left
 */
int gettoken(char **p1, char **p2) {
    char *s;
    if ((s = *p1) == NULL) {
        return 0;
    }
    while (strchr(WHITESPACE, *s) != NULL) {
        *s++ = '\0';
    }
    if (*s == '\0') {
        return 0;
    }

    *p2 = s;
    int token = 'w';
    if (strchr(SYMBOLS, *s) != NULL) {
        token = *s, *s++ = '\0';
    } else {
        bool flag = 0;
        while (*s != '\0' && (flag || strchr(WHITESPACE SYMBOLS, *s) == NULL)) {
            if (*s == '"') {
                *s = ' ', flag = !flag;
            }
            s++;
        }
    }
    *p1 = (*s != '\0' ? s : NULL);
    return token;
}

/**
 * @brief Read one line from the standard input, echoing it, with simple line editing.
 *
 * Characters are read one at a time from descriptor 0 and echoed. A backspace removes the last
 * character; a newline or carriage return ends the line and is not stored. Characters beyond
 * BUFSIZE - 1 are dropped. The line is returned without the line terminator.
 * @param[in] prompt  text printed before reading, or NULL for no prompt (used in command-file mode)
 * @return a pointer to a static buffer that holds the NUL-terminated line, or NULL when the
 *         input ends (end of file with no characters read), when a read fails, or when Ctrl-C
 *         (character 3) is typed
 * @note The buffer is static and is overwritten by the next call.
 */
char *readline(const char *prompt) {
    static char buffer[BUFSIZE];
    if (prompt != NULL) {
        printf("%s", prompt);
    }
    int ret, i = 0;
    while (1) {
        char c;
        if ((ret = read(0, &c, sizeof(char))) < 0) {
            return NULL;
        } else if (ret == 0) {
            if (i > 0) {
                buffer[i] = '\0';
                break;
            }
            return NULL;
        }

        if (c == 3) {
            return NULL;
        } else if (c >= ' ' && i < BUFSIZE - 1) {
            putc(c);
            buffer[i++] = c;
        } else if (c == '\b' && i > 0) {
            putc(c);
            i--;
        } else if (c == '\n' || c == '\r') {
            putc(c);
            buffer[i] = '\0';
            break;
        }
    }
    return buffer;
}

/** @brief Print the usage line of the shell. */
void usage(void) {
    printf("usage: sh [command-file]\n");
}

/**
 * @brief Redirect a standard descriptor to a file.
 *
 * Closes @p fd2, opens @p filename and makes @p fd2 refer to it. Used for "<" and for the
 * command-file argument of main() (fd2 = 0), and for ">" (fd2 = 1).
 * @param[in] fd2        standard descriptor to redirect (0 = input, 1 = output)
 * @param[in] filename   file to open
 * @param[in] open_flags flags for open(), for example O_RDONLY or O_RDWR | O_TRUNC | O_CREAT
 * @return 0 on success, a negative error code otherwise (for example -E_NOENT when the file does
 *         not exist); on failure @p fd2 stays closed
 */
int reopen(int fd2, const char *filename, uint32_t open_flags) {
    int ret, fd1;
    close(fd2);
    if ((ret = open(filename, open_flags)) >= 0 && ret != fd2) {
        close(fd2);
        fd1 = ret, ret = dup2(fd1, fd2);
        close(fd1);
    }
    return ret < 0 ? ret : 0;
}

/**
 * @brief Check that a file can be opened for reading.
 * @param[in] name  path of the file
 * @return 0 if the file exists and can be opened (it is closed again), otherwise the negative
 *         error code of open(), typically -E_NOENT
 */
int testfile(const char *name) {
    int ret;
    if ((ret = open(name, O_RDONLY)) < 0) {
        return ret;
    }
    close(ret);
    return 0;
}

/**
 * @brief Parse one command line and run it.
 *
 * Called in a child process by main(). The tokens of the line are collected into argv: words are
 * arguments, "<" and ">" redirect the standard input and output to the file named by the next
 * word, and ";" runs the command so far in a child process and waits for it before the rest of the
 * line is parsed (the "again" label). An end of line runs the command (the "runit" label): an empty
 * command does nothing, "cd" with exactly one argument sets shcwd (any other number of arguments
 * returns -1), and any other command is started with __exec(). If testfile() reports that argv[0]
 * does not exist, "/" is put in front of it and the name is tried in the root directory.
 * @param[in,out] cmd  the command line; it is changed in place by gettoken()
 * @return 0 for an empty line and for a successful "cd"; otherwise the error code of the failing
 *         step: the negative result of reopen(), testfile() or __exec(), or -1 for a syntax error
 *         or too many arguments. When __exec() succeeds it does not return.
 * @note The "|" branch is not functional: the call that creates the pipe is commented out, so
 *       p[] is never set before it is used. Typing "|" therefore passes undefined descriptors to
 *       dup2().
 * @note The symbol "&" is recognised by gettoken() but has no case here, so it reports
 *       "bad return" and fails.
 */
int runcmd(char *cmd) {
    static char argv0[BUFSIZE];
    static const char *argv[EXEC_MAX_ARG_NUM + 1];  //must be static!
    char *t;
    int argc, token, ret, p[2];
again:
    argc = 0;
    while (1) {
        switch (token = gettoken(&cmd, &t)) {
            case 'w':
                if (argc == EXEC_MAX_ARG_NUM) {
                    printf("sh error: too many arguments\n");
                    return -1;
                }
                argv[argc++] = t;
                break;
            case '<':
                if (gettoken(&cmd, &t) != 'w') {
                    printf("sh error: syntax error: < not followed by word\n");
                    return -1;
                }
                if ((ret = reopen(0, t, O_RDONLY)) != 0) {
                    return ret;
                }
                break;
            case '>':
                if (gettoken(&cmd, &t) != 'w') {
                    printf("sh error: syntax error: > not followed by word\n");
                    return -1;
                }
                if ((ret = reopen(1, t, O_RDWR | O_TRUNC | O_CREAT)) != 0) {
                    return ret;
                }
                break;
            case '|':
                //  if ((ret = pipe(p)) != 0) {
                //      return ret;
                //  }
                if ((ret = fork()) == 0) {
                    close(0);
                    if ((ret = dup2(p[0], 0)) < 0) {
                        return ret;
                    }
                    close(p[0]), close(p[1]);
                    goto again;
                } else {
                    if (ret < 0) {
                        return ret;
                    }
                    close(1);
                    if ((ret = dup2(p[1], 1)) < 0) {
                        return ret;
                    }
                    close(p[0]), close(p[1]);
                    goto runit;
                }
                break;
            case 0:
                goto runit;
            case ';':
                if ((ret = fork()) == 0) {
                    goto runit;
                } else {
                    if (ret < 0) {
                        return ret;
                    }
                    waitpid(ret, NULL);
                    goto again;
                }
                break;
            default:
                printf("sh error: bad return %d from gettoken\n", token);
                return -1;
        }
    }

runit:
    if (argc == 0) {
        return 0;
    } else if (strcmp(argv[0], "cd") == 0) {
        if (argc != 2) {
            return -1;
        }
        strcpy(shcwd, argv[1]);
        return 0;
    }
    if ((ret = testfile(argv[0])) != 0) {
        if (ret != -E_NOENT) {
            return ret;
        }
        snprintf(argv0, sizeof(argv0), "/%s", argv[0]);
        argv[0] = argv0;
    }
    argv[argc] = NULL;
    return __exec(argv[0], argv);
}

/**
 * @brief Entry point of the shell.
 *
 * Without arguments the shell is interactive and prints the prompt "$ ". With one argument, the
 * argument is a command file: the standard input is redirected to it and no prompt is printed.
 * For each line read, a child process runs runcmd(); the shell waits for it and prints
 * "error: <code> - <message>" when the command fails. The loop ends when readline() returns NULL.
 * @param argc number of arguments, 1 or 2 (more is an error)
 * @param argv argv[1], if present, is the command file
 * @return 0 at the end of input or after Ctrl-C; -1 after a usage error; the error code of
 *         reopen() if the command file cannot be opened
 * @note The "cd" command runs in the child that executes runcmd(), so the shell never sees the new
 *       working directory. The test on shcwd after waitpid() therefore never succeeds.
 */
int main(int argc, char **argv) {
    cputs("user sh is running!!!");
    int ret, interactive = 1;
    if (argc == 2) {
        if ((ret = reopen(0, argv[1], O_RDONLY)) != 0) {
            return ret;
        }
        interactive = 0;
    } else if (argc > 2) {
        usage();
        return -1;
    }
    //shcwd = malloc(BUFSIZE);
    assert(shcwd != NULL);

    char *buffer;
    while ((buffer = readline((interactive) ? "$ " : NULL)) != NULL) {
        shcwd[0] = '\0';
        int pid;
        if ((pid = fork()) == 0) {
            ret = runcmd(buffer);
            exit(ret);
        }
        assert(pid >= 0);
        if (waitpid(pid, &ret) == 0) {
            if (ret == 0 && shcwd[0] != '\0') {
                ret = 0;
            }
            if (ret != 0) {
                printf("error: %d - %e\n", ret, ret);
            }
        }
    }
    return 0;
}
