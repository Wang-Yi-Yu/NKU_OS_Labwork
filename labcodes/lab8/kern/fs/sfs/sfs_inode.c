/**
 * @file   sfs_inode.c
 * @brief  SFS inode layer: the in-memory inode table, block mapping (sfs_bmap_*), directory
 *         entries and lookup, the file read/write wrapper, and the inode_ops tables of files and
 *         directories.
 *
 * This is the layer between the VFS and the block I/O of sfs_io.c. The VFS reaches it through
 * vop_* (inode_ops); the entry points are the two tables at the end of this file, sfs_node_dirops
 * (directories) and sfs_node_fileops (regular files). A read or write goes
 * vop_read -> sfs_read -> sfs_io -> sfs_io_nolock; the last one moves the bytes and is the lab8
 * exercise of this file (marker `//lab8 function2` below).
 *
 * Block mapping. A logical block index i of a file maps to a disk block number:
 *  - sfs_bmap_get_nolock(): i < SFS_NDIRECT takes din->direct[i]; otherwise (i - SFS_NDIRECT) is an
 *    entry of the indirect block, read and written by sfs_bmap_get_sub_nolock(). With create set,
 *    missing blocks (and the indirect block itself) are allocated and zero-filled.
 *  - sfs_bmap_load_nolock(): the checked form used by everything else. The index must not exceed
 *    din->blocks; index == din->blocks appends a new block and increments din->blocks.
 *  - sfs_bmap_free_nolock() / sfs_bmap_truncate_nolock(): release a block. The indirect block
 *    itself is kept until the inode is reclaimed (sfs_reclaim()).
 *
 * Directories. The logical blocks of a directory are its slots; each slot is one block that
 * holds one struct sfs_disk_entry (ino 0 = empty). sfs_dirent_search_nolock() and
 * sfs_lookup_once() implement name lookup, sfs_getdirentry() implements readdir.
 *
 * Locking. Two kinds of lock are used (see sfs.h for the lock order fs_sem -> sin->sem -> io_sem):
 *  - sin->sem (lock_sin / unlock_sin) protects one inode: its din and its file data. The
 *    functions that change din or read it for a multi-step operation take it; the bmap and
 *    dirent helpers named `_nolock` do not, and require the caller to hold it.
 *  - fs_sem (lock_sfs_fs / unlock_sfs_fs) protects the inode table: sfs_load_inode(),
 *    sfs_reclaim() and sfs_sync() (sfs_fs.c) take it. The free map and the superblock are
 *    changed by the allocation helpers, which are called with the inode semaphore (sfs_truncfile)
 *    or with the fs semaphore (sfs_reclaim); no single lock covers all of their callers.
 *  The reference counts follow the VFS rules of inode.h: vop_ref_inc() / vop_ref_dec() keep the
 *  struct inode alive, and vop_ref_dec() on the last reference calls sfs_reclaim().
 */
#include <defs.h>
#include <string.h>
#include <stdlib.h>
#include <list.h>
#include <stat.h>
#include <kmalloc.h>
#include <vfs.h>
#include <dev.h>
#include <sfs.h>
#include <inode.h>
#include <iobuf.h>
#include <bitmap.h>
#include <error.h>
#include <assert.h>

/** @brief Operations of directories (defined at the end of this file, sfs_node_dirops). */
static const struct inode_ops sfs_node_dirops;
/** @brief Operations of regular files (defined at the end of this file, sfs_node_fileops). */
static const struct inode_ops sfs_node_fileops;

/**
 * @brief Take the inode semaphore of an SFS inode.
 *
 * Must not be called while the caller already holds the same inode (the semaphore is not
 * recursive).
 * @param sin inode to lock
 */
static void lock_sin(struct sfs_inode *sin) {
    down(&(sin->sem));
}

/**
 * @brief Release the inode semaphore taken by lock_sin().
 * @param sin inode to unlock
 */
static void unlock_sin(struct sfs_inode *sin) {
    up(&(sin->sem));
}

/**
 * @brief Return the inode_ops table for an on-disk inode type.
 * @param type SFS_TYPE_DIR or SFS_TYPE_FILE
 * @return pointer to sfs_node_dirops or sfs_node_fileops
 * @note Any other type (including SFS_TYPE_LINK, which has no operations here) panics; the
 *       function never returns NULL.
 */
static const struct inode_ops *sfs_get_ops(uint16_t type) {
    switch (type) {
        case SFS_TYPE_DIR:
            return &sfs_node_dirops;
        case SFS_TYPE_FILE:
            return &sfs_node_fileops;
    }
    panic("invalid file type %d.\n", type);
}

/**
 * @brief Return the hash bucket of the in-memory inode table that an inode number maps to.
 * @param sfs mounted volume
 * @param ino inode number (block number)
 * @return pointer to the list head of the bucket
 */
static list_entry_t *sfs_hash_list(struct sfs_fs *sfs, uint32_t ino) {
    return sfs->hash_list + sin_hashfn(ino);
}

/**
 * @brief Put an inode into the list of all inodes and into its hash bucket.
 * @param sfs mounted volume
 * @param sin inode to add; its ino selects the bucket
 * @note The fs semaphore must be held (the lists are shared).
 */
static void sfs_set_links(struct sfs_fs *sfs, struct sfs_inode *sin) {
    list_add(&(sfs->inode_list), &(sin->inode_link));
    list_add(sfs_hash_list(sfs, sin->ino), &(sin->hash_link));
}

/**
 * @brief Remove an inode from the list of all inodes and from its hash bucket.
 * @param sin inode to remove
 * @note The fs semaphore must be held. The inode is not freed here.
 */
static void sfs_remove_links(struct sfs_inode *sin) {
    list_del(&(sin->inode_link));
    list_del(&(sin->hash_link));
}

/**
 * @brief Check whether a block is in use according to the free map.
 * @param sfs mounted volume
 * @param ino block number, must be non-zero and below super.blocks (otherwise the function panics)
 * @return true if the block is allocated (its bit in the free map is clear)
 */
