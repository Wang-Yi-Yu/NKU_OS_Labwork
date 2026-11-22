/**
 * @file   trap.c
 * @brief  C part of trap handling for kernel and user mode: trap vector setup, frame printing,
 *         dispatch, interrupt and exception handlers and the return path of user traps.
 *
 * Flow of one trap: hardware jumps to stvec (= __alltraps) -> SAVE_ALL builds a struct trapframe
 * -> trap() -> trap_dispatch() -> interrupt_handler() or exception_handler() -> back in assembly
 * RESTORE_ALL + sret. Bit 63 of scause tells interrupts and exceptions apart. interrupt_handler()
 * and exception_handler() are the lab3 exercise 2 task, each with its own `lab3 function2` marker
 * below. lab5 extends exception_handler() with the system call (ecall from U mode) path, and
 * trap() with the checks that run before a trap from user mode returns (kill, reschedule). lab6
 * changes the timer branch of interrupt_handler() (marker `lab6 function1-2`): the tick is handed
 * to the scheduling class through sched_class_proc_tick(), which is what makes time slices run
 * out. lab7 routes the tick through run_timer_list() instead, which also wakes up the processes
 * whose sleep timers have expired (do_sleep()) and then makes the same call to the scheduling
 * class. The first run of a new process does not come through trap() but through forkrets() in
 * trapentry.S.
 */
#include <defs.h>
#include <mmu.h>
#include <memlayout.h>
#include <clock.h>
#include <trap.h>
#include <riscv.h>
#include <stdio.h>
#include <assert.h>
#include <console.h>
#include <vmm.h>
#include <kdebug.h>
#include <unistd.h>
#include <syscall.h>
#include <error.h>
#include <sched.h>
#include <proc.h>
#include <sync.h>
#include <sbi.h>

/**
 * @brief Number of timer interrupts between two tick lines printed by print_ticks().
 * @note The value comes from the lab7 framework. Nothing calls print_ticks() any more (the timer
 *       branch of interrupt_handler() uses run_timer_list()), so it has no effect.
 */
#define TICK_NUM 2

/**
 * @brief Print the "<TICK_NUM> ticks" line (not called in lab7).
 *
 * If DEBUG_GRADE is defined it also prints "End of Test." and panics, which ends the run. For
 * that reason it must stay uncalled: the lab7 grading run builds with DEBUG_GRADE and needs
 * the kernel to keep running for the whole test.
 */
static void print_ticks() {
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/**
 * @brief Set up the trap vector and let the kernel touch user memory.
 *
 * sscratch is set to 0, which tells the trap entry that the CPU is currently in the kernel (while
 * a user program runs, sscratch holds the kernel stack pointer, see trapentry.S). stvec is set to
 * __alltraps (Direct mode, 4-byte aligned). sstatus.SUM is set so that S-mode code may read and
 * write U-mode pages, which system calls need for their pointer arguments.
 */
void idt_init(void) {
    extern void __alltraps(void);
    /* Set sscratch register to 0, indicating to exception vector that we are
     * presently executing in the kernel */
    write_csr(sscratch, 0);
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps);
    /* Allow kernel to access user memory */
    set_csr(sstatus, SSTATUS_SUM);
}

/**
 * @brief Tell whether a trap was taken from kernel mode (provided by the lab framework).
 * @param tf trap frame of the trap
 * @return true if sstatus.SPP is set in the frame (the trap came from S mode), false for U mode
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
    // cprintf("trapframe at 0x%x\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);
    cprintf("  epc      0x%08x\n", tf->epc);
    cprintf("  tval 0x%08x\n", tf->tval);
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

/**
 * @brief Pointer to the mm_struct used by the VMM self-test (vmm.h).
 * @note Declared only: this file never defines or dereferences it, so it is a harmless leftover
 *       from the VMM test code.
 */
extern struct mm_struct *check_mm_struct;

//lab3 function2 2414015

