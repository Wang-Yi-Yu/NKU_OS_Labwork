/**
 * @file   vector.c
 * @brief  Host tool inherited from the x86 ucore: prints the assembly source of the 256 x86 interrupt
 *         vector stubs and the vector table.
 *
 * Not used by lab3: RISC-V has a single trap entry (__alltraps in kern/trap/trapentry.S) and no
 * vector stubs. Kept only because it is part of the original lab tree.
 */
#include <stdio.h>

/**
 * @brief Print the vector stubs (`pushl` + `jmp __alltraps`) and the vector table to stdout.
 * @return 0
 */
int
main(void) {
    printf("# handler\n");
    printf(".text\n");
    printf(".globl __alltraps\n");

    int i;
    for (i = 0; i < 256; i ++) {
        printf(".globl vector%d\n", i);
        printf("vector%d:\n", i);
        if ((i < 8 || i > 14) && i != 17) {
            printf("  pushl $0\n");
        }
        printf("  pushl $%d\n", i);
        printf("  jmp __alltraps\n");
    }
    printf("\n");
    printf("# vector table\n");
    printf(".data\n");
    printf(".globl __vectors\n");
    printf("__vectors:\n");
    for (i = 0; i < 256; i ++) {
        printf("  .long vector%d\n", i);
    }
    return 0;
}

