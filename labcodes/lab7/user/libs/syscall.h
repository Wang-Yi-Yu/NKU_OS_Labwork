/**
 * @file   syscall.h
 * @brief  User-side system call stubs: prototypes of the sys_* functions in syscall.c.
 */
#ifndef __USER_LIBS_SYSCALL_H__
#define __USER_LIBS_SYSCALL_H__

/**
 * @brief Issue the SYS_exit system call.
 * @param[in] error_code  Exit code of the process.
 */
int sys_exit(int64_t error_code);
/**
 * @brief Issue the SYS_fork system call.
 * @return See fork().
 */
int sys_fork(void);
/**
 * @brief Issue the SYS_wait system call.
 * @param[in] pid  PID of the child to wait for, or 0 for any child.
 * @param[in] store  Where to store the exit code, or NULL.
 * @return See waitpid().
 */
int sys_wait(int64_t pid, int *store);
/**
 * @brief Issue the SYS_yield system call.
 */
int sys_yield(void);
/**
 * @brief Issue the SYS_kill system call.
 * @param[in] pid  PID of the process to kill.
 * @return See kill().
 */
int sys_kill(int64_t pid);
/**
 * @brief Issue the SYS_getpid system call.
 * @return The PID of the caller.
 */
int sys_getpid(void);
/**
 * @brief Issue the SYS_putc system call: write one character to the console.
 * @param[in] c  Character to write.
 * @return The value returned by the kernel.
 */
int sys_putc(int64_t c);
/**
 * @brief Issue the SYS_pgdir system call.
 */
int sys_pgdir(void);
/**
 * @brief Issue the SYS_gettime system call (lab6).
 * @return Milliseconds since boot.
 */
int sys_gettime(void);
/**
 * @brief Issue the SYS_lab6_set_priority system call (lab6 only).
 * @param[in] priority  New priority of the caller.
 */
void sys_lab6_set_priority(uint64_t priority);
/**
 * @brief Issue the SYS_sleep system call (lab7).
 * @param[in] time  Number of timer ticks to sleep.
 * @return 0.
 */
int sys_sleep(uint64_t time);

#endif /* !__USER_LIBS_SYSCALL_H__ */
