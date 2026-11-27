/**
 * @file   sfs.h
 * @brief  Simple File System (SFS): on-disk format, in-memory structures and the internal API.
 *
 * SFS is the only on-disk file system of lab9. It lives on the block device "disk0" (the IDE disk),
 * is mounted by sfs_init() during kernel start-up and is reached only through the VFS: the
 * sysfile_* / file_* layers call vop_* on an inode, whose inode_ops are the sfs_node_dirops or
 * sfs_node_fileops tables of sfs_inode.c, and fsop_* on the mounted struct fs, whose functions are
 * set in sfs_fs.c. This header is used by sfs.c, sfs_fs.c, sfs_inode.c, sfs_io.c and sfs_lock.c.
 *
 * On-disk layout. The disk is an array of blocks of SFS_BLKSIZE (4096) bytes:
 *
 *     block 0                  superblock (struct sfs_super), SFS_BLKN_SUPER
 *     block 1                  inode of the root directory, SFS_BLKN_ROOT
 *     blocks 2 .. 2+F-1        free map, F = sfs_freemap_blocks(super), SFS_BLKN_FREEMAP
 *     all other blocks         inodes (one struct sfs_disk_inode per block), file data blocks,
 *                              indirect blocks (arrays of block numbers) and directory entries
 *
 * - The superblock holds the magic number, the total number of blocks, the number of free blocks
 *   and a short label. It is read by sfs_do_mount() and written back by sfs_sync_super().
 * - The free map is a bitmap with one bit per block (see bitmap.h): bit i is 1 if block i is free
 *   and 0 if it is in use. Bits past super.blocks are padding. The map is read at mount time and
 *   written back by sfs_sync_freemap() when the superblock is dirty.
 * - An inode is identified by the number of the block it occupies: that number is the `ino` used
 *   everywhere in the code. The inode of a file or directory stores its size in bytes, its type,
 *   its link count, the number of data blocks it uses and the block numbers of its data:
 *     - logical block i < SFS_NDIRECT (12) is block direct[i];
 *     - logical block i with 12 <= i < 12 + SFS_BLK_NENTRY (1024) is entry (i - 12) of the
 *       indirect block, a block holding 1024 uint32_t block numbers (0 = not allocated).
 *   So one file has at most 12 + 1024 = 1036 data blocks, about 4 MB; see the note at
 *   SFS_MAX_FILE_SIZE.
 * - The data of a directory is its list of logical blocks, and each block holds exactly one
 *   struct sfs_disk_entry: the inode number of the entry (0 = empty slot) and its name. Slot i of
 *   a directory is its logical block i.
 *
 * The in-memory counterpart of an inode is struct sfs_inode, the one of the whole file system is
 * struct sfs_fs. Each mounted SFS is one struct sfs_fs; it caches the superblock, the free map, the
 * list of inodes in memory, a hash table of those inodes indexed by ino, and one scratch block
 * (sfs_buffer). Three semaphores protect it (see struct sfs_fs). Lock order, as observed in the
 * code: fs_sem, then the inode semaphore sin->sem, then io_sem, which is always the innermost.
 */
#ifndef __KERN_FS_SFS_SFS_H__
#define __KERN_FS_SFS_SFS_H__

#include <defs.h>
#include <mmu.h>
#include <list.h>
#include <sem.h>
#include <unistd.h>

/** @brief Magic number that identifies an SFS superblock (checked at mount time). */
#define SFS_MAGIC 0x2f8dbe2a
/** @brief Size of a block of the file system: the same as a page, PGSIZE (4096 bytes). */
#define SFS_BLKSIZE PGSIZE
/** @brief Number of direct block pointers in an inode. */
#define SFS_NDIRECT 12
/** @brief Length of the label stored in the superblock (without the terminating NUL). */
#define SFS_MAX_INFO_LEN 31
/** @brief Maximum length of a file name in a directory entry (without the NUL). */
#define SFS_MAX_FNAME_LEN FS_MAX_FNAME_LEN
/**
 * @brief Largest file size accepted by the SFS seek and truncate operations, 128 MB.
 * @note The on-disk mapping (direct blocks plus one indirect block) only reaches 1036 blocks, about
 *       4 MB. Larger sizes pass the checks in sfs_tryseek() and sfs_truncfile() and then reach the
 *       panic in sfs_bmap_get_nolock(). The constant and the mapping do not agree.
 */
