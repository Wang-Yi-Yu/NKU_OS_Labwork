/**
 * @file   syscall.c
 * @brief  User-side system call stubs: the generic syscall() and one sys_* wrapper per call.
 *
 * The syscall number goes in a0, the arguments in a1..a5, and the kernel's result comes back in
 * a0. The sys_* functions below are the public interface that ulib.c and file.c use. lab6 adds
 * sys_gettime() and sys_lab6_set_priority(); lab7 adds sys_sleep(); lab8 adds sys_exec() and the
 * file system calls sys_open, sys_close, sys_read, sys_write, sys_seek, sys_fstat, sys_fsync,
 * sys_getcwd, sys_getdirentry and sys_dup.
 */
#include <defs.h>
#include <unistd.h>
#include <stdarg.h>
#include <syscall.h>
#include <stat.h>
#include <dirent.h>

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
 * @brief Issue SYS_exec: replace the calling process by a program loaded from the file system (lab8).
 * @param[in] name  Name of the program, copied into the process name.
 * @param[in] argc  Number of entries in @p argv.
 * @param[in] argv  Argument vector; argv[0] is the path of the program file.
 * @return Does not return when it succeeds; a negative error code otherwise.
 */
int sys_exec(const char *name, int64_t argc, const char **argv) {
    return syscall(SYS_exec, name, argc, argv);
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

/**
 * @brief Issue SYS_open: open or create a file (lab8).
 * @param[in] path        Path of the file.
 * @param[in] open_flags  Access mode and flags, for example O_RDONLY or O_RDWR | O_CREAT.
 * @return The new file descriptor, or a negative error code.
 */
int sys_open(const char *path, uint64_t open_flags) {
    return syscall(SYS_open, path, open_flags);
}

/**
 * @brief Issue SYS_close: close a file descriptor (lab8).
 * @param[in] fd  File descriptor to close.
 * @return 0 on success, a negative error code otherwise.
 */
int sys_close(int64_t fd) {
    return syscall(SYS_close, fd);
}

/**
 * @brief Issue SYS_read: read from an open file (lab8).
 * @param[in]  fd    File descriptor.
 * @param[out] base  Buffer that receives the data.
 * @param[in]  len   Number of bytes to read.
 * @return The number of bytes read (0 at end of file), or a negative error code.
 */
int sys_read(int64_t fd, void *base, size_t len) {
    return syscall(SYS_read, fd, base, len);
}

/**
 * @brief Issue SYS_write: write to an open file (lab8).
 * @param[in] fd    File descriptor.
 * @param[in] base  Buffer that holds the data.
 * @param[in] len   Number of bytes to write.
 * @return The number of bytes written, or a negative error code.
 */
int sys_write(int64_t fd, void *base, size_t len) {
    return syscall(SYS_write, fd, base, len);
}

/**
 * @brief Issue SYS_seek: change the position of an open file (lab8).
 * @param[in] fd      File descriptor.
 * @param[in] pos     Offset relative to @p whence.
 * @param[in] whence  LSEEK_SET, LSEEK_CUR or LSEEK_END.
 * @return 0 on success, a negative error code otherwise.
 */
int sys_seek(int64_t fd, off_t pos, int64_t whence) {
    return syscall(SYS_seek, fd, pos, whence);
}

/**
 * @brief Issue SYS_fstat: get the status of an open file (lab8).
 * @param[in]  fd    File descriptor.
 * @param[out] stat  Receives the file mode, link count, block count and size.
 * @return 0 on success, a negative error code otherwise.
 */
int sys_fstat(int64_t fd, struct stat *stat) {
    return syscall(SYS_fstat, fd, stat);
}

/**
 * @brief Issue SYS_fsync: flush an open file to the disk (lab8).
 * @param[in] fd  File descriptor.
 * @return 0 on success, a negative error code otherwise.
 */
int sys_fsync(int64_t fd) {
    return syscall(SYS_fsync, fd);
}

/**
 * @brief Issue SYS_getcwd: get the current working directory (lab8).
 * @param[out] buffer  Receives the NUL-terminated path.
 * @param[in]  len     Size of @p buffer in bytes.
 * @return 0 on success, a negative error code otherwise.
 */
int sys_getcwd(char *buffer, size_t len) {
    return syscall(SYS_getcwd, buffer, len);
}

/**
 * @brief Issue SYS_getdirentry: read the next entry of an open directory (lab8).
 * @param[in]  fd      File descriptor of the directory.
 * @param[out] dirent  Receives the entry; its offset field selects which entry is read.
 * @return 0 on success, a negative error code otherwise.
 */
int sys_getdirentry(int64_t fd, struct dirent *dirent) {
    return syscall(SYS_getdirentry, fd, dirent);
}

/**
 * @brief Issue SYS_dup: make @p fd2 refer to the same open file as @p fd1 (lab8).
 * @param[in] fd1  Existing file descriptor.
 * @param[in] fd2  File descriptor that is to be redirected.
 * @return @p fd2 on success, a negative error code otherwise.
 */
int sys_dup(int64_t fd1, int64_t fd2) {
    return syscall(SYS_dup, fd1, fd2);
}
