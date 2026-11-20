/**
 * @file   unistd.h
 * @brief  System call numbers and flags shared by the kernel and user programs.
 *
 * The numbers are passed in register a0 when a user program executes ecall (see
 * user/libs/syscall.c). The kernel dispatch table in kern/syscall/syscall.c is indexed by these
 * values. Numbers marked "reserved" are defined for later labs and have no handler yet.
 */

#ifndef __LIBS_UNISTD_H__
#define __LIBS_UNISTD_H__

/**
 * @brief Legacy x86-style system call trap number (0x80). Not used by the RISC-V kernel, which
 *        recognises an ecall from user mode by CAUSE_USER_ECALL instead.
 */
#define T_SYSCALL 0x80

/** @brief Syscall number of sys_exit(): terminate the calling process. */
#define SYS_exit 1
/** @brief Syscall number of sys_fork(): create a child process as a copy of the caller. */
#define SYS_fork 2
/** @brief Syscall number of sys_wait(): wait for a child to become a zombie and reap it. */
#define SYS_wait 3
/** @brief Syscall number of sys_exec(). Listed in the dispatch table; handler not written yet. */
#define SYS_exec 4
/** @brief Syscall number of sys_clone(). Reserved: not implemented yet. */
#define SYS_clone 5
/** @brief Syscall number of sys_yield(): give up the CPU voluntarily. */
#define SYS_yield 10
/** @brief Syscall number of sys_sleep(). Reserved: not implemented yet. */
#define SYS_sleep 11
/** @brief Syscall number of sys_kill(): mark a process as killed. */
#define SYS_kill 12
/** @brief Syscall number of sys_gettime(). Reserved: not implemented yet. */
#define SYS_gettime 17
/** @brief Syscall number of sys_getpid(): return the PID of the caller. */
#define SYS_getpid 18
/** @brief Syscall number of sys_brk(). Reserved: not implemented yet. */
#define SYS_brk 19
/** @brief Syscall number of sys_mmap(). Reserved: not implemented yet. */
#define SYS_mmap 20
/** @brief Syscall number of sys_munmap(). Reserved: not implemented yet. */
#define SYS_munmap 21
/** @brief Syscall number of sys_shmem(). Reserved: not implemented yet. */
#define SYS_shmem 22
/** @brief Syscall number of sys_putc(): write one character to the console. */
#define SYS_putc 30
/** @brief Syscall number of sys_pgdir(): print the current page directory and page tables. */
#define SYS_pgdir 31

/**
 * @brief clone flag: the child shares the virtual memory of the parent.
 * @note Also defined in kern/mm/pmm.h with the same value.
 */
#define CLONE_VM 0x00000100  // set if VM shared between processes
/**
 * @brief clone flag: the child joins the thread group of the parent. Reserved, not used yet.
 * @note Also defined in kern/mm/pmm.h with the same value.
 */
#define CLONE_THREAD 0x00000200  // thread group

#endif /* !__LIBS_UNISTD_H__ */
