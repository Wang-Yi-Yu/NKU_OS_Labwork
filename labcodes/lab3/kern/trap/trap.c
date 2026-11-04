/**
 * @file   trap.c
 * @brief  C part of trap handling: trap vector setup, frame printing and dispatch.
 *
 * Flow of one trap: hardware jumps to stvec (= __alltraps) -> SAVE_ALL builds a struct trapframe
 * -> trap() -> trap_dispatch() -> interrupt_handler() or exception_handler() -> back in assembly
 * RESTORE_ALL + sret. Bit 63 of scause tells the two kinds apart. interrupt_handler() and
 * exception_handler() are the lab3 exercise 2 task and go at the `lab3 function2` marker below.
 */
#include <assert.h>
#include <clock.h>
#include <console.h>
#include <defs.h>
#include <kdebug.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>
#include <sbi.h>
#include <stdio.h>
#include <trap.h>

/** @brief Number of timer interrupts between two "100 ticks" lines (100 ticks = 1 second at 10 ms per tick). */
#define TICK_NUM 100

/** @brief Number of "100 ticks" lines after which the kernel shuts down. */
#define SHUTDOWN_AFTER 10

/**
 * @brief Print the "100 ticks" line.
 *
 * If DEBUG_GRADE is defined it also prints "End of Test." and panics, which ends the run.
 */
static void print_ticks() {
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/**
 * @brief Set up the trap vector: all traps jump to __alltraps (Direct mode).
 *
 * sscratch is set to 0 to tell the trap entry that the kernel is running (S mode); stvec holds
 * the 4-byte aligned address of __alltraps, whose low two bits (mode) are 00 = Direct.
 */
void idt_init(void) {
    extern void __alltraps(void);
    /* Set sup0 scratch register to 0, indicating to exception vector
       that we are presently executing in the kernel */
    write_csr(sscratch, 0);
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps);
}

/**
 * @brief Test whether the trap happened in the kernel (sstatus.SPP == 1).
 * @param tf trap frame of the trap
 * @return true if the trap came from S mode
 */
bool trap_in_kernel(struct trapframe *tf) {
    return (tf->status & SSTATUS_SPP) != 0;
}

/**
 * @brief Print the trap frame address, all general-purpose registers and the four CSRs.
 *
 * This is also the fallback output for traps without a dedicated handler. The scause line has
 * the form "  cause    0x%08x".
 *
 * @param tf trap frame to print
 */
void print_trapframe(struct trapframe *tf) {
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);
    cprintf("  epc      0x%08x\n", tf->epc);
    cprintf("  badvaddr 0x%08x\n", tf->badvaddr);
    cprintf("  cause    0x%08x\n", tf->cause);
}

/**
 * @brief Print the 32 general-purpose registers, one per line, in the order of struct pushregs.
 * @param gpr register block of a trap frame
 */
void print_regs(struct pushregs *gpr) {
    cprintf("  zero     0x%08x\n", gpr->zero);
    cprintf("  ra       0x%08x\n", gpr->ra);
    cprintf("  sp       0x%08x\n", gpr->sp);
    cprintf("  gp       0x%08x\n", gpr->gp);
    cprintf("  tp       0x%08x\n", gpr->tp);
    cprintf("  t0       0x%08x\n", gpr->t0);
    cprintf("  t1       0x%08x\n", gpr->t1);
    cprintf("  t2       0x%08x\n", gpr->t2);
    cprintf("  s0       0x%08x\n", gpr->s0);
    cprintf("  s1       0x%08x\n", gpr->s1);
    cprintf("  a0       0x%08x\n", gpr->a0);
    cprintf("  a1       0x%08x\n", gpr->a1);
    cprintf("  a2       0x%08x\n", gpr->a2);
    cprintf("  a3       0x%08x\n", gpr->a3);
    cprintf("  a4       0x%08x\n", gpr->a4);
    cprintf("  a5       0x%08x\n", gpr->a5);
    cprintf("  a6       0x%08x\n", gpr->a6);
    cprintf("  a7       0x%08x\n", gpr->a7);
    cprintf("  s2       0x%08x\n", gpr->s2);
    cprintf("  s3       0x%08x\n", gpr->s3);
    cprintf("  s4       0x%08x\n", gpr->s4);
    cprintf("  s5       0x%08x\n", gpr->s5);
    cprintf("  s6       0x%08x\n", gpr->s6);
    cprintf("  s7       0x%08x\n", gpr->s7);
    cprintf("  s8       0x%08x\n", gpr->s8);
    cprintf("  s9       0x%08x\n", gpr->s9);
    cprintf("  s10      0x%08x\n", gpr->s10);
    cprintf("  s11      0x%08x\n", gpr->s11);
    cprintf("  t3       0x%08x\n", gpr->t3);
    cprintf("  t4       0x%08x\n", gpr->t4);
    cprintf("  t5       0x%08x\n", gpr->t5);
    cprintf("  t6       0x%08x\n", gpr->t6);
}

