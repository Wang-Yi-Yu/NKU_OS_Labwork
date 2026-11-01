/**
 * @file   kmonitor.c
 * @brief  Simple command-line kernel monitor for controlling the kernel and exploring the system.
 *
 * kmonitor() is entered from __panic() (see panic.c). It reads a line, splits it into
 * arguments (parse()), looks the first word up in the commands[] table (runcmd()) and
 * calls the handler. The available commands are help, kerninfo and backtrace.
 */
#include <stdio.h>
#include <string.h>
#include <mmu.h>
#include <trap.h>
#include <kmonitor.h>
#include <kdebug.h>

/** @brief One entry of the monitor command table. */
struct command {
    const char *name; /**< Command name typed by the user. */
    const char *desc; /**< One-line description shown by `help`. */
    /** Handler; return -1 to force the monitor to exit. */
    int (*func)(int argc, char **argv, struct trapframe *tf);
};

/** @brief The monitor command table. */
static struct command commands[] = {
    {"help", "Display this list of commands.", mon_help},
    {"kerninfo", "Display information about the kernel.", mon_kerninfo},
    {"backtrace", "Print backtrace of stack frame.", mon_backtrace},
};

/** @brief Return whether the kernel is in the panic state; defined in kern/debug/panic.c. */
bool is_kernel_panic(void);

/** @brief Number of entries of commands[]. */
#define NCOMMANDS (sizeof(commands) / sizeof(struct command))

/***** Kernel monitor command interpreter *****/

/** @brief Maximum number of arguments of one command line. */
#define MAXARGS 16
/** @brief Characters that separate arguments. */
#define WHITESPACE " \t\n\r"

/**
 * @brief Split a command buffer into whitespace-separated arguments, in place.
 *
 * Separators are overwritten with '\0'; argv[i] points into @p buf.
 *
 * @param buf  NUL-terminated command line (modified)
 * @param argv output array of at least MAXARGS pointers
 * @return the number of arguments found
 */
static int parse(char *buf, char **argv) {
    int argc = 0;
    while (1) {
        // find global whitespace
        while (*buf != '\0' && strchr(WHITESPACE, *buf) != NULL) {
            *buf++ = '\0';
        }
        if (*buf == '\0') {
            break;
        }

        // save and scan past next arg
        if (argc == MAXARGS - 1) {
            cprintf("Too many arguments (max %d).\n", MAXARGS);
        }
        argv[argc++] = buf;
        while (*buf != '\0' && strchr(WHITESPACE, *buf) == NULL) {
            buf++;
        }
    }
    return argc;
}

/**
 * @brief Parse a command line and run the matching command.
 *
 * @param buf command line (modified by parse())
 * @param tf  trap frame passed through to the handler, may be NULL
 * @return the handler's return value (negative means exit the monitor); 0 for an empty or
 *         unknown command
 */
static int runcmd(char *buf, struct trapframe *tf) {
    char *argv[MAXARGS];
    int argc = parse(buf, argv);
    if (argc == 0) {
        return 0;
    }
    int i;
    for (i = 0; i < NCOMMANDS; i++) {
        if (strcmp(commands[i].name, argv[0]) == 0) {
            return commands[i].func(argc - 1, argv + 1, tf);
        }
    }
    cprintf("Unknown command '%s'\n", argv[0]);
    return 0;
}

/***** Implementations of basic kernel monitor commands *****/

/**
 * @brief Run the interactive kernel monitor until a command asks to exit.
 *
 * Prints a banner, dumps @p tf if it is not NULL, then repeatedly prompts with `K> ` and runs
 * the commands read with readline().
 *
 * @param tf trap frame of the interrupted context, or NULL
 */
void kmonitor(struct trapframe *tf) {
    cprintf("Welcome to the kernel debug monitor!!\n");
    cprintf("Type 'help' for a list of commands.\n");

    if (tf != NULL) {
        print_trapframe(tf);
    }

    char *buf;
    while (1) {
        if ((buf = readline("K> ")) != NULL) {
            if (runcmd(buf, tf) < 0) {
                break;
            }
        }
    }
}

/**
 * @brief Command `help`: print the name and description of every monitor command.
 * @return 0
 */
int mon_help(int argc, char **argv, struct trapframe *tf) {
    int i;
    for (i = 0; i < NCOMMANDS; i++) {
        cprintf("%s - %s\n", commands[i].name, commands[i].desc);
    }
    return 0;
}

/**
 * @brief Command `kerninfo`: call print_kerninfo() (kern/debug/kdebug.c) to print the memory
 *        occupancy of the kernel.
 * @return 0
 */
int mon_kerninfo(int argc, char **argv, struct trapframe *tf) {
    print_kerninfo();
    return 0;
}

/**
 * @brief Command `backtrace`: call print_stackframe() (kern/debug/kdebug.c) to print a
 *        backtrace of the stack.
 * @return 0
 */
int mon_backtrace(int argc, char **argv, struct trapframe *tf) {
    print_stackframe();
    return 0;
}