static bool sfs_block_inuse(struct sfs_fs *sfs, uint32_t ino) {
    if (ino != 0 && ino < sfs->super.blocks) {
        return !bitmap_test(sfs->freemap, ino);
    }
    panic("sfs_block_inuse: called out of range (0, %u) %u.\n", sfs->super.blocks, ino);
}

/**
 * @brief Allocate one free block, zero-fill it and return its number.
 *
 * The first free block (lowest number) is taken from the free map, the free-block count of the
 * superblock is decremented and super_dirty is set. Then the block is cleared on disk with
 * sfs_clear_block().
 * @param sfs      mounted volume
 * @param ino_store [out] number of the new block
 * @return 0 on success, -E_NO_MEM if no block is free, or the device error of sfs_clear_block()
 * @note If sfs_clear_block() fails, the block stays marked as allocated and the caller gets the
 *       error without the block number, so the block is leaked. Callers must not assume that a
 *       failed allocation left the free map unchanged.
 * @note The caller holds either the inode semaphore (sfs_truncfile() and the bmap helpers) or
 *       the fs semaphore (sfs_reclaim()). No single lock covers every caller of the free map.
 */
static int sfs_block_alloc(struct sfs_fs *sfs, uint32_t *ino_store) {
    int ret;
    if ((ret = bitmap_alloc(sfs->freemap, ino_store)) != 0) {
        return ret;
    }
    assert(sfs->super.unused_blocks > 0);
    sfs->super.unused_blocks--, sfs->super_dirty = 1;
    assert(sfs_block_inuse(sfs, *ino_store));
    return sfs_clear_block(sfs, *ino_store, 1);
}

/**
 * @brief Give a block back to the free map.
 *
 * Sets the block's bit to 1 (free), increments the free-block count and sets super_dirty. The
 * block must be in use (asserted). The data on disk is not erased.
 * @param sfs mounted volume
 * @param ino block number to free
 * @note Same locking situation as sfs_block_alloc(): there is no single lock for the free map.
 */
static void sfs_block_free(struct sfs_fs *sfs, uint32_t ino) {
    assert(sfs_block_inuse(sfs, ino));
    bitmap_free(sfs->freemap, ino);
    sfs->super.unused_blocks++, sfs->super_dirty = 1;
}

/**
 * @brief Create the in-memory inode for a disk inode that has just been read.
 *
 * vop_init() gives the new inode reference count 1. The sfs_inode takes ownership of din: it must
 * not be freed by the caller after a success (sfs_reclaim() frees it). The inode semaphore is
 * initialised and dirty is cleared.
 * @param sfs        mounted volume
 * @param din        disk inode already read into memory (ownership passes on success)
 * @param ino        block number the disk inode came from
 * @param node_store [out] the new inode, with one reference; set only on success
 * @return 0 on success, -E_NO_MEM if the inode cannot be allocated (din is then still owned by the
 *         caller)
 * @note The inode is not linked into the table; sfs_load_inode() does that.
 */
static int sfs_create_inode(struct sfs_fs *sfs, struct sfs_disk_inode *din, uint32_t ino,
                            struct inode **node_store) {
    struct inode *node;
    if ((node = alloc_inode(sfs_inode)) != NULL) {
        vop_init(node, sfs_get_ops(din->type), info2fs(sfs, sfs));
        struct sfs_inode *sin = vop_info(node, sfs_inode);
        sin->din = din, sin->ino = ino, sin->dirty = 0, sin->reclaim_count = 1;
        sem_init(&(sin->sem), 1);
        *node_store = node;
        return 0;
    }
    return -E_NO_MEM;
}

/**
 * @brief Find an inode of the volume that is already in memory, by its number.
 *
 * If found, its reference count is incremented; when the count goes from 0 to 1, reclaim_count is
 * incremented too (see struct sfs_inode).
 * @param sfs mounted volume
 * @param ino inode number to look for
 * @return the inode with one new reference, or NULL if it is not in memory
 * @note Lookup in the table only. The `_nolock` suffix means the fs semaphore must be held.
 */
static struct inode *lookup_sfs_nolock(struct sfs_fs *sfs, uint32_t ino) {
    struct inode *node;
    list_entry_t *list = sfs_hash_list(sfs, ino), *le = list;
    while ((le = list_next(le)) != list) {
        struct sfs_inode *sin = le2sin(le, hash_link);
        if (sin->ino == ino) {
            node = info2node(sin, sfs_inode);
            if (vop_ref_inc(node) == 1) {
                sin->reclaim_count++;
            }
            return node;
        }
    }
    return NULL;
}

/**
 * @brief Get the VFS inode of the on-disk inode at block ino, loading it if needed.
 *
 * If the inode is in memory, it is returned with one more reference. Otherwise the on-disk inode is
 * read (sfs_rbuf()), checked for nlinks != 0 (asserted), wrapped in a new inode and linked into the
 * table.
 * @param sfs        mounted volume
 * @param node_store [out] the inode; the caller owns one reference (release with vop_ref_dec())
 * @param ino        block number of the inode, which must be allocated (asserted)
 * @return 0 on success, -E_NO_MEM if memory is exhausted, or the error of the disk read
 * @note Takes the fs semaphore itself; the caller must not hold it.
 */
int sfs_load_inode(struct sfs_fs *sfs, struct inode **node_store, uint32_t ino) {
    lock_sfs_fs(sfs);
    struct inode *node;
    if ((node = lookup_sfs_nolock(sfs, ino)) != NULL) {
        goto out_unlock;
    }

    int ret = -E_NO_MEM;
    struct sfs_disk_inode *din;
    if ((din = kmalloc(sizeof(struct sfs_disk_inode))) == NULL) {
        goto failed_unlock;
    }

    assert(sfs_block_inuse(sfs, ino));
    if ((ret = sfs_rbuf(sfs, din, sizeof(struct sfs_disk_inode), ino, 0)) != 0) {
        goto failed_cleanup_din;
    }

    assert(din->nlinks != 0);
    if ((ret = sfs_create_inode(sfs, din, ino, &node)) != 0) {
        goto failed_cleanup_din;
    }
    sfs_set_links(sfs, vop_info(node, sfs_inode));

out_unlock:
    unlock_sfs_fs(sfs);
    *node_store = node;
    return 0;

failed_cleanup_din:
    kfree(din);
failed_unlock:
    unlock_sfs_fs(sfs);
    return ret;
}

