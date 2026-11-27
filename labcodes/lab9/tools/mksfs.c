/**
 * @file   mksfs.c
 * @brief  Host tool that writes a Simple File System (SFS) image from a directory tree (lab8).
 *
 * The Makefile creates a zero-filled image with dd and then runs "mksfs <image.img> <dir>". The
 * tool runs on the host, not in the kernel, and fills the image with the SFS layout that
 * kern/fs/sfs/sfs.h reads back: the superblock in block 0, the root directory inode in block 1, the
 * free map in the blocks from SFS_BLKN_FREEMAP on, and then the inodes, directory entries, file
 * data and symbolic link targets of every entry under <dir>.
 *
 * Blocks are handed out in increasing order by sfs_alloc_ino(). Data blocks are written as they
 * are read from the host file; the cached inodes are written by close_sfs(), which also writes the
 * free map (a set bit means a free block, as in kern/fs/sfs/bitmap.c) and the superblock. A
 * directory is walked with readdir(). Names that start with '.' are skipped, hard links share one
 * SFS inode (the host st_ino is the key), symbolic links keep their target as data, and FIFOs,
 * sockets and devices are reported with warn() and not copied.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>

/** @brief Boolean type of the host tool (plain int, as in C89). */
typedef int bool;

/**
 * @brief Print a diagnostic on stderr with the function name, the line and the errno message.
 *
 * The message starts with the name of the level (@p msg), the function and the line where the
 * diagnostic was raised, then the errno text if errno is set. errno is cleared afterwards. The
 * process exits with status -1 when @p quit is non-zero.
 * @param msg  level name, stringified: "warn" or "bug"
 * @param quit non-zero to exit after printing
 * @param ...  format string and arguments, as for fprintf()
 */
