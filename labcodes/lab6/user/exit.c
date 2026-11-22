/**
 * @file   exit.c
 * @brief  Test fork(), exit() and waitpid() return values.
 *
 * The parent forks a child that yields seven times and exits with the value of magic. The parent
 * checks that waitpid() returns that exit code, and that a second waitpid() and wait() both fail
 * because no children are left.
 *
 * tools/grade.sh expects these lines from this program:
 *   kernel_execve: pid = 2, name = "exit".
 *   I am the parent. Forking the child...
 *   I am the parent, waiting now..
 *   I am the child.
 *   waitpid <pid> ok. (regex: waitpid.*ok.*)
 *   exit pass.
 *   all user-mode processes have quit.
 *   init check memory pass.
 *   must NOT contain: user panic at ...
 */
#include <stdio.h>
#include <ulib.h>

/** @brief Exit code that the child passes to exit() and the parent must get back from waitpid(). */
int magic = -0x10384;

/**
 * @brief Entry point of the test program.
 *
 * Fork a child that calls exit(magic). The parent waits for it and checks the code,
 * then checks that there is no second child to wait for.
 * @return 0 on success.
 */
int main(void) {
    int pid, code;
    cprintf("I am the parent. Forking the child...\n");
    if ((pid = fork()) == 0) {
        cprintf("I am the child.\n");
        yield();
        yield();
        yield();
        yield();
        yield();
        yield();
        yield();
        exit(magic);
    } else {
        cprintf("I am parent, fork a child pid %d\n", pid);
    }
    assert(pid > 0);
    cprintf("I am the parent, waiting now..\n");

    assert(waitpid(pid, &code) == 0 && code == magic);
    assert(waitpid(pid, &code) != 0 && wait() != 0);
    cprintf("waitpid %d ok.\n", pid);

    cprintf("exit pass.\n");
    return 0;
}
