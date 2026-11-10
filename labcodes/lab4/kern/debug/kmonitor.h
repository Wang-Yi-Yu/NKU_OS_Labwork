/**
 * @file   kmonitor.h
 * @brief  Interface of the kernel monitor, a tiny interactive command-line debugger.
 *
 * Every command has the signature `int mon_xxx(int argc, char **argv, struct trapframe *tf)`.
 * A command returning a negative value makes kmonitor() return. Only mon_help,
 * mon_kerninfo and mon_backtrace are defined in kmonitor.c; the other commands are
 * declared for later labs and have no definition yet.
 */
#ifndef __KERN_DEBUG_MONITOR_H__
#define __KERN_DEBUG_MONITOR_H__

#include <trap.h>

/**
 * @brief Run the interactive monitor loop (prompt `K> `), reading commands with readline().
 * @param tf trap frame of the interrupted context, printed on entry if not NULL
 */
void kmonitor(struct trapframe *tf);

/** @brief List all monitor commands. */
int mon_help(int argc, char **argv, struct trapframe *tf);
/** @brief Print the kernel memory layout (print_kerninfo()). */
int mon_kerninfo(int argc, char **argv, struct trapframe *tf);
/** @brief Print a stack backtrace (print_stackframe()). */
int mon_backtrace(int argc, char **argv, struct trapframe *tf);
/** @brief Declared only: continue execution. */
int mon_continue(int argc, char **argv, struct trapframe *tf);
/** @brief Declared only: single step. */
int mon_step(int argc, char **argv, struct trapframe *tf);
/** @brief Declared only: set a breakpoint. */
int mon_breakpoint(int argc, char **argv, struct trapframe *tf);
/** @brief Declared only: set a watchpoint. */
int mon_watchpoint(int argc, char **argv, struct trapframe *tf);
/** @brief Declared only: delete a debug register setting. */
int mon_delete_dr(int argc, char **argv, struct trapframe *tf);
/** @brief Declared only: list debug register settings. */
int mon_list_dr(int argc, char **argv, struct trapframe *tf);

#endif /* !__KERN_DEBUG_MONITOR_H__ */