/**
 * @brief Get the block that an entry of an indirect block points to, allocating it if asked.
 *
 * *entp is the block number of the indirect block (0 if it does not exist yet). Entry index of
 * that block is read through sfs_rbuf(). If it is already set, its value is returned. If it is zero
 * and create is set, a new data block is allocated (zero-filled) and its number is written into the
 * entry with sfs_wbuf(). A missing indirect block is allocated too when create is set, and
 * *entp is updated to its number on success.
 *
 * On failure, a newly allocated indirect block is freed again, so nothing new is left allocated
 * and *entp is unchanged.
 * @param sfs       mounted volume
 * @param entp      [in/out] block number of the indirect block; updated if a new one was allocated
 * @param index     index of the entry inside the indirect block, below SFS_BLK_NENTRY (asserted)
 * @param create    if non-zero, allocate the data block (and the indirect block) when missing
 * @param ino_store [out] block number of the data block, or 0 if it is missing and create is 0
 * @return 0 on success, -E_NO_MEM if no block is free, or a device error
 * @note The caller must hold the inode semaphore of the file (lock_sin), because din is changed.
 */
static int sfs_bmap_get_sub_nolock(struct sfs_fs *sfs, uint32_t *entp, uint32_t index, bool create,
                                   uint32_t *ino_store) {
    assert(index < SFS_BLK_NENTRY);
    int ret;
    uint32_t ent, ino = 0;
    off_t offset = index * sizeof(uint32_t);  // the offset of entry in entry block
    // if entry block is existd, read the content of entry block into  sfs->sfs_buffer
    if ((ent = *entp) != 0) {
        if ((ret = sfs_rbuf(sfs, &ino, sizeof(uint32_t), ent, offset)) != 0) {
            return ret;
        }
        if (ino != 0 || !create) {
            goto out;
        }
    } else {
        if (!create) {
            goto out;
        }
        //if entry block isn't existd, allocated a entry block (for indrect block)
        if ((ret = sfs_block_alloc(sfs, &ent)) != 0) {
            return ret;
        }
    }

    if ((ret = sfs_block_alloc(sfs, &ino)) != 0) {
        goto failed_cleanup;
    }
    if ((ret = sfs_wbuf(sfs, &ino, sizeof(uint32_t), ent, offset)) != 0) {
        sfs_block_free(sfs, ino);
        goto failed_cleanup;
    }

out:
    if (ent != *entp) {
        *entp = ent;
    }
    *ino_store = ino;
    return 0;

failed_cleanup:
    if (ent != *entp) {
        sfs_block_free(sfs, ent);
    }
    return ret;
}

/**
 * @brief Map a logical block index of a file to its disk block, allocating it if asked.
 *
 * Indices below SFS_NDIRECT use din->direct[index]. The others are entries of the indirect block,
 * handled by sfs_bmap_get_sub_nolock(). When a new block is allocated, din is changed and
 * sin->dirty is set. Indices past SFS_NDIRECT + SFS_BLK_NENTRY panic: the on-disk format has no
 * further level.
 * @param sfs       mounted volume
 * @param sin       inode of the file
 * @param index     logical block index (not checked against din->blocks here)
 * @param create    if non-zero, allocate the block when it is missing
 * @param ino_store [out] block number, or 0 if the block is missing and create is 0
 * @return 0 on success, -E_NO_MEM if no block is free, or a device error
 * @note The caller must hold the inode semaphore of sin (lock_sin). The `_nolock` suffix means the
 *       lock is not taken here. The returned block (if any) is asserted to be in use.
 */
static int sfs_bmap_get_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, uint32_t index,
                               bool create, uint32_t *ino_store) {
    struct sfs_disk_inode *din = sin->din;
    int ret;
    uint32_t ent, ino;
    // the index of disk block is in the fist SFS_NDIRECT  direct blocks
    if (index < SFS_NDIRECT) {
        if ((ino = din->direct[index]) == 0 && create) {
            if ((ret = sfs_block_alloc(sfs, &ino)) != 0) {
                return ret;
            }
            din->direct[index] = ino;
            sin->dirty = 1;
        }
        goto out;
    }
    // the index of disk block is in the indirect blocks.
    index -= SFS_NDIRECT;
    if (index < SFS_BLK_NENTRY) {
        ent = din->indirect;
        if ((ret = sfs_bmap_get_sub_nolock(sfs, &ent, index, create, &ino)) != 0) {
            return ret;
        }
        if (ent != din->indirect) {
            assert(din->indirect == 0);
            din->indirect = ent;
            sin->dirty = 1;
        }
        goto out;
    } else {
        panic("sfs_bmap_get_nolock - index out of range");
    }
out:
    assert(ino == 0 || sfs_block_inuse(sfs, ino));
    *ino_store = ino;
    return 0;
}

/**
 * @brief Release the data block that an entry of an indirect block points to.
 *
 * If the entry is non-zero, the data block is freed (sfs_block_free()) and the entry is set to 0 on
 * disk with sfs_wbuf(). The indirect block itself is not freed.
 * @param sfs   mounted volume
 * @param ent   block number of the indirect block, which must be in use (asserted)
 * @param index entry index inside the indirect block, below SFS_BLK_NENTRY (asserted)
 * @return 0 on success, or a device error (the block is then not freed)
 * @note The caller must hold the inode semaphore of the file. The free map is changed by
 *       sfs_block_free(), see the locking note there.
 */
static int sfs_bmap_free_sub_nolock(struct sfs_fs *sfs, uint32_t ent, uint32_t index) {
    assert(sfs_block_inuse(sfs, ent) && index < SFS_BLK_NENTRY);
    int ret;
    uint32_t ino, zero = 0;
    off_t offset = index * sizeof(uint32_t);
    if ((ret = sfs_rbuf(sfs, &ino, sizeof(uint32_t), ent, offset)) != 0) {
        return ret;
    }
    if (ino != 0) {
        if ((ret = sfs_wbuf(sfs, &zero, sizeof(uint32_t), ent, offset)) != 0) {
            return ret;
        }
        sfs_block_free(sfs, ino);
    }
    return 0;
}

