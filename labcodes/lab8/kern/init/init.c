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
#include <ide.h>
#include <proc.h>
#include <sched.h>
#include <kmonitor.h>
#include <dtb.h>
#include <fs.h>

/**
 * @brief Kernel entry after the boot assembly: initialise all subsystems and run idleproc.
 *
 * Order matters:
 *  1. clear .bss (edata..end);
 *  2. console, then print the kernel info and parse the device tree (dtb_init);
 *  3. pmm_init(): physical memory and page allocator;
 *  4. pic_init() and idt_init(): interrupt controller and trap vector (stvec);
 *  5. vmm_init(): virtual memory manager;
 *  6. sched_init() (lab6): bind the scheduling class and initialise the run queue; it must come
 *     before proc_init(), because creating a process makes it runnable and enqueues it;
 *  7. proc_init(): create idleproc (the current context) and initproc;
 *  8. clock_init() and intr_enable(): timer interrupts on;
 *  9. cpu_idle(): this context becomes idleproc and never returns.
 *
 * @return never returns (declared noreturn)
 * @note Runs on the boot stack with interrupts disabled until step 7.
 */
int kern_init(void) __attribute__((noreturn));
void grade_backtrace(void);

int kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);
    cons_init();  // init the console

    const char *message = "(THU.CST) os is loading ...";
    cprintf("%s\n\n", message);

    print_kerninfo();

    // grade_backtrace();

    dtb_init();  // init dtb

    pmm_init();  // init physical memory management

    pic_init();  // init interrupt controller
    idt_init();  // init interrupt descriptor table

    vmm_init();    // init virtual memory management
    sched_init();  // init the scheduler (lab6)
    proc_init();   // init process table

    ide_init();  // init ide devices
    fs_init();

    clock_init();   // init clock interrupt
    intr_enable();  // enable irq interrupt

    cpu_idle();  // run idle process
}

/**
 * @brief Leftover from lab1: count how many times it was called (static round counter).
 * @note Not called anywhere. Kept as it was in the template.
 */
static void lab1_print_cur_status(void) {
    static int round = 0;
    round++;
}
