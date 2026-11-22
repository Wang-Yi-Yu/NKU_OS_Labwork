/**
 * @file   sleepkill.c
 * @brief  Test kill() on a sleeping process (lab7).
 *
 * The child goes to sleep for the longest possible time (sleep(~0)). The parent sleeps for 100
 * ticks, then kills the child. kill() must wake the sleeper up, since a timer wait is
 * interruptible (WT_TIMER), so that the child exits. This test is not in the grade list of
 * tools/grade.sh. Expected output: "sleepkill pass.".
 */
#include <stdio.h>
#include <ulib.h>

/**
 * @brief Entry point of the test program.
 * @return 0 on success.
 */
int main(void) {
    int pid;
    if ((pid = fork()) == 0) {
        sleep(~0);
        exit(0xdead);
    }
    assert(pid > 0);

    sleep(100);
    assert(kill(pid) == 0);
    cprintf("sleepkill pass.\n");
    return 0;
}