/**
 * @brief Release the disk block of one logical index of a file and clear its pointer.
 *
 * For a direct index the block is freed and din->direct[index] is set to 0. For an indirect index
 * sfs_bmap_free_sub_nolock() is used, and only when the indirect block exists. An index past
 * SFS_NDIRECT + SFS_BLK_NENTRY does nothing and returns 0. The indirect block stays allocated.
 * @param sfs   mounted volume
 * @param sin   inode of the file
 * @param index logical block index
 * @return 0 on success, or a device error
 * @note The caller must hold the inode semaphore of sin. The free map is changed, see
 *       sfs_block_free().
 */
static int sfs_bmap_free_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, uint32_t index) {
    struct sfs_disk_inode *din = sin->din;
    int ret;
    uint32_t ent, ino;
    if (index < SFS_NDIRECT) {
        if ((ino = din->direct[index]) != 0) {
            // free the block
            sfs_block_free(sfs, ino);
            din->direct[index] = 0;
            sin->dirty = 1;
        }
        return 0;
    }

    index -= SFS_NDIRECT;
    if (index < SFS_BLK_NENTRY) {
        if ((ent = din->indirect) != 0) {
            // set the entry item to 0 in the indirect block
            if ((ret = sfs_bmap_free_sub_nolock(sfs, ent, index)) != 0) {
                return ret;
            }
        }
        return 0;
    }
    return 0;
}

/**
 * @brief Map a logical block of a file to its disk block, checked against the file size in blocks.
 *
 * index must not exceed din->blocks (asserted). If index == din->blocks, a new block is appended:
 * sfs_bmap_get_nolock() allocates it and din->blocks is incremented. Otherwise the existing
 * block is looked up. The logical blocks are always allocated (no holes), so the result is
 * asserted to be in use.
 * @param sfs       mounted volume
 * @param sin       inode of the file
 * @param index     logical block index, 0 .. din->blocks
 * @param ino_store [out] block number of the logical block; may be NULL
 * @return 0 on success, -E_NO_MEM if no block is free, or a device error
 * @note The caller must hold the inode semaphore of sin.
 */
static int sfs_bmap_load_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, uint32_t index,
                                uint32_t *ino_store) {
    struct sfs_disk_inode *din = sin->din;
    assert(index <= din->blocks);
    int ret;
    uint32_t ino;
    bool create = (index == din->blocks);
    if ((ret = sfs_bmap_get_nolock(sfs, sin, index, create, &ino)) != 0) {
        return ret;
    }
    assert(sfs_block_inuse(sfs, ino));
    if (create) {
        din->blocks++;
    }
    if (ino_store != NULL) {
        *ino_store = ino;
    }
    return 0;
}

/**
 * @brief Free the last logical block of a file and decrement the block count.
 *
 * The file must have at least one block (asserted). The block at index din->blocks - 1 is released
 * with sfs_bmap_free_nolock(), then din->blocks is decremented and sin->dirty is set.
 * @param sfs mounted volume
 * @param sin inode of the file
 * @return 0 on success, or the error of sfs_bmap_free_nolock()
 * @note The caller must hold the inode semaphore of sin.
 */
static int sfs_bmap_truncate_nolock(struct sfs_fs *sfs, struct sfs_inode *sin) {
    struct sfs_disk_inode *din = sin->din;
    assert(din->blocks != 0);
    int ret;
    if ((ret = sfs_bmap_free_nolock(sfs, sin, din->blocks - 1)) != 0) {
        return ret;
    }
    din->blocks--;
    sin->dirty = 1;
    return 0;
}

/**
 * @brief Read one directory entry (one slot of a directory).
 *
 * The slot's logical block is found with sfs_bmap_load_nolock() and its first bytes are read into
 * entry. The name is forced to be NUL-terminated.
 * @param sfs   mounted volume
 * @param sin   inode of the directory (type SFS_TYPE_DIR, asserted)
 * @param slot  slot index, 0 <= slot < din->blocks (asserted)
 * @param entry [out] receives the entry (ino and name)
 * @return 0 on success, or a device error
 * @note The caller must hold the inode semaphore of sin.
 */
static int sfs_dirent_read_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, int slot,
                                  struct sfs_disk_entry *entry) {
    assert(sin->din->type == SFS_TYPE_DIR && (slot >= 0 && slot < sin->din->blocks));
    int ret;
    uint32_t ino;
    // according to the DIR's inode and the slot of file entry, find the index of disk block
    // which contains this file entry
    if ((ret = sfs_bmap_load_nolock(sfs, sin, slot, &ino)) != 0) {
        return ret;
    }
    assert(sfs_block_inuse(sfs, ino));
    // read the content of file entry in the disk block
    if ((ret = sfs_rbuf(sfs, entry, sizeof(struct sfs_disk_entry), ino, 0)) != 0) {
        return ret;
    }
    entry->name[SFS_MAX_FNAME_LEN] = '\0';
    return 0;
}

/**
 * @brief Call sfs_dirent_link_nolock() and print a warning if it fails. (Unused macro.)
 * @param sfs    mounted volume
 * @param sin    directory inode
 * @param slot   slot to link into
 * @param lnksin inode to link
 * @param name   name of the entry
 * @note Not expanded anywhere in this tree, and sfs_dirent_link_nolock() is not defined in it.
 */
#define sfs_dirent_link_nolock_check(sfs, sin, slot, lnksin, name)               \
    do {                                                                         \
        int err;                                                                 \
        if ((err = sfs_dirent_link_nolock(sfs, sin, slot, lnksin, name)) != 0) { \
            warn("sfs_dirent_link error: %e.\n", err);                           \
        }                                                                        \
    } while (0)

/**
 * @brief Call sfs_dirent_unlink_nolock() and print a warning if it fails. (Unused macro.)
 * @param sfs    mounted volume
 * @param sin    directory inode
 * @param slot   slot to unlink
 * @param lnksin inode that is unlinked
 * @note Not expanded anywhere in this tree, and sfs_dirent_unlink_nolock() is not defined in it.
 */
