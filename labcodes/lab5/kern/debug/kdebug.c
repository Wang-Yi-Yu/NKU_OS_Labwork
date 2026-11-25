/**
 * @file   kdebug.c
 * @brief  Kernel debugging helpers: kernel layout printout and the (unimplemented) stack backtrace.
 */
#include <assert.h>
#include <defs.h>
#include <stdio.h>

/**
 * @brief Print the key kernel symbol addresses and the kernel's memory footprint.
 *
 * Prints the virtual addresses of kern_init (entry), etext, edata and end (linker symbols
 * defined in kernel.ld), then the footprint in KB computed as (end - kern_init) rounded up.
 * The footprint is measured from kern_init, not from the start of the kernel image.
 *
 * @note Uses cprintf, so output goes to the console. Takes no arguments and returns nothing.
 */
void print_kerninfo(void) {
    extern char etext[], edata[], end[], kern_init[];
    cprintf("Special kernel symbols:\n");
    cprintf("  entry  0x%08x (virtual)\n", kern_init);
    cprintf("  etext  0x%08x (virtual)\n", etext);
    cprintf("  edata  0x%08x (virtual)\n", edata);
    cprintf("  end    0x%08x (virtual)\n", end);
    cprintf("Kernel executable memory footprint: %dKB\n", (end - kern_init + 1023) / 1024);
}

/**
 * @brief Print the source file, line number and function name that belong to an instruction address.
 *
 * Skeleton of the early labs (it needs the stabs debug tables described in stab.h). It is not
 * implemented: it only calls panic("Not Implemented!"), so calling it halts the kernel. Nothing in
 * lab5 calls it directly; print_stackframe() would be its caller.
 *
 * @param eip instruction address to describe
 */
void print_debuginfo(uintptr_t eip) {
    panic("Not Implemented!");
}

/**
 * @brief Print the chain of saved return addresses that led to the current point of execution.
 *
 * The classic implementation walks the frame-pointer chain: every function prologue pushes the
 * caller's frame pointer, so following the saved pointers from the current one up to the boundary
 * (a frame pointer of zero) yields the nested call sequence, and print_debuginfo() turns each saved
 * address into a file, line and function name. The kmonitor "backtrace" command calls this
 * function.
 *
 * @note Not implemented in the skeleton: it only calls panic("Not Implemented!"), so calling it
 *       halts the kernel. The original description in the skeleton was written for x86 (ebp/esp);
 *       RISC-V would use s0/fp instead.
 */
void print_stackframe(void) {
    panic("Not Implemented!");
}
