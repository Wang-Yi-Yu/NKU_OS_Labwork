/**
 * @file   faultreadkernel.c
 * @brief  Test that user-mode code cannot read kernel memory.
 *
 * 0xfac00000 is in the kernel's virtual address range, so a user-mode load from it must fail. The
 * kernel must terminate the process. The grader requires that the kernel reports the process and
 * that the user program does not print "user panic at".
 *
 * tools/grade.sh expects these lines from this program:
 *   kernel_execve: pid = 2, name = "faultreadkernel".
 *   must NOT contain: user panic at ...
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