#define sfs_dirent_unlink_nolock_check(sfs, sin, slot, lnksin)               \
    do {                                                                     \
        int err;                                                             \
        if ((err = sfs_dirent_unlink_nolock(sfs, sin, slot, lnksin)) != 0) { \
            warn("sfs_dirent_unlink error: %e.\n", err);                     \
        }                                                                    \
    } while (0)

/**
 * @brief Search a directory for a name and, on the way, note the first free slot.
 *
 * Every slot of the directory is read in order. A slot with ino 0 is empty: its index is stored
 * into *empty_slot and the scan goes on, so *empty_slot ends with the last empty slot seen before
 * the scan stopped (or the slot count if none was seen). When a slot holds the name, the scan
 * stops and its slot index and inode number are returned.
 * @param sfs        mounted volume
 * @param sin        directory inode
 * @param name       name to search for, at most SFS_MAX_FNAME_LEN characters (asserted)
 * @param ino_store  [out] inode number of the entry found; may be NULL
 * @param slot       [out] slot index of the entry found; may be NULL
 * @param empty_slot [out] index of an empty slot (see above); may be NULL
 * @return 0 if the name was found, -E_NOENT if not, -E_NO_MEM if the scratch entry cannot be
 *         allocated, or a device error
 * @note The caller must hold the inode semaphore of the directory.
 */
static int sfs_dirent_search_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, const char *name,
                                    uint32_t *ino_store, int *slot, int *empty_slot) {
    assert(strlen(name) <= SFS_MAX_FNAME_LEN);
    struct sfs_disk_entry *entry;
    if ((entry = kmalloc(sizeof(struct sfs_disk_entry))) == NULL) {
        return -E_NO_MEM;
    }

/**
 * @brief Store v into *x if the pointer x is not NULL.
 * @param x pointer to the output variable, may be NULL
 * @param v value to store
 */
#define set_pvalue(x, v)   \
    do {                   \
        if ((x) != NULL) { \
            *(x) = (v);    \
        }                  \
    } while (0)
    int ret, i, nslots = sin->din->blocks;
    set_pvalue(empty_slot, nslots);
    for (i = 0; i < nslots; i++) {
        if ((ret = sfs_dirent_read_nolock(sfs, sin, i, entry)) != 0) {
            goto out;
        }
        if (entry->ino == 0) {
            set_pvalue(empty_slot, i);
            continue;
        }
        if (strcmp(name, entry->name) == 0) {
            set_pvalue(slot, i);
            set_pvalue(ino_store, entry->ino);
            goto out;
        }
    }
#undef set_pvalue
    ret = -E_NOENT;
out:
    kfree(entry);
    return ret;
}

/**
 * @brief Find the entry of a directory that holds a given inode number.
 *
 * Reads the slots in order and copies the first entry whose ino equals the argument into entry.
 * @param sfs   mounted volume
 * @param sin   directory inode
 * @param ino   inode number to look for
 * @param entry [out] the entry found (ino and name)
 * @return 0 if found, -E_NOENT if no entry holds ino, or a device error
 * @note The caller must hold the inode semaphore of the directory.
 */
static int sfs_dirent_findino_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, uint32_t ino,
                                     struct sfs_disk_entry *entry) {
    int ret, i, nslots = sin->din->blocks;
    for (i = 0; i < nslots; i++) {
        if ((ret = sfs_dirent_read_nolock(sfs, sin, i, entry)) != 0) {
            return ret;
        }
        if (entry->ino == ino) {
            return 0;
        }
    }
    return -E_NOENT;
}

/**
 * @brief Find the inode of a name in a directory, once (a single lookup step).
 *
 * The directory is searched under its inode semaphore, which is then released; only after that is
 * the found inode loaded with sfs_load_inode(). The caller therefore must not hold the semaphore of
 * sin when calling this function.
 * @param sfs        mounted volume
 * @param sin        directory inode
 * @param name       name to look up
 * @param node_store [out] the inode of the name, with one reference; set only on success
 * @param slot       [out] slot index of the entry, may be NULL
 * @return 0 on success, -E_NOENT if the name does not exist, or an error of the search or of
 *         the load
 */
static int sfs_lookup_once(struct sfs_fs *sfs, struct sfs_inode *sin, const char *name,
                           struct inode **node_store, int *slot) {
    int ret;
    uint32_t ino;
    lock_sin(sin);
    {  // find the NO. of disk block and logical index of file entry
        ret = sfs_dirent_search_nolock(sfs, sin, name, &ino, slot, NULL);
    }
    unlock_sin(sin);
    if (ret == 0) {
        // load the content of inode with the the NO. of disk block
        ret = sfs_load_inode(sfs, node_store, ino);
    }
    return ret;
}

/**
 * @brief Open a directory: only read-only opening is allowed.
 * @param node       the directory (unused)
 * @param open_flags open flags of the request (O_RDONLY, O_WRONLY, O_RDWR, O_APPEND)
 * @return 0 for O_RDONLY without O_APPEND, -E_ISDIR for any write mode or O_APPEND
 */
static int sfs_opendir(struct inode *node, uint32_t open_flags) {
    switch (open_flags & O_ACCMODE) {
        case O_RDONLY:
            break;
        case O_WRONLY:
        case O_RDWR:
        default:
            return -E_ISDIR;
    }
    if (open_flags & O_APPEND) {
        return -E_ISDIR;
    }
    return 0;
}

/**
 * @brief Open a regular file. Nothing is checked: the open flags are accepted as they are.
 * @param node       the file (unused)
 * @param open_flags open flags (unused)
 * @return always 0
 */
static int sfs_openfile(struct inode *node, uint32_t open_flags) {
    return 0;
}

/**
 * @brief Close a file or directory: write back the inode if it is dirty.
 * @param node the inode being closed
 * @return the result of vop_fsync(node)
 */
static int sfs_close(struct inode *node) {
    return vop_fsync(node);
}

//lab8 function2 YOUR CODE