#define __error(msg, quit, ...)                                                         \
    do {                                                                                \
        fprintf(stderr, #msg ": function %s - line %d: ", __FUNCTION__, __LINE__);      \
        if (errno != 0) {                                                               \
            fprintf(stderr, "[error] %s: ", strerror(errno));                           \
        }                                                                               \
        fprintf(stderr, "\n\t"), fprintf(stderr, __VA_ARGS__);                          \
        errno = 0;                                                                      \
        if (quit) {                                                                     \
            exit(-1);                                                                   \
        }                                                                               \
    } while (0)

/**
 * @brief Print a warning and continue.
 * @param ... format string and arguments
 */
#define warn(...)           __error(warn, 0, __VA_ARGS__)
/**
 * @brief Print an error and exit with status -1.
 * @param ... format string and arguments
 */
#define bug(...)            __error(bug, 1, __VA_ARGS__)

/**
 * @brief Compile-time assertion: fails to compile unless @p x is a non-zero integer constant.
 *
 * The switch has the labels "case 0" and "case (x)": a zero @p x makes the labels clash, and a
 * value that is not a constant expression is rejected by the compiler.
 * @param x integer constant expression that must be non-zero
 */
#define static_assert(x)                                                                \
    switch (x) {case 0: case (x): ; }

/**
 * @brief Multiplier of the hash function, 2^31 + 2^29 - 2^25 + 2^22 - 2^19 - 2^16 + 1 (0x9e370001).
 */
#define GOLDEN_RATIO_PRIME_32       0x9e370001UL

/** @brief Number of bits of a bucket index: the caches have 2^HASH_SHIFT buckets. */
#define HASH_SHIFT                              10
/** @brief Number of buckets of the inode cache and of the block cache (1024). */
#define HASH_LIST_SIZE                          (1 << HASH_SHIFT)

/**
 * @brief Multiplicative hash: the top @p bits bits of val * GOLDEN_RATIO_PRIME_32.
 * @param val  value to hash
 * @param bits number of bits of the result
 * @return a value in [0, 2^bits)
 */
static inline uint32_t
__hash32(uint32_t val, unsigned int bits) {
    uint32_t hash = val * GOLDEN_RATIO_PRIME_32;
    return (hash >> (32 - bits));
}

/**
 * @brief Hash a block number to a bucket of the block cache.
 * @param val block number
 * @return bucket index in [0, HASH_LIST_SIZE)
 */
static uint32_t
hash32(uint32_t val) {
    return __hash32(val, HASH_SHIFT);
}

/**
 * @brief Hash a host inode number to a bucket of the inode cache.
 *
 * Only the low 32 bits of @p val are used.
 * @param val host inode number (st_ino)
 * @return bucket index in [0, HASH_LIST_SIZE)
 */
static uint32_t
hash64(uint64_t val) {
    return __hash32((uint32_t)val, HASH_SHIFT);
}

/**
 * @brief malloc() that stops the tool on failure.
 * @param size number of bytes
 * @return the new block; never NULL (failure calls bug())
 */
void *
safe_malloc(size_t size) {
    void *ret;
    if ((ret = malloc(size)) == NULL) {
        bug("malloc %lu bytes failed.\n", (long unsigned)size);
    }
    return ret;
}

/**
 * @brief strdup() that stops the tool on failure.
 * @param str string to copy
 * @return a new copy of @p str, to be released with free()
 */
char *
safe_strdup(const char *str) {
    char *ret;
    if ((ret = strdup(str)) == NULL) {
        bug("strdup failed: %s\n", str);
    }
    return ret;
}

/**
 * @brief stat() that stops the tool on failure.
 * @param filename path to examine (symbolic links are followed)
 * @return pointer to a static struct stat; the next call overwrites it
 */
struct stat *
safe_stat(const char *filename) {
    static struct stat __stat;
    if (stat(filename, &__stat) != 0) {
        bug("stat %s failed.\n", filename);
    }
    return &__stat;
}

/**
 * @brief fstat() that stops the tool on failure.
 * @param fd open file descriptor
 * @return pointer to a static struct stat; the next call overwrites it
 */
struct stat *
safe_fstat(int fd) {
    static struct stat __stat;
    if (fstat(fd, &__stat) != 0) {
        bug("fstat %d failed.\n", fd);
    }
    return &__stat;
}

/**
 * @brief lstat() that stops the tool on failure. Unlike safe_stat(), it does not follow links.
 * @param name path to examine
 * @return pointer to a static struct stat; the next call overwrites it
 */
struct stat *
safe_lstat(const char *name) {
    static struct stat __stat;
    if (lstat(name, &__stat) != 0) {
        bug("lstat '%s' failed.\n", name);
    }
    return &__stat;
}

/**
 * @brief fchdir() that stops the tool on failure.
 *
 * Used to walk the host tree: the current host directory is changed to @p fd.
 * @param fd descriptor of the directory to enter
 */
void
safe_fchdir(int fd) {
    if (fchdir(fd) != 0) {
        bug("fchdir failed %d.\n", fd);
    }
}

/** @brief Magic number of the superblock, the same as in kern/fs/sfs/sfs.h. */
#define SFS_MAGIC                               0x2f8dbe2a
/** @brief Number of direct block pointers in an inode. */
#define SFS_NDIRECT                             12
/** @brief Size of a block in bytes (4 KB). */
#define SFS_BLKSIZE                             4096
/** @brief Largest number of blocks an image may have (512K blocks, 2 GB). */
#define SFS_MAX_NBLKS                           (1024UL * 512)
/** @brief Maximum length of the file system label, without the terminating NUL. */
#define SFS_MAX_INFO_LEN                        31
/** @brief Maximum length of a file name, without the terminating NUL. */
#define SFS_MAX_FNAME_LEN                       255
/**
 * @brief Largest file size in bytes (128 MB).
 *
 * @note The kernel reads only the direct and the single indirect blocks of an inode (about 4 MB),
 *       so a file bigger than that is written here but cannot be read back by the kernel.
 */
#define SFS_MAX_FILE_SIZE                       (1024UL * 1024 * 128)

/** @brief Number of bits in one block: one free-map block covers this many blocks. */
#define SFS_BLKBITS                             (SFS_BLKSIZE * CHAR_BIT)
/** @brief Inode type of a regular file. */
#define SFS_TYPE_FILE                           1
/** @brief Inode type of a directory. */
#define SFS_TYPE_DIR                            2
/** @brief Inode type of a symbolic link; its data is the link target. */
#define SFS_TYPE_LINK                           3

/** @brief Block number of the superblock. */
#define SFS_BLKN_SUPER                          0
/** @brief Block number of the inode of the root directory. */
#define SFS_BLKN_ROOT                           1
/** @brief Block number of the first free-map block. */
#define SFS_BLKN_FREEMAP                        2

/**
 * @brief A block of the image that is being built, kept in memory until close_sfs() writes it.
 * @var cache_block::ino        block number in the image
 * @var cache_block::hash_next  next entry in the same bucket of the block cache
 * @var cache_block::cache      SFS_BLKSIZE bytes of the block, zero-filled when allocated
 */
struct cache_block {
    uint32_t ino;
    struct cache_block *hash_next;
    void *cache;
};

/**
 * @brief An inode of a file or directory that is being built, kept in memory until close_sfs().
 * @var cache_inode::inode     the disk inode (see the nested struct inode)
 * @var cache_inode::real      host inode number (st_ino) of the file; the key of the inode cache,
 *                             so that hard links find the same cache_inode
 * @var cache_inode::ino       block number of the inode in the image
 * @var cache_inode::nblks     number of logical data blocks appended so far
 * @var cache_inode::l1        cached single indirect block, or NULL
 * @var cache_inode::l2        cached double indirect block, or NULL
 * @var cache_inode::hash_next next entry in the same bucket of the inode cache
 */
struct cache_inode {
    /**
     * @brief The disk inode as it is written to its block. It matches struct sfs_disk_inode of
     *        kern/fs/sfs/sfs.h, except for the extra db_indirect field at the end, which the kernel
     *        does not have.
     * @var inode::size        size of the file in bytes
     * @var inode::type        SFS_TYPE_FILE, SFS_TYPE_DIR or SFS_TYPE_LINK
     * @var inode::nlinks      number of directory entries that name the inode
     * @var inode::blocks      number of logical data blocks
     * @var inode::direct      block numbers of logical blocks 0 .. SFS_NDIRECT-1
     * @var inode::indirect    block number of the single indirect block, 0 if none
     * @var inode::db_indirect block number of the double indirect block, 0 if none; see the
     *                         note of SFS_MAX_FILE_SIZE, the kernel does not read it
     */
    struct inode {
        uint32_t size;
        uint16_t type;
        uint16_t nlinks;
        uint32_t blocks;
        uint32_t direct[SFS_NDIRECT];
        uint32_t indirect;
        uint32_t db_indirect;
    } inode;
    ino_t real;
    uint32_t ino;
    uint32_t nblks;
    struct cache_block *l1, *l2;
    struct cache_inode *hash_next;
};

/**
 * @brief The image being built: the superblock copy, the path of the directory being walked and
 *        the caches of inodes and blocks.
 * @var sfs_fs::super.magic         SFS_MAGIC
 * @var sfs_fs::super.blocks        number of blocks of the image
 * @var sfs_fs::super.unused_blocks number of blocks that are still free
 * @var sfs_fs::super.info          label of the file system, "simple file system"
 * @var sfs_fs::__sp_nil            sentinel element of the path list (subpath)
 * @var sfs_fs::sp_root             first element of the path list, that is &__sp_nil
 * @var sfs_fs::sp_end              last element of the path list (the current directory name)
 * @var sfs_fs::imgfd              file descriptor of the image, opened for writing
 * @var sfs_fs::ninos              number of blocks of the image
 * @var sfs_fs::next_ino           next block number that sfs_alloc_ino() hands out
 * @var sfs_fs::root               cache_inode of the root directory (block SFS_BLKN_ROOT)
 * @var sfs_fs::inodes             buckets of the inode cache, keyed by the host inode number
 * @var sfs_fs::blocks             buckets of the block cache, keyed by the block number
 */
struct sfs_fs {
    struct {
        uint32_t magic;
        uint32_t blocks;
        uint32_t unused_blocks;
        char info[SFS_MAX_INFO_LEN + 1];
    } super;
    /**
     * @brief One element of the path list, used only to print the current host path in messages.
     * @var subpath::next    next element, towards the current directory
     * @var subpath::prev    previous element, towards the root
     * @var subpath::subname name of the directory, a copy owned by the element
     */
    struct subpath {
        struct subpath *next, *prev;
        char *subname;
    } __sp_nil, *sp_root, *sp_end;
    int imgfd;
    uint32_t ninos, next_ino;
    struct cache_inode *root;
    struct cache_inode *inodes[HASH_LIST_SIZE];
    struct cache_block *blocks[HASH_LIST_SIZE];
};

/**
 * @brief A directory entry as it is written to its block. It must match struct sfs_disk_entry of
 *        kern/fs/sfs/sfs.h.
 * @var sfs_entry::ino  block number of the inode of the named file
 * @var sfs_entry::name NUL-terminated name of the entry
 * @note add_entry() writes only the first sizeof(name) bytes of the entry, see that function.
 */
struct sfs_entry {
    uint32_t ino;
    char name[SFS_MAX_FNAME_LEN + 1];
};

/**
 * @brief Hand out the next free block of the image.
 *
 * Blocks are taken in increasing order, starting at next_ino, and super.unused_blocks is
 * decremented.
 * The tool stops with "out of disk space" when no block is left.
 * @param sfs the image being built
 * @return the block number
 */
static uint32_t
sfs_alloc_ino(struct sfs_fs *sfs) {
    if (sfs->next_ino < sfs->ninos) {
        sfs->super.unused_blocks --;
        return sfs->next_ino ++;
    }
    bug("out of disk space.\n");
}

/**
 * @brief Create a zero-filled cache entry for a block and put it into the block cache.
 * @param sfs the image being built
 * @param ino block number to use, or 0 to take the next free block
 * @return the new cache entry, whose ino is the block number used
 * @note The entry is put in the bucket of the argument @p ino, not of cb->ino. When @p ino is 0
 *       the bucket is hash32(0), so search_cache_block() would not find the entry by its real
 *       number. The sequential code paths here never search for such entries (see update_cache()).
 */
static struct cache_block *
alloc_cache_block(struct sfs_fs *sfs, uint32_t ino) {
    struct cache_block *cb = safe_malloc(sizeof(struct cache_block));
    cb->ino = (ino != 0) ? ino : sfs_alloc_ino(sfs);
    cb->cache = memset(safe_malloc(SFS_BLKSIZE), 0, SFS_BLKSIZE);
    struct cache_block **head = sfs->blocks + hash32(ino);
    cb->hash_next = *head, *head = cb;
    return cb;
}

/**
 * @brief Find the cache entry of a block number.
 * @param sfs the image being built
 * @param ino block number
 * @return the cache entry, or NULL if the block is not cached
 */
struct cache_block *
search_cache_block(struct sfs_fs *sfs, uint32_t ino) {
    struct cache_block *cb = sfs->blocks[hash32(ino)];
    while (cb != NULL && cb->ino != ino) {
        cb = cb->hash_next;
    }
    return cb;
}

/**
 * @brief Create the in-memory inode of a host file or directory and put it into the inode cache.
 * @param sfs  the image being built
 * @param real host inode number (st_ino) that identifies the file
 * @param ino  block number of the inode, or 0 to take the next free block
 * @param type SFS_TYPE_FILE, SFS_TYPE_DIR or SFS_TYPE_LINK
 * @return the new cache_inode; the disk inode starts zeroed except for its type
 */
static struct cache_inode *
alloc_cache_inode(struct sfs_fs *sfs, ino_t real, uint32_t ino, uint16_t type) {
    struct cache_inode *ci = safe_malloc(sizeof(struct cache_inode));
    ci->ino = (ino != 0) ? ino : sfs_alloc_ino(sfs);
    ci->real = real, ci->nblks = 0, ci->l1 = ci->l2 = NULL;
    struct inode *inode = &(ci->inode);
    memset(inode, 0, sizeof(struct inode));
    inode->type = type;
    struct cache_inode **head = sfs->inodes + hash64(real);
    ci->hash_next = *head, *head = ci;
    return ci;
}

/**
 * @brief Find the cached inode of a host inode number, so that hard links share one inode.
 * @param sfs  the image being built
 * @param real host inode number (st_ino)
 * @return the cache_inode, or NULL if the host file has not been added yet
 */
struct cache_inode *
search_cache_inode(struct sfs_fs *sfs, ino_t real) {
    struct cache_inode *ci = sfs->inodes[hash64(real)];
    while (ci != NULL && ci->real != real) {
        ci = ci->hash_next;
    }
    return ci;
}

/**
 * @brief Set up the in-memory image for the file descriptor of an existing image file.
 *
 * The number of blocks is the image size divided by SFS_BLKSIZE (capped at SFS_MAX_NBLKS with a
 * warning). The first free block follows the superblock, the root inode and the free-map blocks.
 * The root directory is created as the cache inode of block SFS_BLKN_ROOT.
 * @param imgfd file descriptor of the image, opened for writing
 * @return the new image; the tool stops if the image is too small for its free map
 */
struct sfs_fs *
create_sfs(int imgfd) {
    uint32_t ninos, next_ino;
    struct stat *stat = safe_fstat(imgfd);
    if ((ninos = stat->st_size / SFS_BLKSIZE) > SFS_MAX_NBLKS) {
        ninos = SFS_MAX_NBLKS;
        warn("img file is too big (%llu bytes, only use %u blocks).\n",
                (unsigned long long)stat->st_size, ninos);
    }
    if ((next_ino = SFS_BLKN_FREEMAP + (ninos + SFS_BLKBITS - 1) / SFS_BLKBITS) >= ninos) {
        bug("img file is too small (%llu bytes, %u blocks, bitmap use at least %u blocks).\n",
                (unsigned long long)stat->st_size, ninos, next_ino - 2);
    }

    struct sfs_fs *sfs = safe_malloc(sizeof(struct sfs_fs));
    sfs->super.magic = SFS_MAGIC;
    sfs->super.blocks = ninos, sfs->super.unused_blocks = ninos - next_ino;
    snprintf(sfs->super.info, SFS_MAX_INFO_LEN, "simple file system");

    sfs->ninos = ninos, sfs->next_ino = next_ino, sfs->imgfd = imgfd;
    sfs->sp_root = sfs->sp_end = &(sfs->__sp_nil);
    sfs->sp_end->prev = sfs->sp_end->next = NULL;

    int i;
    for (i = 0; i < HASH_LIST_SIZE; i ++) {
        sfs->inodes[i] = NULL;
        sfs->blocks[i] = NULL;
    }

    sfs->root = alloc_cache_inode(sfs, 0, SFS_BLKN_ROOT, SFS_TYPE_DIR);
    return sfs;
}

/**
 * @brief Enter a sub-directory: append its name to the path list (for messages).
 * @param sfs     the image being built
 * @param subname name of the directory; it is copied
 */
static void
subpath_push(struct sfs_fs *sfs, const char *subname) {
    struct subpath *subpath = safe_malloc(sizeof(struct subpath));
    subpath->subname = safe_strdup(subname);
    sfs->sp_end->next = subpath;
    subpath->prev = sfs->sp_end;
    subpath->next = NULL;
    sfs->sp_end = subpath;
}

/**
 * @brief Leave a sub-directory: remove the last name of the path list and free it.
 * @param sfs the image being built; the path list must not be empty
 */
static void
subpath_pop(struct sfs_fs *sfs) {
    assert(sfs->sp_root != sfs->sp_end);
    struct subpath *subpath = sfs->sp_end;
    sfs->sp_end = sfs->sp_end->prev, sfs->sp_end->next = NULL;
    free(subpath->subname), free(subpath);
}

/**
 * @brief Print the current host path, followed by @p name, as "current is: /a/b/name".
 * @param fout destination stream, for example stderr
 * @param[in]  sfs  the image being built
 * @param[in]  name optional name appended to the path, or NULL
 */
static void
subpath_show(FILE *fout, struct sfs_fs *sfs, const char *name) {
    struct subpath *subpath = sfs->sp_root;
    fprintf(fout, "current is: /");
    while ((subpath = subpath->next) != NULL) {
        fprintf(fout, "%s/", subpath->subname);
    }
    if (name != NULL) {
        fprintf(fout, "%s", name);
    }
    fprintf(fout, "\n");
}

/**
 * @brief Write one block of the image.
 * @param sfs  the image being built
 * @param data bytes to write
 * @param len  number of bytes in @p data, at most SFS_BLKSIZE; a shorter block is zero-padded
 * @param ino  block number, below ninos
 */
static void
write_block(struct sfs_fs *sfs, void *data, size_t len, uint32_t ino) {
    assert(len <= SFS_BLKSIZE && ino < sfs->ninos);
    static char buffer[SFS_BLKSIZE];
    if (len != SFS_BLKSIZE) {
        memset(buffer, 0, sizeof(buffer));
        data = memcpy(buffer, data, len);
    }
    off_t offset = (off_t)ino * SFS_BLKSIZE;
    ssize_t ret;
    if ((ret = pwrite(sfs->imgfd, data, SFS_BLKSIZE, offset)) != SFS_BLKSIZE) {
        bug("write %u block failed: (%d/%d).\n", ino, (int)ret, SFS_BLKSIZE);
    }
}

/**
 * @brief Write a cached data or indirect block to its place in the image.
 * @param sfs the image being built
 * @param cb  cache entry to write
 */
static void
flush_cache_block(struct sfs_fs *sfs, struct cache_block *cb) {
    write_block(sfs, cb->cache, SFS_BLKSIZE, cb->ino);
}

/**
 * @brief Write a cached inode to its block. Only the disk inode (sizeof(struct inode) bytes) is
 *        written, the rest of the block is zero.
 * @param sfs the image being built
 * @param ci  cache entry to write
 */
static void
flush_cache_inode(struct sfs_fs *sfs, struct cache_inode *ci) {
    write_block(sfs, &(ci->inode), sizeof(ci->inode), ci->ino);
}

/**
 * @brief Finish the image: write the free map, the superblock and every cached block and inode.
 *
 * Bit i of the free map is set when block i is free, that is when i is at least next_ino and below
 * ninos. The blocks of the free map and the superblock are written first, then the cache.
 * @param sfs the image being built; it is not used after this call
 */
void
close_sfs(struct sfs_fs *sfs) {
    static char buffer[SFS_BLKSIZE];
    uint32_t i, j, ino = SFS_BLKN_FREEMAP;
    uint32_t ninos = sfs->ninos, next_ino = sfs->next_ino;
    for (i = 0; i < ninos; ino ++, i += SFS_BLKBITS) {
        memset(buffer, 0, sizeof(buffer));
        if (i + SFS_BLKBITS > next_ino) {
            uint32_t start = 0, end = SFS_BLKBITS;
            if (i < next_ino) {
                start = next_ino - i;
            }
            if (i + SFS_BLKBITS > ninos) {
                end = ninos - i;
            }
            uint32_t *data = (uint32_t *)buffer;
            const uint32_t bits = sizeof(bits) * CHAR_BIT;
            for (j = start; j < end; j ++) {
                data[j / bits] |= (1 << (j % bits));
            }
        }
        write_block(sfs, buffer, sizeof(buffer), ino);
    }
    write_block(sfs, &(sfs->super), sizeof(sfs->super), SFS_BLKN_SUPER);

    for (i = 0; i < HASH_LIST_SIZE; i ++) {
        struct cache_block *cb = sfs->blocks[i];
        while (cb != NULL) {
            flush_cache_block(sfs, cb);
            cb = cb->hash_next;
        }
        struct cache_inode *ci = sfs->inodes[i];
        while (ci != NULL) {
            flush_cache_inode(sfs, ci);
            ci = ci->hash_next;
        }
    }
}

/**
 * @brief Check the name of the image and open it for writing.
 *
 * The name must end with ".img" and the file must already exist.
 * @param imgname path of the image, as given on the command line
 * @return the image, as returned by create_sfs(); the tool stops on any error
 */
struct sfs_fs *
open_img(const char *imgname) {
    const char *expect = ".img", *ext = imgname + strlen(imgname) - strlen(expect);
    if (ext <= imgname || strcmp(ext, expect) != 0) {
        bug("invalid .img file name '%s'.\n", imgname);
    }
    int imgfd;
    if ((imgfd = open(imgname, O_WRONLY)) < 0) {
        bug("open '%s' failed.\n", imgname);
    }
    return create_sfs(imgfd);
}

/**
 * @brief Print the current host path, then the message, and stop the tool.
 * @param sfs  the image being built (for the path)
 * @param name file name to show after the path, or NULL
 * @param ...  format string and arguments of the message
 */
#define open_bug(sfs, name, ...)                                                        \
    do {                                                                                \
        subpath_show(stderr, sfs, name);                                                \
        bug(__VA_ARGS__);                                                               \
    } while (0)

/**
 * @brief Print the current host path on stderr, followed by @p name.
 * @param sfs  the image being built
 * @param name file name to show after the path, or NULL
 */
#define show_fullpath(sfs, name) subpath_show(stderr, sfs, name)

/**
 * @brief Forward declarations of open_dir(), open_file() and open_link(); see their definitions.
 */
void open_dir(struct sfs_fs *sfs, struct cache_inode *current, struct cache_inode *parent);
void open_file(struct sfs_fs *sfs, struct cache_inode *file, const char *filename, int fd);
void open_link(struct sfs_fs *sfs, struct cache_inode *file, const char *filename);

/** @brief Number of block numbers in one indirect block (4096 / 4 = 1024). */
#define SFS_BLK_NENTRY                          (SFS_BLKSIZE / sizeof(uint32_t))
/** @brief Logical blocks reached through the direct pointers (12). */
#define SFS_L0_NBLKS                            SFS_NDIRECT
/** @brief Logical blocks reached through the direct and the single indirect pointers. */
#define SFS_L1_NBLKS                            (SFS_BLK_NENTRY + SFS_L0_NBLKS)
/** @brief Logical blocks reached through the double indirect block as well. */
#define SFS_L2_NBLKS                            (SFS_BLK_NENTRY * SFS_BLK_NENTRY + SFS_L1_NBLKS)
/** @brief Largest number of logical blocks of a file: SFS_MAX_FILE_SIZE / SFS_BLKSIZE. */
#define SFS_LN_NBLKS                            (SFS_MAX_FILE_SIZE / SFS_BLKSIZE)

/**
 * @brief Make the cache entry of an indirect block current, allocating the block if needed.
 *
 * If *inop is 0 a new block is allocated and its number is stored in *inop. Otherwise the block
 * *inop is looked up in the cache unless *cbp already is that block.
 * @param sfs  the image being built
 * @param[in,out] cbp  the caller's current cache entry for this level (file->l1 or file->l2)
 * @param[in,out] inop the block number slot (in the inode or in the parent indirect block)
 */
static void
update_cache(struct sfs_fs *sfs, struct cache_block **cbp, uint32_t *inop) {
    uint32_t ino = *inop;
    struct cache_block *cb = *cbp;
    if (ino == 0) {
        cb = alloc_cache_block(sfs, 0);
        ino = cb->ino;
    }
    else if (cb == NULL || cb->ino != ino) {
        cb = search_cache_block(sfs, ino);
        assert(cb != NULL && cb->ino == ino);
    }
    *cbp = cb, *inop = ino;
}

/**
 * @brief Add block @p ino as the next logical data block of a file.
 *
 * The block goes into the direct pointers, the single indirect block or the double indirect
 * block, depending on the number of blocks the file already has. Then the file size grows by
 * @p size, and the block count and nblks are increased.
 * @param sfs      the image being built
 * @param file     inode of the file
 * @param size     number of valid bytes in the block (at most SFS_BLKSIZE)
 * @param ino      block number of the data block
 * @param filename name of the host file, shown in the error message
 */
static void
append_block(struct sfs_fs *sfs, struct cache_inode *file, size_t size, uint32_t ino, const char *filename) {
    static_assert(SFS_LN_NBLKS <= SFS_L2_NBLKS);
    assert(size <= SFS_BLKSIZE);
    uint32_t nblks = file->nblks;
    struct inode *inode = &(file->inode);
    if (nblks >= SFS_LN_NBLKS) {
        open_bug(sfs, filename, "file is too big.\n");
    }
    if (nblks < SFS_L0_NBLKS) {
        inode->direct[nblks] = ino;
    }
    else if (nblks < SFS_L1_NBLKS) {
        nblks -= SFS_L0_NBLKS;
        update_cache(sfs, &(file->l1), &(inode->indirect));
        uint32_t *data = file->l1->cache;
        data[nblks] = ino;
    }
    else if (nblks < SFS_L2_NBLKS) {
        nblks -= SFS_L1_NBLKS;
        update_cache(sfs, &(file->l2), &(inode->db_indirect));
        uint32_t *data2 = file->l2->cache;
        update_cache(sfs, &(file->l1), &data2[nblks / SFS_BLK_NENTRY]);
        uint32_t *data1 = file->l1->cache;
        data1[nblks % SFS_BLK_NENTRY] = ino;
    }
    file->nblks ++;
    inode->size += size;
    inode->blocks ++;
}

/**
 * @brief Add a directory entry that names @p file inside directory @p current.
 *
 * One block is allocated for the entry and appended to the directory, and the link count of
 * @p file is increased.
 * @param sfs     the image being built
 * @param current directory that receives the entry
 * @param file    inode that the entry names
 * @param name    name of the entry
 * @note sizeof(entry->name) (256 bytes) is written from the start of struct sfs_entry, so the
 *       4-byte ino and the first 252 bytes of the name are stored. A name of 252 characters or more
 *       loses its end in the image, while the directory still counts 256 bytes for the entry.
 */
static void
add_entry(struct sfs_fs *sfs, struct cache_inode *current, struct cache_inode *file, const char *name) {
    static struct sfs_entry __entry, *entry = &__entry;
    assert(current->inode.type == SFS_TYPE_DIR && strlen(name) <= SFS_MAX_FNAME_LEN);
    entry->ino = file->ino, strcpy(entry->name, name);
    uint32_t entry_ino = sfs_alloc_ino(sfs);
    write_block(sfs, entry, sizeof(entry->name), entry_ino);
    append_block(sfs, current, sizeof(entry->name), entry_ino, name);
    file->inode.nlinks ++;
}

/**
 * @brief Add a host sub-directory: build its inode and contents, then link it into @p parent.
 *
 * The tool enters the sub-directory with fchdir(@p fd) so that open_dir() can read it, and it
 * returns to @p curfd afterwards.
 * @param sfs     the image being built
 * @param parent  directory that receives the entry
 * @param dirname name of the sub-directory
 * @param curfd   descriptor of the host directory that is being read; the tool returns to it
 * @param fd      descriptor of the sub-directory
 * @param real    host inode number of the sub-directory; it must not be cached yet
 */
static void
add_dir(struct sfs_fs *sfs, struct cache_inode *parent, const char *dirname, int curfd, int fd, ino_t real) {
    assert(search_cache_inode(sfs, real) == NULL);
    struct cache_inode *current = alloc_cache_inode(sfs, real, 0, SFS_TYPE_DIR);
    safe_fchdir(fd), subpath_push(sfs, dirname);
    open_dir(sfs, current, parent);
    safe_fchdir(curfd), subpath_pop(sfs);
    add_entry(sfs, parent, current, dirname);
}

/**
 * @brief Add a host regular file to @p current.
 *
 * If the host inode is already cached the file is a hard link: only the entry is added. Otherwise
 * a new inode is created and the contents of the file are copied by open_file().
 * @param sfs      the image being built
 * @param current  directory that receives the entry
 * @param filename name of the entry
 * @param fd       descriptor of the host file
 * @param real     host inode number of the file
 */
static void
add_file(struct sfs_fs *sfs, struct cache_inode *current, const char *filename, int fd, ino_t real) {
    struct cache_inode *file;
    if ((file = search_cache_inode(sfs, real)) == NULL) {
        file = alloc_cache_inode(sfs, real, 0, SFS_TYPE_FILE);
        open_file(sfs, file, filename, fd);
    }
    add_entry(sfs, current, file, filename);
}

/**
 * @brief Add a host symbolic link to @p current.
 *
 * The link gets its own SFS inode of type SFS_TYPE_LINK, whose data is the target path (see
 * open_link()).
 * @param sfs      the image being built
 * @param current  directory that receives the entry
 * @param filename name of the link in the host directory
 * @param real     host inode number of the link itself
 */
static void
add_link(struct sfs_fs *sfs, struct cache_inode *current, const char *filename, ino_t real) {
    struct cache_inode *file = alloc_cache_inode(sfs, real, 0, SFS_TYPE_LINK);
    open_link(sfs, file, filename);
    add_entry(sfs, current, file, filename);
}

/**
 * @brief Fill an SFS directory from the host directory that is the current directory.
 *
 * "." and ".." are added first. Then every entry is handled: names that start with '.' and names
 * longer than SFS_MAX_FNAME_LEN stop the tool; symbolic links go to
 * add_link(); sub-directories to add_dir(); regular files to add_file(); any other type gives a
 * warning and is skipped. Each entry other than a symbolic link is opened before its type is
 * known, which is why a host FIFO in the tree would block the tool.
 * @param sfs     the image being built
 * @param current the SFS directory to fill
 * @param parent  its parent directory, the target of ".."
 */
void
open_dir(struct sfs_fs *sfs, struct cache_inode *current, struct cache_inode *parent) {
    DIR *dir;
    if ((dir = opendir(".")) == NULL) {
        open_bug(sfs, NULL, "opendir failed.\n");
    }
    add_entry(sfs, current, current, ".");
    add_entry(sfs, current, parent, "..");
    struct dirent *direntp;
    while ((direntp = readdir(dir)) != NULL) {
        const char *name = direntp->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue ;
        }
        if (name[0] == '.') {
            continue ;
        }
        if (strlen(name) > SFS_MAX_FNAME_LEN) {
            open_bug(sfs, NULL, "file name is too long: %s\n", name);
        }
        struct stat *stat = safe_lstat(name);
        if (S_ISLNK(stat->st_mode)) {
            add_link(sfs, current, name, stat->st_ino);
        }
        else {
            int fd;
            if ((fd = open(name, O_RDONLY)) < 0) {
                open_bug(sfs, NULL, "open failed: %s\n", name);
            }
            if (S_ISDIR(stat->st_mode)) {
                add_dir(sfs, current, name, dirfd(dir), fd, stat->st_ino);
            }
            else if (S_ISREG(stat->st_mode)) {
                add_file(sfs, current, name, fd, stat->st_ino);
            }
            else {
                char mode = '?';
                if (S_ISFIFO(stat->st_mode)) mode = 'f';
                if (S_ISSOCK(stat->st_mode)) mode = 's';
                if (S_ISCHR(stat->st_mode)) mode = 'c';
                if (S_ISBLK(stat->st_mode)) mode = 'b';
                show_fullpath(sfs, NULL);
                warn("unsupported mode %07x (%c): file %s\n", stat->st_mode, mode, name);
            }
            close(fd);
        }
    }
    closedir(dir);
}

