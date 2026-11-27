/**
 * @file   sysfile.h
 * @brief  System-call level of the file system: the sysfile_* functions called by the sys_* handlers.
 *
 * Each sysfile_* function is the kernel side of one file system system call (libs/unistd.h, SYS_*).
 * It receives arguments that may point into user memory. Paths and buffers are copied across with
 * copy_path(), copy_to_user() and copy_from_user() (vmm.h), with the process's mm locked, so that
 * a bad user pointer makes the call fail instead of crashing the kernel. Then the work is handed to
 * the file layer (file.h) or the VFS (vfs.h). The sys_* handlers that call these functions are in
 * kern/syscall/syscall.c.
 *
 * Note: sysfile_mkdir() is declared here but has no definition in lab8, and sysfile_pipe() and
 * sysfile_mkfifo() exist only as -E_UNIMP stubs (see sysfile.c).
 */
#ifndef __KERN_FS_SYSFILE_H__
#define __KERN_FS_SYSFILE_H__

#include <defs.h>

struct stat;
struct dirent;

/**
 * @brief Open or create a file for the current process.
 * @param path       user-space path, copied in with copy_path() (at most FS_MAX_FPATH_LEN bytes)
 * @param open_flags O_RDONLY/O_WRONLY/O_RDWR with O_CREAT, O_TRUNC, O_APPEND, ...
 * @return the new file descriptor, or a negative error (see file_open(); -E_INVAL if the path
 *         cannot be copied from user space)
 */
int sysfile_open(const char *path, uint32_t open_flags);
/**
 * @brief Close a file descriptor.
 * @param fd the descriptor
 * @return 0 on success, -E_INVAL if fd is not open
 */
int sysfile_close(int fd);
/**
 * @brief Read from a file into user memory.
 *
 * Reads in chunks of IOBUF_SIZE bytes through a kernel buffer, and copies each chunk into the
 * user buffer with copy_to_user(). Stops at the first error or at end of file.
 *
 * @param fd   the descriptor; must be open and readable
 * @param base user-space buffer that receives the data
 * @param len  maximum number of bytes to read; 0 returns 0 at once
 * @return the number of bytes read if any were read, otherwise 0 or a negative error (-E_INVAL for
 *         a bad descriptor or a bad user buffer, -E_NO_MEM if the kernel buffer cannot be allocated)
 */
int sysfile_read(int fd, void *base, size_t len);
/**
 * @brief Write user memory to a file.
 *
 * Copies chunks of IOBUF_SIZE bytes from the user buffer into a kernel buffer with copy_from_user()
 * and writes them with file_write(). Stops at the first error.
 *
 * @param fd   the descriptor; must be open and writable
 * @param base user-space buffer with the data
 * @param len  number of bytes to write; 0 returns 0 at once
 * @return the number of bytes written if any were written, otherwise a negative error
 *         (-E_INVAL for a bad descriptor or a bad user buffer, -E_NO_MEM)
 */
int sysfile_write(int fd, void *base, size_t len);
/**
 * @brief Move the position of an open file (lseek()).
 * @param fd     the descriptor
 * @param pos    offset relative to whence
 * @param whence LSEEK_SET, LSEEK_CUR or LSEEK_END
 * @return 0 on success, or a negative error (see file_seek())
 */
int sysfile_seek(int fd, off_t pos, int whence);
/**
 * @brief Get the status of an open file (fstat()) and copy it to user memory.
 * @param fd    the descriptor
 * @param stat  user-space struct stat to fill in
 * @return 0 on success; the error of file_fstat() if fd is not open; -E_INVAL if stat is not
 *         writable user memory
 */
int sysfile_fstat(int fd, struct stat *stat);
/**
 * @brief Flush an open file to its device (fsync()).
 * @param fd the descriptor
 * @return 0 on success, or a negative error (see file_fsync())
 */
int sysfile_fsync(int fd);
/**
 * @brief Change the current directory of the calling process.
 * @param path user-space path of the new directory
 * @return 0 on success, -E_INVAL if the path cannot be copied, or the error of vfs_chdir()
 */
