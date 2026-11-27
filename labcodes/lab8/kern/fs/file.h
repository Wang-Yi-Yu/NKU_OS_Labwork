/**
 * @file   file.h
 * @brief  Open-file layer: the struct file table entries of a process and the file_* operations.
 *
 * A process's open files are the fd_array of its files_struct (fs.h); the index of an entry is the
 * file descriptor (fd). Each entry records the state of one open file: its VFS inode, the position,
 * the access mode and a reference count. The file_* functions are called by the sysfile_* layer
 * (sysfile.c), which has already copied the arguments from user space. They look up the entry with
 * the fd, check the mode, and call the vop_* operations of the inode (vfs.h). The fd_array_*
 * functions move an entry between its states (see struct file).
 *
 * Reference-count rules, in short:
 *   - fd_array_open() / fd_array_dup() give an entry one count for the descriptor table;
 *   - every operation in progress (file_read(), file_write(), ...) adds one count for its duration
 *     (fd_array_acquire()) and drops it at the end (fd_array_release());
 *   - fd_array_close() drops the descriptor's count. When the count reaches 0 the entry is freed
 *     (fd_array_free()), which closes the inode with vfs_close().
 * So a file closed while another process is still reading from it stays usable until that read ends.
 */
#ifndef __KERN_FS_FILE_H__
#define __KERN_FS_FILE_H__

//#include <types.h>
#include <fs.h>
#include <proc.h>
#include <atomic.h>
#include <assert.h>

struct inode;
struct stat;
struct dirent;

/**
 * @brief The state of one entry of a process's open-file table.
 *
 * @var file::status     state of the entry, one of the values of the enum below
 * @var file::readable   1 if the file was opened for reading (O_RDONLY or O_RDWR)
 * @var file::writable   1 if the file was opened for writing (O_WRONLY or O_RDWR)
 * @var file::fd        the file descriptor of this entry, i.e. its index in fd_array. Set once by
 *                       fd_array_init() and never changed
 * @var file::pos       byte position for the next read or write on the file; changed by reads,
 *                       writes and seeks
 * @var file::node      the VFS inode of the file, opened with vfs_open(). The entry owns that open
 *                       reference and gives it back with vfs_close() when it is freed. NULL while
 *                       the entry is FD_NONE or FD_INIT
 * @var file::open_count number of references to this entry: one from the descriptor table while it
 *                       is FD_OPENED, plus one for each operation in progress. The entry is freed
 *                       when it reaches 0
 */
struct file {
    /**
     * @brief State of an open-file table entry.
     *
     * @note Transitions: FD_NONE -> FD_INIT (slot allocated) -> FD_OPENED (node set up and
     *       opened) -> FD_CLOSED (close() called while operations are still using it) -> FD_NONE
     *       (last reference dropped). The state FD_INIT is used only while the entry is being set up.
     */
    enum {
        /** @brief The slot is free. */
        FD_NONE,
        /** @brief The slot is allocated but not yet open; node may still be NULL. */
        FD_INIT,
        /** @brief The file is open and can be used. */
        FD_OPENED,
        /** @brief Closed by close(), but still referenced by operations in progress. */
        FD_CLOSED,
    } status;
    bool readable;
    bool writable;
    int fd;
    off_t pos;
    struct inode *node;
    int open_count;
};

/**
 * @brief Set up a new open-file table: every entry FD_NONE with its fd number and count 0.
 * @param fd_array the table, FILES_STRUCT_NENTRY entries long
 */
void fd_array_init(struct file *fd_array);
/**
 * @brief Mark a set-up entry as open: status FD_OPENED, and one count for the descriptor table.
 *
 * The entry must be FD_INIT with a node set. Used by file_open() and fd_array_dup().
 *
 * @param file the entry, with node already set
 */
void fd_array_open(struct file *file);
/**
 * @brief Close the descriptor of an open entry (the close() of the file layer).
 *
 * Sets the status to FD_CLOSED and drops the descriptor's reference. If operations are still in
 * progress the entry lives on until they finish; otherwise it is freed at once. The entry must be
 * FD_OPENED.
 *
 * @param file the entry to close
 */
void fd_array_close(struct file *file);
/**
 * @brief Make an entry a second descriptor of the same open file (the dup() of the file layer).
 *
 * Copies position and access mode, takes one more inode reference and open count (vop_ref_inc(),
 * vop_open_inc()) and opens the new entry. The entry "to" must be FD_INIT, and "from" FD_OPENED.
 *
 * @param to   the new entry, FD_INIT with no node
 * @param from the open entry to duplicate
 */
void fd_array_dup(struct file *to, struct file *from);
/**
 * @brief Check that a descriptor is open and has the requested access mode.
 * @param fd       the file descriptor to test
 * @param readable true if the caller needs read access
 * @param writable true if the caller needs write access
 * @return true if fd is an open file with every requested mode, false otherwise
 */
bool file_testfd(int fd, bool readable, bool writable);

/**
 * @brief Open or create a file for the current process (open() in the file layer).
 *
 * Finds a free descriptor, opens the file through vfs_open(), and makes the entry FD_OPENED. With
 * O_APPEND the position is set to the end of the file.
 *
 * @param path       path of the file, passed to vfs_open()
 * @param open_flags O_RDONLY, O_WRONLY or O_RDWR, combined with O_CREAT, O_TRUNC, O_APPEND, ...
 * @return the new file descriptor (>= 0) on success, or a negative error:
 *         -E_INVAL for an invalid access mode; -E_MAX_OPEN if the process has no free descriptor;
 *         or the error of vfs_open() or vop_fstat(), for example -E_NOENT
 */
