/**
 * @file   cowtest.c
 * @brief  Copy-on-write test: write isolation between parent and child, repeated sharing and
 *         reclaim of the shared pages.
 *
 * Phase 1 (12 rounds): the parent forks a child; child and parent each write to the same global
 * variables, and both must still read back their own values (the first write after fork hits a
 * COW page and triggers a page copy). The child's exit code is received with waitpid() into a
 * page-aligned buffer, which makes the kernel itself write into a COW user page.
 * Phase 2: four children exist at the same time, so the pages are shared by five processes; each
 * child writes its own values and the parent's data stays unchanged.
 *
 * Run it with `make build-cowtest`. It prints "cowtest pass." on success.
 */
// lab5 Challenge 2411622张雨函
#include <ulib.h>
#include <stdio.h>

/** @brief Global written by parent and children; lives in a data page that fork shares. */
static volatile int value = 7;
/** @brief Array spanning several data pages, written at index 0 and at the child indexes. */
static volatile int pages[1024];
/** @brief Page-aligned buffer for waitpid(): the kernel writes the exit code into a COW page. */
static int status_page[1024] __attribute__((aligned(4096)));

/**
 * @brief Entry point of the test program.
 *
 * Runs the two phases described in the file comment; every check uses assert(), which ends the
 * program with a user panic on failure.
 * @return 0 on success.
 */
int main(void) {
    pages[0] = 11;
    for (int round = 0; round < 12; round++) {
        status_page[0] = -1;
        int pid = fork();
        assert(pid >= 0);
        if (pid == 0) {
            assert(value == 7 + round);
            assert(pages[0] == 11 + round);
            value = 100 + round;
            pages[0] = 200 + round;
            assert(value == 100 + round && pages[0] == 200 + round);
            exit(round + 1);
        }

        /* Parent writes while the child still maps the original pages. */
        value = 8 + round;
        pages[0] = 12 + round;
        assert(waitpid(pid, &status_page[0]) == 0 && status_page[0] == round + 1);
        assert(value == 8 + round && pages[0] == 12 + round);
    }
    int children[4];
    value = 50;
    for (int i = 0; i < 4; i++) {
        children[i] = fork();
        assert(children[i] >= 0);
        if (children[i] == 0) {
            assert(value == 50);
            value = 60 + i;
            pages[i] = 70 + i;
            assert(value == 60 + i && pages[i] == 70 + i);
            exit(20 + i);
        }
    }
    for (int i = 0; i < 4; i++) {
        int status;
        assert(waitpid(children[i], &status) == 0 && status == 20 + i);
    }
    assert(value == 50);
    cprintf("cowtest pass.\n");
    return 0;
}
