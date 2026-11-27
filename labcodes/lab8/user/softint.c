/**
 * @file   softint.c
 * @brief  Exit immediately; the x86 software-interrupt test is disabled on RISC-V.
 *
 * The original test executes int $14 (a software interrupt on x86). That line is commented out,
 * so the program only calls exit(0). The kernel must report that all user processes have quit
 * afterwards.
 *
 * Expected output (tools/grade.sh of lab8 does not check this program):
 *   all user-mode processes have quit.
 *   init check memory pass.
 *
 * The kernel_execve line is printed only when user_main() starts the program itself
 * (make build-softint); a program started from sh does not print it.
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
