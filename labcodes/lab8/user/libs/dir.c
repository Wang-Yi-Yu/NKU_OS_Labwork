/**
 * @file   dir.c
 * @brief  User-level directory access: opendir(), readdir(), closedir() and getcwd() (lab8).
 *
 * The functions are thin wrappers around open(), fstat(), close() and the system calls
 * sys_getdirentry() and sys_getcwd(). The DIR object is one global variable, so the library can
 * read one directory at a time. The kernel side is sysfile_getdirentry() in kern/fs/sysfile.c,
 * which calls file_getdirentry() and vop_getdirentry(); for SFS directories the entries are read
 * by sfs_getdirentry() in kern/fs/sfs/sfs_inode.c.
 */
#include <defs.h>
#include <string.h>
#include <syscall.h>
#include <stat.h>
#include <dirent.h>
#include <file.h>
#include <dir.h>
#include <error.h>
#include <unistd.h>

/** @brief The only DIR object of the library (dir) and a pointer to it (dirp), set by opendir(). */
DIR dir, *dirp = &dir;

/**
 * @brief Open a directory for reading its entries.
 *
 * Opens @p path, checks with fstat() that the file is a directory and sets the offset of the
 * entry record to 0, so the first readdir() returns the first entry.
 * @param[in] path  path of the directory to open
 * @return the address of the global DIR object, or NULL if the path cannot be opened or is not
 *         a directory
 * @note The object is global and shared: a second opendir() overwrites the first one.
 * @note On failure the descriptor is not closed when open() succeeded but fstat() failed or the
 *       file is not a directory, so the descriptor stays open in the process.
 */
DIR *opendir(const char *path) {
    if ((dirp->fd = open(path, O_RDONLY)) < 0) {
        goto failed;
    }
    struct stat __stat, *stat = &__stat;
    if (fstat(dirp->fd, stat) != 0 || !S_ISDIR(stat->st_mode)) {
        goto failed;
    }
    dirp->dirent.offset = 0;
    return dirp;

failed:
    return NULL;
}

/**
 * @brief Read the next entry of an open directory.
 *
 * sys_getdirentry() takes dirp->dirent.offset as the position to read and, on success, copies
 * the entry name into dirp->dirent.name and advances the offset past the entry.
 * @param[in,out] dirp  directory returned by opendir()
 * @return a pointer to dirp->dirent on success, NULL at the end of the directory or on error
 */
struct dirent *readdir(DIR *dirp) {
    if (sys_getdirentry(dirp->fd, &(dirp->dirent)) == 0) {
        return &(dirp->dirent);
    }
    return NULL;
}

/**
 * @brief Close a directory opened by opendir().
 *
 * Only the descriptor is closed; the global DIR object keeps its values until the next opendir().
 * @param[in] dirp  directory to close
 */
void closedir(DIR *dirp) {
    close(dirp->fd);
}

/**
 * @brief Copy the path of the current working directory into a user buffer.
 * @param[out] buffer  user buffer that receives the path, NUL-terminated
 * @param[in]  len     size of @p buffer in bytes
 * @return 0 on success, a negative error code otherwise (-E_INVAL for len == 0 or a bad buffer)
 */
int getcwd(char *buffer, size_t len) {
    return sys_getcwd(buffer, len);
}
