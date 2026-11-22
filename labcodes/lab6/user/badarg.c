/**
 * @file   badarg.c
 * @brief  Test that waitpid() rejects bad arguments and returns the exit code of the child.
 *
 * Exercises fork(), yield(), exit() and waitpid() in the kernel. The child yields ten times and
 * exits with 0xbeaf. The parent checks that waitpid() fails for pid -1 and for a store pointer in
 * kernel space (0xC0000000), and that waitpid() on the real child returns 0 with the exit code.
 *
 * tools/grade.sh expects these lines from this program:
 *   kernel_execve: pid = 2, name = "badarg".
 *   fork ok.
 *   badarg pass.
 *   all user-mode processes have quit.
 *   init check memory pass.
 *   must NOT contain: user panic at ...
 */
#include <stdio.h>
#include <ulib.h>

/**
 * @brief Entry point of the test program.
 *
 * Fork a child that yields and exits with 0xbeaf, then check the waitpid() error
 * cases (-1 pid and a kernel-space pointer) and the normal case.
 * @return 0 on success.
 */
int main(void) {
    int pid, exit_code;
    if ((pid = fork()) == 0) {
        cprintf("fork ok.\n");
        int i;
        for (i = 0; i < 10; i++) {
            yield();
        }
        exit(0xbeaf);
    }
    assert(pid > 0);
    assert(waitpid(-1, NULL) != 0);
    assert(waitpid(pid, (void *)0xC0000000) != 0);
    assert(waitpid(pid, &exit_code) == 0 && exit_code == 0xbeaf);
    cprintf("badarg pass.\n");
    return 0;
}
