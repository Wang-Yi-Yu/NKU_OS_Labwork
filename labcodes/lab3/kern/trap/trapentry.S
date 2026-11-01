/**
 * @file   trapentry.S
 * @brief  Assembly trap entry and exit: build a struct trapframe, call trap(), restore and sret.
 *
 * stvec points to __alltraps. SAVE_ALL pushes 36 words on the current stack in the layout of
 * struct trapframe (trap.h): x0..x31 at words 0..31, then sstatus, sepc, stval, scause at words
 * 32..35. RESTORE_ALL is its inverse. REGBYTES, STORE and LOAD come from riscv.h.
 */
#include <riscv.h>

    /*
     * SAVE_ALL - push the whole CPU context as a struct trapframe.
     *
     * The original sp is parked in sscratch before sp is lowered, because the x2 slot has to
     * hold the sp of the interrupted code, not the address of the frame; that is why x2 is not
     * stored with the other registers but written at the end from sscratch.
     * Clobbers s0..s4 after they have been saved.
     */
    .macro SAVE_ALL

    csrw sscratch, sp

    addi sp, sp, -36 * REGBYTES
    # save x registers
    STORE x0, 0*REGBYTES(sp)
    STORE x1, 1*REGBYTES(sp)
    STORE x3, 3*REGBYTES(sp)
    STORE x4, 4*REGBYTES(sp)
    STORE x5, 5*REGBYTES(sp)
    STORE x6, 6*REGBYTES(sp)
    STORE x7, 7*REGBYTES(sp)
    STORE x8, 8*REGBYTES(sp)
    STORE x9, 9*REGBYTES(sp)
    STORE x10, 10*REGBYTES(sp)
    STORE x11, 11*REGBYTES(sp)
    STORE x12, 12*REGBYTES(sp)
    STORE x13, 13*REGBYTES(sp)
    STORE x14, 14*REGBYTES(sp)
    STORE x15, 15*REGBYTES(sp)
    STORE x16, 16*REGBYTES(sp)
    STORE x17, 17*REGBYTES(sp)
    STORE x18, 18*REGBYTES(sp)
    STORE x19, 19*REGBYTES(sp)
    STORE x20, 20*REGBYTES(sp)
    STORE x21, 21*REGBYTES(sp)
    STORE x22, 22*REGBYTES(sp)
    STORE x23, 23*REGBYTES(sp)
    STORE x24, 24*REGBYTES(sp)
    STORE x25, 25*REGBYTES(sp)
    STORE x26, 26*REGBYTES(sp)
    STORE x27, 27*REGBYTES(sp)
    STORE x28, 28*REGBYTES(sp)
    STORE x29, 29*REGBYTES(sp)
    STORE x30, 30*REGBYTES(sp)
    STORE x31, 31*REGBYTES(sp)

    # get sr, epc, badvaddr, cause
    # Set sscratch register to 0, so that if a recursive exception
    # occurs, the exception vector knows it came from the kernel
    csrrw s0, sscratch, x0
    csrr s1, sstatus
    csrr s2, sepc
    csrr s3, stval
    csrr s4, scause

    STORE s0, 2*REGBYTES(sp)
    STORE s1, 32*REGBYTES(sp)
    STORE s2, 33*REGBYTES(sp)
    STORE s3, 34*REGBYTES(sp)
    STORE s4, 35*REGBYTES(sp)
    .endm

    /*
     * RESTORE_ALL - inverse of SAVE_ALL (lab3 exercise 1).
     *
     * Contract: reload every register and the CSRs that must survive a trap from the trap frame
     * at sp (trap() may have changed epc), and leave sp equal to the sp of the interrupted code.
     * The frame layout and the special role of the x2 slot are described at SAVE_ALL.
     */
    .macro RESTORE_ALL

    #lab3 function1 2414007

    # Restore the CSRs first: they have to be staged through s1/s2, which are overwritten by
    # the register reloads below. sepc is reloaded because trap() may have advanced it.
    LOAD s1, 32*REGBYTES(sp)
    LOAD s2, 33*REGBYTES(sp)
    csrw sstatus, s1
    csrw sepc, s2

    # x0 is hard-wired to zero and x2 (sp) is handled last, so both are skipped here.
    LOAD x1, 1*REGBYTES(sp)
    LOAD x3, 3*REGBYTES(sp)
    LOAD x4, 4*REGBYTES(sp)
    LOAD x5, 5*REGBYTES(sp)
    LOAD x6, 6*REGBYTES(sp)
    LOAD x7, 7*REGBYTES(sp)
    LOAD x8, 8*REGBYTES(sp)
    LOAD x9, 9*REGBYTES(sp)
    LOAD x10, 10*REGBYTES(sp)
    LOAD x11, 11*REGBYTES(sp)
    LOAD x12, 12*REGBYTES(sp)
    LOAD x13, 13*REGBYTES(sp)
    LOAD x14, 14*REGBYTES(sp)
    LOAD x15, 15*REGBYTES(sp)
    LOAD x16, 16*REGBYTES(sp)
    LOAD x17, 17*REGBYTES(sp)
    LOAD x18, 18*REGBYTES(sp)
    LOAD x19, 19*REGBYTES(sp)
    LOAD x20, 20*REGBYTES(sp)
    LOAD x21, 21*REGBYTES(sp)
    LOAD x22, 22*REGBYTES(sp)
    LOAD x23, 23*REGBYTES(sp)
    LOAD x24, 24*REGBYTES(sp)
    LOAD x25, 25*REGBYTES(sp)
    LOAD x26, 26*REGBYTES(sp)
    LOAD x27, 27*REGBYTES(sp)
    LOAD x28, 28*REGBYTES(sp)
    LOAD x29, 29*REGBYTES(sp)
    LOAD x30, 30*REGBYTES(sp)
    LOAD x31, 31*REGBYTES(sp)

    # The x2 slot holds the sp of the interrupted code, not the frame address. Every load above
    # is addressed through sp, so x2 must be the very last register to be reloaded.
    LOAD x2, 2*REGBYTES(sp)

    .endm

    /*
     * __alltraps - trap entry installed in stvec; 4-byte aligned as stvec requires.
     * Saves the context, then calls trap(struct trapframe *tf) with the address of the frame
     * (lab3 exercise 1); trap() returns into __trapret below.
     */
    .globl __alltraps
    .align(2)
__alltraps:
    SAVE_ALL

    #lab3 function1 2414007
    # The frame lives at the current sp, so sp is the first argument (a0) of trap(tf).
    move a0, sp
    jal trap
    # sp should be the same as before "jal trap"; execution falls through into __trapret.

    /*
     * __trapret - common exit of a trap: restore the context and return with sret
     * (pc = sepc, privilege = sstatus.SPP, SIE = SPIE).
     */
    .globl __trapret
__trapret:
    RESTORE_ALL
    # return from supervisor call
    sret
