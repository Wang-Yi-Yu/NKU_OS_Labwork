/**
 * @file   kdebug.h
 * @brief  Declarations of the kernel debugging helpers implemented in kdebug.c.
 */

#ifndef __KERN_DEBUG_KDEBUG_H__
#define __KERN_DEBUG_KDEBUG_H__

#include <defs.h>
#include <trap.h>

/** @brief Print the addresses of the kernel entry, etext, edata and end, and the kernel memory footprint. */
void print_kerninfo(void);
/** @brief Print a stack backtrace. Not implemented in lab4: it panics. */
void print_stackframe(void);

#endif /* !__KERN_DEBUG_KDEBUG_H__ */
