/**
 * @file   dtb.h
 * @brief  Device tree (FDT/DTB) parsing interface, used to obtain the physical memory base and size.
 *
 * At boot, OpenSBI passes the physical address of the device tree in register
 * a1; entry.S saves it to boot_dtb. dtb_init parses that device tree, and the
 * result is queried through get_memory_base and get_memory_size.
 */

#ifndef __KERN_DRIVER_DTB_H__
#define __KERN_DRIVER_DTB_H__

#include <defs.h>

/**
 * @brief Boot hardware thread (hart) ID.
 *
 * Set by entry.S on entry from register a0.
 */
extern uint64_t boot_hartid;

/**
 * @brief Physical address of the device tree (DTB).
 *
 * Set by entry.S on entry from register a1. This is a physical address; add
 * PHYSICAL_MEMORY_OFFSET to obtain a kernel virtual address before accessing it.
 * A value of 0 means that no device tree was provided.
 */
extern uint64_t boot_dtb;

/**
 * @brief Parse the device tree, extract and store the base and size of the first physical memory range, and print the results.
 *
 * Processing steps: check whether boot_dtb is 0; map it to a virtual address
 * and verify the FDT magic 0xd00dfeed; walk the structure block looking for the
 * reg property of a memory node. On success the results are written to
 * module-private static variables, which get_memory_base and get_memory_size
 * return.
 *
 * @note Relies on the linear mapping of physical memory at PHYSICAL_MEMORY_OFFSET
 *       set up by the boot page table in entry.S. Must be called from kern_init,
 *       before memory management is initialised.
 * @warning On failure (DTB address is 0, bad magic, or no memory node found), the static
 *          variables remain 0 and an error is printed to the console; the function does not halt the kernel.
 * @see get_memory_base, get_memory_size
 */
void dtb_init(void);

/**
 * @brief Get the physical memory base address obtained by dtb_init.
 *
 * @return The starting physical address of memory; 0 if dtb_init has not been called or parsing failed.
 * @see dtb_init
 */
uint64_t get_memory_base(void);

/**
 * @brief Get the physical memory size obtained by dtb_init.
 *
 * @return The memory size in bytes; 0 if dtb_init has not been called or parsing failed.
 * @note Only the first reg range of the first memory node in the device tree is
 *       reflected; any other memory ranges are not counted.
 * @see dtb_init
 */
uint64_t get_memory_size(void);

#endif /* !__KERN_DRIVER_DTB_H__ */
