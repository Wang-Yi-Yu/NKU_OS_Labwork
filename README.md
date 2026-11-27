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
|   lab4   | [Here](labcodes/lab4/) | [Here](labcodes/lab4/docs/report.md) | [Here](labcodes/lab4/docs/prompt.md) |
|   lab5   | [Here](labcodes/lab5/) | [Here](labcodes/lab5/docs/report.md) | [Here](labcodes/lab5/docs/prompt.md) |
|   lab6   | [Here](labcodes/lab6/) | [Here](labcodes/lab6/docs/report.md) | [Here](labcodes/lab6/docs/prompt.md) |
|   lab7   | [Here](labcodes/lab7/) | [Here](labcodes/lab7/docs/report.md) | [Here](labcodes/lab7/docs/prompt.md) |
|   lab8   | [Here](labcodes/lab8/) | [Here](labcodes/lab8/docs/report.md) | [Here](labcodes/lab8/docs/prompt.md) |

## 3. QEMU 8.2+OpenSBI v1.3 环境下的适配修改

lab 框架原本针对 QEMU 4.x + OpenSBI v0.4 编写。本仓库的实验环境是 QEMU 8.2.x + OpenSBI v1.3（以及较新的 binutils），直接运行会出现内核不启动、静默卡死或汇编报错的问题。下面按出现的顺序列出需要做的适配修改，后续实验遇到框架回退成旧写法时可对照修改。

> 若某次实验并没有新的针对 QEMU 8.2+OpenSBI v1.3 环境的适配性修改，不要在改项下新增内容。

1. **用 `-kernel` 加载内核，不再用 `-device loader`。** 将 `Makefile` 中 `qemu`、`debug`、`test` 目标以及 `tools/grade.sh` 里的 `-device loader,file=bin/ucore.img,addr=0x80200000` 改为 `-kernel bin/ucore.img`。OpenSBI v1.3 使用 `fw_dynamic`，需要由 QEMU 通过 `-kernel` 告知下一级镜像的入口；用旧写法时启动信息里的 `Domain0 Next Address` 是 `0`，内核根本不会被执行。改对之后该项应为 `0x0000000080200000`。lab8 骨架同样是旧写法，评分脚本 `tools/grade.sh` 里对应的是变量 `qemuopts`（不是命令行），记得一并改掉。
2. **在 `kern/init/entry.S` 的启动页表中增加 `0x80000000` 的恒等映射。** 在 `boot_page_table_sv39` 的第 2 项（对应虚拟地址 `0x80000000`，1 GiB 大页）写入 `.quad (0x80000 << 10) | 0xcf`，权限为 VRWXAD。原因是 `csrw satp` 之后紧跟的几条指令仍然从低物理地址 `0x802000xx` 取指：QEMU 4.x 会在与 `csrw` 同一个已翻译的基本块内继续执行，只映射高地址就够；QEMU 8.x 在写 `satp` 之后会用新页表重新取指，缺少低地址映射会触发指令页错误，内核在没有任何输出的情况下卡死。
3. **把 `SAVE_ALL` 中的 `sbadaddr` 改为 `stval`（lab3 起）。** `trapentry.S` 里原有 `csrr s3, sbadaddr`，新版 binutils（本环境为 riscv64-unknown-elf-gcc 15.1）已经删除这个旧名字，汇编时报 `unknown CSR 'sbadaddr'`。`sbadaddr` 就是特权架构 1.9 中 `stval` 的旧称，改成 `csrr s3, stval` 后语义完全相同。`libs/riscv.h` 中的 `DECLARE_CSR(sbadaddr, ...)` 只是宏声明、没有被使用，不需要改。
4. **保留 lab2 的物理内存布局宏，以便回填 lab2 代码。** lab3 的 `kern/mm/memlayout.h` 骨架没有 `PHYSICAL_MEMORY_END`、`KERNEL_BEGIN_PADDR`、`KERNEL_BEGIN_VADDR`，而 lab2 的 `page_init` 需要用到 `PHYSICAL_MEMORY_END`，不加回去会编译失败。这一项与 QEMU 版本无关，但每次把 lab2 代码带到新实验时都要检查。
5. **QEMU 8.x 不接受带 U/A/D 位的非叶子页表项（lab4 起）。** RISC-V 规范里非叶子项（R=W=X=0）的 U/A/D 是保留位，QEMU 8.x 会判定为非法翻译。lab4 骨架的 `get_pte()` 给中间层写 `PTE_U | PTE_V`，一旦真正通过这些中间项访问虚拟地址（`check_boot_pgdir()` 里 `page_insert` 映射 `0x100` 后 `strcpy`）就触发 store page fault；此时 `idt_init()` 还没执行，`stvec` 未设置，现象是内核反复重启、不停打印 `DTB Init`。可以用 `qemu ... -d int -D log` 看到 `store_page_fault`，`tval` 就是出错的虚拟地址。适配：`get_pte()` 创建中间页表项时改用 `ptd_create()`（只有 `PTE_V`），并删除 `check_pgdir()` 里 `assert(boot_pgdir_va[0] & PTE_U)`（它断言的正是这个写法）。叶子项上的 `PTE_U` 不受影响。
6. **GCC 15 把 `-Wint-conversion` 和 `-Wincompatible-pointer-types` 升级成错误（lab4 起）。** lab4 骨架的 `kmalloc.c`（整数传给 `kva2page()`）和 `tools/test_lab4.c`（`void (*)(void *)` 传给 `kernel_thread`）依赖旧行为。适配：在 `Makefile` 的 `CFLAGS` 里加 `-Wno-error=int-conversion -Wno-error=incompatible-pointer-types`，不改动老师给的代码；编译时会看到相应警告，属正常现象。
7. **GCC 15 把用户程序 `divzero` 里的除零折叠成别的值（lab5 起）。** `user/divzero.c` 原本靠 `1 / zero` 触发 RISC-V 的除零行为：RISC-V 的 `div` 指令不会陷入异常，而是返回 -1，评分脚本据此检查输出行 `value is -1.`。除零在 C 里是未定义行为，GCC 15 在 `-O2` 下把 `1 / x` 改写成 `(x + 1 < 3) ? x : 0`，`zero` 为 0 时程序打印的是 `value is 0.`，该用例拿不到分；只把 `zero` 改成 `volatile` 没有用，因为被折叠的是除法本身。适配：把被除数也改成 `volatile int one = 1;`，写成 `one / zero`，编译器就必须真正发出 `div` 指令。反汇编里能看到 `sltiu` 而没有 `div`，就是遇到了这个问题。

