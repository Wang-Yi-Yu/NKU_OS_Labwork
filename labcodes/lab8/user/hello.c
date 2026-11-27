/**
 * @file   hello.c
 * @brief  Basic user program: print a greeting and the PID of the process.
 *
 * Checks that a user program can run, call getpid() and return normally to exit().
 *
 * tools/grade.sh (lab8) runs this program from sh and checks its pass line; the output is:
 *   Hello world!!.
 *   I am process <pid>.
 *   hello pass.
 *
 * The kernel_execve line is printed only when user_main() starts the program itself
 * (make build-hello); a program started from sh does not print it.
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
