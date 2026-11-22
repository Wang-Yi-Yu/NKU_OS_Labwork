/**
 * @file   yield.c
 * @brief  Test yield(): the process gives up the CPU and is later scheduled again.
 *
 * The program yields five times and prints its PID and iteration after each yield.
 *
 * tools/grade.sh expects these lines from this program:
 *   kernel_execve: pid = 2, name = "yield".
 *   Hello, I am process 2.
 *   Back in process 2, iteration 0. ... iteration 4.
 *   All done in process 2.
 *   yield pass.
 *   must NOT contain: user panic at ...
 */
#include <ulib.h>
#include <stdio.h>

/**
 * @brief Entry point of the test program.
 *
 * Yield five times, printing after each one, then print a pass line.
 * @return 0 on success.
 */
int main(void) {
    int i;
    cprintf("Hello, I am process %d.\n", getpid());
    for (i = 0; i < 5; i++) {
        yield();
        cprintf("Back in process %d, iteration %d.\n", getpid(), i);
    }
    cprintf("All done in process %d.\n", getpid());
    cprintf("yield pass.\n");
    return 0;
}
