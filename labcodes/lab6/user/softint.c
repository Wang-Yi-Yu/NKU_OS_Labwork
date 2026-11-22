/**
 * @file   softint.c
 * @brief  Exit immediately; the x86 software-interrupt test is disabled on RISC-V.
 *
 * The original test executes int $14 (a software interrupt on x86). That line is commented out,
 * so the program only calls exit(0). The grader checks that all user processes quit afterwards.
 *
 * tools/grade.sh expects these lines from this program:
 *   kernel_execve: pid = 2, name = "softint".
 *   all user-mode processes have quit.
 *   init check memory pass.
 */
#include <stdio.h>
#include <ulib.h>

/**
 * @brief Entry point of the test program.
 *
 * Call exit(0) at once. panic() is only reached if exit() returns.
 * @return Does not return: exit(0) is called first.
 */
int main(void) {
    // Never mind
    // asm volatile("int $14");
    exit(0);
    panic("FAIL: T.T\n");
}
