/**
 * @file   spin.c
 * @brief  Test kill(): a parent kills a child that spins forever and then reaps it.
 *
 * The child loops forever without yielding. The parent yields three times so the child gets the
 * CPU, kills it, and waits for it with waitpid(). Kill and wait must both return 0. The kernel must
 * preempt the spinning child (timer interrupt) or the parent would never run again.
 *
 * Expected output (tools/grade.sh of lab9 does not check this program):
 *   I am the parent. Forking the child...
 *   I am the parent. Running the child...
 *   I am the child. spinning ...
 *   I am the parent.  Killing the child...
 *   kill returns 0
 *   wait returns 0
 *   spin may pass.
 *   all user-mode processes have quit.
 *   init check memory pass.
 *
 * The kernel_execve line is printed only when user_main() starts the program itself
 * (make build-spin); a program started from sh does not print it.
 */
#include <stdio.h>
#include <ulib.h>

/**
 * @brief Entry point of the test program.
 *
 * Fork a child that spins, let it run, kill it and reap it.
 * @return 0 on success.
 */
int main(void) {
    int pid, ret;
    cprintf("I am the parent. Forking the child...\n");
    if ((pid = fork()) == 0) {
        cprintf("I am the child. spinning ...\n");
        while (1)
            ;
    }
    cprintf("I am the parent. Running the child...\n");

    yield();
    yield();
    yield();

    cprintf("I am the parent.  Killing the child...\n");

    assert((ret = kill(pid)) == 0);
    cprintf("kill returns %d\n", ret);

    assert((ret = waitpid(pid, NULL)) == 0);
    cprintf("wait returns %d\n", ret);

    cprintf("spin may pass.\n");
    return 0;
}
