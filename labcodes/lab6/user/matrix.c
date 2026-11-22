/**
 * @file   matrix.c
 * @brief  Scheduler stress test (lab6): 21 processes multiply 10x10 matrices for different times.
 *
 * Every child fills two matrices, yields once, and then multiplies them over and over for a number
 * of rounds that depends on its index, so the children have different run times. The parent waits
 * for all of them and prints "matrix pass." if every wait succeeded. It is not part of the grading
 * script; it is a convenient workload to watch the scheduler with.
 */
#include <ulib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/** @brief Side length of the square matrices. */
#define MATSIZE 10

static int mata[MATSIZE][MATSIZE];
static int matb[MATSIZE][MATSIZE];
static int matc[MATSIZE][MATSIZE];

/**
 * @brief Body of a child: multiply the matrices @p times times, then exit.
 * @param[in] times  Number of multiplication rounds.
 * @note Prints "pid N is running" after the first yield and "pid N done!" at the end. Never returns.
 */
void work(unsigned int times) {
    int i, j, k, size = MATSIZE;
    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            mata[i][j] = matb[i][j] = 1;
        }
    }

    yield();

    cprintf("pid %d is running (%d times)!.\n", getpid(), times);

    while (times-- > 0) {
        for (i = 0; i < size; i++) {
            for (j = 0; j < size; j++) {
                matc[i][j] = 0;
                for (k = 0; k < size; k++) {
                    matc[i][j] += mata[i][k] * matb[k][j];
                }
            }
        }
        for (i = 0; i < size; i++) {
            for (j = 0; j < size; j++) {
                mata[i][j] = matb[i][j] = matc[i][j];
            }
        }
    }
    cprintf("pid %d done!.\n", getpid());
    exit(0);
}

/** @brief Number of child processes. */
const int total = 21;

/**
 * @brief Entry point of the test program.
 *
 * Fork @c total children with pseudo-random work sizes and wait for all of them.
 * @return 0 on success; on a failed fork or wait the children are killed and the program panics.
 */
int main(void) {
    int pids[total];
    memset(pids, 0, sizeof(pids));

    int i;
    for (i = 0; i < total; i++) {
        if ((pids[i] = fork()) == 0) {
            srand(i * i);
            int times = (((unsigned int)rand()) % total);
            times = (times * times + 10) * 100;
            work(times);
        }
        if (pids[i] < 0) {
            goto failed;
        }
    }

    cprintf("fork ok.\n");

    for (i = 0; i < total; i++) {
        if (wait() != 0) {
            cprintf("wait failed.\n");
            goto failed;
        }
    }

    cprintf("matrix pass.\n");
    return 0;

failed:
    for (i = 0; i < total; i++) {
        if (pids[i] > 0) {
            kill(pids[i]);
        }
    }
    panic("FAIL: T.T\n");
}
