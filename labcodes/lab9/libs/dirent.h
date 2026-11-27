/**
 * @file   dirent.h
 * @brief  Directory entry record shared by the kernel and user programs (lab8).
 *
 * readdir() in user/libs/dir.c fills one struct dirent per call. The kernel fills the name and
 * advances the offset through sys_getdirentry() (kern/fs/sysfile.c, file_getdirentry()); the
 * offset is the position of the entry inside the directory, so the same record remembers where
 * the next call has to continue.
 */

#ifndef __LIBS_DIRENT_H__
#define __LIBS_DIRENT_H__

#include <defs.h>
#include <unistd.h>

/**
 * @brief One entry of a directory, as returned by readdir().
 * @var dirent::offset  byte offset of the entry inside the directory. Set to 0 by opendir(); the
 *                      kernel adds the size of the entry it has returned, so the next readdir()
 *                      continues with the following entry
 * @var dirent::name    NUL-terminated file name of the entry, at most FS_MAX_FNAME_LEN characters
 */
struct dirent {
    off_t offset;
    char name[FS_MAX_FNAME_LEN + 1];
};

#endif /* !__LIBS_DIRENT_H__ */
