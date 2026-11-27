/**
 * @file   badsegment.c
 * @brief  Placeholder for the x86 TSS segment test; on RISC-V it only calls panic().
 *
 * The original test loads the kernel's TSS selector into the DS register, which raises a
 * protection fault on x86. RISC-V has no TSS, so the instruction is commented out and main()
 * always reaches panic(). The kernel must report this process and keep running afterwards.
 *
 * Expected output (tools/grade.sh of lab9 does not check this program):
 *   init check memory pass.
 *
 * The kernel_execve line is printed only when user_main() starts the program itself
 * (make build-badsegment); a program started from sh does not print it.
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
