/**
 * @file   init.c
 * @brief  C entry point of the kernel: kern_init() and the boot-time kernel info printout.
 */
#include <console.h>
#include <defs.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>
#include <dtb.h>
#include <slub.h>

int kern_init(void) __attribute__((noreturn));
/** @brief Backtrace helper for lab1 grading; declared but not implemented in this lab. */
void grade_backtrace(void);
void print_kerninfo(void);

/**
 * @brief Print information about the kernel image.
 *
 * Prints the location of the kernel entry, the end addresses of the text and
 * data segments, the end of the kernel image (`end`, where free memory
 * starts) and how much memory the kernel image occupies. All addresses are
 * virtual addresses.
 */
void print_kerninfo(void) {
    extern char etext[], edata[], end[];
    cprintf("Special kernel symbols:\n");
    cprintf("  entry  0x%016lx (virtual)\n", (uintptr_t)kern_init);
    cprintf("  etext  0x%016lx (virtual)\n", etext);
    cprintf("  edata  0x%016lx (virtual)\n", edata);
    cprintf("  end    0x%016lx (virtual)\n", end);
    cprintf("Kernel executable memory footprint: %dKB\n", (end - (char *)kern_init + 1023) / 1024);
}

/**
 * @brief C entry point of the kernel, jumped to from kern_entry in entry.S once
 *        paging is enabled.
 *
 * Clears the BSS segment, reads the physical memory layout from the device tree
 * (dtb_init), initialises the console, prints the kernel info, initialises
 * physical memory management (pmm_init), runs the SLUB allocator self test
 * (slub_init, slub_check) and then idles forever.
 *
 * @note The BSS must be cleared before dtb_init(), because dtb_init() stores its
 *       result in BSS variables.
 * @return never returns
 */
int kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);
    dtb_init();
    cons_init();  // init the console
    const char *message = "(THU.CST) os is loading ...\0";
    //cprintf("%s\n\n", message);
    cputs(message);

    print_kerninfo();

    // grade_backtrace();
    pmm_init();  // init physical memory management
    slub_init();
    slub_check();

    /* do nothing */
    while (1)
        ;
}
