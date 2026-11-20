/**
 * @file   badsegment.c
 * @brief  Placeholder for the x86 TSS segment test; on RISC-V it only calls panic().
 *
 * The original test loads the kernel's TSS selector into the DS register, which raises a
 * protection fault on x86. RISC-V has no TSS, so the instruction is commented out and main()
 * always reaches panic(). The grader only checks that the kernel reports this process and keeps
 * running afterwards.
 *
 * tools/grade.sh expects these lines from this program:
 *   kernel_execve: pid = 2, name = "badsegment".
 *   init check memory pass.
 */
#include <stdio.h>
#include <ulib.h>

/**
 * @brief Entry point of the test program.
 *
 * Calls panic() unconditionally, because the segment load is commented out.
 * @return Does not return: panic() calls exit().
 */
int main(void) {
    // There is no such thing as TSS in RISC-V
    // asm volatile("movw $0x28,%ax; movw %ax,%ds");
    panic("FAIL: T.T\n");
}
