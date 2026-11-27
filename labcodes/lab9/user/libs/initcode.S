/**
 * @file   initcode.S
 * @brief  Entry point (_start) of every user program.
 *
 * tools/user.ld makes _start the ELF entry point. _start calls umain() (umain.c), which runs
 * main() and then exit(). umain() does not return, so the final loop at label 1 is only a
 * safety net that keeps the process from running into whatever follows the code.
 *
 * Arguments. load_icode() (kern/process/proc.c) leaves the program arguments in the usual
 * process-entry layout: sp points at argc, argv[0..argc] (NULL terminated) follows at sp + 8, and
 * the strings lie above it. _start reads them from there instead of trusting a0/a1: when the
 * program is started by exec() from user mode, the system call return path overwrites a0 with the
 * return value 0 of sys_exec(), so a0 would be wrong.
 */
.text
.globl _start
_start:
    # umain(argc, argv): argc is at the top of the stack, argv right behind it
    ld a0, 0(sp)
    addi a1, sp, 8
    # call user-program function
    call umain
1:  j 1b
