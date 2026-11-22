/**
 * @file   switch.S
 * @brief  switch_to(): the register-level context switch between two kernel threads (exercise).
 *
 * Contract of switch_to(struct context *from, struct context *to), called from proc_run() with
 * interrupts disabled:
 *  - save the callee-saved state of the running thread into *from: ra, sp and s0..s11, at the
 *    offsets of struct context in proc.h (word-sized, in that field order);
 *  - load the same fields from *to;
 *  - return with ret, which jumps to the restored ra. For a thread that has never run, ra is
 *    forkret (set by copy_thread), so the first "return" enters C code that then goes to
 *    forkrets() and __trapret.
 * Caller-saved registers need no saving: the compiler already spills them around the call to
 * switch_to(). The function must not decide what to run; that is schedule()'s job.
 */
#include <riscv.h>

//LAB4 function3 2414007
.text
.globl switch_to

/**
 * switch_to(struct context *from, struct context *to)
 *   a0 = from (where the running thread's registers are saved)
 *   a1 = to   (where the next thread's registers are loaded from)
 *
 * The slot offsets are n * REGBYTES and follow the field order of struct context in proc.h:
 * ra, sp, s0, s1, ..., s11. a0/a1 themselves are caller-saved, so they are not part of the context.
 */
switch_to:
    # save the registers of the thread that is being switched out into *from
    STORE ra,   0 * REGBYTES(a0)
    STORE sp,   1 * REGBYTES(a0)
    STORE s0,   2 * REGBYTES(a0)
    STORE s1,   3 * REGBYTES(a0)
    STORE s2,   4 * REGBYTES(a0)
    STORE s3,   5 * REGBYTES(a0)
    STORE s4,   6 * REGBYTES(a0)
    STORE s5,   7 * REGBYTES(a0)
    STORE s6,   8 * REGBYTES(a0)
    STORE s7,   9 * REGBYTES(a0)
    STORE s8,  10 * REGBYTES(a0)
    STORE s9,  11 * REGBYTES(a0)
    STORE s10, 12 * REGBYTES(a0)
    STORE s11, 13 * REGBYTES(a0)

    # restore the registers of the thread that is being switched in from *to
    LOAD ra,   0 * REGBYTES(a1)
    LOAD sp,   1 * REGBYTES(a1)
    LOAD s0,   2 * REGBYTES(a1)
    LOAD s1,   3 * REGBYTES(a1)
    LOAD s2,   4 * REGBYTES(a1)
    LOAD s3,   5 * REGBYTES(a1)
    LOAD s4,   6 * REGBYTES(a1)
    LOAD s5,   7 * REGBYTES(a1)
    LOAD s6,   8 * REGBYTES(a1)
    LOAD s7,   9 * REGBYTES(a1)
    LOAD s8,  10 * REGBYTES(a1)
    LOAD s9,  11 * REGBYTES(a1)
    LOAD s10, 12 * REGBYTES(a1)
    LOAD s11, 13 * REGBYTES(a1)

    # jump to the restored ra: the point where the new thread last called switch_to(), or forkret
    # for a thread that has never run
    ret

