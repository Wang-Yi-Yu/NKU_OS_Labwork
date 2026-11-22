/**
 * @file   faultread.c
 * @brief  Test that a user-mode load from address 0 kills the process instead of panicking.
 *
 * The load from address 0 is not mapped for user code, so the kernel must handle the page fault
 * by terminating the process. The cprintf() is never reached. The grader requires that the
 * kernel reports the process and that the user program does not print "user panic at".
 *
 * tools/grade.sh expects these lines from this program:
 *   kernel_execve: pid = 2, name = "faultread".
 *   must NOT contain: user panic at ...
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
