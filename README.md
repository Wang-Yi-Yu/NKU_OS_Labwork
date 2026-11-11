# 南开大学 COSC0009《操作系统》实验代码仓库

## 1. 小组成员

- 王奕瑜（组长）
- 鲁昕宁
- 张雨函

## 2. 实验结果链接

| 实验编号 |          代码          |               实验报告               |              AI 提示词               |
| :------: | :--------------------: | :----------------------------------: | :----------------------------------: |
|   lab1   | [Here](labcodes/lab1/) | [Here](labcodes/lab1/docs/report.md) |                  -                   |
|   lab2   | [Here](labcodes/lab2/) | [Here](labcodes/lab2/docs/report.md) | [Here](labcodes/lab2/docs/prompt.md) |
|   lab3   | [Here](labcodes/lab3/) | [Here](labcodes/lab3/docs/report.md) | [Here](labcodes/lab3/docs/prompt.md) |

# 3. QEMU 8.2+OpenSBI v1.3 环境下的适配修改

lab 框架原本针对 QEMU 4.x + OpenSBI v0.4 编写。本仓库的实验环境是 QEMU 8.2.x + OpenSBI v1.3（以及较新的 binutils），直接运行会出现内核不启动、静默卡死或汇编报错的问题。下面按出现的顺序列出需要做的适配修改，后续实验遇到框架回退成旧写法时可对照修改。

1. **用 `-kernel` 加载内核，不再用 `-device loader`。** 将 `Makefile` 中 `qemu`、`debug`、`test` 目标以及 `tools/grade.sh` 里的 `-device loader,file=bin/ucore.img,addr=0x80200000` 改为 `-kernel bin/ucore.img`。OpenSBI v1.3 使用 `fw_dynamic`，需要由 QEMU 通过 `-kernel` 告知下一级镜像的入口；用旧写法时启动信息里的 `Domain0 Next Address` 是 `0`，内核根本不会被执行。改对之后该项应为 `0x0000000080200000`。
2. **在 `kern/init/entry.S` 的启动页表中增加 `0x80000000` 的恒等映射。** 在 `boot_page_table_sv39` 的第 2 项（对应虚拟地址 `0x80000000`，1 GiB 大页）写入 `.quad (0x80000 << 10) | 0xcf`，权限为 VRWXAD。原因是 `csrw satp` 之后紧跟的几条指令仍然从低物理地址 `0x802000xx` 取指：QEMU 4.x 会在与 `csrw` 同一个已翻译的基本块内继续执行，只映射高地址就够；QEMU 8.x 在写 `satp` 之后会用新页表重新取指，缺少低地址映射会触发指令页错误，内核在没有任何输出的情况下卡死。
3. **把 `SAVE_ALL` 中的 `sbadaddr` 改为 `stval`（lab3 起）。** `trapentry.S` 里原有 `csrr s3, sbadaddr`，新版 binutils（本环境为 riscv64-unknown-elf-gcc 15.1）已经删除这个旧名字，汇编时报 `unknown CSR 'sbadaddr'`。`sbadaddr` 就是特权架构 1.9 中 `stval` 的旧称，改成 `csrr s3, stval` 后语义完全相同。`libs/riscv.h` 中的 `DECLARE_CSR(sbadaddr, ...)` 只是宏声明、没有被使用，不需要改。
4. **保留 lab2 的物理内存布局宏，以便回填 lab2 代码。** lab3 的 `kern/mm/memlayout.h` 骨架没有 `PHYSICAL_MEMORY_END`、`KERNEL_BEGIN_PADDR`、`KERNEL_BEGIN_VADDR`，而 lab2 的 `page_init` 需要用到 `PHYSICAL_MEMORY_END`，不加回去会编译失败。这一项与 QEMU 版本无关，但每次把 lab2 代码带到新实验时都要检查。
