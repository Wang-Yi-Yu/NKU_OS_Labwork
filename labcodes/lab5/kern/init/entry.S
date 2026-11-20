/**
 * @file   entry.S
 * @brief  Kernel entry point (kern_entry), boot stack and boot page table.
 *
 * Boot flow: OpenSBI (M mode) jumps to the physical address 0x80200000 in S
 * mode with a0 = hart id and a1 = physical address of the device tree blob
 * (DTB). The kernel is linked at the high virtual address 0xFFFFFFFFC0200000, so
 * kern_entry must first turn on Sv39 paging with the page table
 * boot_page_table_sv39, and only then can it use virtual addresses (set sp and
 * jump to kern_init).
 *
 * Data defined here and used from C: bootstack/bootstacktop (boot stack),
 * boot_page_table_sv39 (one-page root page table with 1GiB gigapage mappings),
 * boot_hartid and boot_dtb (copies of a0 and a1, declared in dtb.h).
 */
#include <mmu.h>
#include <memlayout.h>

    .section .text,"ax",%progbits
    .globl kern_entry
kern_entry:
    # a0: hartid
    # a1: dtb physical address
    # save hartid and dtb address
    la t0, boot_hartid
    sd a0, 0(t0)
    la t0, boot_dtb
    sd a1, 0(t0)

    # t0 := 三级页表的虚拟地址
    lui     t0, %hi(boot_page_table_sv39)
    # t1 := 0xffffffff40000000 即虚实映射偏移量
    li      t1, 0xffffffffc0000000 - 0x80000000
    # t0 减去虚实映射偏移量 0xffffffff40000000，变为三级页表的物理地址
    sub     t0, t0, t1
    # t0 >>= 12，变为三级页表的物理页号
    srli    t0, t0, 12

    # t1 := 8 << 60，设置 satp 的 MODE 字段为 Sv39
    li      t1, 8 << 60
    # 将刚才计算出的预设三级页表物理页号附加到 satp 中
    or      t0, t0, t1
    # 将算出的 t0(即新的MODE|页表基址物理页号) 覆盖到 satp 中
    csrw    satp, t0
    # 使用 sfence.vma 指令刷新 TLB
    sfence.vma
    # 从此，我们给内核搭建出了一个完美的虚拟内存空间！
    #nop # 可能映射的位置有些bug。。插入一个nop
    
    # 我们在虚拟内存空间中：随意将 sp 设置为虚拟地址！
    lui sp, %hi(bootstacktop)

    # 我们在虚拟内存空间中：随意跳转到虚拟地址！
    # 1. 使用临时寄存器 t1 计算栈顶的精确地址
    lui t1, %hi(bootstacktop)
    addi t1, t1, %lo(bootstacktop)
    # 2. 将精确地址一次性地、安全地传给 sp
    mv sp, t1
    # 现在栈指针已经完美设置，可以安全地调用任何C函数了
    # 然后跳转到 kern_init (不再返回)
    lui t0, %hi(kern_init)
    addi t0, t0, %lo(kern_init)
    jr t0

.section .data
    # .align 2^12
    .align PGSHIFT
    .global bootstack
bootstack:
    .space KSTACKSIZE
    .global bootstacktop
bootstacktop:

    # a page table for Sv39
    .align PGSHIFT
    .global boot_page_table_sv39
# 分配 4KiB 内存给预设的三级页表
boot_page_table_sv39:
    # Every entry not set below is 0, i.e. V=0 (unmapped).
    # Entries 0 and 1: 0x00000000~0x7fffffff, unmapped
    .zero 8 * 2
    # Entry 2: 0x80000000 identity map to 0x80000000 (1G)
    # [QEMU 8.x adaptation] The instruction right after `csrw satp` is still
    # fetched from the low physical address (0x802000xx). QEMU 4.1 keeps running
    # it inside the same already-translated block as the csrw, so a high-address
    # mapping alone was enough. QEMU 8.x re-fetches with the new page table after
    # a satp write; without this identity mapping that fetch raises an
    # instruction page fault and the kernel hangs with no output at all.
    .quad (0x80000 << 10) | 0xcf # VRWXAD
    # Entries 3~510: unmapped
    .zero 8 * 508
    # Entry 511: 0xffffffff_c0000000 map to 0x80000000 (1G)
    # PPN=0x80000, flag bits VRWXAD are all 1
    .quad (0x80000 << 10) | 0xcf # VRWXAD

    .global boot_hartid
boot_hartid:
    .quad 0
    .global boot_dtb
boot_dtb:
    .quad 0