## 4. 迁移前期实验代码时的一些修改

这里主要记录迁移前期实验代码过程中，需要修改前期实验代码的一些地方。

> 1. 若某次实验迁移前期代码时没有改动，无需在这里添加占位项
> 2. 若某次实验迁移前期代码时改动的地方在前期实验中已经有记录，无需重复添加
> 3. 不要添加对于后续实验没有参考意义的修改

### Lab 5

- `default_pmm.h` 声明了 `extern free_area_t free_area;`，所以 `default_pmm.c` 里的 `free_area` 不能是 `static`，否则报 `static declaration of 'free_area' follows non-static declaration`。
- 评分脚本 `tools/grade.sh` 会查找 `memory management: default_pmm_manager`，所以 Best-Fit 管理器的 `.name` 要写成 `"default_pmm_manager"`（lab4 里用的是 `"best_fit_pmm_manager"`）。
- lab4 的 `libs/riscv.h` 里 `lsatp()` 是 RV32 写法（`SATP32_MODE`），lab5 的 `proc_run` 和 `load_icode` 真正依赖它切换页表，必须使用 lab5 骨架里的 RV64 版本（`0x8000000000000000 | (pgdir >> RISCV_PGSHIFT)`）。
- `trap_in_kernel()`、`print_pgdir()` 等：前者是骨架提供的函数，合并代码时不要丢；后者在骨架里只有声明、没有定义，而 `sys_pgdir` 需要它，要自己补上。
- lab3 的时钟中断处理只打印 `100 ticks`，lab5 需要在每 `TICK_NUM` 个 tick 时设置 `current->need_resched = 1`，否则一个死循环的用户进程（`spin` 用例）永远不会被抢占。

### Lab 6

- `tools/grade.sh` 带 `-DDEBUG_GRADE` 编译，此时 `print_ticks()` 在第一个 100 ticks（约 1 秒）处打印 `End of Test.` 并 panic。`priority` 要跑约 2 秒，所以 lab6 的 `interrupt_handler()` 里不能再调用 `print_ticks()`，要像任务书写的那样每个 tick 都调用 `sched_class_proc_tick(current)`。lab5 的时钟代码（每 100 个 tick 打印并置位 `need_resched`）原样带过来会让 `priority` 的检查全部失败。
- 注入的测试 `tools/test_lab6.c` 在一个被 `memset` 清零的运行队列上调用 `default_sched_class.init()`，随后断言 `rq->max_time_slice == MAX_TIME_SLICE`，再断言入队后 `time_slice == MAX_TIME_SLICE`。所以 `RR_init()` 必须自己把 `max_time_slice` 设成 `MAX_TIME_SLICE`（`sched_init()` 在调用 `init` 之前设置过，但测试不经过它）。任务书对 `RR_init` 的描述里没有这一条。
- lab6 的 `schedule()` 不能再沿用 lab4/lab5 扫描 `proc_list` 的写法：`wakeup_proc()` 已经把进程放进运行队列，扫描链表会绕过调度类。要改成 "当前进程仍可运行则入队 → `pick_next` → `dequeue` → 为空则选 idleproc"。

### Lab 7

