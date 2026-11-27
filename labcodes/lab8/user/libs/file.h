/**
 * @file   file.h
 * @brief  User-level file access: open(), close(), read(), write(), seek(), fstat(), fsync() and
 *         dup2(), plus print_stat() (lab8).
 *
 * Each function is a wrapper around one system call from user/libs/syscall.c. The kernel side is
 * the sysfile_* layer (kern/fs/sysfile.c), which calls the file_* layer (kern/fs/file.c) and then
 * the vop_* inode operations. A file descriptor is an index into the files_struct of the calling
 * process. Negative return values are error codes from error.h. dup(), pipe() and mkfifo() are
 * declared but have no definition in lab8.
 */

#ifndef __USER_LIBS_FILE_H__
#define __USER_LIBS_FILE_H__

#include <defs.h>

struct stat;

/**
 * @brief Open a file, or create it, and return a new file descriptor.
 * @param[in] path        path of the file, absolute or relative to the working directory
 * @param[in] open_flags  one access mode (O_RDONLY, O_WRONLY or O_RDWR) optionally ORed with
 *                        O_CREAT, O_EXCL, O_TRUNC or O_APPEND
 * @return the new file descriptor (>= 0) on success, a negative error code otherwise (for example
 *         -E_NOENT or -E_NO_MEM)
 */
int open(const char *path, uint32_t open_flags);
/**
 * @brief Close a file descriptor.
 * @param[in] fd  file descriptor to close
 * @return 0 on success, a negative error code otherwise
 */
int close(int fd);
/**
 * @brief Read from a file descriptor at its current position.
 * @param[in]  fd    file descriptor to read from (0 is the console, stdin)
 * @param[out] base  user buffer that receives the data
 * @param[in]  len   maximum number of bytes to read
 * @return the number of bytes read (0 at end of file), or a negative error code
 */
int read(int fd, void *base, size_t len);
/**
 * @brief Write to a file descriptor at its current position.
 * @param[in] fd    file descriptor to write to (1 is the console, stdout)
 * @param[in] base  user buffer with the data
 * @param[in] len   number of bytes to write
 * @return the number of bytes written, or a negative error code
 */
int write(int fd, void *base, size_t len);
/**
 * @brief Move the position of a file descriptor.
 * @param[in] fd      file descriptor
 * @param[in] pos     offset, relative to the origin given by @p whence
 * @param[in] whence  LSEEK_SET, LSEEK_CUR or LSEEK_END
 * @return 0 on success, a negative error code otherwise
 */
int seek(int fd, off_t pos, int whence);
/**
 * @brief Get the status of an open file.
 * @param[in]  fd    file descriptor
 * @param[out] stat  receives the file type, link count, block count and size
 * @return 0 on success, a negative error code otherwise
 */
int fstat(int fd, struct stat *stat);
/**
 * @brief Flush the data of an open file to the disk.
 * @param[in] fd  file descriptor
 * @return 0 on success, a negative error code otherwise
 */
int fsync(int fd);
/**
 * @brief Duplicate a file descriptor (declared, not implemented in lab8).
 * @param[in] fd  file descriptor to duplicate
 * @return the new file descriptor, or a negative error code
 */
int dup(int fd);
/**
 * @brief Make @p fd2 refer to the same open file as @p fd1.
 * @param[in] fd1  file descriptor whose file is shared
 * @param[in] fd2  free file descriptor that will refer to the file; the call does not close an
 *                 open one (see the note below)
 * @return @p fd2 on success, a negative error code otherwise (-E_BUSY if @p fd2 is in use)
 * @note The kernel refuses to replace an open descriptor. sh's reopen() therefore calls close()
 *       on @p fd2 before dup2().
 */
int dup2(int fd1, int fd2);
/**
 * @brief Create a pipe (declared, not implemented in lab8).
 * @param[out] fd_store  receives the read and write descriptors of the pipe
 * @return 0 on success, a negative error code otherwise
 */
int pipe(int *fd_store);
/**
 * @brief Create a FIFO (declared, not implemented in lab8).
 * @param[in] name        name of the FIFO
 * @param[in] open_flags  open flags, as for open()
 * @return 0 on success, a negative error code otherwise
 */
int mkfifo(const char *name, uint32_t open_flags);

/**
 * @brief Print the fields of a struct stat on the console (a helper; no lab8 program calls it).
 * @param[in] name  name of the file, printed in the first line
 * @param[in] fd    file descriptor, printed in the first line
 * @param[in] stat  status of the file, as returned by fstat()
 */
void print_stat(const char *name, int fd, struct stat *stat);

#endif /* !__USER_LIBS_FILE_H__ */
