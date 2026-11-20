/**
 * @file   initcode.S
 * @brief  Entry point (_start) of every user program.
 *
 * tools/user.ld makes _start the ELF entry point. _start calls umain() (umain.c), which runs
 * main() and then exit(). umain() does not return, so the final loop at label 1 is only a
 * safety net that keeps the process from running into whatever follows the code.
 */
.text
.globl _start
_start:
    # call user-program function
    call umain
1:  j 1b