/**
 * @brief Copy the contents of a host file into new data blocks of an SFS inode.
 *
 * The file is read in SFS_BLKSIZE pieces. Each piece gets a new block (sfs_alloc_ino()), is
 * written to it, and is appended to @p file. Every piece except the last must be full.
 * @param sfs      the image being built
 * @param file     inode that receives the data
 * @param filename name of the host file, shown in error messages
 * @param fd       descriptor of the host file, read from its current position
 */
void
open_file(struct sfs_fs *sfs, struct cache_inode *file, const char *filename, int fd) {
    static char buffer[SFS_BLKSIZE];
    ssize_t ret, last = SFS_BLKSIZE;
    while ((ret = read(fd, buffer, sizeof(buffer))) != 0) {
        assert(last == SFS_BLKSIZE);
        uint32_t ino = sfs_alloc_ino(sfs);
        write_block(sfs, buffer, ret, ino);
        append_block(sfs, file, ret, ino, filename);
        last = ret;
    }
    if (ret < 0) {
        open_bug(sfs, filename, "read file failed.\n");
    }
}

/**
 * @brief Store the target of a host symbolic link as the data of an SFS inode.
 *
 * The target is put in one block without a terminating NUL. The target must be shorter than
 * SFS_BLKSIZE bytes; a longer one stops the tool.
 * @param sfs      the image being built
 * @param file     inode of type SFS_TYPE_LINK that receives the target
 * @param filename path of the link in the host directory
 */
