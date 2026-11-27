/**
 * @file   fs.h
 * @brief  Top-level file system definitions: sector and device constants, and the per-process
 *         file table (files_struct) with its creation, copy and destruction functions.
 *
 * Every process has a struct files_struct (proc->filesp) that holds its current directory and its
 * open files. The table is one page: the header is followed by the fd_array of struct file entries,
 * whose index is the file descriptor. A table is created by files_create(), filled as a copy of the
 * parent's table by dup_files() (called by copy_files() in proc.c on fork), and released by
 * files_destroy() when the last process that uses it exits. Processes created with CLONE_FS share one table, which is counted by files_count.
 *
 * The inline helpers at the end only read or change files_count.
 */
#ifndef __KERN_FS_FS_H__
#define __KERN_FS_FS_H__

#include <defs.h>
#include <mmu.h>
#include <sem.h>
#include <atomic.h>

/** @brief Size of one disk sector in bytes. */
#define SECTSIZE 512
/** @brief Number of sectors in one page (PGSIZE / SECTSIZE = 8). */
#define PAGE_NSECT (PGSIZE / SECTSIZE)

/** @brief Device number of the swap device (the swap image, see ramdisk.c). */
#define SWAP_DEV_NO 1
/** @brief Device number of the disk that holds the SFS file system (the sfs.img image). */
#define DISK0_DEV_NO 2
/** @brief Device number of a third disk. Not used in lab9: no disk1 is created. */
#define DISK1_DEV_NO 3

/**
 * @brief Set up the file system layer at boot: VFS, the built-in devices, and the SFS mount.
 *
 * Calls vfs_init(), dev_init() and sfs_init() in that order. Called once by kern_init(), after
 * ide_init() has set up the disks (init.c).
 */
void fs_init(void);
/**
 * @brief Shut the file system layer down: flush every mounted file system (vfs_cleanup()).
 *
 * Called by initproc in proc.c, after all user processes have exited.
 */
void fs_cleanup(void);

struct inode;
struct file;

/**
 * @brief The file-related state of a process: current directory, open files and a lock.
 *
 * The table lives in one page. The fd_array is placed right after this header, so its length is
 * FILES_STRUCT_NENTRY (see below). A table can be shared by several processes (CLONE_FS), which is
 * tracked by files_count.
 *
 * @var files_struct::pwd          inode of the current working directory (one reference held), or NULL
 * @var files_struct::fd_array     the open-file table; fd_array[fd] is file descriptor fd
 * @var files_struct::files_count  number of processes that use this table
 * @var files_struct::files_sem    mutex (initial value 1) held by lock_files() around a change of the
 *                                 current directory or of the open files
 */
struct files_struct {
    struct inode *pwd;
    struct file *fd_array;
    int files_count;
    semaphore_t files_sem;
};

/** @brief Bytes of the page left for the fd_array after the files_struct header. */
#define FILES_STRUCT_BUFSIZE (PGSIZE - sizeof(struct files_struct))
/** @brief Number of file descriptors a process can have: FILES_STRUCT_BUFSIZE / sizeof(struct file). */
#define FILES_STRUCT_NENTRY (FILES_STRUCT_BUFSIZE / sizeof(struct file))

/**
 * @brief Take files_sem of a file table. May sleep, so only process context may call it.
 * @param filesp the file table to lock
 */
void lock_files(struct files_struct *filesp);
/**
 * @brief Release files_sem taken by lock_files().
 * @param filesp the file table to unlock
 */
void unlock_files(struct files_struct *filesp);

/**
 * @brief Create an empty file table: no current directory, no open files, count 0.
 *
 * Allocates one page, places fd_array after the header, marks every descriptor FD_NONE and sets
 * files_sem to 1.
 *
 * @return the new table, or NULL if no memory is available
 */
struct files_struct *files_create(void);
/**
 * @brief Free a file table: release the current directory and close every open file.
 *
 * Called when the last process using the table has gone. Asserts that files_count is 0.
 * Each open file is closed with fd_array_close(), and the directory reference is dropped with
 * vop_ref_dec() if pwd is set. The table page is freed.
 *
 * @param filesp the table; must not be used afterwards
 */
void files_destroy(struct files_struct *filesp);
/**
 * @brief Close every open file of a table except stdin (fd 0) and stdout (fd 1).
 *
 * Used by do_execve() before a new program is loaded. The two standard descriptors stay open, so
 * the new program can still use them. The current directory is not changed.
 *
 * @param filesp the table; asserts that files_count is positive
 */
void files_closeall(struct files_struct *filesp);
/**
 * @brief Fill an empty table as a copy of another one: same current directory, duplicated open
 *        files (fork without CLONE_FS).
 *
 * The directory reference is shared (vop_ref_inc). Each open file is duplicated with
 * fd_array_dup(), which takes its own inode and open references, so the child can read and write
 * the same files at its own positions.
 *
 * @param to   the new, empty table (files_count must be 0)
 * @param from the parent table (files_count must be positive)
 * @return always 0
 * @note files_count of to is not changed here; the caller increments it (copy_files() in proc.c).
 */
int dup_files(struct files_struct *to, struct files_struct *from);

/**
 * @brief Return the number of processes that use a file table.
 * @param filesp the table
 * @return files_count
 */
static inline int files_count(struct files_struct *filesp) {
    return filesp->files_count;
}

/**
 * @brief Count one more process that uses a file table.
 * @param filesp the table
 * @return the new files_count
 */
static inline int files_count_inc(struct files_struct *filesp) {
    filesp->files_count += 1;
    return filesp->files_count;
}

/**
 * @brief Count one process less that uses a file table. The caller destroys the table at 0.
 * @param filesp the table
 * @return the new files_count
 */
static inline int files_count_dec(struct files_struct *filesp) {
    filesp->files_count -= 1;
    return filesp->files_count;
}

#endif /* !__KERN_FS_FS_H__ */
