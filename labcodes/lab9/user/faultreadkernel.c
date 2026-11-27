/**
 * @file   faultreadkernel.c
 * @brief  Test that user-mode code cannot read kernel memory.
 *
 * 0xfac00000 is above USERTOP (0x80000000), so it is outside user space, and a user-mode load
 * from it must fail. The kernel must terminate the process, and the program must not print
 * "user panic at".
 *
 * tools/grade.sh of lab9 does not check this program.
 *
 * The kernel_execve line is printed only when user_main() starts the program itself
 * (make build-faultreadkernel); a program started from sh does not print it.
 */
#include <stdio.h>
#include <ulib.h>

/**
 * @brief Entry point of the test program.
 *
 * Read 0xfac00000. If the kernel does not stop the process, panic() reports the failure.
 * @return Does not return: panic() calls exit().
 */
int main(void) {
    cprintf("I read %08x from 0xfac00000!\n", *(unsigned *)0xfac00000);
    panic("FAIL: T.T\n");
}
