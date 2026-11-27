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
 * @brief Issue the SYS_exec system call (lab8).
 * @param[in] name  Name of the program.
 * @param[in] argc  Number of entries in @p argv.
 * @param[in] argv  Argument vector; argv[0] is the path of the program file.
 * @return Does not return when it succeeds; a negative error code otherwise.
 */
int sys_exec(const char *name, int64_t argc, const char **argv);
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

struct stat;
struct dirent;

/**
 * @brief Issue the SYS_open system call (lab8).
 * @param[in] path        Path of the file.
 * @param[in] open_flags  Access mode and flags.
 * @return The new file descriptor, or a negative error code.
 */
int sys_open(const char *path, uint64_t open_flags);
/**
 * @brief Issue the SYS_close system call (lab8).
 * @param[in] fd  File descriptor to close.
 * @return 0 on success, a negative error code otherwise.
 */
int sys_close(int64_t fd);
/**
 * @brief Issue the SYS_read system call (lab8).
 * @param[in]  fd    File descriptor.
 * @param[out] base  Buffer that receives the data.
 * @param[in]  len   Number of bytes to read.
 * @return The number of bytes read, or a negative error code.
 */
int sys_read(int64_t fd, void *base, size_t len);
/**
 * @brief Issue the SYS_write system call (lab8).
 * @param[in] fd    File descriptor.
 * @param[in] base  Buffer that holds the data.
 * @param[in] len   Number of bytes to write.
 * @return The number of bytes written, or a negative error code.
 */
int sys_write(int64_t fd, void *base, size_t len);
/**
 * @brief Issue the SYS_seek system call (lab8).
 * @param[in] fd      File descriptor.
 * @param[in] pos     Offset relative to @p whence.
 * @param[in] whence  LSEEK_SET, LSEEK_CUR or LSEEK_END.
 * @return 0 on success, a negative error code otherwise.
 */
int sys_seek(int64_t fd, off_t pos, int64_t whence);
/**
 * @brief Issue the SYS_fstat system call (lab8).
 * @param[in]  fd    File descriptor.
 * @param[out] stat  Receives the file status.
 * @return 0 on success, a negative error code otherwise.
 */
int sys_fstat(int64_t fd, struct stat *stat);
/**
 * @brief Issue the SYS_fsync system call (lab8).
 * @param[in] fd  File descriptor.
 * @return 0 on success, a negative error code otherwise.
 */
int sys_fsync(int64_t fd);
/**
 * @brief Issue the SYS_getcwd system call (lab8).
 * @param[out] buffer  Receives the NUL-terminated path.
 * @param[in]  len     Size of @p buffer in bytes.
 * @return 0 on success, a negative error code otherwise.
 */
int sys_getcwd(char *buffer, size_t len);
/**
 * @brief Issue the SYS_getdirentry system call (lab8).
 * @param[in]  fd      File descriptor of the directory.
 * @param[out] dirent  Receives the entry.
 * @return 0 on success, a negative error code otherwise.
 */
int sys_getdirentry(int64_t fd, struct dirent *dirent);
/**
 * @brief Issue the SYS_dup system call (lab8).
 * @param[in] fd1  Existing file descriptor.
 * @param[in] fd2  File descriptor that is to be redirected.
 * @return @p fd2 on success, a negative error code otherwise.
 */
int sys_dup(int64_t fd1, int64_t fd2);

#endif /* !__USER_LIBS_SYSCALL_H__ */
