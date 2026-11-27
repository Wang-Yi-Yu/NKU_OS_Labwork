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
#include <swap.h>
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
 *  8. ide_init(), swap_init() (lab9) and fs_init(): the disk devices, the swap manager (swap_init()
 *     also runs the check_swap() self-test) and the file system;
 *  9. clock_init() and intr_enable(): timer interrupts on;
 * 10. cpu_idle(): this context becomes idleproc and never returns.
 *
 * @return never returns (declared noreturn)
 * @note Runs on the boot stack with interrupts disabled until intr_enable() in step 9.
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

    ide_init();   // init ide devices
    swap_init();  // init swap
    fs_init();

    clock_init();   // init clock interrupt
    intr_enable();  // enable irq interrupt

    cpu_idle();  // run idle process
}

/**
 * @brief Innermost frame of the backtrace test: print the call stack with mon_backtrace().
 * @param arg0 dummy argument (the first argument of grade_backtrace1())
 * @param arg1 address of the first argument of grade_backtrace1()
 * @param arg2 dummy argument (the second argument of grade_backtrace1())
 * @param arg3 address of the second argument of grade_backtrace1(); together with arg1 it makes the
 *             arguments of the callers visible in the printed stack frames
 * @note Left over from lab1. grade_backtrace() is not called by kern_init() (the call is commented
 *       out), so none of the grade_backtrace* functions run.
 */
void __attribute__((noinline)) grade_backtrace2(int arg0, int arg1, int arg2, int arg3) {
    mon_backtrace(0, NULL, NULL);
}

/**
 * @brief Second frame of the backtrace test: call grade_backtrace2() with the addresses of its own arguments.
 * @param arg0 dummy argument
 * @param arg1 dummy argument
 */
void __attribute__((noinline)) grade_backtrace1(int arg0, int arg1) {
    grade_backtrace2(arg0, (int64_t)&arg0, arg1, (int64_t)&arg1);
}

/**
 * @brief Third frame of the backtrace test: call grade_backtrace1().
 * @param arg0 dummy argument
 * @param arg1 unused
 * @param arg2 dummy argument, passed on as the second argument of grade_backtrace1()
 */
void __attribute__((noinline)) grade_backtrace0(int arg0, int arg1, int arg2) {
    grade_backtrace1(arg0, arg2);
}

/**
 * @brief Entry of the backtrace test (lab1): start a chain of nested calls that ends in mon_backtrace().
 */
void grade_backtrace(void) {
    grade_backtrace0(0, (int64_t)kern_init, 0xffff0000);
}
