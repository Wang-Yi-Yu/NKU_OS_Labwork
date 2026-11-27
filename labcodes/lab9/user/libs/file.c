/**
 * @file   file.c
 * @brief  User-level file access: one wrapper per system call of the file system (lab8), plus
 *         mmap() and munmap() (lab9).
 *
 * Every open(), close(), read(), write(), seek(), fstat(), fsync() and dup2() forwards its
 * arguments unchanged to the matching sys_* function of user/libs/syscall.c. The system call
 * traps into the kernel (kern/syscall/syscall.c), which calls the sysfile_* layer, then the file_*
 * layer that works on the descriptor table of the process, then the vop_* operations of the inode.
 * print_stat() is a small helper that formats a struct stat for the console. dup(), pipe() and
 * mkfifo() are declared in file.h but are not defined here. mmap() converts the kernel's negative
 * error code into the MAP_FAILED pointer; munmap() returns the kernel result unchanged.
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

/**
 * @brief Map a file or anonymous memory into the address space (lab9).
 *
 * Calls sys_mmap() and turns a negative result into MAP_FAILED. The result is carried in an int,
 * which is wide enough because every user address is below USERTOP (0x80000000).
 *
 * @param[in] addr    Wanted start address, or NULL to let the kernel choose.
 * @param[in] len     Length of the mapping in bytes.
 * @param[in] prot    PROT_READ / PROT_WRITE / PROT_EXEC.
 * @param[in] flags   MAP_* flags, for example MAP_PRIVATE | MAP_ANONYMOUS.
 * @param[in] fd      File descriptor of the file to map; ignored with MAP_ANONYMOUS.
 * @param[in] offset  Byte offset into the file, a multiple of the page size.
 * @return The start address of the mapping, or MAP_FAILED on error.
 */
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset) {
    int ret = sys_mmap(addr, len, prot, flags, fd, offset);
    if (ret < 0) {
        return MAP_FAILED;
    }
    return (void *)(uintptr_t)ret;
}

/**
 * @brief Remove a mapping from the address space (lab9).
 * @param[in] addr  Start address of the mapping.
 * @param[in] len   Length of the range to unmap in bytes.
 * @return 0 on success, a negative error code otherwise.
 */
int munmap(void *addr, size_t len) {
    return sys_munmap(addr, len);
}
