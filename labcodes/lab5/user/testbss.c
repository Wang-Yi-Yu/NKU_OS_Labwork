/**
 * @file   testbss.c
 * @brief  Test that the .bss section is zero-filled and writable.
 *
 * bigarray is a 4 MiB global that is never initialised, so it lives in .bss. The program checks
 * that every element is 0, writes each index, reads the values back and then writes 1024 elements
 * past the end of the array. The grader only requires that the kernel does not report
 * "user panic at".
 *
 * tools/grade.sh expects these lines from this program:
 *   kernel_execve: pid = 2, name = "testbss".
 *   Making sure bss works right...
 *   Yes, good.  Now doing a wild write off the end...
 *   testbss may pass.
 *   must NOT contain: user panic at ...
 */
#include <stdio.h>
#include <ulib.h>

/** @brief Number of uint32_t elements in bigarray (4 MiB in total). */
#define ARRAYSIZE (1024 * 1024)

/** @brief 4 MiB global array in .bss; it must read as zero before the first write. */
uint32_t bigarray[ARRAYSIZE];

/**
 * @brief Entry point of the test program.
 *
 * Check that bigarray is zero, fill it with the index, check the values and then write
 * past the end. The last write is expected to be handled by the kernel, not to panic.
 * @return Does not return: exit(0) is called before the final panic().
 */
int main(void) {
    cprintf("Making sure bss works right...\n");
    int i;
    for (i = 0; i < ARRAYSIZE; i++) {
        if (bigarray[i] != 0) {
            panic("bigarray[%d] isn't cleared!\n", i);
        }
    }
    for (i = 0; i < ARRAYSIZE; i++) {
        bigarray[i] = i;
    }
    for (i = 0; i < ARRAYSIZE; i++) {
        if (bigarray[i] != i) {
            panic("bigarray[%d] didn't hold its value!\n", i);
        }
    }

    cprintf("Yes, good.  Now doing a wild write off the end...\n");
    cprintf("testbss may pass.\n");

    bigarray[ARRAYSIZE + 1024] = 0;
    // asm volatile ("int $0x14");
    exit(0);
    panic("FAIL: T.T\n");
}