#define SFS_MAX_FILE_SIZE (1024UL * 1024 * 128)
/** @brief Block number of the superblock (block 0). */
#define SFS_BLKN_SUPER 0
/** @brief Block number of the inode of the root directory (block 1). */
#define SFS_BLKN_ROOT 1
/** @brief Block number of the first block of the free map (block 2). */
#define SFS_BLKN_FREEMAP 2

/** @brief Number of bits in one block (one bit per block in the free map). */
#define SFS_BLKBITS (SFS_BLKSIZE * CHAR_BIT)

/** @brief Number of uint32_t block numbers that fit in one block (entries of an indirect block). */
#define SFS_BLK_NENTRY (SFS_BLKSIZE / sizeof(uint32_t))

/** @brief Type of an inode that must not occur on disk (an unused or corrupted inode). */
#define SFS_TYPE_INVAL 0
/** @brief Type of a regular file. */
#define SFS_TYPE_FILE 1
/** @brief Type of a directory. */
#define SFS_TYPE_DIR 2
/** @brief Type of a symbolic link (only reported by sfs_gettype(); no code creates one). */
#define SFS_TYPE_LINK 3

/**
 * @brief On-disk superblock, stored in block SFS_BLKN_SUPER. Only the first bytes of the block
 *        are used; the rest is zero.
 * @var sfs_super::magic          must be SFS_MAGIC
 * @var sfs_super::blocks         total number of blocks of the file system
 * @var sfs_super::unused_blocks  number of free blocks (bits set in the free map)
 * @var sfs_super::info           label of the file system, SFS_MAX_INFO_LEN + 1 bytes, NUL
 *                               terminated
 */
struct sfs_super {
    uint32_t magic;
    uint32_t blocks;
    uint32_t unused_blocks;
    char info[SFS_MAX_INFO_LEN + 1];
};

/**
 * @brief On-disk inode, one per block. The block number of the inode is its ino.
 * @var sfs_disk_inode::size     size of the file in bytes
 * @var sfs_disk_inode::type     one of SFS_TYPE_FILE, SFS_TYPE_DIR, SFS_TYPE_LINK
 * @var sfs_disk_inode::nlinks   number of hard links (directory entries) that name this inode;
 *                               an inode with nlinks == 0 is deleted when its last reference goes
 * @var sfs_disk_inode::blocks   number of logical data blocks in use; the logical blocks are
 *                               0 .. blocks-1 and they are always allocated (there are no holes)
 * @var sfs_disk_inode::direct   block numbers of logical blocks 0 .. SFS_NDIRECT-1
 * @var sfs_disk_inode::indirect block number of the indirect block (logical blocks SFS_NDIRECT and
 *                               above), 0 if none has been allocated yet
 * @note The original ucore also had a double-indirect pointer here. It was removed, which is why
 *       the maximum file size is about 4 MB (see SFS_MAX_FILE_SIZE).
 */
struct sfs_disk_inode {
    uint32_t size;
    uint16_t type;
    uint16_t nlinks;
    uint32_t blocks;
    uint32_t direct[SFS_NDIRECT];
    uint32_t indirect;
};

/**
 * @brief On-disk directory entry. A directory uses one block per entry (see the file header).
 * @var sfs_disk_entry::ino   inode number (block number) of the named file, 0 for an empty slot
 * @var sfs_disk_entry::name  name of the file, NUL-terminated, at most SFS_MAX_FNAME_LEN characters
 */
struct sfs_disk_entry {
    uint32_t ino;
    char name[SFS_MAX_FNAME_LEN + 1];
};

/**
 * @brief Size in bytes of the name field of a directory entry (the unit of getdirentry offsets).
 */
#define sfs_dentry_size sizeof(((struct sfs_disk_entry *)0)->name)

/**
 * @brief In-memory inode of SFS. It is embedded in the VFS struct inode (see vop_info()).
 *
 * Its fields are protected by sem (the inode semaphore) and by the fs semaphore of the file
 * system when it is linked into the lists; see the file header of sfs_inode.c.
 * @var sfs_inode::din           the on-disk inode (heap copy, freed by sfs_reclaim())
 * @var sfs_inode::ino           block number of the on-disk inode
 * @var sfs_inode::dirty         1 if din has been changed and must be written back by sfs_fsync()
 * @var sfs_inode::reclaim_count 1 when the inode is created, plus one each time its reference
 *                               count goes from 0 to 1 in lookup_sfs_nolock(); sfs_reclaim()
 *                               decrements it and frees the inode only when it reaches 0
 * @var sfs_inode::sem           inode semaphore: serialises reads and writes of din and of the
 *                               file contents (lock_sin() / unlock_sin())
 * @var sfs_inode::inode_link    link in sfs_fs::inode_list (all inodes in memory)
 * @var sfs_inode::hash_link     link in the bucket sfs_fs::hash_list[sin_hashfn(ino)]
 */
