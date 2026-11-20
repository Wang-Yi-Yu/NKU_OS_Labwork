/**
 * @file   entry.S
 * @brief  First code executed by every kernel thread created with kernel_thread().
 *
 * A new kernel thread never returns from switch_to() into C code directly. It reaches this
 * entry through forkrets/__trapret: kernel_thread() puts this address in the trap frame's epc
 * and the thread's sret jumps here. By convention the function pointer is in s0 and its argument
 * in s1 of that frame, so the entry needs no stack frame of its own.
 *
 * kernel_thread_entry: a0 := s1 (arg), call s0 (fn), then do_exit(fn's return value).
 * do_exit() never returns: it turns the thread into a zombie and calls schedule(); see do_exit()
 * in proc.c (lab5 exercise).
 */
.text
.globl kernel_thread_entry
kernel_thread_entry:        # void kernel_thread(void)
	move a0, s1
	jalr s0

	jal do_exit
