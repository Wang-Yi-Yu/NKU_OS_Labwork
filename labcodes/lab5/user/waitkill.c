/**
 * @file   waitkill.c
 * @brief  Test kill() on a process that is blocked in waitpid() and on a busy child.
 *
 * The parent forks child 1, which loops forever, and child 2, which tries to kill the parent and
 * then child 1. The parent waits for child 1 with waitpid(). This test is not in the grade list of
 * tools/grade.sh; it is for manual checks of kill() and waitpid().
 */
#include <ulib.h>
#include <stdio.h>

/**
 * @brief Give up the CPU six times, so that the other processes get to run.
 */
void do_yield(void) {
    yield();
    yield();
    yield();
    yield();
    yield();
    yield();
}

/**
 * @brief PIDs used by the test: parent (the process that runs main()), pid1 (child 1, which loops
 *        forever) and pid2 (child 2, which runs work()). Set by main().
 */
int parent, pid1, pid2;

/**
 * @brief Body of child 1: print a line, then loop forever without yielding.
 */
void loop(void) {
    cprintf("child 1.\n");
    while (1)
        ;
}

/**
 * @brief Body of child 2: try to kill the parent and then child 1.
 *
 * A successful kill() of the parent prints "kill parent ok." and a successful kill() of child 1
 * prints "kill child1 ok." before exit(0). Any failure exits with -1.
 */
void work(void) {
    cprintf("child 2.\n");
    do_yield();
    if (kill(parent) == 0) {
        cprintf("kill parent ok.\n");
        do_yield();
        if (kill(pid1) == 0) {
            cprintf("kill child1 ok.\n");
            exit(0);
        }
    }
    exit(-1);
}

/**
 * @brief Entry point of the test program.
 *
 * Fork child 1 (loops) and child 2 (works). The parent calls waitpid() on child 1; if that call
 * returns, the test panics.
 * @return Does not return: every path ends in exit() or panic().
 */
int main(void) {
    parent = getpid();
    if ((pid1 = fork()) == 0) {
        loop();
    }

    assert(pid1 > 0);

    if ((pid2 = fork()) == 0) {
        work();
    }
    if (pid2 > 0) {
        cprintf("wait child 1.\n");
        waitpid(pid1, NULL);
        panic("waitpid %d returns\n", pid1);
    } else {
        kill(pid1);
    }
    panic("FAIL: T.T\n");
}
