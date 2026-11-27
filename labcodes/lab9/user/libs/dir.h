/**
 * @file   dir.h
 * @brief  User-level directory access: opendir(), readdir(), closedir() and getcwd() (lab8).
 *
 * A directory is an open file descriptor whose inode is a directory. opendir() opens it with
 * open() and checks the file type with fstat(); readdir() asks the kernel for the next entry
 * through sys_getdirentry(). The functions are wrappers of the system calls in user/libs/syscall.c
 * and must be linked into a user program together with user/libs/file.c. chdir() is declared
 * here but not implemented in lab8.
 */

#ifndef __USER_LIBS_DIR_H__
#define __USER_LIBS_DIR_H__

#include <defs.h>
#include <dirent.h>

/**
 * @brief An open directory, as returned by opendir().
 * @var DIR::fd          file descriptor of the directory, as returned by open()
 * @var DIR::dirent      the entry filled in by the last readdir(); its offset is the position
 *                       where the next readdir() continues
 */
typedef struct {
    int fd;
    struct dirent dirent;
} DIR;

/**
 * @brief Open a directory for reading its entries.
 *
 * Opens @p path with O_RDONLY, checks with fstat() that it is a directory and resets the
 * offset of the entry record to 0.
 * @param[in] path  path of the directory to open
 * @return a pointer to the directory object, or NULL if @p path cannot be opened or is not a
 *         directory
 * @note The object is one global DIR shared by all calls: a second opendir() overwrites the
 *       first one, and the pointer returned each time is the same. Only one directory can be
 *       read at a time.
 * @note If @p path is a file, the descriptor is not closed before NULL is returned.
 */
DIR *opendir(const char *path);
/**
 * @brief Read the next entry of an open directory.
 * @param[in,out] dirp  directory returned by opendir(); its dirent is updated on success
 * @return a pointer to dirp->dirent holding the name of the next entry, or NULL when there are
 *         no more entries or the read fails
 */
struct dirent *readdir(DIR *dirp);
/**
 * @brief Close a directory opened by opendir().
 * @param[in] dirp  directory to close; its descriptor is closed with close()
 */
void closedir(DIR *dirp);
/**
 * @brief Change the current working directory.
 * @note Declared only: no definition exists in user/libs in lab8, so a call fails at link time.
 * @param[in] path  path of the new working directory
 * @return 0 on success, a negative error code otherwise
 */
int chdir(const char *path);
/**
 * @brief Copy the path of the current working directory into a user buffer.
 * @param[out] buffer  user buffer that receives the NUL-terminated path
 * @param[in]  len     size of @p buffer in bytes; 0 is rejected
 * @return 0 on success, -E_INVAL if @p len is 0 or @p buffer is not valid user memory, another
 *         negative error code otherwise
 */
int getcwd(char *buffer, size_t len);

#endif /* !__USER_LIBS_DIR_H__ */
