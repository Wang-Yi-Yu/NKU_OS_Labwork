/**
 * @file   yield.c
 * @brief  Test yield(): the process gives up the CPU and is later scheduled again.
 *
 * The program yields five times and prints its PID and iteration after each yield.
 *
 * tools/grade.sh of lab8 ran this program from sh and checked its pass line (the lab9 grade.sh
 * does not run it); the output is:
 *   Hello, I am process 2.
 *   Back in process 2, iteration 0. ... iteration 4.
 *   All done in process 2.
 *   yield pass.
 *
 * The kernel_execve line is printed only when user_main() starts the program itself
 * (make build-yield); a program started from sh does not print it.
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
