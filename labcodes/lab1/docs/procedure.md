# Lab1 操作过程记录

> 本文档记录我们实际执行的命令、观察到的结果以及对结果的分析。实验环境为 WSL2 (Ubuntu, Linux 6.6.87) 。

## 1. 环境检查

首先确认交叉编译工具链、QEMU 和 tmux 是否就绪：

```console
$ which riscv64-unknown-elf-gcc riscv64-unknown-elf-gdb qemu-system-riscv64 tmux
/home/yiyuwang/riscv-elf-toolchains/bin/riscv64-unknown-elf-gcc
/home/yiyuwang/riscv-elf-toolchains/bin/riscv64-unknown-elf-gdb
/usr/bin/qemu-system-riscv64
/usr/bin/tmux

$ riscv64-unknown-elf-gcc --version | head -1
riscv64-unknown-elf-gcc (g1b306039ac4) 15.1.0

$ qemu-system-riscv64 --version | head -1
QEMU emulator version 8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.18)
```

**结果分析**：工具链完整。注意我们的 QEMU 是 **8.2.2** 版本，而实验指导书是按 QEMU 4.x + OpenSBI v0.6 编写的，这个版本差异后来导致了下述第 3 步的问题。

> **附记（2026-10-05）**：经与小组成员确认，各成员 WSL 环境的 QEMU 均为 8.x（**不要求小版本完全一致**：8.x 各版本的 `-kernel` 加载行为、fw_dynamic 固件接口、legacy SBI 扩展支持完全一致，仅固件横幅中 OpenSBI 版本号一行略有差异，不影响功能）。我们曾评估过按手册源码编译安装 QEMU 4.1.1（官方源 download.qemu.org 不可达，且 2019 年的代码在本机 gcc 13 + Python 3.14 上需要打 `-fcommon` 等补丁），最终**决定不安装 4.1.1，全组直接使用各自 WSL 的系统 QEMU 8.x**。手册要求"版本在 4.1.0 以上"，8.x 满足；唯一需要适配的地方就是各 lab Makefile 中 `qemu`/`debug` 目标的启动命令（见第 4 节，lab1 已完成修改），后续每个 lab 检查同一处即可。报告测试截图建议由同一位成员统一生成，避免不同成员机器上 OpenSBI 横幅版本号不一致。



## 2. 编译内核

```console
$ make
+ cc kern/init/entry.S
+ cc kern/init/init.c
+ cc kern/libs/stdio.c
+ cc kern/driver/console.c
+ cc libs/printfmt.c
+ cc libs/readline.c
+ cc libs/sbi.c
+ cc libs/string.c
+ ld bin/kernel
riscv64-unknown-elf-objcopy bin/kernel --strip-all -O binary bin/ucore.img
```

**结果分析**：

- 编译过程：把 `kern/` 和 `libs/` 下的所有 `.c/.S` 文件编译成 `obj/` 下的目标文件；
- 链接过程：`riscv64-unknown-elf-ld` 使用 `-T tools/kernel.ld` 链接脚本把所有目标文件链接为 ELF 格式的 `bin/kernel`；
- 镜像生成：`objcopy --strip-all -O binary` 把 ELF 转成纯二进制镜像 `bin/ucore.img`（剥掉符号表和调试信息，只保留各段内容）。
- 一次通过，无报错。



## 3. 第一次 make qemu：内核没有运行（版本兼容问题）

按照指导书使用原始 Makefile（`-device loader` 方式）运行：

```console
$ timeout 15 make qemu
OpenSBI v1.3
   ____                    _____ ____ _____
  ...（OpenSBI 横幅与平台信息）...
Domain0 Next Address      : 0x0000000000000000
Domain0 Next Mode         : S-mode
Boot HART MEDELEG         : 0x000000000000f0b509
```

**结果**：OpenSBI 横幅正常打印，但**始终没有出现** `(THU.CST) os is loading ...`，内核没有被执行。

**结果分析**：

- 关键线索在 `Domain0 Next Address : 0x0000000000000000`——OpenSBI 不知道要跳转到哪里去执行内核。
- 指导书使用的 QEMU 4.x 自带 OpenSBI **v0.6** 的 `fw_jump` 固件，它把"跳转到 0x80200000"**编译死在固件里**，所以配合 `-device loader,file=ucore.img,addr=0x80200000`（只是把数据裸搬运到内存）即可工作。
- 我们的 QEMU 8.2.2 自带 OpenSBI **v1.3** 的 `fw_dynamic` 固件，它通过 QEMU 动态传入的配置结构（fw_dynamic info，见第 5 步 GDB 观察到的 0x1028 处数据）获知内核入口地址；而 `-device loader` 不会生成这个配置，于是 `Next Address` 为 0，固件启动完就停在原地。
- 解决办法：改用 `-kernel` 参数。QEMU 的 riscv virt 机器在加载非 ELF 的裸二进制 `-kernel` 镜像时默认加载到 **0x80200000**，同时自动生成 fw_dynamic 配置告知 OpenSBI 入口地址。