struct sfs_inode {
    struct sfs_disk_inode *din;
    uint32_t ino;
    bool dirty;
    int reclaim_count;
    semaphore_t sem;
    list_entry_t inode_link;
    list_entry_t hash_link;
};

/**
 * @brief Get the struct sfs_inode that contains the list entry le.
 * @param le     pointer to a list_entry_t inside a struct sfs_inode
 * @param member name of the list_entry_t field (inode_link or hash_link)
 */
#define le2sin(le, member) to_struct((le), struct sfs_inode, member)

/**
 * @brief State of one mounted SFS volume. It is reached through struct fs (see fsop_info()).
 *
 * Three semaphores protect it; see the file header for the lock order.
 * @var sfs_fs::super           in-memory copy of the superblock; super.unused_blocks is kept up to
 *                              date by sfs_block_alloc() and sfs_block_free()
 * @var sfs_fs::dev             block device the file system is mounted on (disk0)
 * @var sfs_fs::freemap         in-memory free map (1 = free), written by sfs_sync_freemap()
 * @var sfs_fs::super_dirty     1 if super or freemap changed since the last sync
 * @var sfs_fs::sfs_buffer      one block of scratch memory, used by sfs_rbuf(), sfs_wbuf(),
 *                              sfs_clear_block() and sfs_sync_super() under io_sem
 * @var sfs_fs::fs_sem          fs semaphore: protects inode_list, hash_list, the loading and the
 *                              reclaiming of inodes (lock_sfs_fs())
 * @var sfs_fs::io_sem          io semaphore: serialises every access to the device and to
 *                              sfs_buffer (lock_sfs_io())
 * @var sfs_fs::mutex_sem      semaphore reserved for link, unlink and rename; initialised at mount,
 *                              not used by any code of this tree
 * @var sfs_fs::inode_list      list of all in-memory sfs_inode objects of this volume
 * @var sfs_fs::hash_list       SFS_HLIST_SIZE buckets of sfs_inode objects, indexed by ino
 */
struct sfs_fs {
    struct sfs_super super;
    struct device *dev;
    struct bitmap *freemap;
    bool super_dirty;
    void *sfs_buffer;
    semaphore_t fs_sem;
    semaphore_t io_sem;
    semaphore_t mutex_sem;
    list_entry_t inode_list;
    list_entry_t *hash_list;
};

/** @brief log2 of the number of hash buckets (the in-memory inode table). */
#define SFS_HLIST_SHIFT 10
/** @brief Number of hash buckets for the in-memory inodes, 2^SFS_HLIST_SHIFT. */
#define SFS_HLIST_SIZE (1 << SFS_HLIST_SHIFT)
/**
 * @brief Map an inode number to its hash bucket index.
 * @param x inode number (block number)
 */
#define sin_hashfn(x) (hash32(x, SFS_HLIST_SHIFT))

/**
 * @brief Number of bits of the free map: super.blocks rounded up to a whole block of bits.
 * @param super pointer to a struct sfs_super
 */
#define sfs_freemap_bits(super) ROUNDUP((super)->blocks, SFS_BLKBITS)

/**
 * @brief Number of blocks that hold the free map on disk.
 * @param super pointer to a struct sfs_super
 */
#define sfs_freemap_blocks(super) ROUNDUP_DIV((super)->blocks, SFS_BLKBITS)

struct fs;
struct inode;

/**
 * @brief Register the SFS driver with the VFS and mount it on "disk0".
 *
 * Called once from fs_init() during kernel start-up. A failure to mount panics.
 */
void sfs_init(void);

/**
 * @brief Mount the SFS volume on the named device.
 * @param devname name of the block device, for example "disk0"
 * @return 0 on success, otherwise the error of vfs_mount() or of sfs_do_mount() (for example
 *         -E_INVAL for a bad magic number or a superblock that claims more blocks than the device
 *         has, -E_NO_MEM, -E_NA_DEV when the device block size is not SFS_BLKSIZE)
 */
