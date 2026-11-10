/**
 * @file   init.c
 * @brief  Kernel main: initialises every subsystem in order and finally becomes the idle thread.
 *
 * kern_init() is called from entry.S after Sv39 paging is on. It never returns: its last call,
 * cpu_idle(), loops forever as idleproc.
 */
#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <kdebug.h>
#include <picirq.h>
#include <trap.h>
#include <clock.h>
#include <intr.h>
#include <pmm.h>
#include <vmm.h>
#include <proc.h>
#include <kmonitor.h>
#include <dtb.h>

/**
 * @brief Kernel entry after the boot assembly: initialise all subsystems and run idleproc.
 *
 * Order matters:
 *  1. clear .bss (edata..end) and parse the device tree (dtb_init);
 *  2. console, then print the kernel info;
 *  3. pmm_init(): physical memory and page allocator;
 *  4. pic_init() and idt_init(): interrupt controller and trap vector (stvec);
 *  5. vmm_init(): virtual memory manager;
 *  6. proc_init(): create idleproc (the current context) and initproc;
 *  7. clock_init() and intr_enable(): timer interrupts on;
 *  8. cpu_idle(): this context becomes idleproc and never returns.
 *
 * @return never returns (declared noreturn)
 * @note Runs on the boot stack with interrupts disabled until step 7.
 */
int kern_init(void) __attribute__((noreturn));
void grade_backtrace(void);

int kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);
    dtb_init();
    cons_init();  // init the console

    const char *message = "(THU.CST) os is loading ...";
    cprintf("%s\n\n", message);

    print_kerninfo();

    // grade_backtrace();

    pmm_init();  // init physical memory management

    pic_init();  // init interrupt controller
    idt_init();  // init interrupt descriptor table

    vmm_init();   // init virtual memory management
    proc_init();  // init process table

    clock_init();   // init clock interrupt
    intr_enable();  // enable irq interrupt

    cpu_idle();  // run idle process
}

/**
 * @brief Leftover from lab1: count how many times it was called (static round counter).
 * @note Not called anywhere in lab4. Kept as it was in the template.
 */
static void lab1_print_cur_status(void) {
    static int round = 0;
    round++;
}