## 4. 修改 Makefile 并重新验证

修改 `Makefile` 中 `qemu` 和 `debug` 两个目标（保留原命令为注释，便于对照）：

```makefile
qemu: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		-kernel $(UCOREIMG)
# 旧版 QEMU(4.x)/OpenSBI v0.6 可用 -device loader,file=$(UCOREIMG),addr=0x80200000，
# 但 QEMU 8.2 自带 OpenSBI v1.3 fw_dynamic，需通过 -kernel 告知固件内核入口地址
```

重新运行（内核进入死循环，故用 timeout 限时观察）：

```console
$ timeout 15 make qemu
OpenSBI v1.3
   ____                    _____ ____ _____
  / __ \                  / ____|  _ \_   _|
 | |  | |_ __   ___ _ __ | (___ | |_) || |
 | |  | | '_ \ / _ \ '_ \ \___ \|  _ < | |
 | |__| | |_) |  __/ | | |____) | |_) || |
  \____/| .__/ \___|_| |_|_____/|___/_____|
        | |
        |_|

Platform Name             : riscv-virtio,qemu
Platform HART Count       : 1
Platform Console Device   : uart8250
Firmware Base             : 0x80000000
Firmware Size             : 322 KB
Runtime SBI Version       : 1.0
Domain0 Name              : root
Domain0 Next Address      : 0x0000000080200000    <-- 修复后正确指向内核入口
Domain0 Next Mode         : S-mode                 <-- 以 S 模式移交内核
...
Boot HART MEDELEG         : 0x000000000000f0b509
(THU.CST) os is loading ...
```

**结果分析**：

- `Domain0 Next Address` 从 `0x0` 变为 `0x80200000`，证明 `-kernel` 成功把内核入口告知了 OpenSBI 固件；
- `(THU.CST) os is loading ...` 正常输出，说明 OpenSBI → `entry.S` → `kern_init` → `cprintf` 整条链路打通；
- 与手册的预期输出相比，OpenSBI 版本号/信息有差异（v0.6 vs v1.3），属正常环境差异。



## 5. GDB 跟踪启动流程（练习2）



### 5.1 启动调试环境

两个终端窗格（我们用脚本后台启动 QEMU 代替 tmux 左窗格，效果等同）：

- 左窗格：`make debug`（QEMU 以 `-s`（监听 1234 端口）`-S`（CPU 冻结，等待 GDB）启动）
- 右窗格：`make gdb`（加载 `bin/kernel` 符号、设置 `riscv:rv64` 架构、连接 `localhost:1234`）

连接成功后 GDB 显示：

```txt
0x0000000000001000 in ?? ()
```

即 **CPU 停在物理地址 0x1000**——这是 QEMU virt 机器的复位地址，说明"加电"后 PC 就是 0x1000。

### 5.2 观察复位向量（0x1000 处最初执行的指令）

```txt
(gdb) x/10i $pc
=> 0x1000:	auipc	t0,0x0
   0x1004:	addi	a2,t0,40
   0x1008:	csrr	a0,mhartid
   0x100c:	ld	a1,32(t0)
   0x1010:	ld	t0,24(t0)
   0x1014:	jr	t0
   ...

(gdb) x/8xg 0x1000
0x1000:	0x0282861300000297	0x0202b583f1402573
0x1010:	0x000280670182b283	0x0000000080000000
0x1020:	0x0000000087e00000	0x000000004942534f
0x1030:	0x0000000000000002	0x0000000080200000
```

**逐条分析**（这就是 RISC-V 加电后最初执行的指令）：


| 指令                 | 效果                                             |
| ------------------ | ---------------------------------------------- |
| `auipc t0, 0x0`    | t0 = 0x1000（当前 PC），作为访问 ROM 数据的基址              |
| `addi a2, t0, 40`  | a2 = 0x1028，指向 QEMU 为 fw_dynamic 准备的配置结构       |
| `csrr a0, mhartid` | a0 = 当前硬件线程号（hart 0）                           |
| `ld a1, 32(t0)`    | a1 = `*(0x1020)` = 0x87e00000，设备树（DTB）地址       |
| `ld t0, 24(t0)`    | t0 = `*(0x1018)` = **0x80000000**，OpenSBI 固件入口 |
| `jr t0`            | 跳转到 0x80000000，把 a0/a1/a2 作为参数移交给 OpenSBI      |


内存数据佐证：0x1018 处存着 0x80000000（OpenSBI 入口），0x1020 处存着 0x87e00000（DTB 地址，与 OpenSBI 打印的 `Domain0 Next Arg1` 一致），0x1028 处开始是 fw_dynamic 配置结构（`OSBI` 魔数、版本号 2、next address = 0x80200000）。**这段 MROM 代码只做了"取参数 + 跳转到 0x80000000"两件事**，其余所有初始化都由 OpenSBI 完成。