//lab3 function2 2411622

/**
 * @brief Handle an interrupt. The low bits of tf->cause select the source.
 *
 * A supervisor timer interrupt arms the next event, counts it, prints one line every TICK_NUM
 * ticks and shuts down after SHUTDOWN_AFTER such lines. Other known sources print one line.
 * The trap frame is left unchanged, so sret resumes the interrupted instruction.
 *
 * @param tf trap frame of the interrupt
 */
void interrupt_handler(struct trapframe *tf) {
    /* Bit 63 marks "interrupt". The source number is the rest, and a direct call may or may not have set the bit. */
    uintptr_t cause = tf->cause << 1 >> 1;

    switch (cause) {
    case IRQ_S_TIMER:
        clock_set_next_event();
        ticks++;
        if (ticks % TICK_NUM == 0) {
            print_ticks();
        }
        if (ticks / TICK_NUM == SHUTDOWN_AFTER) {
            sbi_shutdown();
        }
        break;
    case IRQ_U_SOFT:
        cprintf("User software interrupt\n");
        break;
    case IRQ_S_SOFT:
        cprintf("Supervisor software interrupt\n");
        break;
    case IRQ_H_SOFT:
        cprintf("Hypervisor software interrupt\n");
        break;
    case IRQ_M_SOFT:
        cprintf("Machine software interrupt\n");
        break;
    case IRQ_U_TIMER:
        cprintf("User timer interrupt\n");
        break;
    case IRQ_H_TIMER:
        cprintf("Hypervisor timer interrupt\n");
        break;
    case IRQ_M_TIMER:
        cprintf("Machine timer interrupt\n");
        break;
    case IRQ_U_EXT:
        cprintf("User external interrupt\n");
        break;
    case IRQ_S_EXT:
        cprintf("Supervisor external interrupt\n");
        break;
    case IRQ_H_EXT:
        cprintf("Hypervisor external interrupt\n");
        break;
    case IRQ_M_EXT:
        cprintf("Machine external interrupt\n");
        break;
    case IRQ_COP:
        cprintf("Coprocessor interrupt\n");
        break;
    case IRQ_HOST:
        cprintf("Host interrupt\n");
        break;
    default:
        cprintf("Unknown interrupt: cause %d\n", (int)cause);
        break;
    }
}

/**
 * @brief Handle an exception (scause bit 63 clear). Empty stub: owned by lab3 exercise 2-b.
 * @param tf trap frame of the exception
 */
void exception_handler(struct trapframe *tf) {
    if (tf->cause == CAUSE_ILLEGAL_INSTRUCTION || tf->cause == CAUSE_BREAKPOINT) {
        uint16_t instruction = *(uint16_t *)(uintptr_t)tf->epc;
        uintptr_t instruction_length = ((instruction & 0x3) == 0x3) ? 4 : 2;

        if (tf->cause == CAUSE_ILLEGAL_INSTRUCTION) {
            cprintf("Illegal instruction at 0x%08x\n", tf->epc);
        } else {
            cprintf("Breakpoint at 0x%08x\n", tf->epc);
        }
        tf->epc += instruction_length;
    } else {
        print_trapframe(tf);
    }
}


/**
 * @brief Route a trap to the interrupt or the exception handler.
 *
 * The top bit of scause is 1 for an interrupt, so the value is negative when read as signed.
 *
 * @param tf trap frame of the trap
 */
static inline void trap_dispatch(struct trapframe *tf) {
    if ((intptr_t)tf->cause < 0) {
        // interrupts
        interrupt_handler(tf);
    } else {
        // exceptions
        exception_handler(tf);
    }
}

/**
 * @brief Handle or dispatch an exception/interrupt.
 *
 * If and when trap() returns, the code in kern/trap/trapentry.S restores the CPU state saved in
 * the trap frame and returns from the trap with sret.
 *
 * @param tf trap frame built by SAVE_ALL
 */
void trap(struct trapframe *tf) {
    // dispatch based on what type of trap occurred
    trap_dispatch(tf);
}