void
open_link(struct sfs_fs *sfs, struct cache_inode *file, const char *filename) {
    static char buffer[SFS_BLKSIZE];
    uint32_t ino = sfs_alloc_ino(sfs);
    ssize_t ret = readlink(filename, buffer, sizeof(buffer));
    if (ret < 0 || ret == SFS_BLKSIZE) {
        open_bug(sfs, filename, "read link failed, %d", (int)ret);
    }
    write_block(sfs, buffer, ret, ino);
    append_block(sfs, file, ret, ino, filename);
}

/**
 * @brief Write the complete image: the tree under @p home, then close_sfs().
 *
 * The root directory is filled from @p home, which is opened with O_NOFOLLOW (it must not be a
 * symbolic link). The root is its own parent, so ".." of the root refers to the root. The
 * current host directory is restored before the image is closed.
 * @param sfs  the image being built
 * @param home directory of the host that holds the files
 * @return 0
 */
int
create_img(struct sfs_fs *sfs, const char *home) {
    int curfd, homefd;
    if ((curfd = open(".", O_RDONLY)) < 0) {
        bug("get current fd failed.\n");
    }
    if ((homefd = open(home, O_RDONLY | O_NOFOLLOW)) < 0) {
        bug("open home directory '%s' failed.\n", home);
    }
    safe_fchdir(homefd);
    open_dir(sfs, sfs->root, sfs->root);
    safe_fchdir(curfd);
    close(curfd), close(homefd);
    close_sfs(sfs);
    return 0;
}