/**
 * @brief Common part of sfs_read() and sfs_write(): transfer data between a file and an iobuf.
 *
 * The inode semaphore is held for the whole transfer. The requested length is passed in alen (the
 * remaining length of iob); sfs_io_nolock() transfers at iob->io_offset and writes the number of
 * bytes it actually transferred back into alen. Then the iobuf is advanced by that number, also
 * when an error occurred after part of the data was moved.
 *
 * sfs_io_nolock() is called with the inode lock held and receives the in/out length alen; it does
 * not take any lock itself. It is the lab exercise marked above.
 * @param node  the file (its vop_fs and vop_info are used)
 * @param iob   buffer of the transfer; its io_base and io_offset are advanced by the bytes moved
 * @param write false to read the file into iob, true to write iob into the file
 * @return the return value of sfs_io_nolock()
 */
static inline int sfs_io(struct inode *node, struct iobuf *iob, bool write) {
    struct sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
    struct sfs_inode *sin = vop_info(node, sfs_inode);
    int ret;
    lock_sin(sin);
    {
        size_t alen = iob->io_resid;
        ret = sfs_io_nolock(sfs, sin, iob->io_base, iob->io_offset, &alen, write);
        if (alen != 0) {
            iobuf_skip(iob, alen);
        }
    }
    unlock_sin(sin);
    return ret;
}

/**
 * @brief Read from a regular file into an iobuf (vop_read operation).
 * @param node the file
 * @param iob  destination; advanced by the number of bytes read
 * @return the result of sfs_io() (0 on success, otherwise an error code)
 */
static int sfs_read(struct inode *node, struct iobuf *iob) {
    return sfs_io(node, iob, 0);
}

/**
 * @brief Write an iobuf into a regular file (vop_write operation).
 * @param node the file
 * @param iob  source; advanced by the number of bytes written
 * @return the result of sfs_io() (0 on success, otherwise an error code)
 */
static int sfs_write(struct inode *node, struct iobuf *iob) {
    return sfs_io(node, iob, 1);
}

/**
 * @brief Fill a struct stat with the mode, link count, block count and size (vop_fstat).
 *
 * st_blocks is the number of SFS blocks (SFS_BLKSIZE bytes each), not of 512-byte sectors. The
 * on-disk inode is read without taking the inode semaphore.
 * @param node the file or directory
 * @param stat [out] receives the information; all fields not listed here are zero
 * @return 0 (vop_gettype() does not fail for valid inode types)
 */
static int sfs_fstat(struct inode *node, struct stat *stat) {
    int ret;
    memset(stat, 0, sizeof(struct stat));
    if ((ret = vop_gettype(node, &(stat->st_mode))) != 0) {
        return ret;
    }
    struct sfs_disk_inode *din = vop_info(node, sfs_inode)->din;
    stat->st_nlinks = din->nlinks;
    stat->st_blocks = din->blocks;
    stat->st_size = din->size;
    return 0;
}

/**
 * @brief Write the on-disk inode back if it is dirty (vop_fsync).
 *
 * The check of dirty is repeated under the inode semaphore, so concurrent syncs write at most
 * once. dirty is cleared before the write and set again if the write fails.
 * @param node the file or directory
 * @return 0 on success or when the inode was clean, otherwise the error of sfs_wbuf()
 * @note The inode semaphore must not be held by the caller. sfs_sync() calls this with the fs
 *       semaphore held, so the lock order is fs_sem, then the inode semaphore.
 */
static int sfs_fsync(struct inode *node) {
    struct sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
    struct sfs_inode *sin = vop_info(node, sfs_inode);
    int ret = 0;
    if (sin->dirty) {
        lock_sin(sin);
        {
            if (sin->dirty) {
                sin->dirty = 0;
                if ((ret = sfs_wbuf(sfs, sin->din, sizeof(struct sfs_disk_inode), sin->ino, 0)) !=
                    0) {
                    sin->dirty = 1;
                }
            }
        }
        unlock_sin(sin);
    }
    return ret;
}

/**
 * @brief Write the absolute path of a directory, relative to the root of this file system, into an
 *        iobuf (vop_namefile). Used by getcwd.
 *
 * The path is built from the end of the buffer backwards: the name of the directory is found in
 * its parent (through the entry ".."), prepended with a '/', and so on up to the root. The result
 * is "/a/b/" followed by a NUL byte, written at the current position of iob, and iob is advanced by
 * the length of the path without the leading '/' and the NUL. vfs_getcwd() writes the device
 * prefix before this call.
 *
 * The function takes a reference to node for the walk and releases it on all paths.
 * @param node the directory whose path is wanted
 * @param iob  [in/out] buffer; at least 3 bytes must be left (io_resid > 2)
 * @return 0 on success, -E_NO_MEM if the buffer is too small for the path or if the scratch entry
 *         cannot be allocated, or another error of the lookup
 * @note A buffer that is too small is reported as -E_NO_MEM. The function is meant for directories:
 *       sfs_dirent_read_nolock() asserts that the directory type is SFS_TYPE_DIR.
 */
static int sfs_namefile(struct inode *node, struct iobuf *iob) {
    struct sfs_disk_entry *entry;
    if (iob->io_resid <= 2 || (entry = kmalloc(sizeof(struct sfs_disk_entry))) == NULL) {
        return -E_NO_MEM;
    }

    struct sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
    struct sfs_inode *sin = vop_info(node, sfs_inode);

    int ret;
    char *ptr = iob->io_base + iob->io_resid;
    size_t alen, resid = iob->io_resid - 2;
    vop_ref_inc(node);
    while (1) {
        struct inode *parent;
        if ((ret = sfs_lookup_once(sfs, sin, "..", &parent, NULL)) != 0) {
            goto failed;
        }

        uint32_t ino = sin->ino;
        vop_ref_dec(node);
        if (node == parent) {
            vop_ref_dec(node);
            break;
        }

        node = parent, sin = vop_info(node, sfs_inode);
        assert(ino != sin->ino && sin->din->type == SFS_TYPE_DIR);

        lock_sin(sin);
        { ret = sfs_dirent_findino_nolock(sfs, sin, ino, entry); }
        unlock_sin(sin);

        if (ret != 0) {
            goto failed;
        }

        if ((alen = strlen(entry->name) + 1) > resid) {
            goto failed_nomem;
        }
        resid -= alen, ptr -= alen;
        memcpy(ptr, entry->name, alen - 1);
        ptr[alen - 1] = '/';
    }
    alen = iob->io_resid - resid - 2;
    ptr = memmove(iob->io_base + 1, ptr, alen);
    ptr[-1] = '/', ptr[alen] = '\0';
    iobuf_skip(iob, alen);
    kfree(entry);
    return 0;

failed_nomem:
    ret = -E_NO_MEM;
failed:
    vop_ref_dec(node);
    kfree(entry);
    return ret;
}