int file_open(char *path, uint32_t open_flags);
/**
 * @brief Close a descriptor (close() in the file layer).
 * @param fd the descriptor to close
 * @return 0 on success, -E_INVAL if fd is not an open file
 */
int file_close(int fd);
/**
 * @brief Read from an open file at its current position and advance the position.
 *
 * The transfer is done by vop_read() on a stack iobuf that starts at file->pos. On return
 * *copied_store holds the number of bytes read; the position is advanced by that amount unless the
 * file was closed during the read.
 *
 * @param fd           the descriptor; must be open and readable
 * @param base         kernel buffer that receives the data, at least len bytes
 * @param len          number of bytes wanted
 * @param copied_store [out] set to the number of bytes actually read (0 on error)
 * @return 0 or the result of vop_read(); -E_INVAL if fd is not open or not readable
 */
int file_read(int fd, void *base, size_t len, size_t *copied_store);
/**
 * @brief Write to an open file at its current position and advance the position.
 *
 * Mirror of file_read(), calling vop_write().
 *
 * @param fd           the descriptor; must be open and writable
 * @param base         kernel buffer with the data, at least len bytes
 * @param len          number of bytes to write
 * @param copied_store [out] set to the number of bytes actually written (0 on error)
 * @return 0 or the result of vop_write(); -E_INVAL if fd is not open or not writable
 */
int file_write(int fd, void *base, size_t len, size_t *copied_store);
/**
 * @brief Move the position of an open file (lseek() in the file layer).
 *
 * The new position is computed from pos and whence, then checked with vop_tryseek(). Only on
 * success is file->pos changed.
 *
 * @param fd     the descriptor; must be open
 * @param pos    offset, relative to the origin given by whence
 * @param whence LSEEK_SET (from the start), LSEEK_CUR (from the current position) or
 *               LSEEK_END (from the end of the file)
 * @return 0 on success; -E_INVAL for an unknown whence or a fd that is not open; or the error of
 *         vop_fstat() or vop_tryseek(), for example -E_INVAL for a bad position on a device
 */
int file_seek(int fd, off_t pos, int whence);
/**
 * @brief Get the status of an open file (fstat() in the file layer).
 * @param fd   the descriptor; must be open
 * @param stat [out] receives the status of the file
 * @return 0 on success; -E_INVAL if fd is not open; or the error of vop_fstat()
 */
int file_fstat(int fd, struct stat *stat);
/**
 * @brief Flush an open file to its device (fsync() in the file layer).
 * @param fd the descriptor; must be open
 * @return 0 on success; -E_INVAL if fd is not open; or the error of vop_fsync()
 */
int file_fsync(int fd);
/**
 * @brief Read the next directory entry of an open directory.
 *
 * The name is read at dirent->offset, and dirent->offset advances past it on success. The
 * caller keeps the offset from one call to the next.
 *
 * @param fd     the descriptor of an open directory
 * @param dirent [in,out] the entry to fill: offset on input and output, name on output
 * @return 0 on success; -E_INVAL if fd is not open; -E_NOENT when there are no more entries; or
 *         the error of vop_getdirentry()
 */
int file_getdirentry(int fd, struct dirent *dirent);
/**
 * @brief Make a new descriptor refer to an open file (dup2()-like, in the file layer).
 *
 * If fd2 is NO_FD the lowest free descriptor is used, otherwise fd2 itself. An error is returned if
 * fd2 is already open: the file is not closed first.
 *
 * @param fd1 an open descriptor to duplicate
 * @param fd2 the descriptor to use, or NO_FD to pick the lowest free one
 * @return the new descriptor on success; -E_INVAL if fd1 is not open or fd2 is out of range;
 *         -E_BUSY if fd2 is already in use; -E_MAX_OPEN if NO_FD was given and none is free
 */
int file_dup(int fd1, int fd2);
/**
 * @brief Create a pipe. Declared but not implemented in lab8; sysfile_pipe() returns -E_UNIMP.
 * @param fd array that would receive the two descriptors (unused)
 * @return not implemented; no body exists in file.c
 */
int file_pipe(int fd[]);
/**
 * @brief Create a named pipe. Declared but not implemented in lab8; sysfile_mkfifo() returns -E_UNIMP.
 * @param name       the name of the pipe (unused)
 * @param open_flags open flags (unused)
 * @return not implemented; no body exists in file.c
 */
int file_mkfifo(const char *name, uint32_t open_flags);

/**
 * @brief Return the open count of a file-table entry.
 * @param file the entry
 * @return file->open_count
 */
static inline int fopen_count(struct file *file) {
    return file->open_count;
}

/**
 * @brief Add one reference to a file-table entry.
 * @param file the entry
 * @return the new open count
 */
static inline int fopen_count_inc(struct file *file) {
    file->open_count += 1;
    return file->open_count;
}

/**
 * @brief Drop one reference from a file-table entry. The caller frees the entry at 0.
 * @param file the entry
 * @return the new open count
 */
static inline int fopen_count_dec(struct file *file) {
    file->open_count -= 1;
    return file->open_count;
}

#endif /* !__KERN_FS_FILE_H__ */
