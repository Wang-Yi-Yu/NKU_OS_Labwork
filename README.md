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
|   lab4   | [Here](labcodes/lab4/) |                  -                   |                  -                   |
|   lab5   | [Here](labcodes/lab5/) |                  -                   |                  -                   |

# 3. QEMU 8.2+OpenSBI v1.3 环境下的适配修改

lab 框架原本针对 QEMU 4.x + OpenSBI v0.4 编写。本仓库的实验环境是 QEMU 8.2.x + OpenSBI v1.3（以及较新的 binutils），直接运行会出现内核不启动、静默卡死或汇编报错的问题。下面按出现的顺序列出需要做的适配修改，后续实验遇到框架回退成旧写法时可对照修改。

1. **用 `-kernel` 加载内核，不再用 `-device loader`。** 将 `Makefile` 中 `qemu`、`debug`、`test` 目标以及 `tools/grade.sh` 里的 `-device loader,file=bin/ucore.img,addr=0x80200000` 改为 `-kernel bin/ucore.img`。OpenSBI v1.3 使用 `fw_dynamic`，需要由 QEMU 通过 `-kernel` 告知下一级镜像的入口；用旧写法时启动信息里的 `Domain0 Next Address` 是 `0`，内核根本不会被执行。改对之后该项应为 `0x0000000080200000`。
2. **在 `kern/init/entry.S` 的启动页表中增加 `0x80000000` 的恒等映射。** 在 `boot_page_table_sv39` 的第 2 项（对应虚拟地址 `0x80000000`，1 GiB 大页）写入 `.quad (0x80000 << 10) | 0xcf`，权限为 VRWXAD。原因是 `csrw satp` 之后紧跟的几条指令仍然从低物理地址 `0x802000xx` 取指：QEMU 4.x 会在与 `csrw` 同一个已翻译的基本块内继续执行，只映射高地址就够；QEMU 8.x 在写 `satp` 之后会用新页表重新取指，缺少低地址映射会触发指令页错误，内核在没有任何输出的情况下卡死。
3. **把 `SAVE_ALL` 中的 `sbadaddr` 改为 `stval`（lab3 起）。** `trapentry.S` 里原有 `csrr s3, sbadaddr`，新版 binutils（本环境为 riscv64-unknown-elf-gcc 15.1）已经删除这个旧名字，汇编时报 `unknown CSR 'sbadaddr'`。`sbadaddr` 就是特权架构 1.9 中 `stval` 的旧称，改成 `csrr s3, stval` 后语义完全相同。`libs/riscv.h` 中的 `DECLARE_CSR(sbadaddr, ...)` 只是宏声明、没有被使用，不需要改。
4. **保留 lab2 的物理内存布局宏，以便回填 lab2 代码。** lab3 的 `kern/mm/memlayout.h` 骨架没有 `PHYSICAL_MEMORY_END`、`KERNEL_BEGIN_PADDR`、`KERNEL_BEGIN_VADDR`，而 lab2 的 `page_init` 需要用到 `PHYSICAL_MEMORY_END`，不加回去会编译失败。这一项与 QEMU 版本无关，但每次把 lab2 代码带到新实验时都要检查。
5. **QEMU 8.x 不接受带 U/A/D 位的非叶子页表项（lab4 起）。** RISC-V 规范里非叶子项（R=W=X=0）的 U/A/D 是保留位，QEMU 8.x 会判定为非法翻译。lab4 骨架的 `get_pte()` 给中间层写 `PTE_U | PTE_V`，一旦真正通过这些中间项访问虚拟地址（`check_boot_pgdir()` 里 `page_insert` 映射 `0x100` 后 `strcpy`）就触发 store page fault；此时 `idt_init()` 还没执行，`stvec` 未设置，现象是内核反复重启、不停打印 `DTB Init`。可以用 `qemu ... -d int -D log` 看到 `store_page_fault`，`tval` 就是出错的虚拟地址。适配：`get_pte()` 创建中间页表项时改用 `ptd_create()`（只有 `PTE_V`），并删除 `check_pgdir()` 里 `assert(boot_pgdir_va[0] & PTE_U)`（它断言的正是这个写法）。叶子项上的 `PTE_U` 不受影响。
6. **GCC 15 把 `-Wint-conversion` 和 `-Wincompatible-pointer-types` 升级成错误（lab4 起）。** lab4 骨架的 `kmalloc.c`（整数传给 `kva2page()`）和 `tools/test_lab4.c`（`void (*)(void *)` 传给 `kernel_thread`）依赖旧行为。适配：在 `Makefile` 的 `CFLAGS` 里加 `-Wno-error=int-conversion -Wno-error=incompatible-pointer-types`，不改动老师给的代码；编译时会看到相应警告，属正常现象。
7. **GCC 15 把用户程序 `divzero` 里的除零折叠成别的值（lab5 起）。** `user/divzero.c` 原本靠 `1 / zero` 触发 RISC-V 的除零行为：RISC-V 的 `div` 指令不会陷入异常，而是返回 -1，评分脚本据此检查输出行 `value is -1.`。除零在 C 里是未定义行为，GCC 15 在 `-O2` 下把 `1 / x` 改写成 `(x + 1 < 3) ? x : 0`，`zero` 为 0 时程序打印的是 `value is 0.`，该用例拿不到分；只把 `zero` 改成 `volatile` 没有用，因为被折叠的是除法本身。适配：把被除数也改成 `volatile int one = 1;`，写成 `one / zero`，编译器就必须真正发出 `div` 指令。反汇编里能看到 `sltiu` 而没有 `div`，就是遇到了这个问题。
8. **lab5 把 lab2/3/4 的代码迁入骨架时要检查的几处（与 QEMU 版本无关）。**
   - `default_pmm.h` 声明了 `extern free_area_t free_area;`，所以 `default_pmm.c` 里的 `free_area` 不能是 `static`，否则报 `static declaration of 'free_area' follows non-static declaration`。
   - 评分脚本 `tools/grade.sh` 会查找 `memory management: default_pmm_manager`，所以 Best-Fit 管理器的 `.name` 要写成 `"default_pmm_manager"`（lab4 里用的是 `"best_fit_pmm_manager"`）。
   - lab4 的 `libs/riscv.h` 里 `lsatp()` 是 RV32 写法（`SATP32_MODE`），lab5 的 `proc_run` 和 `load_icode` 真正依赖它切换页表，必须使用 lab5 骨架里的 RV64 版本（`0x8000000000000000 | (pgdir >> RISCV_PGSHIFT)`）。
   - `trap_in_kernel()`、`print_pgdir()` 等：前者是骨架提供的函数，合并代码时不要丢；后者在骨架里只有声明、没有定义，而 `sys_pgdir` 需要它，要自己补上。
   - lab3 的时钟中断处理只打印 `100 ticks`，lab5 需要在每 `TICK_NUM` 个 tick 时设置 `current->need_resched = 1`，否则一个死循环的用户进程（`spin` 用例）永远不会被抢占。