### 5.3 在内核入口设断点，验证 OpenSBI 移交控制权

```txt
(gdb) break *0x80200000
Breakpoint 1 at 0x80200000: file kern/init/entry.S, line 7.
(gdb) continue
Breakpoint 1, kern_entry () at kern/init/entry.S:7
7	    la sp, bootstacktop
(gdb) info registers pc
pc    0x80200000	0x80200000 <kern_entry>
```

**结果分析**：continue 之后 CPU 从 0x1000 → 0x80000000（OpenSBI 初始化、配置 PMP/中断代理等，即屏幕上那一大段 OpenSBI 输出）→ 跳转到 0x80200000，断点精确命中 `kern_entry` 第一条指令。这证明 OpenSBI 完成初始化后确实把 PC 移交到了 0x80200000，即内核链接脚本指定的入口地址。

### 5.4 测试手册提到的 watch *0x80200000 技巧

重新以 `make debug` 启动干净的 QEMU 实例后：

```txt
(gdb) watch *0x80200000
Hardware watchpoint 1: *0x80200000
(gdb) break *0x80200000
Breakpoint 2 at 0x80200000: file kern/init/entry.S, line 7.
(gdb) continue
Breakpoint 2, kern_entry () at kern/init/entry.S:7      <-- 先命中的是断点而不是观察点
```

**结果分析**：在整个启动过程中观察点**始终没有触发**，是断点先命中。原因是：使用 `-kernel` 参数时，QEMU 在**机器初始化阶段（CPU 执行第一条指令之前）**就把内核镜像放入了 0x80200000，之后 CPU 才从 0x1000 复位启动，因此不存在"运行时写入 0x80200000"的动作供观察点捕获。手册中该技巧针对的是"固件在运行阶段搬移内核"的场景（老版本环境），在我们 QEMU 8.2 + `-kernel` 的环境下观察点不会触发——这一点与手册描述不同，是环境差异造成的，特此记录。**结论：本环境中应直接用** `b *0x80200000` **验证内核开始执行。**

### 5.5 单步验证练习1的两条指令（la sp / tail）

断点命中 `kern_entry` 后连续单步：

```txt
(gdb) si                                # auipc sp, 0x3 —— la sp, bootstacktop 展开的第一条
0x0000000080200004 in kern_entry () at kern/init/entry.S:7
7           la sp, bootstacktop
(gdb) si                                # addi sp, sp, 0x1000 —— la 的第二条，sp 装载完成
9           tail kern_init
(gdb) info registers pc sp
pc             0x80200008       0x80200008 <kern_entry+8>
sp             0x80203000       0x80203000 <SBI_CONSOLE_PUTCHAR>

(gdb) si                                # tail kern_init 对应的 j 指令，跳入 kern_init
kern_init () at kern/init/init.c:8
8           memset(edata, 0, end - edata);
(gdb) info registers pc sp
pc             0x8020000a       0x8020000a <kern_init>
sp             0x80203000       0x80203000 <SBI_CONSOLE_PUTCHAR>
(gdb) x/3i $pc
=> 0x8020000a <kern_init>:      auipc   a0,0x3
   0x8020000e <kern_init+4>:    addi    a0,a0,-2
   0x80200012 <kern_init+8>:    auipc   a2,0x3

(gdb) info address bootstack
Symbol "bootstack" is at 0x80201000 in a file compiled without debugging.
(gdb) info address bootstacktop
Symbol "bootstacktop" is at 0x80203000 in a file compiled without debugging.
```

**结果分析**：

- `la sp, bootstacktop` 是伪指令，被汇编成 `auipc sp, 0x3` + `addi sp, sp, 0x1000` 两条；执行完 `sp = 0x80203000`，与符号 `bootstacktop` 的链接地址**完全一致**；
- 内核栈区为 `bootstack(0x80201000) ~ bootstacktop(0x80203000)`，大小 KSTACKSIZE = 2 页 = 8KB，位于 `.data` 段中，且按 PGSHIFT 对齐到页边界；
- `tail kern_init` 被汇编为一条 `j` 指令（尾调用，不压返回地址），执行后 `pc = 0x8020000a <kern_init>`，`sp` 不变——控制权干净地移交给 C 代码，`kern_init` 中第一件事就是 `memset(edata, 0, end - edata)` 清 .bss。



## 6. 待办（需要人工完成的部分）

- [ ] 组员按第 5.1 节流程用 tmux 亲自走一遍 `make debug` + `make gdb`，并截图（GDB 连接成功停在 0x1000、`b *kern_entry` / `c` 命中、`i r` 寄存器输出）放入 `report.md` 第五节；
- [ ] 截图 `make qemu` 成功输出（含 `(THU.CST) os is loading ...`）；
- [ ] report.md 中填写各练习负责人姓名；
- [ ] 注意：`docs/mynotes.md` 已被 `docs/.gitignore` 忽略，不会随仓库提交（个人笔记）。