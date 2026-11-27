/**
 * @file   sysfile.c
 * @brief  Kernel side of the file system system calls: copies arguments from user space, calls the
 *         file layer or the VFS, and copies results back.
 *
 * Each sysfile_* function is called by a sys_* handler in kern/syscall/syscall.c. The handler gives
 * raw user values; this layer makes them safe to use. Paths are copied into a kernel buffer with
 * copy_path(). Data for read, write, fstat, getcwd and getdirentry is moved in and out of user
 * memory with copy_to_user()/copy_from_user(), while the process's mm is locked (lock_mm()), and
 * the user ranges are checked by the vmm layer (vmm.h). A failed check gives -E_INVAL.
 *
 * Call chain, for example write(fd, buf, n): sys_write() -> sysfile_write() (copy from user) ->
 * file_write() (file.c) -> vop_write() -> the file system or device.
 */
#include <defs.h>
#include <string.h>
#include <vmm.h>
#include <proc.h>
#include <kmalloc.h>
#include <vfs.h>
#include <file.h>
#include <iobuf.h>
#include <sysfile.h>
#include <stat.h>
#include <dirent.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>

/** @brief Size of the kernel bounce buffer used by sysfile_read() and sysfile_write(), in bytes. */
#define IOBUF_SIZE 4096

/**
 * @brief Copy a path string from user memory into a newly allocated kernel buffer.
 *
 * At most FS_MAX_FPATH_LEN characters are copied, plus the terminating NUL. The buffer is allocated
 * with kmalloc() and must be released with kfree() by the caller when the copy succeeds.
 *
 * @param to   [out] receives the kernel buffer on success; not changed on failure
 * @param from user-space address of the NUL-terminated path
 * @return 0 on success; -E_NO_MEM if the buffer cannot be allocated; -E_INVAL if the string cannot
 *         be read from user memory or is longer than FS_MAX_FPATH_LEN
 */
static int copy_path(char **to, const char *from) {
    struct mm_struct *mm = current->mm;
    char *buffer;
    if ((buffer = kmalloc(FS_MAX_FPATH_LEN + 1)) == NULL) {
        return -E_NO_MEM;
    }
    lock_mm(mm);
    if (!copy_string(mm, buffer, from, FS_MAX_FPATH_LEN + 1)) {
        unlock_mm(mm);
        goto failed_cleanup;
    }
    unlock_mm(mm);
    *to = buffer;
    return 0;

failed_cleanup:
    kfree(buffer);
    return -E_INVAL;
}

/**
 * @brief Open or create a file for the current process (open() system call).
 *
 * The path is copied from user space, passed to file_open(), and the kernel copy is freed.
 *
 * @param __path     user-space path
 * @param open_flags access mode and modifiers, see file_open()
 * @return the new descriptor, or a negative error (-E_INVAL, -E_MAX_OPEN, -E_NO_MEM, or the VFS error)
 */
int sysfile_open(const char *__path, uint32_t open_flags) {
    int ret;
    char *path;
    if ((ret = copy_path(&path, __path)) != 0) {
        return ret;
    }
    ret = file_open(path, open_flags);
    kfree(path);
    return ret;
}

/**
 * @brief Close a file descriptor (close() system call). A thin wrapper around file_close().
 *
 * @param fd the descriptor
 * @return 0 on success, -E_INVAL if fd is not open
 */
int sysfile_close(int fd) {
    return file_close(fd);
}

/**
 * @brief Read from a file into user memory (read() system call).
 *
 * The request is split into pieces of at most IOBUF_SIZE bytes. Each piece is read into a kernel
 * buffer by file_read() and copied to the user buffer with copy_to_user(), then the loop goes on
 * with the rest. The loop stops at the first error, at end of file (a piece of length 0), or when
 * len bytes have been done. Bytes copied before an error are not lost: the count is returned.
 *
 * @param fd   the descriptor; must be open and readable
 * @param base user-space buffer that receives the data
 * @param len  number of bytes wanted; 0 returns 0
 * @return the number of bytes copied to user memory if any; otherwise the error code, or 0 at end of file.
 *         -E_INVAL if fd is not readable or base cannot be written. -E_NO_MEM if the kernel buffer
 *         cannot be allocated.
 */
int sysfile_read(int fd, void *base, size_t len) {
    struct mm_struct *mm = current->mm;
    if (len == 0) {
        return 0;
    }
    if (!file_testfd(fd, 1, 0)) {
        return -E_INVAL;
    }
    void *buffer;
    if ((buffer = kmalloc(IOBUF_SIZE)) == NULL) {
        return -E_NO_MEM;
    }

    int ret = 0;
    size_t copied = 0, alen;
    while (len != 0) {
        if ((alen = IOBUF_SIZE) > len) {
            alen = len;
        }
        ret = file_read(fd, buffer, alen, &alen);
        if (alen != 0) {
            lock_mm(mm);
            {
                if (copy_to_user(mm, base, buffer, alen)) {
                    assert(len >= alen);
                    base += alen, len -= alen, copied += alen;
                } else if (ret == 0) {
                    ret = -E_INVAL;
                }
            }
            unlock_mm(mm);
        }
        if (ret != 0 || alen == 0) {
            goto out;
        }
    }

out:
    kfree(buffer);
    if (copied != 0) {
        return copied;
    }
    return ret;
}

