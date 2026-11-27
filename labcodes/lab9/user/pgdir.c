/**
 * @file   pgdir.c
 * @brief  Test the SYS_pgdir system call: print the page tables of the current process.
 *
 * print_pgdir() asks the kernel to dump the page directory and page tables. The output itself
 * is not checked; only the first and last lines below are required.
 *
 * Expected output (tools/grade.sh of lab9 does not check this program):
 *   I am 2, print pgdir.
 *   init check memory pass.
 *   ("pgdir pass." is printed but not checked)
 *
 * The kernel_execve line is printed only when user_main() starts the program itself
 * (make build-pgdir); a program started from sh does not print it.
 */
#include <stdio.h>
#include <ulib.h>

/**
 * @brief Entry point of the test program.
 *
 * Print this process's PID, dump its page tables and print a pass line.
 * @return 0 on success.
 */
int main(void) {
    cprintf("I am %d, print pgdir.\n", getpid());
    print_pgdir();
    cprintf("pgdir pass.\n");
    return 0;
}