int sfs_mount(const char *devname);

/**
 * @brief Take the fs semaphore of a volume (inode list, hash table, load and reclaim of inodes).
 * @param sfs mounted volume
 * @note May sleep. io_sem is the innermost lock, so the caller must not hold io_sem.
 */
void lock_sfs_fs(struct sfs_fs *sfs);

/**
 * @brief Take the io semaphore of a volume (device access and sfs_buffer).
 * @param sfs mounted volume
 * @note May sleep. It is the innermost SFS lock; the functions in sfs_io.c take it themselves.
 */
void lock_sfs_io(struct sfs_fs *sfs);

/**
 * @brief Release the fs semaphore taken by lock_sfs_fs().
 * @param sfs mounted volume
 */
void unlock_sfs_fs(struct sfs_fs *sfs);

/**
 * @brief Release the io semaphore taken by lock_sfs_io().
 * @param sfs mounted volume
 */
void unlock_sfs_io(struct sfs_fs *sfs);

/**
 * @brief Read whole blocks from the disk, with the io lock taken.
 * @param sfs   mounted volume
 * @param buf   [out] buffer of nblks * SFS_BLKSIZE bytes
 * @param blkno first block to read
 * @param nblks number of consecutive blocks
 * @return 0 on success, or the error code of the device; blocks read before an error are kept
 */
int sfs_rblock(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks);

/**
 * @brief Write whole blocks to the disk, with the io lock taken.
 * @param sfs   mounted volume
 * @param buf   [in] buffer of nblks * SFS_BLKSIZE bytes
 * @param blkno first block to write
 * @param nblks number of consecutive blocks
 * @return 0 on success, or the error code of the device; blocks written before an error stay
 */
int sfs_wblock(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks);

/**
 * @brief Read part of one block through sfs_buffer, with the io lock taken.
 * @param sfs    mounted volume
 * @param buf    [out] destination of len bytes
 * @param len    number of bytes to read
 * @param blkno  block to read
 * @param offset offset inside the block; offset + len must not exceed SFS_BLKSIZE (asserted)
 * @return 0 on success, or the device error code
 */
int sfs_rbuf(struct sfs_fs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset);

/**
 * @brief Write part of one block: the block is read, the bytes are replaced and it is written
 *        back, all with the io lock held.
 * @param sfs    mounted volume
 * @param buf    [in] source of len bytes
 * @param len    number of bytes to write
 * @param blkno  block to change
 * @param offset offset inside the block; offset + len must not exceed SFS_BLKSIZE (asserted)
 * @return 0 on success, or the device error code (the block may then be unchanged)
 */
int sfs_wbuf(struct sfs_fs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset);

/**
 * @brief Write the in-memory superblock to block SFS_BLKN_SUPER.
 * @param sfs mounted volume
 * @return 0 on success, or the device error code
 */
int sfs_sync_super(struct sfs_fs *sfs);

/**
 * @brief Write the in-memory free map to its blocks on disk.
 * @param sfs mounted volume
 * @return 0 on success, or the device error code
 */
int sfs_sync_freemap(struct sfs_fs *sfs);

/**
 * @brief Fill nblks blocks with zeros on disk, starting at blkno.
 * @param sfs   mounted volume
 * @param blkno first block to clear
 * @param nblks number of blocks to clear
 * @return 0 on success, or the device error code
 */
int sfs_clear_block(struct sfs_fs *sfs, uint32_t blkno, uint32_t nblks);

/**
 * @brief Get the VFS inode of the on-disk inode at block ino, loading it if needed.
 *
 * If the inode is already in memory its reference count is incremented and it is returned.
 * Otherwise the on-disk inode is read into a new sfs_inode with reference count 1 and linked into
 * the lists of the volume.
 * @param sfs        mounted volume
 * @param node_store [out] the inode; the caller owns one reference and releases it with
 *                         vop_ref_dec()
 * @param ino        block number of the inode, must be in use in the free map
 * @return 0 on success, -E_NO_MEM if memory is exhausted, or the error code of the disk read
 * @note Takes the fs semaphore itself; the caller must not hold it.
 */
int sfs_load_inode(struct sfs_fs *sfs, struct inode **node_store, uint32_t ino);

#endif /* !__KERN_FS_SFS_SFS_H__ */
