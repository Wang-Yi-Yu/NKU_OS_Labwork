/**
 * @file   hello.c
 * @brief  Basic user program: print a greeting and the PID of the process.
 *
 * Checks that a user program can run, call getpid() and return normally to exit().
 *
 * tools/grade.sh expects these lines from this program:
 *   kernel_execve: pid = 2, name = "hello".
 *   Hello world!!.
 *   I am process 2.
 *   hello pass.
 *   must NOT contain: user panic at ...
 */
#include <stdio.h>
#include <ulib.h>

/**
 * @brief Entry point of the test program.
 *
 * Print a greeting, the PID of this process and a pass line.
 * @return 0 on success.
 */
int main(void) {
    cprintf("Hello world!!.\n");
    cprintf("I am process %d.\n", getpid());
    cprintf("hello pass.\n");
    return 0;
}