/**
 * @brief Return the entry with a given number of live entries before it (readdir helper).
 *
 * The slots are read in order; empty slots (ino 0) are skipped, and the slot-th live entry
 * (counting from 0) is copied into entry.
 * @param sfs   mounted volume
 * @param sin   directory inode
 * @param slot  number of live entries to skip (0 = the first live entry)
 * @param entry [out] the entry found
 * @return 0 if found, -E_NOENT if there are not enough live entries, or a device error
 * @note The caller must hold the inode semaphore of the directory.
 */
static int sfs_getdirentry_sub_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, int slot,
                                      struct sfs_disk_entry *entry) {
    int ret, i, nslots = sin->din->blocks;
    for (i = 0; i < nslots; i++) {
        if ((ret = sfs_dirent_read_nolock(sfs, sin, i, entry)) != 0) {
            return ret;
        }
        if (entry->ino != 0) {
            if (slot == 0) {
                return 0;
            }
            slot--;
        }
    }
    return -E_NOENT;
}

/**
 * @brief Read the name of a directory entry, selected by a byte offset (vop_getdirentry).
 *
 * The offset must be a non-negative multiple of sfs_dentry_size (256). Its quotient is the index
 * of the live entry, not of the disk slot: empty slots are not counted. The name of that entry is
 * copied to iob (sfs_dentry_size bytes). If iob has less room than that, the copy is partial and
 * -E_NO_MEM is returned; iob is advanced by what was copied.
 * @param node the directory
 * @param iob  [in/out] receives the name; io_offset gives the byte offset of the entry
 * @return 0 on success, -E_INVAL for a negative or unaligned offset, -E_NOENT if the offset is past
 *         the last slot or there is no such live entry, -E_NO_MEM if the buffer is too small or
 *         the scratch entry cannot be allocated, or a device error
 */
static int sfs_getdirentry(struct inode *node, struct iobuf *iob) {
    struct sfs_disk_entry *entry;
    if ((entry = kmalloc(sizeof(struct sfs_disk_entry))) == NULL) {
        return -E_NO_MEM;
    }

    struct sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
    struct sfs_inode *sin = vop_info(node, sfs_inode);

    int ret, slot;
    off_t offset = iob->io_offset;
    if (offset < 0 || offset % sfs_dentry_size != 0) {
        kfree(entry);
        return -E_INVAL;
    }
    if ((slot = offset / sfs_dentry_size) > sin->din->blocks) {
        kfree(entry);
        return -E_NOENT;
    }
    lock_sin(sin);
    if ((ret = sfs_getdirentry_sub_nolock(sfs, sin, slot, entry)) != 0) {
        unlock_sin(sin);
        goto out;
    }
    unlock_sin(sin);
    ret = iobuf_move(iob, entry->name, sfs_dentry_size, 1, NULL);
out:
    kfree(entry);
    return ret;
}

/**
 * @brief Release the resources of an inode that is no longer referenced (vop_reclaim).
 *
 * Called by inode_ref_dec() when the reference count drops to 0. Under the fs semaphore:
 *  1. reclaim_count is decremented; if it is still non-zero, or the inode is still referenced,
 *     the function fails with -E_BUSY and keeps the inode;
 *  2. if nlinks is 0 the file is deleted: all its data blocks are freed (vop_truncate to 0);
 *  3. a dirty inode is written back (vop_fsync);
 *  4. the inode is unlinked from the table.
 * Then, without the fs semaphore, a deleted inode frees its own block and its indirect block; the
 * din buffer is freed and the struct inode is killed with vop_kill().
 * @param node the inode being reclaimed
 * @return 0 when the inode was released, -E_BUSY if it is still in use, or the error of the
 *         truncate or of the write-back (the inode is then kept)
 * @note reclaim_count is decremented before the test, and the code does not restore it on the
 *       -E_BUSY path.
 * @note The block frees after step 4 are done after the fs semaphore is released, so the free map
 *       and the superblock are changed without that lock held.
 */
static int sfs_reclaim(struct inode *node) {
    struct sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
    struct sfs_inode *sin = vop_info(node, sfs_inode);

    int ret = -E_BUSY;
    uint32_t ent;
    lock_sfs_fs(sfs);
    assert(sin->reclaim_count > 0);
    if ((--sin->reclaim_count) != 0 || inode_ref_count(node) != 0) {
        goto failed_unlock;
    }
    if (sin->din->nlinks == 0) {
        if ((ret = vop_truncate(node, 0)) != 0) {
            goto failed_unlock;
        }
    }
    if (sin->dirty) {
        if ((ret = vop_fsync(node)) != 0) {
            goto failed_unlock;
        }
    }
    sfs_remove_links(sin);
    unlock_sfs_fs(sfs);

    if (sin->din->nlinks == 0) {
        sfs_block_free(sfs, sin->ino);
        if ((ent = sin->din->indirect) != 0) {
            sfs_block_free(sfs, ent);
        }
    }
    kfree(sin->din);
    vop_kill(node);
    return 0;

failed_unlock:
    unlock_sfs_fs(sfs);
    return ret;
}

/**
 * @brief Return the file type in stat format (vop_gettype).
 * @param node       the file or directory
 * @param type_store [out] S_IFDIR, S_IFREG or S_IFLNK
 * @return 0
 * @note Any other type on disk (SFS_TYPE_INVAL or a corrupted value) panics.
 */
