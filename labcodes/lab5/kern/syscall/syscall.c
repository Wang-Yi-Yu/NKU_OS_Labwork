/**
 * @file   syscall.c
 * @brief  System call handlers (sys_*), the dispatch table and the syscall() entry of the kernel.
 *
 * A user program puts the syscall number in a0 and up to five arguments in a1..a5, then executes
 * ecall. The trap path (exception_handler() in kern/trap/trap.c) calls syscall(), which looks up
 * the handler in the table and passes it the arguments. The handler's return value is stored in
 * tf->gpr.a0, where the user program reads it. Each sys_* handler only unpacks its arguments and
 * calls the do_* function of the process manager (proc.c) that does the real work.
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
#include <error.h>

//lab5 function1 2414007
/**
 * @brief SYS_exit handler: terminate the calling process.
 * @param arg arg[0] is the exit code, delivered to the parent by wait()
 * @return does not return when it succeeds; do_exit() ends in schedule()
 */
static int sys_exit(uint64_t arg[]) {
    int error_code = (int)arg[0];
    return do_exit(error_code);
}

/**
 * @brief SYS_fork handler: create a child that is a copy of the caller.
 *
 * The child receives a copy of the caller's trap frame, and with it the user stack pointer, so it
 * continues after the ecall on its own copy of the address space. do_fork() makes its a0 zero.
 * @param arg unused
 * @return the PID of the child in the parent; a negative error code on failure
 */
static int sys_fork(uint64_t arg[]) {
    struct trapframe *tf = current->tf;
    uintptr_t stack = tf->gpr.sp;
    return do_fork(0, stack, tf);
}

/**
 * @brief SYS_wait handler: wait for a child to exit and reap it.
 * @param arg arg[0] is the PID to wait for (0 = any child); arg[1] is a user address that receives
 *            the exit code, or NULL (checked by do_wait() with user_mem_check())
 * @return 0 on success; a negative error code otherwise
 */
static int sys_wait(uint64_t arg[]) {
    int pid = (int)arg[0];
    int *store = (int *)arg[1];
    return do_wait(pid, store);
}

/**
 * @brief SYS_exec handler: replace the process image by a program held in memory.
 * @param arg arg[0] is the user address of the program name, arg[1] its length, arg[2] the start of
 *            the ELF image and arg[3] its size in bytes
 * @return 0 on success; a negative error code otherwise
 * @note The name pointer is validated by do_execve(); the image pointer comes from the caller and is
 *       only meaningful in this lab, where programs are linked into the kernel image.
 */
static int sys_exec(uint64_t arg[]) {
    const char *name = (const char *)arg[0];
    size_t len = (size_t)arg[1];
    unsigned char *binary = (unsigned char *)arg[2];
    size_t size = (size_t)arg[3];
    return do_execve(name, len, binary, size);
}

/**
 * @brief SYS_yield handler: ask the scheduler to run another process.
 * @param arg unused
 * @return 0
 */
static int sys_yield(uint64_t arg[]) {
    return do_yield();
}

/**
 * @brief SYS_kill handler: mark a process for termination.
 * @param arg arg[0] is the PID of the victim
 * @return 0 on success; -E_INVAL if the PID does not exist; -E_KILLED if it is already exiting
 */
static int sys_kill(uint64_t arg[]) {
    int pid = (int)arg[0];
    return do_kill(pid);
}

/**
 * @brief SYS_getpid handler.
 * @param arg unused
 * @return the PID of the calling process
 */
static int sys_getpid(uint64_t arg[]) {
    return current->pid;
}

/**
 * @brief SYS_putc handler: write one character to the console.
 *
 * User mode cannot reach the console, so cprintf() of the user library sends every character
 * through this call.
 * @param arg arg[0] is the character
 * @return 0
 */
static int sys_putc(uint64_t arg[]) {
    int c = (int)arg[0];
    cputchar(c);
    return 0;
}

/**
 * @brief SYS_pgdir handler: print the page table of the calling process (debugging aid, see print_pgdir()).
 * @param arg unused
 * @return 0
 */
static int sys_pgdir(uint64_t arg[]) {
    print_pgdir();
    return 0;
}

/**
 * @brief Table of kernel handlers, indexed by syscall number.
 *
 * Each handler takes the array of arguments as arg[] and returns the value that is delivered to
 * the user program in a0. The handlers are
 * sys_exit, sys_fork, sys_wait, sys_exec, sys_yield, sys_kill, sys_getpid, sys_putc and
 * sys_pgdir, defined above.
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

//lab5 function1 2414007
/**
 * @brief Dispatch the system call requested by the current process.
 *
 * Called from exception_handler() for an ecall from user mode. The system call number is in a0
 * of current->tf, the arguments are in a1..a5 and are passed to the handler as arg[0..4]. The
 * handler's result is written back into a0 of the same frame, which is where the user library
 * reads it after sret.
 *
 * A number that is out of range, or inside the range but without a handler, is not dispatched:
 * the call is rejected with -E_INVAL in a0, so a user program cannot make the kernel jump through
 * a NULL or out-of-bounds table entry.
 * @note Runs in the context of the calling process (current), with the trap frame already updated
 *       by exception_handler(). Handlers such as sys_exit() do not return here.
 */
void syscall(void) {
    struct trapframe *tf = current->tf;
    uint64_t arg[5];
    int num = (int)tf->gpr.a0;

    if (num >= 0 && (size_t)num < NUM_SYSCALLS && syscalls[num] != NULL) {
        arg[0] = tf->gpr.a1;
        arg[1] = tf->gpr.a2;
        arg[2] = tf->gpr.a3;
        arg[3] = tf->gpr.a4;
        arg[4] = tf->gpr.a5;
        tf->gpr.a0 = syscalls[num](arg);
        return;
    }
    // Not a valid system call: report it to the user program instead of dispatching.
    cprintf("syscall: invalid system call number %d from pid %d\n", num, current->pid);
    tf->gpr.a0 = -E_INVAL;
}
