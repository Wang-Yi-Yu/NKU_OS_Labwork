/**
 * @file   kmonitor.c
 * @brief  Simple interactive kernel monitor: a command loop with help, kerninfo and backtrace commands.
 *
 * kmonitor() prints a welcome message, optionally a trap frame, and then reads commands with readline().
 * The command table is searched by name. The monitor is also the loop that panic() spins in.
 */
#include <stdio.h>
#include <string.h>
#include <mmu.h>
#include <trap.h>
#include <kmonitor.h>
#include <kdebug.h>
#include <sbi.h>

/* *
 * Simple command-line kernel monitor useful for controlling the
 * kernel and exploring the system interactively.
 * */

/**
 * @brief Entry of the monitor command table.
 */
struct command {
    /** @brief Command name typed by the user, e.g. "help". */
    const char *name;
    /** @brief One-line description printed by the help command. */
    const char *desc;
    // return -1 to force monitor to exit
    /** @brief Handler: receives the arguments after the command name; returns -1 to leave the monitor. */
    int (*func)(int argc, char **argv, struct trapframe *tf);
};

/**
 * @brief Table of monitor commands, searched in order by runcmd().
 *
 * Built-in commands: help, kerninfo (print_kerninfo) and backtrace (print_stackframe).
 */
static struct command commands[] = {
    {"help", "Display this list of commands.", mon_help},
    {"kerninfo", "Display information about the kernel.", mon_kerninfo},
    {"backtrace", "Print backtrace of stack frame.", mon_backtrace},
};

/**
 * @brief Query whether the kernel has entered the panic state. Defined in kern/debug/panic.c.
 * @return Non-zero if a panic has occurred.
 */
/* return if kernel is panic, in kern/debug/panic.c */
bool is_kernel_panic(void);

/** @brief Number of entries in the commands table. */
#define NCOMMANDS (sizeof(commands) / sizeof(struct command))

/***** Kernel monitor command interpreter *****/

/** @brief Maximum number of argv entries, including the command name. */
#define MAXARGS 16
/** @brief Characters that separate arguments on the command line. */
#define WHITESPACE " \t\n\r"

/**
 * @brief Split a command line in place into whitespace-separated arguments.
 *
 * Every separator is overwritten with '\0' and argv[i] points at the start of each token.
 * Arguments beyond MAXARGS - 1 are not stored; a message is printed instead.
 *
 * @param[in,out] buf   Mutable NUL-terminated command line; modified in place.
 * @param[out]    argv  Array of at least MAXARGS pointers that receives the tokens.
 * @return The number of tokens stored in argv.
 */
/* parse - parse the command buffer into whitespace-separated arguments */
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
 * @brief Parse one command line and dispatch it to the matching command handler.
 *
 * @param[in,out] buf  Command line from readline(); modified by parse().
 * @param[in]     tf   Trap frame passed through to the handler (may be NULL).
 * @return The handler's return value (-1 asks the monitor to exit), 0 for an empty line or an unknown command.
 */
/* *
 * runcmd - parse the input string, split it into separated arguments
 * and then lookup and invoke some related commands/
 * */
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
 * @brief Run the interactive kernel monitor.
 *
 * Prints a welcome message and, if @p tf is not NULL, the trap frame. It then calls
 * sbi_shutdown() before entering the read-parse-dispatch loop.
 *
 * @param[in] tf  Trap frame that caused the monitor to be entered, or NULL (e.g. from panic).
 * @note In lab4 sbi_shutdown() is called before the loop, so the machine is asked to power off
 *       as soon as the monitor starts. The loop is reached only if the firmware returns.
 * @warning Loops forever unless a command returns -1; panic() relies on this.
 */
void kmonitor(struct trapframe *tf) {
    cprintf("Welcome to the kernel debug monitor!!\n");
    cprintf("Type 'help' for a list of commands.\n");

    if (tf != NULL) {
        print_trapframe(tf);
    }
    sbi_shutdown();
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
 * @brief Command handler for "help": list every monitor command with its description.
 *
 * @param[in] argc  Number of arguments after the command name (ignored).
 * @param[in] argv  Arguments after the command name (ignored).
 * @param[in] tf    Trap frame (ignored).
 * @return Always 0.
 */
/* mon_help - print the information about mon_* functions */
int mon_help(int argc, char **argv, struct trapframe *tf) {
    int i;
    for (i = 0; i < NCOMMANDS; i++) {
        cprintf("%s - %s\n", commands[i].name, commands[i].desc);
    }
    return 0;
}

/**
 * @brief Command handler for "kerninfo": print the kernel layout by calling print_kerninfo().
 *
 * @param[in] argc  Number of arguments after the command name (ignored).
 * @param[in] argv  Arguments after the command name (ignored).
 * @param[in] tf    Trap frame (ignored).
 * @return Always 0.
 */
/* *
 * mon_kerninfo - call print_kerninfo in kern/debug/kdebug.c to
 * print the memory occupancy in kernel.
 * */
int mon_kerninfo(int argc, char **argv, struct trapframe *tf) {
    print_kerninfo();
    return 0;
}

/**
 * @brief Command handler for "backtrace": print the stack by calling print_stackframe().
 *
 * @param[in] argc  Number of arguments after the command name (ignored).
 * @param[in] argv  Arguments after the command name (ignored).
 * @param[in] tf    Trap frame (ignored).
 * @return Always 0 if it returns; in lab4 print_stackframe() panics, so it does not return.
 */
/* *
 * mon_backtrace - call print_stackframe in kern/debug/kdebug.c to
 * print a backtrace of the stack.
 * */
int mon_backtrace(int argc, char **argv, struct trapframe *tf) {
    print_stackframe();
    return 0;
}
