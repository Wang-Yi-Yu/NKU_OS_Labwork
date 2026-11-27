/**
 * @file   swap_fifo.h
 * @brief  The FIFO page replacement manager (lab9); the instance is defined in swap_fifo.c.
 */
#ifndef __KERN_MM_SWAP_FIFO_H__
#define __KERN_MM_SWAP_FIFO_H__

#include <swap.h>
/** @brief The FIFO swap manager: evicts the page that has been resident the longest. */
extern struct swap_manager swap_manager_fifo;

#endif /* !__KERN_MM_SWAP_FIFO_H__ */