static int sfs_gettype(struct inode *node, uint32_t *type_store) {
    struct sfs_disk_inode *din = vop_info(node, sfs_inode)->din;
    switch (din->type) {
        case SFS_TYPE_DIR:
            *type_store = S_IFDIR;
            return 0;
        case SFS_TYPE_FILE:
            *type_store = S_IFREG;
            return 0;
        case SFS_TYPE_LINK:
            *type_store = S_IFLNK;
            return 0;
    }
    panic("invalid file type %d.\n", din->type);
}

/**
 * @brief Check a seek position and extend the file if the position is past its end (vop_tryseek).
 *
 * A position in [0, SFS_MAX_FILE_SIZE) is accepted. If it is past the current size, the file is
 * grown to that position with vop_truncate(): the new space is made of zero-filled blocks.
 * @param node the file
 * @param pos  the requested position in bytes
 * @return 0 if the seek is allowed (the file may have been extended), -E_INVAL if pos is
 *         negative or not below SFS_MAX_FILE_SIZE, or the error of the extension
 */
static int sfs_tryseek(struct inode *node, off_t pos) {
    if (pos < 0 || pos >= SFS_MAX_FILE_SIZE) {
        return -E_INVAL;
    }
    struct sfs_inode *sin = vop_info(node, sfs_inode);
    if (pos > sin->din->size) {
        return vop_truncate(node, pos);
    }
    return 0;
}

/**
 * @brief Change the size of a file, adding or freeing blocks at its end (vop_truncate).
 *
 * The number of blocks needed for len bytes is ROUNDUP_DIV(len, SFS_BLKSIZE). If it is larger than
 * din->blocks, blocks are appended with sfs_bmap_load_nolock() (they are zero-filled when
 * allocated); if it is smaller, the last blocks are freed with sfs_bmap_truncate_nolock(). Then
 * din->size is set to len and the inode is marked dirty. When len equals the current size the
 * function returns at once. Bytes of the last kept block beyond len are not cleared.
 * @param node the file
 * @param len  the new size in bytes, 0 .. SFS_MAX_FILE_SIZE (otherwise -E_INVAL)
 * @return 0 on success; on an allocation or device error the blocks changed so far stay changed and
 *         din->size is not updated
 * @note The function takes the inode semaphore itself. It is also called by sfs_reclaim() (with the
 *       fs semaphore held) and by sfs_tryseek().
 */
static int sfs_truncfile(struct inode *node, off_t len) {
    if (len < 0 || len > SFS_MAX_FILE_SIZE) {
        return -E_INVAL;
    }
    struct sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
    struct sfs_inode *sin = vop_info(node, sfs_inode);
    struct sfs_disk_inode *din = sin->din;

    int ret = 0;
    //new number of disk blocks of file
    uint32_t nblks, tblks = ROUNDUP_DIV(len, SFS_BLKSIZE);
    if (din->size == len) {
        assert(tblks == din->blocks);
        return 0;
    }

    lock_sin(sin);
    // old number of disk blocks of file
    nblks = din->blocks;
    if (nblks < tblks) {
        // try to enlarge the file size by add new disk block at the end of file
        while (nblks != tblks) {
            if ((ret = sfs_bmap_load_nolock(sfs, sin, nblks, NULL)) != 0) {
                goto out_unlock;
            }
            nblks++;
        }
    } else if (tblks < nblks) {
        // try to reduce the file size
        while (tblks != nblks) {
            if ((ret = sfs_bmap_truncate_nolock(sfs, sin)) != 0) {
                goto out_unlock;
            }
            nblks--;
        }
    }
    assert(din->blocks == tblks);
    din->size = len;
    sin->dirty = 1;

out_unlock:
    unlock_sin(sin);
    return ret;
}

/**
 * @brief Look up a name in a directory (vop_lookup).
 *
 * The directory must be of type SFS_TYPE_DIR (-E_NOTDIR otherwise). path must not be empty and must
 * not start with '/' (asserted). The name is compared with the whole entry names, so it has to be
 * one name component. The directory is referenced during the search.
 * @param node       the directory to search in
 * @param path       the name to find; relative, non-empty
 * @param node_store [out] the inode found, with one reference; set only on success
 * @return 0 on success, -E_NOTDIR if node is not a directory, -E_NOENT if the name does not exist,
 *         or an error of the load
 */
static int sfs_lookup(struct inode *node, char *path, struct inode **node_store) {
    struct sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
    assert(*path != '\0' && *path != '/');
    vop_ref_inc(node);
    struct sfs_inode *sin = vop_info(node, sfs_inode);
    if (sin->din->type != SFS_TYPE_DIR) {
        vop_ref_dec(node);
        return -E_NOTDIR;
    }
    struct inode *subnode;
    int ret = sfs_lookup_once(sfs, sin, path, &subnode, NULL);

    vop_ref_dec(node);
    if (ret != 0) {
        return ret;
    }
    *node_store = subnode;
    return 0;
}

/**
 * @brief Operations of SFS directories, used through inode_ops.
 *
 * Read-only: opening for writing is refused by sfs_opendir(). The table has no create, link or
 * unlink entries.
 */
static const struct inode_ops sfs_node_dirops = {
    .vop_magic = VOP_MAGIC,
    .vop_open = sfs_opendir,
    .vop_close = sfs_close,
    .vop_fstat = sfs_fstat,
    .vop_fsync = sfs_fsync,
    .vop_namefile = sfs_namefile,
    .vop_getdirentry = sfs_getdirentry,
    .vop_reclaim = sfs_reclaim,
    .vop_gettype = sfs_gettype,
    .vop_lookup = sfs_lookup,
};
/**
 * @brief Operations of SFS regular files, used through inode_ops (read, write, seek, truncate).
 */
static const struct inode_ops sfs_node_fileops = {
    .vop_magic = VOP_MAGIC,
    .vop_open = sfs_openfile,
    .vop_close = sfs_close,
    .vop_read = sfs_read,
    .vop_write = sfs_write,
    .vop_fstat = sfs_fstat,
    .vop_fsync = sfs_fsync,
    .vop_reclaim = sfs_reclaim,
    .vop_gettype = sfs_gettype,
    .vop_tryseek = sfs_tryseek,
    .vop_truncate = sfs_truncfile,
};
