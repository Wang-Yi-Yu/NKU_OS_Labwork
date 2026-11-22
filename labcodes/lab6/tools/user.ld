/*
 * user.ld: linker script for the user-level programs in user/ (tools/user.ld).
 *
 * Every user program is linked at the fixed address 0x800020 with this script. The entry point
 * is _start (user/libs/initcode.S), which calls umain() and then main(). The sections are laid
 * out as .text, .rodata, (page-aligned) .data and .bss. The symbols etext, edata and end mark
 * the end of each section. The Makefile links with this script via "ld -T tools/user.ld".
 * See the GNU ld 'info' manual ("info ld") to learn the syntax.
 */

OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS {
    /* Load programs at this address: "." means the current address */
    . = 0x800020;

    .text : {
        *(.text .stub .text.* .gnu.linkonce.t.*)
    }

    PROVIDE(etext = .); /* Define the 'etext' symbol to this value */

    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    }

    /* Adjust the address for the data segment to the next page */
    . = ALIGN(0x1000);

    .data : {
        *(.data)
    }

    PROVIDE(edata = .);

    .bss : {
        *(.bss)
    }

    PROVIDE(end = .);

    /DISCARD/ : {
        *(.eh_frame .note.GNU-stack .comment)
    }
}
