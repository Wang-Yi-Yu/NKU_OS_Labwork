/**
 * @file   file.c
 * @brief  Open-file layer: the per-process table of struct file entries and the file_* operations
 *         (open, close, read, write, seek, fstat, fsync, getdirentry, dup).
 *
 * The table is the fd_array of the current process (current->filesp, fs.h). Descriptor n is the
 * entry at index n. file_* functions take a descriptor, find its entry with fd2file(), check the
 * access mode, and pass the request to the VFS (vfs.h) through the vop_* macros. The entry is
 * kept alive during the request by fd_array_acquire()/fd_array_release(), so a close() from another
 * path cannot free it in the middle of a read or write.
 *
 * Callers: the file_* functions are called by the sysfile_* layer (sysfile.c). fd_array_init(),
 * fd_array_open(), fd_array_close() and fd_array_dup() are also used by fs.c when a table is
 * created, copied or destroyed.
 */
#include <defs.h>
#include <string.h>
#include <vfs.h>
#include <proc.h>
#include <file.h>
#include <unistd.h>
#include <iobuf.h>
#include <inode.h>
#include <stat.h>
#include <dirent.h>
#include <error.h>
#include <assert.h>

/**
 * @brief Test whether a descriptor number is a valid index into the fd_array.
 *
 * This only checks the range 0 .. FILES_STRUCT_NENTRY - 1; it does not check that the file is open.
 *
 * @param fd the descriptor number to test
 */
#define testfd(fd) ((fd) >= 0 && (fd) < FILES_STRUCT_NENTRY)

/**
 * @brief Return the open-file table of the current process.
 *
 * Asserts that the process has a table that is in use (files_count > 0).
 *
 * @return the fd_array of current->filesp
 */
static struct file *get_fd_array(void) {
    struct files_struct *filesp = current->filesp;
    assert(filesp != NULL && files_count(filesp) > 0);
    return filesp->fd_array;
}

/**
 * @brief Initialise an open-file table: every entry FD_NONE, open count 0, and its fd number set.
 *
 * Called by files_create() (fs.c).
 *
 * @param fd_array the table, FILES_STRUCT_NENTRY entries
 */
void fd_array_init(struct file *fd_array) {
    int fd;
    struct file *file = fd_array;
    for (fd = 0; fd < FILES_STRUCT_NENTRY; fd++, file++) {
        file->open_count = 0;
        file->status = FD_NONE, file->fd = fd;
    }
}

/**
 * @brief Allocate a free entry of the open-file table, either the lowest free one or a given one.
 *
 * If fd is NO_FD, the first entry with status FD_NONE is used. Otherwise entry fd is used, and it
 * must be free. On success the entry becomes FD_INIT with node = NULL.
 *
 * @param fd         the descriptor wanted, or NO_FD for any free one
 * @param file_store [out] receives the allocated entry on success
 * @return 0 on success; -E_MAX_OPEN if NO_FD was given and no entry is free; -E_BUSY if the
 *         requested fd is in use; -E_INVAL if the requested fd is out of range
 */
static int fd_array_alloc(int fd, struct file **file_store) {
    //    panic("debug");
    struct file *file = get_fd_array();
    if (fd == NO_FD) {
        for (fd = 0; fd < FILES_STRUCT_NENTRY; fd++, file++) {
            if (file->status == FD_NONE) {
                goto found;
            }
        }
        return -E_MAX_OPEN;
    } else {
        if (testfd(fd)) {
            file += fd;
            if (file->status == FD_NONE) {
                goto found;
            }
            return -E_BUSY;
        }
        return -E_INVAL;
    }
found:
    assert(fopen_count(file) == 0);
    file->status = FD_INIT, file->node = NULL;
    *file_store = file;
    return 0;
}

/**
 * @brief Give back an entry that was allocated but is not needed, or that is closed and idle.
 *
 * The entry must be FD_INIT (allocation failed, see file_open()) or FD_CLOSED with no references
 * left. For FD_CLOSED the inode is released with vfs_close(). The entry becomes FD_NONE.
 *
 * @param file the entry to free
 */
