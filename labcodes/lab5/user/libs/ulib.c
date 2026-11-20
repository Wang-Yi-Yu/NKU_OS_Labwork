/**
 * @file   ulib.c
 * @brief  User-level wrappers for the system calls: exit(), fork(), wait(), waitpid(), ...
 *
 * Each function is a thin wrapper around one sys_* function from syscall.c. Declarations and
 * the meaning of the return values are in ulib.h.
 */
#include <defs.h>
#include <syscall.h>
#include <stdio.h>
#include <ulib.h>

/**
 * @brief Terminate the calling process.
 * @param[in] error_code  Exit code passed to the kernel.
 * @note Never returns. If sys_exit() ever returned, "BUG: exit failed." would be printed and
 *       the process would spin forever.
 */
void exit(int error_code) {
    sys_exit(error_code);
    cprintf("BUG: exit failed.\n");
    while (1)
        ;
}

/**
 * @brief Create a child process (see sys_fork()).
 * @return In the parent, the PID of the child; in the child, 0; a negative value on error.
 */
int fork(void) {
    return sys_fork();
}

/**
 * @brief Wait for any child process to exit and reap it (see sys_wait()).
 * @return 0 if a child was reaped; a non-zero value if there is no child.
 */
int wait(void) {
    return sys_wait(0, NULL);
}

/**
 * @brief Wait for a specific child process to exit and reap it (see sys_wait()).
 * @param[in] pid  PID of the child to wait for.
 * @param[in] store  Where to store the exit code, or NULL to ignore it.
 * @return 0 on success; a non-zero value on error.
 */
int waitpid(int pid, int *store) {
    return sys_wait(pid, store);
}

/**
 * @brief Give up the CPU voluntarily (see sys_yield()).
 */
void yield(void) {
    sys_yield();
}

/**
 * @brief Mark a process as killed (see sys_kill()).
 * @param[in] pid  PID of the process to kill.
 * @return 0 on success; a non-zero value on error.
 */
int kill(int pid) {
    return sys_kill(pid);
}

/**
 * @brief Return the PID of the calling process (see sys_getpid()).
 * @return The PID of the caller.
 */
int getpid(void) {
    return sys_getpid();
}

/**
 * @brief Ask the kernel to print the page directory and page tables of the caller.
 * @note Output is produced by the kernel (sys_pgdir()).
 */
void print_pgdir(void) {
    sys_pgdir();
}
