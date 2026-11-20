/**
 * @file   trapentry.S
 * @brief  Assembly trap entry and exit for both privilege levels: build a struct trapframe, call
 *         trap(), restore and sret.
 *
 * stvec points to __alltraps. SAVE_ALL pushes 36 words on the kernel stack in the layout of
 * struct trapframe (trap.h): x0..x31 at words 0..31, then sstatus, sepc, stval, scause at words
 * 32..35. RESTORE_ALL is its inverse. REGBYTES, STORE and LOAD come from riscv.h.
 *
 * Since lab5 a trap may come from user mode as well. The convention is: sscratch holds the kernel
 * stack pointer while a user program runs, and 0 while the CPU runs in the kernel. SAVE_ALL tells
 * the two cases apart by swapping sp and sscratch; RESTORE_ALL has to put the right value back
 * into sscratch before returning, depending on the mode that is returned to.
 *
 * forkrets is the exit path used by a brand-new thread or process (see forkret() in proc.c); it
 * reuses __trapret, so a new process leaves the kernel through the same restore/sret code. This is
 * also how kernel_execve() enters user mode for the first time.
 */
#include <riscv.h>

    .altmacro
    .align 2
    /*
     * SAVE_ALL - push the whole CPU context as a struct trapframe (provided by the lab framework).
     *
     * On entry sscratch is the kernel sp if the trap came from user mode and 0 if it came from the
     * kernel. The csrrw swaps sp and sscratch: a non-zero sp is then the kernel stack, otherwise
     * the kernel stack pointer is read back from sscratch. The sp of the interrupted code (the user
     * sp, or the kernel sp) ends up in the x2 slot of the frame, and sscratch is set to 0 so that a
     * nested trap inside the kernel is recognised as coming from the kernel.
     * Clobbers s0..s4 after they have been saved.
     */
    .macro SAVE_ALL
    LOCAL _restore_kernel_sp
    LOCAL _save_context

    # If coming from userspace, preserve the user stack pointer and load
    # the kernel stack pointer. If we came from the kernel, sscratch
    # will contain 0, and we should continue on the current stack.
    csrrw sp, sscratch, sp
    bnez sp, _save_context

_restore_kernel_sp:
    csrr sp, sscratch
_save_context:
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

    # get sr, epc, tval, cause
    # Set sscratch register to 0, so that if a recursive exception
    # occurs, the exception vector knows it came from the kernel
    # (s0 receives the old sscratch: the user sp, or the kernel sp, to be stored in the x2 slot)
    csrrw s0, sscratch, x0
    csrr s1, sstatus
    csrr s2, sepc
    csrr s3, 0x143    # stval
    csrr s4, scause

    STORE s0, 2*REGBYTES(sp)
    STORE s1, 32*REGBYTES(sp)
    STORE s2, 33*REGBYTES(sp)
    STORE s3, 34*REGBYTES(sp)
    STORE s4, 35*REGBYTES(sp)
    .endm

    /*
     * RESTORE_ALL - inverse of SAVE_ALL (lab3 exercise 1, extended in lab5).
     *
     * Contract: reload every register and the CSRs that must survive a trap from the trap frame
     * at sp (trap() may have changed epc), and leave sp equal to the sp of the interrupted code.
     * lab5 requirement: the version below is the kernel-only one carried over from lab3. It must be
     * extended so that it also returns to user mode: test sstatus.SPP (SSTATUS_SPP) of the frame;
     * if the trap came from user mode (SPP = 0) store the kernel stack top (sp + 36 * REGBYTES)
     * into sscratch before sp is replaced by the user sp, otherwise leave sscratch at 0.
     */
    .macro RESTORE_ALL

    #lab3 function1 2414007
    #lab5 function1 YOUR CODE update RESTORE_ALL: add the user/kernel distinction described above

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
__alltraps:
    SAVE_ALL

    #lab3 function1 2414007
    # The frame lives at the current sp, so sp is the first argument (a0) of trap(tf).
    move a0, sp
    jal trap
    # sp should be the same as before "jal trap"

    /*
     * __trapret - common exit of a trap: restore the context and return with sret
     * (pc = sepc, privilege = sstatus.SPP, SIE = SPIE). Falls through from __alltraps.
     */
    .globl __trapret
__trapret:
    RESTORE_ALL
    # return from supervisor call
    sret

    /*
     * forkrets - first exit of a newly created thread or process.
     * Called from C as forkrets(struct trapframe *tf), i.e. tf arrives in a0. It points sp at
     * the trap frame that copy_thread placed on the new kernel stack and jumps into __trapret,
     * so the process starts executing at tf->epc with the registers and status from the frame.
     * Whether it lands in kernel or in user mode is decided by sstatus.SPP in that frame.
     * It never returns to its caller.
     */
    .globl forkrets
forkrets:
    # set stack to this new process's trapframe
    move sp, a0
    j __trapret