static void fd_array_free(struct file *file) {
    assert(file->status == FD_INIT || file->status == FD_CLOSED);
    assert(fopen_count(file) == 0);
    if (file->status == FD_CLOSED) {
        vfs_close(file->node);
    }
    file->status = FD_NONE;
}

/**
 * @brief Add one reference to an open entry for the duration of an operation.
 *
 * Paired with fd_array_release(). The entry must be FD_OPENED.
 *
 * @param file the entry
 */
static void fd_array_acquire(struct file *file) {
    assert(file->status == FD_OPENED);
    fopen_count_inc(file);
}

/**
 * @brief Drop a reference taken by fd_array_acquire() or by the descriptor table.
 *
 * If that was the last reference, the entry is freed with fd_array_free(). The entry must be FD_OPENED
 * or FD_CLOSED, and its count must be positive.
 *
 * @param file the entry
 */
static void fd_array_release(struct file *file) {
    assert(file->status == FD_OPENED || file->status == FD_CLOSED);
    assert(fopen_count(file) > 0);
    if (fopen_count_dec(file) == 0) {
        fd_array_free(file);
    }
}

/**
 * @brief Make an allocated entry an open file: FD_INIT -> FD_OPENED, count 1.
 *
 * The count of 1 is the reference of the descriptor table. The entry must have a node.
 *
 * @param file the entry, FD_INIT with node set
 */
void fd_array_open(struct file *file) {
    assert(file->status == FD_INIT && file->node != NULL);
    file->status = FD_OPENED;
    fopen_count_inc(file);
}

/**
 * @brief Close the descriptor of an open entry: FD_OPENED -> FD_CLOSED, and drop the table's reference.
 *
 * If other operations still hold the entry, it stays FD_CLOSED and is freed by their last release.
 * Otherwise it is freed now.
 *
 * @param file the entry, must be FD_OPENED
 */
void fd_array_close(struct file *file) {
    assert(file->status == FD_OPENED);
    assert(fopen_count(file) > 0);
    file->status = FD_CLOSED;
    if (fopen_count_dec(file) == 0) {
        fd_array_free(file);
    }
}

/**
 * @brief Make an allocated entry a second open descriptor of the file that from refers to.
 *
 * Copies pos, readable and writable from from. Takes one inode reference (vop_ref_inc) and one open
 * count (vop_open_inc) for the new entry, then opens it. Used by dup_files() (fs.c) and file_dup().
 *
 * @param to   the new entry, FD_INIT with no node
 * @param from the open entry to duplicate, FD_OPENED
 */
void fd_array_dup(struct file *to, struct file *from) {
    //cprintf("[fd_array_dup]from fd=%d, to fd=%d\n",from->fd, to->fd);
    assert(to->status == FD_INIT && from->status == FD_OPENED);
    to->pos = from->pos;
    to->readable = from->readable;
    to->writable = from->writable;
    struct inode *node = from->node;
    vop_ref_inc(node), vop_open_inc(node);
    to->node = node;
    fd_array_open(to);
}

/**
 * @brief Look up an open file of the current process by its descriptor.
 *
 * @param fd         the descriptor
 * @param file_store [out] receives the entry on success
 * @return 0 on success; -E_INVAL if fd is out of range or the entry is not FD_OPENED
 */
static inline int fd2file(int fd, struct file **file_store) {
    if (testfd(fd)) {
        struct file *file = get_fd_array() + fd;
        if (file->status == FD_OPENED && file->fd == fd) {
            *file_store = file;
            return 0;
        }
    }
    return -E_INVAL;
}

/**
 * @brief Check that a descriptor is open and has the requested access modes.
 *
 * @param fd       the descriptor
 * @param readable true if read access is needed
 * @param writable true if write access is needed
 * @return true if the file is open and every requested mode is set, false otherwise
 */
bool file_testfd(int fd, bool readable, bool writable) {
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return 0;
    }
    if (readable && !file->readable) {
        return 0;
    }
    if (writable && !file->writable) {
        return 0;
    }
    return 1;
}

