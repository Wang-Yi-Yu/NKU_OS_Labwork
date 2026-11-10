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
//LAB4 function3 YOUR CODE
