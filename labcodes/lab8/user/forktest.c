/**
 * @file   forktest.c
 * @brief  Stress test: fork many children, reap all of them and check that no extra child is left.
 *
 * Each child prints "I am child <n>" and exits with 0. The parent forks max_child children, waits
 * for each one with wait() and then checks that wait() fails. The grader looks for four of the
 * child lines (31, 19, 13 and 0) and requires that the error messages are not printed.
 *
 * tools/grade.sh expects these lines from this program:
 *   kernel_execve: pid = 2, name = "forktest".
 *   I am child 31
 *   I am child 19
 *   I am child 13
 *   I am child 0
 *   forktest pass.
 *   all user-mode processes have quit.
 *   init check memory pass.
 *   must NOT contain: fork claimed to work [0-9]+ times!, wait stopped early, wait got too many,
 *   user panic at ...
 */
#include <ulib.h>
#include <stdio.h>

/** @brief Number of children that main() forks. */
const int max_child = 32;

/**
 * @brief Entry point of the test program.
 *
 * Fork max_child children, then reap them one by one. Any wait() error or any extra
 * child is reported with panic().
 * @return 0 on success.
 */
int main(void) {
    int n, pid;
    for (n = 0; n < max_child; n++) {
        if ((pid = fork()) == 0) {
            cprintf("I am child %d\n", n);
            exit(0);
        }
        assert(pid > 0);
    }

    if (n > max_child) {
        panic("fork claimed to work %d times!\n", n);
    }

    for (; n > 0; n--) {
        if (wait() != 0) {
            panic("wait stopped early\n");
        }
    }

    if (wait() == 0) {
        panic("wait got too many\n");
    }

    cprintf("forktest pass.\n");
    return 0;
}