/**
 * @brief Open or create a file and install it in a free descriptor of the current process.
 *
 * The access mode is taken from open_flags & O_ACCMODE: O_RDONLY, O_WRONLY or O_RDWR. A free entry
 * is allocated first; then vfs_open() opens the inode. With O_APPEND the position starts at the end
 * of the file (vop_fstat()). On any failure the entry is released again.
 *
 * @param path       path of the file, passed to vfs_open()
 * @param open_flags access mode plus O_CREAT, O_TRUNC, O_APPEND, ...
 * @return the new descriptor (>= 0) on success; -E_INVAL for an invalid access mode;
 *         -E_MAX_OPEN if no descriptor is free; or the error of vfs_open() or vop_fstat()
 */
int file_open(char *path, uint32_t open_flags) {
    bool readable = 0, writable = 0;
    switch (open_flags & O_ACCMODE) {
        case O_RDONLY:
            readable = 1;
            break;
        case O_WRONLY:
            writable = 1;
            break;
        case O_RDWR:
            readable = writable = 1;
            break;
        default:
            return -E_INVAL;
    }
    int ret;
    struct file *file;
    if ((ret = fd_array_alloc(NO_FD, &file)) != 0) {
        return ret;
    }
    struct inode *node;
    if ((ret = vfs_open(path, open_flags, &node)) != 0) {
        fd_array_free(file);
        return ret;
    }
    file->pos = 0;
    if (open_flags & O_APPEND) {
        struct stat __stat, *stat = &__stat;
        if ((ret = vop_fstat(node, stat)) != 0) {
            vfs_close(node);
            fd_array_free(file);
            return ret;
        }
        file->pos = stat->st_size;
    }
    file->node = node;
    file->readable = readable;
    file->writable = writable;
    fd_array_open(file);
    return file->fd;
}

/**
 * @brief Close a descriptor of the current process.
 *
 * @param fd the descriptor
 * @return 0 on success, -E_INVAL if fd is not an open file
 */
int file_close(int fd) {
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    fd_array_close(file);
    return 0;
}

/**
 * @brief Read from an open file at its current position.
 *
 * The entry is held for the duration (fd_array_acquire()). A stack iobuf starts at file->pos and
 * vop_read() fills it. The number of bytes read is copied to *copied_store, and file->pos moves
 * forward by that amount unless the file was closed meanwhile.
 *
 * @param fd           the descriptor; must be open and readable
 * @param base         kernel buffer that receives the data
 * @param len          maximum number of bytes to read
 * @param copied_store [out] number of bytes read; set to 0 on entry, so it is 0 on error
 * @return 0 or the result of vop_read(); -E_INVAL if fd is not open or not readable
 */
int file_read(int fd, void *base, size_t len, size_t *copied_store) {
    int ret;
    struct file *file;
    *copied_store = 0;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    if (!file->readable) {
        return -E_INVAL;
    }
    fd_array_acquire(file);

    struct iobuf __iob, *iob = iobuf_init(&__iob, base, len, file->pos);
    ret = vop_read(file->node, iob);

    size_t copied = iobuf_used(iob);
    if (file->status == FD_OPENED) {
        file->pos += copied;
    }
    *copied_store = copied;
    fd_array_release(file);
    return ret;
}

/**
 * @brief Write to an open file at its current position.
 *
 * The same as file_read(), but with vop_write(), and the file must be writable.
 *
 * @param fd           the descriptor; must be open and writable
 * @param base         kernel buffer with the data
 * @param len          number of bytes to write
 * @param copied_store [out] number of bytes written; set to 0 on entry
 * @return 0 or the result of vop_write(); -E_INVAL if fd is not open or not writable
 */
int file_write(int fd, void *base, size_t len, size_t *copied_store) {
    int ret;
    struct file *file;
    *copied_store = 0;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    if (!file->writable) {
        return -E_INVAL;
    }
    fd_array_acquire(file);

    struct iobuf __iob, *iob = iobuf_init(&__iob, base, len, file->pos);
    ret = vop_write(file->node, iob);

    size_t copied = iobuf_used(iob);
    if (file->status == FD_OPENED) {
        file->pos += copied;
    }
    *copied_store = copied;
    fd_array_release(file);
    return ret;
}

