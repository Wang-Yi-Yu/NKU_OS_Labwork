/**
 * @file   file.c
 * @brief  User-level file access: one wrapper per system call of the file system (lab8).
 *
 * Every open(), close(), read(), write(), seek(), fstat(), fsync() and dup2() forwards its
 * arguments unchanged to the matching sys_* function of user/libs/syscall.c. The system call
 * traps into the kernel (kern/syscall/syscall.c), which calls the sysfile_* layer, then the file_*
 * layer that works on the descriptor table of the process, then the vop_* operations of the inode.
 * print_stat() is a small helper that formats a struct stat for the console. dup(), pipe() and
 * mkfifo() are declared in file.h but are not defined here.
 */
#include <defs.h>
#include <string.h>
#include <syscall.h>
#include <stdio.h>
#include <stat.h>
#include <error.h>
#include <unistd.h>

/**
 * @brief Open a file, or create it, and return a new file descriptor.
 * @param[in] path        path of the file
 * @param[in] open_flags  access mode and optional O_* flags
 * @return the new file descriptor on success, a negative error code otherwise
 */
int open(const char *path, uint32_t open_flags) {
    return sys_open(path, open_flags);
}

/**
 * @brief Close a file descriptor.
 * @param[in] fd  file descriptor to close
 * @return 0 on success, a negative error code otherwise
 */
int close(int fd) {
    return sys_close(fd);
}

/**
 * @brief Read from a file descriptor at its current position.
 * @param[in]  fd    file descriptor to read from
 * @param[out] base  user buffer that receives the data
 * @param[in]  len   maximum number of bytes to read
 * @return the number of bytes read (0 at end of file), or a negative error code
 */
int read(int fd, void *base, size_t len) {
    return sys_read(fd, base, len);
}

/**
 * @brief Write to a file descriptor at its current position.
 * @param[in] fd    file descriptor to write to
 * @param[in] base  user buffer with the data
 * @param[in] len   number of bytes to write
 * @return the number of bytes written, or a negative error code
 */
int write(int fd, void *base, size_t len) {
    return sys_write(fd, base, len);
}

/**
 * @brief Move the position of a file descriptor.
 * @param[in] fd      file descriptor
 * @param[in] pos     offset, relative to the origin given by @p whence
 * @param[in] whence  LSEEK_SET, LSEEK_CUR or LSEEK_END
 * @return 0 on success, a negative error code otherwise
 */
int seek(int fd, off_t pos, int whence) {
    return sys_seek(fd, pos, whence);
}

/**
 * @brief Get the status of an open file.
 * @param[in]  fd    file descriptor
 * @param[out] stat  receives the file type, link count, block count and size
 * @return 0 on success, a negative error code otherwise
 */
int fstat(int fd, struct stat *stat) {
    return sys_fstat(fd, stat);
}

/**
 * @brief Flush the data of an open file to the disk.
 * @param[in] fd  file descriptor
 * @return 0 on success, a negative error code otherwise
 */
int fsync(int fd) {
    return sys_fsync(fd);
}

/**
 * @brief Make @p fd2 refer to the same open file as @p fd1.
 *
 * The system call is named sys_dup, but it takes both descriptors. @p fd2 must be free: the kernel
 * refuses to replace an open descriptor, so the caller closes it first (sh does this in reopen()).
 * @param[in] fd1  file descriptor whose file is shared
 * @param[in] fd2  free file descriptor that will refer to the file
 * @return @p fd2 on success, a negative error code otherwise (-E_BUSY if @p fd2 is in use)
 */
int dup2(int fd1, int fd2) {
    return sys_dup(fd1, fd2);
}

/**
 * @brief Map the file type of a struct stat to one character.
 * @param[in] stat  status of the file
 * @return 'r' for a regular file, 'd' for a directory, 'l' for a symbolic link, 'c' for a character
 *         device, 'b' for a block device, '-' for any other type
 */
static char transmode(struct stat *stat) {
    uint32_t mode = stat->st_mode;
    if (S_ISREG(mode))
        return 'r';
    if (S_ISDIR(mode))
        return 'd';
    if (S_ISLNK(mode))
        return 'l';
    if (S_ISCHR(mode))
        return 'c';
    if (S_ISBLK(mode))
        return 'b';
    return '-';
}

/**
 * @brief Print the fields of a struct stat on the console.
 *
 * The first line has the descriptor (three digits) and the name; the following lines have the
 * type character from transmode(), the link count, the block count and the size in bytes.
 * @param[in] name  name of the file, printed in the first line
 * @param[in] fd    file descriptor, printed in the first line
 * @param[in] stat  status of the file, as returned by fstat()
 */
void print_stat(const char *name, int fd, struct stat *stat) {
    cprintf("[%03d] %s\n", fd, name);
    cprintf("    mode    : %c\n", transmode(stat));
    cprintf("    links   : %lu\n", stat->st_nlinks);
    cprintf("    blocks  : %lu\n", stat->st_blocks);
    cprintf("    size    : %lu\n", stat->st_size);
}
