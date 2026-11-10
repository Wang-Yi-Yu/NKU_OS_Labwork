/*
 * kernel.ld: linker script for the lab4 RISC-V kernel (tools/kernel.ld).
 *
 * Lays out the kernel at the high virtual address BASE_ADDRESS, in the order
 * .text, .rodata, (page-aligned) .data/.sdata, .bss. It defines the symbols
 * etext, edata and end, which print_kerninfo() prints and which bound the
 * kernel image. The kernel entry symbol kern_entry comes first in .text.
 * Sections named .eh_frame and .note.GNU-stack are discarded.
 * The Makefile links with this script via "ld -T tools/kernel.ld".
 */
/* Simple linker script for the ucore kernel.
   See the GNU ld 'info' manual ("info ld") to learn the syntax. */

OUTPUT_ARCH(riscv)
ENTRY(kern_entry)

/* Virtual address of the kernel image (Sv39 high half). kern_entry turns on paging so that this
 * address maps to the physical DRAM address 0x80200000, where OpenSBI jumps to the kernel. */
BASE_ADDRESS = 0xFFFFFFFFC0200000;

SECTIONS
{
    /* Load the kernel at this address: "." means the current address */
    . = BASE_ADDRESS;

    /* Code. .text.kern_entry is placed first so that kern_entry is the first instruction at BASE_ADDRESS */
    .text : {
        *(.text.kern_entry .text .stub .text.* .gnu.linkonce.t.*)
    }

    PROVIDE(etext = .); /* Define the 'etext' symbol to this value */

    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    }

    /* Adjust the address for the data segment to the next page */
    . = ALIGN(0x1000);

    /* The data segment */
    .data : {
        *(.data)
        *(.data.*)
    }

    .sdata : {
        *(.sdata)
        *(.sdata.*)
    }

    /* edata: end of the initialised data (.data and .sdata); .bss follows */
    PROVIDE(edata = .);

    .bss : {
        *(.bss)
        *(.bss.*)
        *(.sbss*)
    }

    /* end: first address after the kernel image including .bss; the kernel's memory footprint ends here */
    PROVIDE(end = .);

    /* Sections that are not needed at run time are dropped from the image */
    /DISCARD/ : {
        *(.eh_frame .note.GNU-stack)
    }
}
