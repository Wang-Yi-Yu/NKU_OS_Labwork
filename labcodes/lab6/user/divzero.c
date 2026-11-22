/**
 * @file   divzero.c
 * @brief  Test integer division by zero.
 *
 * RISC-V does not trap on division by zero: the DIV instruction returns -1 (all bits set).
 * The program divides the global one by the global zero and prints the result. Both globals are
 * volatile: with GCC 15 a plain `1 / zero` is folded at -O2 into `(zero + 1 < 3) ? zero : 0` (division
 * by zero is undefined behaviour), no DIV is emitted and the program would print "value is 0.". It then always calls panic(),
 * so the grader does not require the absence of "user panic" for this test.
 *
 * tools/grade.sh expects these lines from this program:
 *   kernel_execve: pid = 2, name = "divzero".
 *   value is -1.
 *   init check memory pass.
 */
#include <stdio.h>
#include <ulib.h>

/** @brief Divisor used by main(). It is zero-initialised and never written; volatile forces a real DIV. */
volatile int zero;
/** @brief Dividend used by main(). volatile, so that the compiler cannot fold the division away. */
volatile int one = 1;

/**
 * @brief Entry point of the test program.
 *
 * Print one / zero. On RISC-V the quotient is -1, so "value is -1." is expected.
 * @return Does not return: panic() calls exit().
 */
int main(void) {
    cprintf("value is %d.\n", one / zero);
    panic("FAIL: T.T\n");
}