/**
 * @brief Write user memory to a file (write() system call).
 *
 * The mirror of sysfile_read(): each piece of at most IOBUF_SIZE bytes is copied from user memory
 * with copy_from_user() and written with file_write(). The loop stops at the first error or when
 * a piece is written with 0 bytes. The number of bytes written so far is returned in that case.
 *
 * @param fd   the descriptor; must be open and writable
 * @param base user-space buffer with the data
 * @param len  number of bytes to write; 0 returns 0
 * @return the number of bytes written if any; otherwise a negative error. -E_INVAL if fd is not
 *         writable or base cannot be read. -E_NO_MEM if the kernel buffer cannot be allocated.
 */
int sysfile_write(int fd, void *base, size_t len) {
    struct mm_struct *mm = current->mm;
    if (len == 0) {
        return 0;
    }
    if (!file_testfd(fd, 0, 1)) {
        return -E_INVAL;
    }
    void *buffer;
    if ((buffer = kmalloc(IOBUF_SIZE)) == NULL) {
        return -E_NO_MEM;
    }

    int ret = 0;
    size_t copied = 0, alen;
    while (len != 0) {
        if ((alen = IOBUF_SIZE) > len) {
            alen = len;
        }
        lock_mm(mm);
        {
            if (!copy_from_user(mm, buffer, base, alen, 0)) {
                ret = -E_INVAL;
            }
        }
        unlock_mm(mm);
        if (ret == 0) {
            ret = file_write(fd, buffer, alen, &alen);
            if (alen != 0) {
                assert(len >= alen);
                base += alen, len -= alen, copied += alen;
            }
        }
        if (ret != 0 || alen == 0) {
            goto out;
        }
    }

out:
    kfree(buffer);
    if (copied != 0) {
        return copied;
    }
    return ret;
}

/**
 * @brief Move the position of an open file (lseek() system call). A thin wrapper around file_seek().
 *
 * @param fd     the descriptor
 * @param pos    offset relative to whence
 * @param whence LSEEK_SET, LSEEK_CUR or LSEEK_END
 * @return 0 on success, or a negative error (see file_seek())
 */
int sysfile_seek(int fd, off_t pos, int whence) {
    return file_seek(fd, pos, whence);
}

/**
 * @brief Get the status of an open file and copy it to user memory (fstat() system call).
 *
 * The status is first written to a kernel struct, then copied out with copy_to_user() under the
 * mm lock.
 *
 * @param fd     the descriptor
 * @param __stat user-space struct stat to fill in
 * @return 0 on success; the error of file_fstat() if fd is not open; -E_INVAL if __stat is not
 *         writable user memory
 */
int sysfile_fstat(int fd, struct stat *__stat) {
    struct mm_struct *mm = current->mm;
    int ret;
    struct stat __local_stat, *stat = &__local_stat;
    if ((ret = file_fstat(fd, stat)) != 0) {
        return ret;
    }

    lock_mm(mm);
    {
        if (!copy_to_user(mm, __stat, stat, sizeof(struct stat))) {
            ret = -E_INVAL;
        }
    }
    unlock_mm(mm);
    return ret;
}

/**
 * @brief Flush an open file to its device (fsync() system call). A thin wrapper around file_fsync().
 *
 * @param fd the descriptor
 * @return 0 on success, or a negative error (see file_fsync())
 */
int sysfile_fsync(int fd) {
    return file_fsync(fd);
}

/**
 * @brief Change the current directory of the calling process (chdir() system call).
 *
 * @param __path user-space path of the new directory
 * @return 0 on success; -E_INVAL if the path cannot be copied; or the error of vfs_chdir()
 */
int sysfile_chdir(const char *__path) {
    int ret;
    char *path;
    if ((ret = copy_path(&path, __path)) != 0) {
        return ret;
    }
    ret = vfs_chdir(path);
    kfree(path);
    return ret;
}

/**
 * @brief Create a hard link (link() system call): path2 names the same file as path1.
 *
 * Both paths are copied; if the second copy fails the first is freed as well.
 *
 * @param __path1 user-space path of the existing file
 * @param __path2 user-space path of the new name
 * @return 0 on success; -E_INVAL or -E_NO_MEM while copying the paths; or the error of vfs_link()
 */
int sysfile_link(const char *__path1, const char *__path2) {
    int ret;
    char *old_path, *new_path;
    if ((ret = copy_path(&old_path, __path1)) != 0) {
        return ret;
    }
    if ((ret = copy_path(&new_path, __path2)) != 0) {
        kfree(old_path);
        return ret;
    }
    ret = vfs_link(old_path, new_path);
    kfree(old_path), kfree(new_path);
    return ret;
}