/**
 * @brief Compile-time check of the host types and of the size limits (uses static_assert()).
 *
 * off_t and ino_t must be 8 bytes, and the block and file size limits must not exceed 2^31.
 */
static void
static_check(void) {
    static_assert(sizeof(off_t) == 8);
    static_assert(sizeof(ino_t) == 8);
    static_assert(SFS_MAX_NBLKS <= 0x80000000UL);
    static_assert(SFS_MAX_FILE_SIZE <= 0x80000000UL);
}

/**
 * @brief Entry point: build the image named in argv[1] from the directory argv[2].
 *
 * Usage: mksfs <input *.img> <input dirname>. The image must already exist (the Makefile
 * zero-fills it first) and be large enough for the file system. A message
 * "create <image> (<dir>) successfully." is printed on success.
 * @param argc number of arguments; must be 3
 * @param argv argv[1] is the image file, argv[2] the directory to copy
 * @return 0 on success; the tool stops with status -1 on any error
 */
int
main(int argc, char **argv) {
    static_check();
    if (argc != 3) {
        bug("usage: <input *.img> <input dirname>\n");
    }
    const char *imgname = argv[1], *home = argv[2];
    if (create_img(open_img(imgname), home) != 0) {
        bug("create img failed.\n");
    }
    printf("create %s (%s) successfully.\n", imgname, home);
    return 0;
}