/**
 * @brief Move the position of an open file.
 *
 * The new position is computed from pos and whence. For LSEEK_END the size comes from vop_fstat().
 * The result is checked by vop_tryseek(), and only then is file->pos set.
 *
 * @param fd     the descriptor; must be open
 * @param pos    offset relative to the origin chosen by whence
 * @param whence LSEEK_SET, LSEEK_CUR or LSEEK_END
 * @return 0 on success; -E_INVAL for an unknown whence; or the error of vop_fstat() or
 *         vop_tryseek()
 */
int file_seek(int fd, off_t pos, int whence) {
    struct stat __stat, *stat = &__stat;
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    fd_array_acquire(file);

    switch (whence) {
        case LSEEK_SET:
            break;
        case LSEEK_CUR:
            pos += file->pos;
            break;
        case LSEEK_END:
            if ((ret = vop_fstat(file->node, stat)) == 0) {
                pos += stat->st_size;
            }
            break;
        default:
            ret = -E_INVAL;
    }

    if (ret == 0) {
        if ((ret = vop_tryseek(file->node, pos)) == 0) {
            file->pos = pos;
        }
        //    cprintf("file_seek, pos=%d, whence=%d, ret=%d\n", pos, whence, ret);
    }
    fd_array_release(file);
    return ret;
}

/**
 * @brief Get the status of an open file.
 *
 * @param fd   the descriptor; must be open
 * @param stat [out] receives the status
 * @return 0 on success; -E_INVAL if fd is not open; or the error of vop_fstat()
 */
int file_fstat(int fd, struct stat *stat) {
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    fd_array_acquire(file);
    ret = vop_fstat(file->node, stat);
    fd_array_release(file);
    return ret;
}

/**
 * @brief Flush an open file to its device.
 *
 * @param fd the descriptor; must be open
 * @return 0 on success; -E_INVAL if fd is not open; or the error of vop_fsync()
 */
int file_fsync(int fd) {
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    fd_array_acquire(file);
    ret = vop_fsync(file->node);
    fd_array_release(file);
    return ret;
}

/**
 * @brief Read the next entry of an open directory.
 *
 * The name is read into direntp->name starting at direntp->offset. On success offset advances by
 * the number of bytes used, so calling again gives the next entry.
 *
 * @param fd      the descriptor of an open directory
 * @param direntp [in,out] offset on input, the position to read from; name and offset on output
 * @return 0 on success; -E_INVAL if fd is not open; -E_NOENT when there are no more entries; or the
 *         error of vop_getdirentry()
 */
int file_getdirentry(int fd, struct dirent *direntp) {
    int ret;
    struct file *file;
    if ((ret = fd2file(fd, &file)) != 0) {
        return ret;
    }
    fd_array_acquire(file);

    struct iobuf __iob,
        *iob = iobuf_init(&__iob, direntp->name, sizeof(direntp->name), direntp->offset);
    if ((ret = vop_getdirentry(file->node, iob)) == 0) {
        direntp->offset += iobuf_used(iob);
    }
    fd_array_release(file);
    return ret;
}

/**
 * @brief Make a new descriptor refer to the same open file (dup2() in the file layer).
 *
 * The new descriptor is fd2 if fd2 is a valid number, or the lowest free one if fd2 is NO_FD.
 * The new entry gets the same position and access mode, and from then on the two descriptors have
 * independent positions. A descriptor that is already open is never replaced.
 *
 * @param fd1 an open descriptor to duplicate
 * @param fd2 the wanted descriptor, or NO_FD
 * @return the new descriptor on success; -E_INVAL if fd1 is not open or fd2 is out of range;
 *         -E_BUSY if fd2 is open; -E_MAX_OPEN if NO_FD was given and none is free
 */
int file_dup(int fd1, int fd2) {
    int ret;
    struct file *file1, *file2;
    if ((ret = fd2file(fd1, &file1)) != 0) {
        return ret;
    }
    if ((ret = fd_array_alloc(fd2, &file2)) != 0) {
        return ret;
    }
    fd_array_dup(file2, file1);
    return file2->fd;
}