/**
 * @brief Rename a file or directory (rename() system call).
 *
 * Both paths are copied; if the second copy fails the first is freed as well.
 *
 * @param __path1 user-space path of the old name
 * @param __path2 user-space path of the new name
 * @return 0 on success; -E_INVAL or -E_NO_MEM while copying the paths; or the error of vfs_rename()
 */
int sysfile_rename(const char *__path1, const char *__path2) {
    int ret;
    char *old_path, *new_path;
    if ((ret = copy_path(&old_path, __path1)) != 0) {
        return ret;
    }
    if ((ret = copy_path(&new_path, __path2)) != 0) {
        kfree(old_path);
        return ret;
    }
    ret = vfs_rename(old_path, new_path);
    kfree(old_path), kfree(new_path);
    return ret;
}

/**
 * @brief Remove a name from the file system (unlink() system call).
 *
 * @param __path user-space path to remove
 * @return 0 on success; -E_INVAL or -E_NO_MEM while copying the path; or the error of vfs_unlink()
 */
int sysfile_unlink(const char *__path) {
    int ret;
    char *path;
    if ((ret = copy_path(&path, __path)) != 0) {
        return ret;
    }
    ret = vfs_unlink(path);
    kfree(path);
    return ret;
}

/**
 * @brief Copy the current directory path to a user buffer (getcwd() system call).
 *
 * The user range [buf, buf + len) is checked for write access with user_mem_check() under the mm
 * lock. vfs_getcwd() then writes the path into it through an iobuf that spans the whole buffer.
 *
 * @param buf user-space buffer that receives the path
 * @param len size of buf in bytes; must not be 0
 * @return 0 on success; -E_INVAL if len is 0, the buffer is not writable user memory, or vfs_getcwd() fails
 */
int sysfile_getcwd(char *buf, size_t len) {
    struct mm_struct *mm = current->mm;
    if (len == 0) {
        return -E_INVAL;
    }

    int ret = -E_INVAL;
    lock_mm(mm);
    {
        if (user_mem_check(mm, (uintptr_t)buf, len, 1)) {
            struct iobuf __iob, *iob = iobuf_init(&__iob, buf, len, 0);
            ret = vfs_getcwd(iob);
        }
    }
    unlock_mm(mm);
    return ret;
}

/**
 * @brief Read the next entry of a directory into user memory (getdirentry system call).
 *
 * The user's offset field is read first, because it says where the read starts. Then
 * file_getdirentry() fills a kernel struct, and the whole struct is copied back to user memory.
 *
 * @param fd        the descriptor of an open directory
 * @param __direntp user-space struct dirent; offset is read, and the entry is written back
 * @return 0 on success; -E_INVAL if the user struct cannot be read or written; -E_NO_MEM if the
 *         kernel struct cannot be allocated; or the error of file_getdirentry() (-E_NOENT at the end)
 */
int sysfile_getdirentry(int fd, struct dirent *__direntp) {
    struct mm_struct *mm = current->mm;
    struct dirent *direntp;
    if ((direntp = kmalloc(sizeof(struct dirent))) == NULL) {
        return -E_NO_MEM;
    }

    int ret = 0;
    lock_mm(mm);
    {
        if (!copy_from_user(mm, &(direntp->offset), &(__direntp->offset), sizeof(direntp->offset),
                            1)) {
            ret = -E_INVAL;
        }
    }
    unlock_mm(mm);

    if (ret != 0 || (ret = file_getdirentry(fd, direntp)) != 0) {
        goto out;
    }

    lock_mm(mm);
    {
        if (!copy_to_user(mm, __direntp, direntp, sizeof(struct dirent))) {
            ret = -E_INVAL;
        }
    }
    unlock_mm(mm);

out:
    kfree(direntp);
    return ret;
}

/**
 * @brief Duplicate an open file descriptor (dup2() system call). A thin wrapper around file_dup().
 *
 * @param fd1 an open descriptor
 * @param fd2 the wanted new descriptor, or NO_FD
 * @return the new descriptor, or a negative error (see file_dup())
 */
int sysfile_dup(int fd1, int fd2) {
    return file_dup(fd1, fd2);
}

/**
 * @brief Create a pipe. Not implemented in lab8.
 *
 * Lab8 only describes the pipe design (Challenge 1) and does not implement it; this stub always
 * fails.
 *
 * @param fd_store user-space array for the two descriptors (unused)
 * @return -E_UNIMP
 */
int sysfile_pipe(int *fd_store) {
    return -E_UNIMP;
}

/**
 * @brief Create a named pipe. Not implemented in lab8.
 *
 * @param __name    user-space name of the pipe (unused)
 * @param open_flags open flags (unused)
 * @return -E_UNIMP
 */
int sysfile_mkfifo(const char *__name, uint32_t open_flags) {
    return -E_UNIMP;
}
