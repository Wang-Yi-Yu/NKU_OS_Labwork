/**
 * @file   kdebug.c
 * @brief  Kernel debugging helpers: kernel memory layout report and stack backtrace.
 *
 * print_kerninfo() is fully implemented and is called from kern_init() and from the
 * kernel monitor command `kerninfo`. print_debuginfo() and print_stackframe() are
 * placeholders inherited from the x86 ucore; they panic when called.
 */
#include <assert.h>
#include <defs.h>
#include <stdio.h>

/**
 * @brief Print the layout of the kernel image.
 *
 * Prints the virtual addresses of the kernel entry, the end of the text segment
 * (etext), the end of the data segment (edata) and the end of the image (end),
 * followed by the memory footprint of the kernel in KB (rounded up). The symbols
 * come from tools/kernel.ld.
 */
void print_kerninfo(void) {
    extern char etext[], edata[], end[], kern_init[];
    cprintf("Special kernel symbols:\n");
    cprintf("  entry  0x%016lx (virtual)\n", kern_init);
    cprintf("  etext  0x%016lx (virtual)\n", etext);
    cprintf("  edata  0x%016lx (virtual)\n", edata);
    cprintf("  end    0x%016lx (virtual)\n", end);
    cprintf("Kernel executable memory footprint: %dKB\n", (end - kern_init + 1023) / 1024);
}

/**
 * @brief Print the symbol information of an instruction address.
 *
 * @param eip address to look up; the original x86 version reads the STABS table (see stab.h)
 * @warning Not implemented: it panics with "Not Implemented!".
 */
void print_debuginfo(uintptr_t eip) {
    panic("Not Implemented!");
}

/**
 * @brief Print the chain of saved return addresses that led to the current point of execution.
 *
 * The x86 ucore walks the chain of saved %ebp values: each stack frame stores the previous
 * frame's base pointer and the return address, so following the chain recovers the nested
 * sequence of calls (the chain ends where the boot code set %ebp to zero). On RISC-V the
 * same idea works with the frame pointer s0/fp, but this lab does not use it.
 *
 * @warning Not implemented: it panics with "Not Implemented!".
 */
void print_stackframe(void) {
    panic("Not Implemented!");
}
