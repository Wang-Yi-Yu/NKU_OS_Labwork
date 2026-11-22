/**
 * @file   umain.c
 * @brief  C entry of every user program, called from _start in initcode.S.
 *
 * umain() calls the program's main() and passes its return value to exit().
 */
#include <ulib.h>

/**
 * @brief Entry point of the user program; defined by each program in user/.
 * @return The exit code of the program.
 */
int main(void);

/**
 * @brief Run main() and exit with its return value.
 * @note Never returns: exit() is called with the value returned by main().
 */
void umain(void) {
    int ret = main();
    exit(ret);
}
