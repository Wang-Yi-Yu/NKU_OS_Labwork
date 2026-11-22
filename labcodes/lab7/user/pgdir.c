/**
 * @file   pgdir.c
 * @brief  Test the SYS_pgdir system call: print the page tables of the current process.
 *
 * print_pgdir() asks the kernel to dump the page directory and page tables. The output itself
 * is not checked; the grader only requires the first and last lines below.
 *
 * tools/grade.sh expects these lines from this program:
 *   kernel_execve: pid = 2, name = "pgdir".
 *   I am 2, print pgdir.
 *   init check memory pass.
 *   ("pgdir pass." is printed but not checked by grade.sh)
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
