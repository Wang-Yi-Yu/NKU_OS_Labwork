/**
 * @file   syscall.c
 * @brief  System call dispatch table for the kernel.
 *
 * A user program puts the syscall number in a0 and up to five arguments in a1..a5, then executes
 * ecall. The trap path (exception_handler() in kern/trap/trap.c) calls syscall(), which looks up
 * the handler in the table below and passes it the arguments. The handler's return value is
 * stored in tf->gpr.a0, where the user program reads it.
 *
 * The table is indexed by the SYS_* numbers from libs/unistd.h. Entries that are not listed are
 * NULL, so they must not be called.
 */

#include <unistd.h>
#include <proc.h>
#include <syscall.h>
#include <trap.h>
#include <stdio.h>
#include <pmm.h>
#include <assert.h>


/**
 * @brief Table of kernel handlers, indexed by syscall number.
 *
 * Each handler takes the array of arguments as arg[] and returns the value that is delivered to
 * the user program in a0. The handlers are
 * sys_exit, sys_fork, sys_wait, sys_exec, sys_yield, sys_kill, sys_getpid, sys_putc and
 * sys_pgdir. Their definitions are part of the lab5 exercise and are not in this file yet.
 * The numbers SYS_clone, SYS_sleep, SYS_gettime, SYS_brk, SYS_mmap, SYS_munmap and SYS_shmem have
 * no entry.
 */
static int (*syscalls[])(uint64_t arg[]) = {
    [SYS_exit] sys_exit,     [SYS_fork] sys_fork,   [SYS_wait] sys_wait,
    [SYS_exec] sys_exec,     [SYS_yield] sys_yield, [SYS_kill] sys_kill,
    [SYS_getpid] sys_getpid, [SYS_putc] sys_putc,   [SYS_pgdir] sys_pgdir,
};

/**
 * @brief Number of slots in the syscalls table, one more than the largest SYS_* number in use.
 *
 * Valid syscall numbers are in [0, NUM_SYSCALLS). A number in this range can still have no
 * handler (a NULL slot), so check the table entry as well as the range.
 */
#define NUM_SYSCALLS ((sizeof(syscalls)) / (sizeof(syscalls[0])))

//lab5 function1 YOUR CODE
// Contract: the lab5 exercise is syscall() and the sys_* handlers above. syscall() reads the
// number from a0 (a0 = syscall number) and the arguments from a1..a5. It stores the handler's
// result in tf->gpr.a0. A number that is out of range (>= NUM_SYSCALLS) or has no handler must
// not be dispatched; it should return -E_INVAL, which test_lab5_syscall() checks for number 0.
