/**
 * @file   syscall.c
 * @brief  User-side system call stubs: the generic syscall() and one sys_* wrapper per call.
 *
 * The syscall number goes in a0, the arguments in a1..a5, and the kernel's result comes back in
 * a0. The sys_* functions below are the public interface that ulib.c uses. lab6 adds
 * sys_gettime() and sys_lab6_set_priority(); lab7 adds sys_sleep().
 */
#include <defs.h>
#include <unistd.h>
#include <stdarg.h>
#include <syscall.h>

/** @brief Number of argument registers (a1..a5) that a system call can pass. */
#define MAX_ARGS 5

/**
 * @brief Execute an ecall with a system call number and up to five arguments.
 * @param[in] num  System call number (SYS_* from libs/unistd.h).
 * @param[in] ...  Arguments of type uint64_t, one per register a1..a5.
 * @return The value the kernel returned in a0, as an int.
 * @note The loop always reads MAX_ARGS (5) variadic values, even when the caller passes fewer, so
 *       the unused slots hold undefined values. The kernel ignores them.
 *       The result is stored with sd (8 bytes) into the 4-byte int ret, which is wider than the
 *       variable. Both points are left unchanged because this change is comments only.
 */
static inline int syscall(int64_t num, ...) {
    va_list ap;
    va_start(ap, num);
    uint64_t a[MAX_ARGS];
    int i, ret;
    for (i = 0; i < MAX_ARGS; i++) {
        a[i] = va_arg(ap, uint64_t);
    }
    va_end(ap);

    asm volatile("ld a0, %1\n"
                 "ld a1, %2\n"
                 "ld a2, %3\n"
                 "ld a3, %4\n"
                 "ld a4, %5\n"
                 "ld a5, %6\n"
                 "ecall\n"
                 "sd a0, %0"
                 : "=m"(ret)
                 : "m"(num), "m"(a[0]), "m"(a[1]), "m"(a[2]), "m"(a[3]), "m"(a[4])
                 : "memory");
    return ret;
}

/**
 * @brief Issue SYS_exit: terminate the calling process.
 * @param[in] error_code  Exit code of the process.
 * @return Does not normally return.
 */
int sys_exit(int64_t error_code) {
    return syscall(SYS_exit, error_code);
}

/**
 * @brief Issue SYS_fork: create a child process.
 * @return See fork() in ulib.c.
 */
int sys_fork(void) {
    return syscall(SYS_fork);
}

/**
 * @brief Issue SYS_wait: wait for a child process and reap it.
 * @param[in] pid  PID of the child to wait for, or 0 for any child.
 * @param[in] store  Where to store the exit code, or NULL.
 * @return 0 on success; a non-zero value on error.
 */
int sys_wait(int64_t pid, int *store) {
    return syscall(SYS_wait, pid, store);
}

/**
 * @brief Issue SYS_yield: give up the CPU.
 */
int sys_yield(void) {
    return syscall(SYS_yield);
}

/**
 * @brief Issue SYS_kill: mark a process as killed.
 * @param[in] pid  PID of the process to kill.
 * @return 0 on success; a non-zero value on error.
 */
int sys_kill(int64_t pid) {
    return syscall(SYS_kill, pid);
}

/**
 * @brief Issue SYS_getpid: get the PID of the caller.
 * @return The PID of the caller.
 */
int sys_getpid(void) {
    return syscall(SYS_getpid);
}

/**
 * @brief Issue SYS_putc: write one character to the console.
 * @param[in] c  Character to write.
 * @return The value returned by the kernel.
 */
int sys_putc(int64_t c) {
    return syscall(SYS_putc, c);
}

/**
 * @brief Issue SYS_pgdir: print the page tables of the caller.
 */
int sys_pgdir(void) {
    return syscall(SYS_pgdir);
}

/**
 * @brief Issue SYS_gettime: read the clock (lab6).
 * @return Milliseconds since boot (one timer tick is 10 ms).
 */
int sys_gettime(void) {
    return syscall(SYS_gettime);
}

/**
 * @brief Issue SYS_lab6_set_priority: set the scheduling priority of the caller (lab6 only).
 * @param[in] priority  New priority; a bigger value gets more CPU time under Stride scheduling.
 */
void sys_lab6_set_priority(uint64_t priority) {
    syscall(SYS_lab6_set_priority, priority);
}

/**
 * @brief Issue SYS_sleep: sleep for a number of timer ticks (lab7).
 * @param[in] time  Number of timer ticks to sleep (one tick is 10 ms).
 * @return 0.
 */
int sys_sleep(uint64_t time) {
    return syscall(SYS_sleep, time);
}
