/**
 * @file   syscall.h
 * @brief  Kernel-side system call entry point.
 */

#ifndef __KERN_SYSCALL_SYSCALL_H__
#define __KERN_SYSCALL_SYSCALL_H__

/**
 * @brief Dispatch one system call requested by a user process.
 *
 * Called from exception_handler() for an ecall from user mode. The syscall number is read from
 * the saved a0 register of current->tf, and the result is written back to tf->gpr.a0.
 * @note syscall() is the lab5 exercise; it is not defined in kern/syscall/syscall.c yet.
 */
void syscall(void);

#endif /* !__KERN_SYSCALL_SYSCALL_H__ */