int sysfile_chdir(const char *path);
/**
 * @brief Create a directory. Declared only: there is no definition in lab8.
 * @param path user-space path of the directory
 * @return not implemented in lab8
 */
int sysfile_mkdir(const char *path);
/**
 * @brief Create a hard link: path2 becomes another name for the file at path1.
 * @param path1 user-space path of the existing file
 * @param path2 user-space path of the new name
 * @return 0 on success, -E_INVAL if a path cannot be copied, or the error of vfs_link()
 */
int sysfile_link(const char *path1, const char *path2);
/**
 * @brief Rename a file or directory.
 * @param path1 user-space path of the old name
 * @param path2 user-space path of the new name
 * @return 0 on success, -E_INVAL if a path cannot be copied, or the error of vfs_rename()
 */
int sysfile_rename(const char *path1, const char *path2);
/**
 * @brief Remove a name (unlink()).
 * @param path user-space path to remove
 * @return 0 on success, -E_INVAL if the path cannot be copied, or the error of vfs_unlink()
 */
int sysfile_unlink(const char *path);
/**
 * @brief Copy the current directory path to a user buffer (getcwd()).
 * @param buf user-space buffer that receives the path, writable
 * @param len size of buf in bytes; must not be 0
 * @return 0 on success; -E_INVAL if len is 0, buf is not writable user memory, or the VFS fails
 */
int sysfile_getcwd(char *buf, size_t len);
/**
 * @brief Read the next directory entry of an open directory into user memory.
 * @param fd      the descriptor of an open directory
 * @param direntp user-space struct dirent; its offset field is read and then updated
 * @return 0 on success; -E_INVAL for a bad user pointer; or the error of file_getdirentry(),
 *         for example -E_NOENT at the end of the directory
 */
int sysfile_getdirentry(int fd, struct dirent *direntp);
/**
 * @brief Duplicate an open file descriptor (see file_dup()).
 * @param fd1 an open descriptor
 * @param fd2 the wanted new descriptor, or NO_FD
 * @return the new descriptor, or a negative error
 */
int sysfile_dup(int fd1, int fd2);
/**
 * @brief Create a pipe. Not implemented in lab8: always returns -E_UNIMP.
 * @param fd_store user-space array for the two descriptors (unused)
 * @return -E_UNIMP
 */
int sysfile_pipe(int *fd_store);
/**
 * @brief Create a named pipe. Not implemented in lab8: always returns -E_UNIMP.
 * @param name       user-space name of the pipe (unused)
 * @param open_flags open flags (unused)
 * @return -E_UNIMP
 */
int sysfile_mkfifo(const char *name, uint32_t open_flags);
/**
 * @brief Map a file or anonymous memory into the address space of the current process (mmap()).
 *
 * Converts the PROT_* bits to VM_* flags, resolves fd to a struct file unless MAP_ANONYMOUS is set,
 * and calls do_mmap() with the mm locked.
 *
 * @param addr   wanted start address, or 0 to let the kernel choose
 * @param len    length in bytes; 0 is rejected
 * @param prot   PROT_READ / PROT_WRITE / PROT_EXEC
 * @param flags  MAP_* flags; MAP_ANONYMOUS means no file
 * @param fd     descriptor of the file to map (ignored for anonymous mappings)
 * @param offset byte offset into the file; must be page aligned
 * @return the start address of the mapping, or a negative error cast to uintptr_t
 */
uintptr_t sysfile_mmap(uintptr_t addr, size_t len, int prot, int flags, int fd, off_t offset);
/**
 * @brief Remove a mapping from the address space of the current process (munmap()).
 * @param addr start address of the mapping
 * @param len  length in bytes; 0 is rejected
 * @return 0 on success, or a negative error (see do_munmap())
 */
int sysfile_munmap(uintptr_t addr, size_t len);

#endif /* !__KERN_FS_SYSFILE_H__ */
