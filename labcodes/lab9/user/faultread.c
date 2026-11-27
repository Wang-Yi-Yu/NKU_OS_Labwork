/**
 * @file   faultread.c
 * @brief  Test that a user-mode load from address 0 kills the process instead of panicking.
 *
 * The load from address 0 is not mapped for user code, so the kernel must handle the page fault
 * by terminating the process. The cprintf() is never reached. The program must not print
 * "user panic at".
 *
 * tools/grade.sh of lab9 does not check this program.
 *
 * The kernel_execve line is printed only when user_main() starts the program itself
 * (make build-faultread); a program started from sh does not print it.
 */
#include <stdio.h>
#include <ulib.h>

/**
 * @brief Entry point of the test program.
 *
 * Read address 0. If the kernel does not stop the process, panic() reports the failure.
 * @return Does not return: panic() calls exit().
 */
int main(void) {
    cprintf("I read %8x from 0.\n", *(unsigned int *)0);
    panic("FAIL: T.T\n");
}
