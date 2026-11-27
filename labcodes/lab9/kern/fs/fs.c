/**
 * @file   fs.c
 * @brief  Start-up and shut-down of the file system layer, and the per-process file table
 *         operations (create, destroy, close-all, duplicate, lock).
 *
 * fs_init() is the entry point of the whole file system: it brings up the VFS, the built-in
 * devices and the SFS mount on disk0. The rest of this file implements struct files_struct (fs.h),
 * the table of open files and the current directory that each process owns. proc.c creates,
 * copies and destroys these tables (copy_files(), put_files(), do_execve()); file.c works on the
 * fd_array entries; vfspath.c takes lock_files() around its current-directory accesses.
 */
#include <defs.h>
#include <kmalloc.h>
#include <sem.h>
#include <vfs.h>
#include <dev.h>
#include <file.h>
#include <sfs.h>
#include <inode.h>
#include <assert.h>

/**
 * @brief Bring up the file system layer. Called once by kern_init(), after ide_init().
 *
 * Calls vfs_init() (the VFS device and file-system tables), dev_init() (stdin, stdout and disk0)
 * and sfs_init() (mounts the SFS on disk0 and makes it the boot file system).
 */
void fs_init(void) {
    vfs_init();
    dev_init();
    sfs_init();
}

/**
 * @brief Shut the file system layer down: flush and clean up every mounted file system.
 *
 * Calls vfs_cleanup(), which calls the fs_cleanup hook of each mounted file system. Called by
 * initproc in proc.c after the user processes have finished.
 */
void fs_cleanup(void) {
    vfs_cleanup();
}

/**
 * @brief Take the lock of a file table (files_sem). May sleep, so process context only.
 *
 * Used around the current-directory accesses of vfspath.c (the *_cwd_nolock helpers there).
 *
 * @param filesp the file table to lock
 */
void lock_files(struct files_struct *filesp) {
    down(&(filesp->files_sem));
}

/**
 * @brief Release the lock of a file table taken by lock_files().
 * @param filesp the file table to unlock
 */
void unlock_files(struct files_struct *filesp) {
    up(&(filesp->files_sem));
}

/**
 * @brief Create an empty file table in one page.
 *
 * The table header and the fd_array share one kmalloc'd page: fd_array starts right after the
 * header. The current directory is NULL, the open count is 0, files_sem is 1 (free), and every
 * descriptor is FD_NONE (fd_array_init()). Used by copy_files() and by idleproc setup in proc.c.
 *
 * @return the new table, or NULL if kmalloc() fails. The caller reports -E_NO_MEM.
 */
struct files_struct *files_create(void) {
    //cprintf("[files_create]\n");
    //static_assert((int)FILES_STRUCT_NENTRY > 128);
    struct files_struct *filesp;
    if ((filesp = kmalloc(sizeof(struct files_struct) + FILES_STRUCT_BUFSIZE)) != NULL) {
        filesp->pwd = NULL;
        filesp->fd_array = (void *)(filesp + 1);
        filesp->files_count = 0;
        sem_init(&(filesp->files_sem), 1);
        fd_array_init(filesp->fd_array);
    }
    return filesp;
}

/**
 * @brief Free a file table that no process uses any more.
 *
 * Asserts that files_count is 0. Drops the reference to the current directory (vop_ref_dec()),
 * closes each file whose status is FD_OPENED with fd_array_close(), and checks that every slot
 * ends as FD_NONE. The page is then freed with kfree().
 *
 * @param filesp the table; must not be used after this call
 * @note Called when files_count_dec() on the table returns 0 (proc.c).
 */
void files_destroy(struct files_struct *filesp) {
    //    cprintf("[files_destroy]\n");
    assert(filesp != NULL && files_count(filesp) == 0);
    if (filesp->pwd != NULL) {
        vop_ref_dec(filesp->pwd);
    }
    int i;
    struct file *file = filesp->fd_array;
    for (i = 0; i < FILES_STRUCT_NENTRY; i++, file++) {
        if (file->status == FD_OPENED) {
            fd_array_close(file);
        }
        assert(file->status == FD_NONE);
    }
    kfree(filesp);
}

/**
 * @brief Close every open file of a table except stdin (fd 0) and stdout (fd 1).
 *
 * Each FD_OPENED entry from index 2 on is closed with fd_array_close(). Used by do_execve() before
 * the new program is loaded, so that the standard descriptors survive the exec.
 *
 * @param filesp the table; asserts that files_count is positive
 */
void files_closeall(struct files_struct *filesp) {
    //    cprintf("[files_closeall]\n");
    assert(filesp != NULL && files_count(filesp) > 0);
    int i;
    struct file *file = filesp->fd_array;
    //skip the stdin & stdout
    for (i = 2, file += 2; i < FILES_STRUCT_NENTRY; i++, file++) {
        if (file->status == FD_OPENED) {
            fd_array_close(file);
        }
    }
}

/**
 * @brief Fill an empty table as a copy of another one (fork without CLONE_FS).
 *
 * The current directory is shared: to->pwd gets the same inode, with one more reference
 * (vop_ref_inc()). For each open file of from, the matching slot of to is made FD_INIT and filled
 * by fd_array_dup(), which copies the position and the access mode and takes new inode and open
 * references. Slots that are not open in from stay FD_NONE. Nothing in this function can fail.
 *
 * @param to   the new table; must be empty (files_count 0)
 * @param from the parent's table; must be in use (files_count > 0)
 * @return always 0
 * @note files_count of to is not changed here; copy_files() in proc.c increments it afterwards.
 */
int dup_files(struct files_struct *to, struct files_struct *from) {
    //    cprintf("[dup_fs]\n");
    assert(to != NULL && from != NULL);
    assert(files_count(to) == 0 && files_count(from) > 0);
    if ((to->pwd = from->pwd) != NULL) {
        vop_ref_inc(to->pwd);
    }
    int i;
    struct file *to_file = to->fd_array, *from_file = from->fd_array;
    for (i = 0; i < FILES_STRUCT_NENTRY; i++, to_file++, from_file++) {
        if (from_file->status == FD_OPENED) {
            /* alloc_fd first */
            to_file->status = FD_INIT;
            fd_array_dup(to_file, from_file);
        }
    }
    return 0;
}
