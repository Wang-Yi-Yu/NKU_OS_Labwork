/**
 * @file   unistd.h
 * @brief  System call numbers and flags shared by the kernel and user programs.
 *
 * The numbers are passed in register a0 when a user program executes ecall (see
 * user/libs/syscall.c). The kernel dispatch table in kern/syscall/syscall.c is indexed by these
 * values. Numbers marked "reserved" are defined for later labs and have no handler yet. lab6 adds
 * SYS_lab6_set_priority and gives SYS_gettime a handler, lab7 adds SYS_sleep, and lab8 adds the
 * file system calls (SYS_open ... SYS_dup) together with the open flags, the lseek origins and the
 * path and argument limits that the file system layer and do_execve() share. lab9 gives SYS_mmap and
 * SYS_munmap handlers and adds the PROT_* and MAP_* constants of mmap().
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
/** @brief Syscall number of sys_exec(): replace the process image by a program loaded from the file system. */
#define SYS_exec 4
/** @brief Syscall number of sys_clone(). Reserved: not implemented yet. */
#define SYS_clone 5
/** @brief Syscall number of sys_yield(): give up the CPU voluntarily. */
#define SYS_yield 10
/** @brief Syscall number of sys_sleep(): sleep for a number of timer ticks (lab7). */
#define SYS_sleep 11
/** @brief Syscall number of sys_kill(): mark a process as killed. */
#define SYS_kill 12
/** @brief Syscall number of sys_gettime(): read the clock in milliseconds (lab6). */
#define SYS_gettime 17
/** @brief Syscall number of sys_getpid(): return the PID of the caller. */
#define SYS_getpid 18
/** @brief Syscall number of sys_mmap(): map a file or anonymous memory into the address space (lab9). */
#define SYS_mmap 20
/** @brief Syscall number of sys_munmap(): remove a mapping from the address space (lab9). */
#define SYS_munmap 21
/** @brief Syscall number of sys_shmem(). Reserved: not implemented yet. */
#define SYS_shmem 22
/** @brief Syscall number of sys_putc(): write one character to the console. */
#define SYS_putc 30
/** @brief Syscall number of sys_pgdir(): print the current page directory and page tables. */
#define SYS_pgdir 31
/** @brief Syscall number of sys_open(): open or create a file (lab8). */
#define SYS_open 100
/** @brief Syscall number of sys_close(): close a file descriptor (lab8). */
#define SYS_close 101
/** @brief Syscall number of sys_read(): read from an open file (lab8). */
#define SYS_read 102
/** @brief Syscall number of sys_write(): write to an open file (lab8). */
#define SYS_write 103
/** @brief Syscall number of sys_seek(): change the position of an open file (lab8). */
#define SYS_seek 104
/** @brief Syscall number of sys_fstat(): get the status of an open file (lab8). */
#define SYS_fstat 110
/** @brief Syscall number of sys_fsync(): flush an open file to the disk (lab8). */
#define SYS_fsync 111
/** @brief Syscall number of sys_getcwd(): get the current working directory (lab8). */
#define SYS_getcwd 121
/** @brief Syscall number of sys_getdirentry(): read the next entry of a directory (lab8). */
#define SYS_getdirentry 128
/** @brief Syscall number of sys_dup(): duplicate a file descriptor onto a given number (lab8). */
#define SYS_dup 130
/** @brief Syscall number of sys_lab6_set_priority(): set the scheduling priority of the caller (lab6 only). */
#define SYS_lab6_set_priority 255

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
/**
 * @brief clone flag: the child shares the file table (struct files_struct) of the parent (lab8).
 *
 * Without it copy_files() gives the child its own table and duplicates the open files.
 */
#define CLONE_FS 0x00000800  // set if shared between processes

/** @name Open flags (lab8). Choose one access mode, then OR in any of the other flags. @{ */
/** @brief Open for reading only. */
#define O_RDONLY 0
/** @brief Open for writing only. */
#define O_WRONLY 1
/** @brief Open for reading and writing. */
#define O_RDWR 2
/** @brief Create the file if it does not exist. */
#define O_CREAT 0x00000004
/** @brief With O_CREAT: fail if the file already exists. */
#define O_EXCL 0x00000008
/** @brief Truncate the file to length 0 when it is opened. */
#define O_TRUNC 0x00000010
/** @brief Every write goes to the end of the file. */
#define O_APPEND 0x00000020
/** @brief Mask that extracts the access mode (O_RDONLY, O_WRONLY or O_RDWR) from the flags. */
#define O_ACCMODE 3
/** @} */

/** @brief Value that means "no file descriptor": output goes to the console, not to a file. */
#define NO_FD -0x9527

/** @name Origins of sys_seek() (lab8). @{ */
/** @brief The offset is relative to the beginning of the file. */
#define LSEEK_SET 0
/** @brief The offset is relative to the current position. */
#define LSEEK_CUR 1
/** @brief The offset is relative to the end of the file. */
#define LSEEK_END 2
/** @} */

/** @brief Maximum length of one directory entry name, without the terminating NUL (lab8). */
#define FS_MAX_DNAME_LEN 31
/** @brief Maximum length of a file name, without the terminating NUL (lab8). */
#define FS_MAX_FNAME_LEN 255
/** @brief Maximum length of a path, without the terminating NUL (lab8). */
#define FS_MAX_FPATH_LEN 4095

/** @brief Maximum number of arguments accepted by exec (lab8). */
#define EXEC_MAX_ARG_NUM 32
/** @brief Maximum length of one exec argument, without the terminating NUL (lab8). */
#define EXEC_MAX_ARG_LEN 4095

/** @name mmap protection flags (lab9): which accesses the mapping allows. @{ */
/** @brief The pages cannot be accessed. */
#define PROT_NONE 0x0
/** @brief The pages can be read; becomes VM_READ in the VMA. */
#define PROT_READ 0x1
/** @brief The pages can be written; becomes VM_WRITE in the VMA. */
#define PROT_WRITE 0x2
/** @brief The pages can be executed; becomes VM_EXEC in the VMA. */
#define PROT_EXEC 0x4
/** @} */

/** @name mmap flags (lab9): how the mapping is made and shared. @{ */
/** @brief Share changes: other processes that map the same file see the writes. */
#define MAP_SHARED 0x01
/** @brief Private mapping: changes are not visible to other processes and not written to the file. */
#define MAP_PRIVATE 0x02
/** @brief Use the given address exactly instead of treating it as a hint. */
#define MAP_FIXED 0x10
/** @brief Anonymous mapping: no file is mapped and the pages start as zeros. */
#define MAP_ANONYMOUS 0x20
/** @brief Alias of MAP_ANONYMOUS. */
#define MAP_ANON MAP_ANONYMOUS
/** @} */

/** @brief Value returned by mmap() in user mode when the mapping fails. */
#define MAP_FAILED ((void *)-1)

#endif /* !__LIBS_UNISTD_H__ */