/**
 * @brief Handle an interrupt. The low bits of tf->cause select the source.
 *
 * A supervisor timer interrupt arms the next event, counts it and, every TICK_NUM ticks, prints one
 * line and (lab5) sets need_resched of the current process so that it is preempted when the trap
 * returns. That is the baseline that lab6 updated: on every tick (not only every TICK_NUM
 * ticks) a valid current process is passed to sched_class_proc_tick(current), so that the
 * scheduling class decrements its time slice and sets need_resched when the slice is used up;
 * the handler does not hard-code any algorithm. The tick line is no longer printed: with
 * DEBUG_GRADE print_ticks() panics, which would end every test run after the first ticks.
 * lab7 changes the same branch once more: it calls run_timer_list() on every tick, which wakes up
 * every process whose do_sleep() timer has expired (this is what lets the philosophers and the
 * sleep tests wake up) and then calls sched_class_proc_tick(current) itself. Other known sources
 * print one line.
 * The trap frame is left unchanged, so sret resumes the interrupted instruction.
 *
 * @param tf trap frame of the interrupt
 */
void interrupt_handler(struct trapframe *tf) {
    /* Bit 63 marks "interrupt". The source number is the rest, and a direct call may or may not have set the bit. */
    uintptr_t cause = tf->cause << 1 >> 1;

    switch (cause) {
        case IRQ_S_TIMER:
            //lab6 function1-2 2414015 update interrupt_handler
            clock_set_next_event();
            ticks++;
            // lab7: run_timer_list() handles the sleep timers and then ticks the scheduling class.
            if (current != NULL) {
                run_timer_list();
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

//lab3 function2 2411622
//lab5 function1 2414007 update exception_handler

/**
 * @brief Handle an exception (scause bit 63 clear).
 *
 * An illegal instruction or a breakpoint prints one line with the address of the faulting
 * instruction and advances tf->epc past it, so that sret resumes at the next instruction instead
 * of re-executing the one that trapped. The length is read from the instruction itself: the low
 * two bits are 0b11 for a 32-bit instruction, anything else is a 16-bit compressed one (for
 * example c.ebreak).
 *
 * An ecall from user mode (CAUSE_USER_ECALL, lab5) is a system call request. The saved epc points
 * at the ecall instruction itself, so it is advanced by 4 bytes first (ecall is always a 32-bit
 * instruction); otherwise sret would execute the same ecall again and the call would repeat
 * forever. The request is then handed to syscall(), which reads the number and the arguments from
 * the saved registers of current->tf and writes the result into a0 of that frame.
 *
 * Any other exception (for example a page fault, which this lab cannot repair) is dumped with
 * print_trapframe(). If it happened in kernel mode the kernel panics; if it happened in user mode
 * the process is terminated with do_exit(-E_KILLED). Merely returning would re-execute the
 * faulting instruction forever.
 *
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
    } else if (tf->cause == CAUSE_USER_ECALL) {
        // Skip the 4-byte ecall so that sret resumes after it, then run the system call.
        tf->epc += 4;
        syscall();
    } else {
        // Any other exception cannot be repaired here (there is no page fault handler), and
        // returning would re-execute the faulting instruction forever. Kernel mode: fatal.
        // User mode: terminate the offending process.
        print_trapframe(tf);
        if (trap_in_kernel(tf)) {
            panic("unhandled exception in kernel mode");
        }
        cprintf("killed by kernel.\n");
        do_exit(-E_KILLED);
    }
}

/**
 * @brief Route a trap to interrupt_handler() or exception_handler() by the top bit of scause.
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
 * @brief Handle or dispatch an exception/interrupt (provided by the lab framework).
 *
 * Before the first process exists (current == NULL) the trap is only dispatched. Afterwards
 * current->tf is pointed at the new frame for the duration of the trap (system calls and
 * do_fork() read it) and restored afterwards. For a trap that came from user mode the function
 * additionally runs two checks before returning to the user program: a process that was marked
 * PF_EXITING (do_kill) exits with -E_KILLED, and a process with need_resched set (timer tick,
 * yield) calls schedule(). If and when trap() returns, kern/trap/trapentry.S restores the CPU
 * state from the trap frame and returns with sret.
 *
 * @param tf trap frame built by SAVE_ALL
 */
void trap(struct trapframe *tf) {
    // dispatch based on what type of trap occurred
    //    cputs("some trap");
    if (current == NULL) {
        trap_dispatch(tf);
    } else {
        struct trapframe *otf = current->tf;
        current->tf = tf;

        bool in_kernel = trap_in_kernel(tf);

        trap_dispatch(tf);

        current->tf = otf;
        if (!in_kernel) {
            if (current->flags & PF_EXITING) {
                do_exit(-E_KILLED);
            }
            if (current->need_resched) {
                schedule();
            }
        }
    }
}
