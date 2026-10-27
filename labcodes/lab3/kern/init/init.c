/**
 * @file   init.c
 * @brief  C entry point of the kernel: kern_init() and the grading backtrace helpers.
 */
#include <clock.h>
#include <console.h>
#include <defs.h>
#include <intr.h>
#include <kdebug.h>
#include <kmonitor.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>
#include <trap.h>
#include <dtb.h>

int kern_init(void) __attribute__((noreturn));
/** @brief Backtrace helper for lab1 grading; kept for reference, its call in kern_init() is commented out. */
void grade_backtrace(void);

/**
 * @brief C entry point of the kernel, jumped to from kern_entry in entry.S once paging is enabled.
 *
 * Boot order: clear the BSS, read the physical memory layout from the device tree, initialise
 * the console and print the kernel info, install the trap vector, initialise physical memory
 * management, install the trap vector once more, start the timer, enable interrupts and then
 * idle forever; from this point on the only activity is trap handling.
 *
 * @note The BSS must be cleared before dtb_init(), because dtb_init() stores its result in BSS
 *       variables.
 * @note tools/inject.py (used by make grade) searches this function for the statements that call the
 *       idt, clock and interrupt-enable initialisers and inserts the test hooks after them, so keep
 *       those statements on their own lines.
 * @return never returns
 */
int kern_init(void) {
    extern char edata[], end[];
    // 先清零 BSS，再读取并保存 DTB 的内存信息，避免被清零覆盖（为了解释变化 正式上传时我觉得应该删去这句话）
    memset(edata, 0, end - edata);
    dtb_init();
    cons_init();  // init the console
    const char *message = "(THU.CST) os is loading ...\0";
    //cprintf("%s\n\n", message);
    cputs(message);

    print_kerninfo();

    // grade_backtrace();
    idt_init();  // init interrupt descriptor table

    pmm_init();  // init physical memory management

    idt_init();  // init interrupt descriptor table

    clock_init();   // init clock interrupt
    intr_enable();  // enable irq interrupt

    /* do nothing */
    while (1)
        ;
}

/** @brief Innermost frame of the grading backtrace chain: calls mon_backtrace(). */
void __attribute__((noinline)) grade_backtrace2(int arg0, int arg1, int arg2, int arg3) {
    mon_backtrace(0, NULL, NULL);
}

/** @brief Second frame of the grading backtrace chain. */
void __attribute__((noinline)) grade_backtrace1(int arg0, int arg1) {
    grade_backtrace2(arg0, (uintptr_t)&arg0, arg1, (uintptr_t)&arg1);
}

/** @brief Third frame of the grading backtrace chain. */
void __attribute__((noinline)) grade_backtrace0(int arg0, int arg1, int arg2) {
    grade_backtrace1(arg0, arg2);
}

/** @brief Start of the grading backtrace chain; pass kern_init as one argument to check the arguments are visible. */
void grade_backtrace(void) {
    grade_backtrace0(0, (uintptr_t)kern_init, 0xffff0000);
}
