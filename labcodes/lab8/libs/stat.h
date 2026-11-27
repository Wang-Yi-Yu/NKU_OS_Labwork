/**
 * @file   stat.h
 * @brief  File status record and file type bits, shared by the kernel and user programs (lab8).
 *
 * fstat() fills a struct stat. The call chain is sys_fstat() -> sysfile_fstat() -> file_fstat() ->
 * vop_fstat(), and the file system fills the record: sfs_inode.c for SFS files and dev.c for
 * devices. The S_IF* values are the ones of this teaching kernel, not the POSIX ones, so compare
 * them only with the S_IS* macros below.
 */

#ifndef __LIBS_STAT_H__
#define __LIBS_STAT_H__

#include <defs.h>

/**
 * @brief Status of an open file, as returned by fstat().
 * @var stat::st_mode    file type, one of the S_IF* values in the S_IFMT field
 * @var stat::st_nlinks  number of directory entries that refer to the file
 * @var stat::st_blocks  number of blocks the file occupies: SFS_BLKSIZE-byte blocks for an SFS
 *                        file, the device's own blocks for a device
 * @var stat::st_size    size of the file in bytes
 */
struct stat {
    uint32_t st_mode;
    size_t st_nlinks;
    size_t st_blocks;
    size_t st_size;
};

/** @brief Mask that selects the file type bits of st_mode. */
#define S_IFMT 070000
/** @brief Ordinary regular file. */
#define S_IFREG 010000
/** @brief Directory. */
#define S_IFDIR 020000
/** @brief Symbolic link. */
#define S_IFLNK 030000
/** @brief Character device, such as stdin or stdout. */
#define S_IFCHR 040000
/** @brief Block device, such as the disk device disk0. */
#define S_IFBLK 050000

/**
 * @brief Test whether @p mode is a regular file.
 * @param mode the st_mode value to test
 * @return non-zero if the file type of @p mode is S_IFREG
 */
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
/**
 * @brief Test whether @p mode is a directory.
 * @param mode the st_mode value to test
 * @return non-zero if the file type of @p mode is S_IFDIR
 */
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
/**
 * @brief Test whether @p mode is a symbolic link.
 * @param mode the st_mode value to test
 * @return non-zero if the file type of @p mode is S_IFLNK
 */
#define S_ISLNK(mode) (((mode) & S_IFMT) == S_IFLNK)
/**
 * @brief Test whether @p mode is a character device.
 * @param mode the st_mode value to test
 * @return non-zero if the file type of @p mode is S_IFCHR
 */
#define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
/**
 * @brief Test whether @p mode is a block device.
 * @param mode the st_mode value to test
 * @return non-zero if the file type of @p mode is S_IFBLK
 */
#define S_ISBLK(mode) (((mode) & S_IFMT) == S_IFBLK)

#endif /* !__LIBS_STAT_H__ */
