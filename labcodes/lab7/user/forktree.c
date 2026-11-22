/**
 * @file   forktree.c
 * @brief  Print the process tree that results from recursive fork().
 *
 * The root process forks two children, and each child forks two more, up to DEPTH levels. Every
 * process prints its PID (4 hex digits) and the path string of its position in the tree. The
 * program is a demonstration only: tools/grade.sh does not check its output.
 */
#include <ulib.h>
#include <stdio.h>
#include <string.h>

/** @brief Maximum length of a branch path, i.e. the number of fork levels below the root. */
#define DEPTH 4

/**
 * @brief Print the current process and create its two children.
 * @param[in] cur  Path string of the current process; the empty string for the root.
 */
void forktree(const char *cur);

/**
 * @brief Fork one child whose path is cur followed by branch, unless cur has reached DEPTH.
 *
 * The child runs forktree() on the new path, yields once and exits. The parent returns at once.
 * @param[in] cur     Path string of the parent process.
 * @param[in] branch  Character appended to cur for the child: '0' or '1'.
 */
void forkchild(const char *cur, char branch) {
    char nxt[DEPTH + 1];

    if (strlen(cur) >= DEPTH)
        return;

    snprintf(nxt, DEPTH + 1, "%s%c", cur, branch);
    if (fork() == 0) {
        forktree(nxt);
        yield();
        exit(0);
    }
}

/**
 * @brief Print the current process and create its two children.
 *
 * @param[in] cur  Path string of the current process; the empty string for the root.
 */
void forktree(const char *cur) {
    cprintf("%04x: I am '%s'\n", getpid(), cur);

    forkchild(cur, '0');
    forkchild(cur, '1');
}

/**
 * @brief Entry point of the test program.
 *
 * Start the tree at the root with the empty path.
 * @return 0 on success.
 */
int main(void) {
    forktree("");
    return 0;
}
