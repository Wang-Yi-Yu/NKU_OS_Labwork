/**
 * @file   priority.c
 * @brief  Test of the scheduler (lab6): five CPU-bound children with different priorities.
 *
 * The parent sets its own priority to TOTAL + 1 and forks TOTAL children. Child i sets its
 * priority to i + 1 and then counts in a busy loop until MAX_TIME ms have passed on the clock; it
 * exits with its counter as exit code. The parent waits for every child and prints the counters.
 * Under Stride scheduling the counters are roughly proportional to the priorities, and the last
 * line "stride sched correct result:" shows the ratios (for example "1 2 3 4 5"); under RR they are
 * about equal.
 *
 * Expected output (tools/grade.sh of lab9 does not check this program):
 *   main: fork ok,now need to wait pids.
 *   set priority to 5 ... set priority to 1.
 *   all user-mode processes have quit.
 *   init check memory pass.
 *
 * The kernel_execve line is printed only when user_main() starts the program itself
 * (make build-priority); a program started from sh does not print it.
 */
#include <ulib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/** @brief Number of child processes (priorities 1..TOTAL). */
#define TOTAL 5
/** @brief Running time of each child in ms; it should be above 1000 ms for enough accuracy (lab7: 10 s). */
#define MAX_TIME 10000
/** @brief Per-child counter of loop iterations, a measure of the CPU time received. */
unsigned int acc[TOTAL];
/** @brief Exit codes (the counters) returned by the children. */
int status[TOTAL];
/** @brief PIDs of the children. */
int pids[TOTAL];

/**
 * @brief Burn a little CPU time: toggle a volatile variable 200 times.
 */
static void spin_delay(void) {
    int i;
    volatile int j;
    for (i = 0; i != 200; ++i) {
        j = !j;
    }
}

/**
 * @brief Entry point of the test program.
 *
 * Fork the children, let each one count until MAX_TIME, then collect and print the counters.
 * @return 0 on success; on a failed fork the children are killed and the program panics.
 */
int main(void) {
    int i, time;
    memset(pids, 0, sizeof(pids));
    lab6_set_priority(TOTAL + 1);

    for (i = 0; i < TOTAL; i++) {
        acc[i] = 0;
        if ((pids[i] = fork()) == 0) {
            lab6_set_priority(i + 1);
            acc[i] = 0;
            while (1) {
                spin_delay();
                ++acc[i];
                if (acc[i] % 4000 == 0) {
                    if ((time = gettime_msec()) > MAX_TIME) {
                        cprintf("child pid %d, acc %d, time %d\n", getpid(), acc[i], time);
                        exit(acc[i]);
                    }
                }
            }
        }
        if (pids[i] < 0) {
            goto failed;
        }
    }

    cprintf("main: fork ok,now need to wait pids.\n");

    for (i = 0; i < TOTAL; i++) {
        status[i] = 0;
        waitpid(pids[i], &status[i]);
        cprintf("main: pid %d, acc %d, time %d\n", pids[i], status[i], gettime_msec());
    }
    cprintf("main: wait pids over\n");
    cprintf("stride sched correct result:");
    for (i = 0; i < TOTAL; i++) {
        cprintf(" %d", (status[i] * 2 / status[0] + 1) / 2);
    }
    cprintf("\n");

    return 0;

failed:
    for (i = 0; i < TOTAL; i++) {
        if (pids[i] > 0) {
            kill(pids[i]);
        }
    }
    panic("FAIL: T.T\n");
}
