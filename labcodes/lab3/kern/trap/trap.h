/**
 * @file   trap.h
 * @brief  Trap frame layout and the C-side trap interface.
 *
 * A trap (interrupt or exception) enters the kernel at __alltraps in trapentry.S, which pushes a
 * struct trapframe on the kernel stack and calls trap(). The assembly macros SAVE_ALL/RESTORE_ALL
 * and the structs below must agree: x0..x31 are stored at offsets 0..31 words, followed by the
 * four CSRs status, epc, badvaddr, cause at words 32..35, 36 words in total.
 */
#ifndef __KERN_TRAP_TRAP_H__
#define __KERN_TRAP_TRAP_H__

#include <defs.h>

/**
 * @brief The 32 general-purpose registers x0..x31, in register-number order.
 *
 * Field order must match the STORE/LOAD offsets in trapentry.S (x<n> at n * REGBYTES). The
 * stored `sp` is the stack pointer at the time of the trap, not the trap frame address.
 */
struct pushregs {
    uintptr_t zero;  // Hard-wired zero
    uintptr_t ra;    // Return address
    uintptr_t sp;    // Stack pointer
    uintptr_t gp;    // Global pointer
    uintptr_t tp;    // Thread pointer
    uintptr_t t0;    // Temporary
    uintptr_t t1;    // Temporary
    uintptr_t t2;    // Temporary
    uintptr_t s0;    // Saved register/frame pointer
    uintptr_t s1;    // Saved register
    uintptr_t a0;    // Function argument/return value
    uintptr_t a1;    // Function argument/return value
    uintptr_t a2;    // Function argument
    uintptr_t a3;    // Function argument
    uintptr_t a4;    // Function argument
    uintptr_t a5;    // Function argument
    uintptr_t a6;    // Function argument
    uintptr_t a7;    // Function argument
    uintptr_t s2;    // Saved register
    uintptr_t s3;    // Saved register
    uintptr_t s4;    // Saved register
    uintptr_t s5;    // Saved register
    uintptr_t s6;    // Saved register
    uintptr_t s7;    // Saved register
    uintptr_t s8;    // Saved register
    uintptr_t s9;    // Saved register
    uintptr_t s10;   // Saved register
    uintptr_t s11;   // Saved register
    uintptr_t t3;    // Temporary
    uintptr_t t4;    // Temporary
    uintptr_t t5;    // Temporary
    uintptr_t t6;    // Temporary
};

/**
 * @brief Complete CPU state saved at trap time; passed to trap() by pointer.
 */
struct trapframe {
    struct pushregs gpr; /**< General-purpose registers. */
    uintptr_t status;    /**< sstatus: saved privilege and interrupt-enable bits. */
    uintptr_t
        epc; /**< sepc: address of the trapping (exception) or interrupted (interrupt) instruction. */
    uintptr_t
        badvaddr; /**< stval: extra trap information, e.g. faulting address or instruction bits. */
    uintptr_t cause; /**< scause: top bit set for an interrupt, the low bits give the cause code. */
};

/**
 * @brief C entry point of every trap, called from __alltraps with the saved trap frame.
 *
 * When it returns, __trapret restores the registers from the (possibly modified) frame and
 * executes sret.
 *
 * @param tf pointer to the trap frame on the kernel stack
 */
void trap(struct trapframe *tf);
/** @brief Install __alltraps as the S-mode trap entry (stvec). */
void idt_init(void);
/**
 * @brief Dump a trap frame (all registers and the four CSRs) to the console.
 * @param tf trap frame to print
 */
void print_trapframe(struct trapframe *tf);
/**
 * @brief Print the 32 general-purpose registers of a trap frame.
 * @param gpr register block to print
 */
void print_regs(struct pushregs *gpr);
/**
 * @brief Tell whether a trap came from S mode.
 * @param tf trap frame of the trap
 * @return true if sstatus.SPP is set (trapped from the kernel), false if it came from U mode
 */
bool trap_in_kernel(struct trapframe *tf);

#endif /* !__KERN_TRAP_TRAP_H__ */