- 骨架里没有任何地方调用 `check_sync()`。要在 `init_main()` 里创建 `user_main` 之后、`do_wait` 循环之前加上 `extern void check_sync(void); check_sync();`，否则注入的测试一行都不会运行，得分是 0/100（已实测）。
- 时钟中断分支要调用 `run_timer_list()`：它负责唤醒 `do_sleep()` 的进程，然后自己调用 `sched_class_proc_tick(current)`。lab7 的 `sched_class_proc_tick` 是 `static` 的，lab6 里直接调用它的写法编译不过；不调用 `run_timer_list()` 则睡眠的哲学家永远醒不来。同 lab6，仍然不能调用 `print_ticks()`。
- `sync.h` 去掉了 `lock_t`、`lock()`、`unlock()`，所以 `mm_struct` 里的 `mm_lock` 换成了信号量 `mm_sem`：`mm_create()` 里 `sem_init(&mm->mm_sem, 1)`，`lock_mm()` / `unlock_mm()` 用 `down()` / `up()`。用户库里的 `lab6_setpriority` 改名为 `lab6_set_priority`，`priority.c` 的输出行也改成了 `stride sched correct result:`。
- `kern/sync/monitor.h` 的头文件保护宏写错了（`#ifndef` 用 `MONITOR`，`#define` 用 `MOINTOR`），编译会有 `-Wheader-guard` 警告，保护也没有生效，已改正。
- lab7 的 `default_sched.c` 和 `default_sched_stride.c` 在骨架里是空的，`sched_init()` 链接时需要 `default_sched_class`，所以要把 lab6 的 RR 和 Stride 迁过来。
- 任务书里 `phi_*_condvar` 的参数类型 `i32_t` 在代码里不存在。框架实际用 `int`，测试里的函数指针类型是 `void (*)(int32_t)`，两者一致。

### Lab 8

- 骨架里没有 `swap_offset()`：`kern/fs/swap/swapfs.c` 用到了这个宏，但 lab3 起已经没有 `swap.h`，整个仓库里都找不到定义。旧版 GCC 会在链接时报 undefined reference，GCC 15 直接把隐式函数声明当成编译错误。已在 `kern/fs/swap/swapfs.h` 里补上这个宏和 `max_swap_offset` 的声明（lab8 并不使用交换分区，只是为了让 `swapfs.c` 能编译）。
- `console.c` 要用带输入缓冲的新版（`cons_intr`、`serial_proc_data`，`cons_getc()` 从缓冲区取字符）。lab7 里空实现的 `kbd_intr()`/`serial_intr()` 不能带过来，否则 stdin 永远读不到字符。
- stdin 设备靠时钟中断喂数据：骨架的 `interrupt_handler()` 整个被挖空，补回 lab3/lab6/lab7 的版本之后，还要在 `IRQ_S_TIMER` 分支里调用 `dev_stdin_write(cons_getc())`。`dev_stdin_write()` 在骨架里没有头文件声明，GCC 15 下会报隐式声明错误，已在 `kern/fs/devs/dev.h` 补上声明。
- `init_main()` 的 lab8 版本先 `vfs_set_bootfs("disk0:")`，结尾调用 `fs_cleanup()`，并且不要再调用 lab7 的 `check_sync()`：哲学家线程的输出会和 sh 的输出交错，而评分脚本是逐行匹配输出的。
- `proc_init()` 里 idle 进程要自己 `files_create()` 再 `files_count_inc()`（`alloc_proc()` 只负责把 `filesp` 置 NULL），`do_exit()` 里要 `put_files(current)`。`do_execve()` 变成 `(name, argc, argv)`，`kernel_execve()` 变成 `(name, argv)`，仍然沿用 lab5 的做法：调完 `do_execve()` 直接跳到 `__trapret`，不需要原版的 `ebreak`。
- `pmm_init()`：lab8 骨架新增了 `switch_kernel_memorylayout()`，`boot_pgdir_va` 不再指向 `boot_page_table_sv39`，而是重新建的内核页表（`entry.S` 多了一页 `bootstackguard` 做栈保护页）。前面实验里"`pmm_init` 直接把 `boot_pgdir_va` 指向 `boot_page_table_sv39`"那段要换掉；第 3 节第 2 条的 `0x80000000` 恒等映射仍要保留。
- 用户库：骨架的 `user/libs/syscall.c` 没有 `sys_exec()`，要补；`printfmt`/`vprintfmt` 多了 `fd` 参数，`error_string` 要补 12 个新错误码的名字；`unistd.h` 里没有 `SYS_brk`；`forktree` 的 `DEPTH` 改成了 3。`umain(argc, argv)` 的参数来自 `a0`/`a1`，而 `initcode.S` 只是 `call umain`，所以 `load_icode()` 要在 trapframe 里设置 `a0 = argc`、`a1 = argv`（`sh` 不带参数时不设也能跑，带脚本参数时会读到错误的 `argc`）。
