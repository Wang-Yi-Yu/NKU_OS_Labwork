/**
 * @file   sleep.c
 * @brief  Test sleep() (lab7): a child sleeps ten times and the parent measures the elapsed time.
 *
 * The child calls sleep(100) ten times, which is 100 timer ticks each (one second), and prints one
 * line after every sleep. The parent waits for the child and prints how many milliseconds passed.
 * It is a manual check of do_sleep() and the timer list, and is not in the grade list of
 * tools/grade.sh. Expected output: ten lines "sleep N x 100 slices.", then "use NNNN msecs." with
 * a value around 10000, and "sleep pass.".
 */
#include <stdio.h>
#include <ulib.h>

/**
 * @brief Body of the child process: sleep ten times and exit.
 * @param pid unused (the value returned by fork() in the child, which is 0)
 * @note Does not return: it ends with exit(0).
 */
void sleepy(int pid) {
    int i, time = 100;
    for (i = 0; i < 10; i++) {
        sleep(time);
        cprintf("sleep %d x %d slices.\n", i + 1, time);
    }
    exit(0);
}

/**
 * @brief Entry point of the test program.
 *
 * Forks the child, waits for it with waitpid() and prints the elapsed time.
 * @return 0 on success.
 */
int main(void) {
    unsigned int time = gettime_msec();
    int pid1, exit_code;

    if ((pid1 = fork()) == 0) {
        sleepy(pid1);
    }

    assert(waitpid(pid1, &exit_code) == 0 && exit_code == 0);
    cprintf("use %04d msecs.\n", gettime_msec() - time);
    cprintf("sleep pass.\n");
    return 0;
}
