/* Linker script for the boot loader image (tools/boot.ld).
 * Placed at 0x7C00, the classic boot-sector load address; the .startup section holds bootasm.o.
 * Unused in the RISC-V build: the kernel is linked with kernel.ld, and OpenSBI loads it.
 * See the GNU ld 'info' manual ("info ld") for the syntax. */
OUTPUT_FORMAT("elf64-rv64")
OUTPUT_ARCH(rv64)

SECTIONS {
    . = 0x7C00;

    .startup : {
        *bootasm.o(.text)
    }

    .text : { *(.text) }
    .data : { *(.data .rodata) }
    
    /DISCARD/ : { *(.eh_*) }
}
