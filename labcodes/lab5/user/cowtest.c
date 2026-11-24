/* lab5 Challenge 2411622张雨函: COW isolation, repeated sharing and reclaim. */
#include <ulib.h>
#include <stdio.h>

static volatile int value = 7;
static volatile int pages[1024];
/* A dedicated page lets waitpid's kernel write exercise the COW fault path. */
static int status_page[1024] __attribute__((aligned(4096)));

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
        assert(waitpid(pid, &status_page[0]) == 0 &&
               status_page[0] == round + 1);
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
