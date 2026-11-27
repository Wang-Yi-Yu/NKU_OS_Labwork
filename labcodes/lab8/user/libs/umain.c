/**
 * @file   umain.c
 * @brief  C entry of every user program, called from _start in initcode.S.
 *
 * umain() first connects the standard descriptors: it opens "stdin:" as file descriptor 0 and
 * "stdout:" as file descriptor 1 (lab8, "everything is a file"). Then it calls the program's
 * main() with argc and argv and passes its return value to exit().
 */
#include <ulib.h>
#include <unistd.h>
#include <file.h>
#include <stat.h>

/**
 * @brief Entry point of the user program; defined by each program in user/.
 * @param[in] argc  Number of arguments.
 * @param[in] argv  Argument vector.
 * @return The exit code of the program.
 */
int main(int argc, char *argv[]);

/**
 * @brief Open @p path and make the result file descriptor @p fd2.
 *
 * The first open() returns the lowest free descriptor. If that is not @p fd2, the descriptor
 * @p fd2 is closed, redirected to the opened file with dup2(), and the temporary one is closed.
 *
 * @param[in] fd2         Wanted file descriptor (0 or 1).
 * @param[in] path        Device path ("stdin:" or "stdout:").
 * @param[in] open_flags  Access mode for open().
 * @return A non-negative value on success, a negative error code otherwise.
 */
static int initfd(int fd2, const char *path, uint32_t open_flags) {
    int fd1, ret;
    if ((fd1 = open(path, open_flags)) < 0) {
        return fd1;
    }
    if (fd1 != fd2) {
        close(fd2);
        ret = dup2(fd1, fd2);
        close(fd1);
    }
    return ret;
}

/**
 * @brief Set up stdin and stdout, run main() and exit with its return value.
 * @param[in] argc  Number of arguments, passed on to main().
 * @param[in] argv  Argument vector, passed on to main().
 * @note Never returns: exit() is called with the value returned by main().
 */
void umain(int argc, char *argv[]) {
    int fd;
    if ((fd = initfd(0, "stdin:", O_RDONLY)) < 0) {
        warn("open <stdin> failed: %e.\n", fd);
    }
    if ((fd = initfd(1, "stdout:", O_WRONLY)) < 0) {
        warn("open <stdout> failed: %e.\n", fd);
    }
    int ret = main(argc, argv);
    exit(ret);
}
