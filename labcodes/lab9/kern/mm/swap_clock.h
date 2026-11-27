/**
 * @file   swap_clock.h
 * @brief  The Clock page replacement manager (lab9 exercise); the instance is defined in swap_clock.c.
 */
#ifndef __KERN_MM_SWAP_CLOCK_H__
#define __KERN_MM_SWAP_CLOCK_H__

#include <swap.h>
/**
 * @brief The Clock (second chance) swap manager.
 *
 * swap.c uses swap_manager_fifo by default. To run the system with Clock, include this header in
 * swap.c and select this manager in swap_init().
 */
extern struct swap_manager swap_manager_clock;

#endif /* !__KERN_MM_SWAP_CLOCK_H__ */
