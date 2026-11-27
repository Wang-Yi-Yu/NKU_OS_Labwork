#ifndef __KERN_FS_SWAP_SWAPFS_H__
#define __KERN_FS_SWAP_SWAPFS_H__

#include <memlayout.h>

void swapfs_init(void);
int swapfs_read(swap_entry_t entry, struct Page *page);
int swapfs_write(swap_entry_t entry, struct Page *page);

extern size_t max_swap_offset;

#define swap_offset(entry)                                    \
    ({                                                        \
        size_t __offset = ((entry) >> 8);                     \
        if (!(__offset > 0 && __offset < max_swap_offset)) {  \
            panic("invalid swap_entry_t = %08x.\n", (entry)); \
        }                                                     \
        __offset;                                             \
    })

#endif /* !__KERN_FS_SWAP_SWAPFS_H__ */
