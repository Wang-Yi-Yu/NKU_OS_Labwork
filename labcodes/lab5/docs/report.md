# 操作系统实验报告

## 实验基本信息

|     项目     |                      内容                      |
| :----------: | :--------------------------------------------: |
| **实验名称** |                 Lab5：用户程序                 |
| **小组成员** | 2414007-王奕瑜、2414015-鲁昕宁、2411622-张雨函 |
| **完成日期** |                   2026-11-29                   |

### 小组分工

练习如何分工？

|      成员      |                                                          负责的练习/模块                                                           |
| :------------: | :--------------------------------------------------------------------------------------------------------------------------------: |
| 2414007-王奕瑜 | 功能模块一：用户态系统调用功能、功能模块二：创建加载第一个用户进程功能、功能模块三：复制用户进程功能、功能模块四：其余系统调用功能 |
| 2414015-鲁昕宁 |                                                                 -                                                                  |
| 2411622-张雨函 |                                 Challenge: Copy on Write 机制、分支任务：gdb 调试系统调用以及返回                                  |

实验报告如何分工？

|      成员      |      负责的实验报告内容       |
| :------------: | :---------------------------: |
| 2414007-王奕瑜 | 3.1, 3.2.1-3.2.4, 4.1-4.4, 五 |
| 2414015-鲁昕宁 |            一, 六             |
| 2411622-张雨函 |        3.2.5, 4.5, 4.6        |

---

## 一、实验目的

本实验的主要目的是：

---

## 二、实验环境

你们使用的 AI 工具

|      成员      | AI 编程工具 |     底层模型      | 备注 |
| :------------: | :---------: | :---------------: | :--: |
| 2414007-王奕瑜 | Claude Code | Claude Sonnet 5.5 |  无  |
| 2414015-鲁昕宁 |   Cursor    |     Grok 4.7      |  无  |
| 2411622-张雨函 |     GPT     |       GPT-6       |  无  |

**说明：**

- **AI 编程工具**：指具体使用的终端工具、编辑器插件、桌面应用或浏览器界面
- **底层模型**：指该工具使用的大语言模型及版本

---

## 三、实验整体逻辑分析

### 3.1 本章节的逻辑主线

本章围绕**用户进程管理**这一核心主题展开：把 lab4 中只能运行在 S 态的内核线程，推进到可以运行在 U 态、并在需要时通过系统调用向内核要服务的用户进程，同时让用户进程从创建、运行到退出、回收的整个生命周期在内核中闭合。

整个实验解决的问题是：**如何让用户程序安全、可控地运行在操作系统之上**。它可以拆成三个层次：

1. **用户态与内核态之间的受控通道。** 用户程序运行在受限的 U 态，不能直接访问硬件和内核数据。它只能通过 `ecall` 触发异常进入内核，内核在 trap 入口保存现场、在出口恢复现场，并按系统调用号把请求分发给对应的内核函数，结果再写回用户可见的寄存器。
2. **第一次进入用户态的"鸡生蛋"问题。** 内核启动后一直在 S 态，没有任何"从用户态陷入"留下的现场可以返回。所以必须手工构造一个指向用户程序入口、用户栈和 U 态特权级的 trapframe，再借用异常返回路径（`__trapret` + `sret`）完成第一次特权级切换。
3. **用户进程的生命周期管理。** `fork` 复制地址空间产生子进程，`exec` 用新程序替换当前映像，`exit` 释放资源并变成僵尸，`wait` 回收僵尸的内核栈和 PCB，`yield`、`kill` 控制调度。进程树、地址空间、内核栈和 PCB 的创建与回收必须一一对应，否则会出现僵尸进程、孤儿进程或资源泄漏。

### 3.2 功能的逐步实现

#### 3.2.1 功能模块一：用户态系统调用功能

**为什么需要。** 用户程序没有权限直接使用内核资源，系统调用是它获得内核服务的唯一入口。如果没有这一层，后面的"启动用户进程""复制进程""退出进程"都无法被用户程序触发，也无法在从用户态陷入后正确返回。所以实验首先要打通"用户 `ecall` → 异常入口 → 系统调用分发 → 内核处理 → `sret` 返回用户态"这条链路：扩展 `RESTORE_ALL` 使它能区分返回用户态和返回内核态，让 `exception_handler` 识别 `ecall`，由 `syscall()` 按编号分发到各个 `sys_*`。

**与其他模块的关联。** 它是后面三个模块的基础。模块二里 `kernel_execve` 直接跳到 `__trapret`，依赖本模块扩展后的 `RESTORE_ALL` 才能进入用户态；模块三的 `do_fork` 和模块四的 `do_exit`、`do_yield`、`do_kill`、模块二的 `do_wait`、`do_execve` 都是通过 `sys_fork`、`sys_exit`、`sys_wait` 等处理函数才被用户程序调用的。本模块只做"解包参数并转发"，真正的语义落在后三个模块的 `do_*` 函数里。

---

#### 3.2.2 功能模块二：创建加载第一个用户进程功能

**为什么需要。** 有了系统调用通道，下一步要让第一个用户程序真正跑起来。内核启动时只有 `idleproc` 和 `initproc` 两个内核线程，需要把其中一个"变成"用户进程：`init_main` 创建 `user_main` 线程，`user_main` 调用 `kernel_execve` 加载嵌入内核镜像的 ELF，`do_execve` 回收旧地址空间，`load_icode` 建立新的地址空间并构造 trapframe，最后跳到 `__trapret` 以 `sret` 进入 U 态。`init_main` 则用 `do_wait` 等所有用户进程结束，作为启动流程的收尾点。

**与其他模块的关联。** 这一模块依赖模块一：`kernel_execve` 走的就是模块一扩展过的返回路径，`sscratch` 约定也要在这里被正确建立。它为模块三、模块四提供运行环境：`fork` 和 `exit` 都需要先有一个运行着的用户进程。同时 `do_wait` 与模块四的 `do_exit` 是一对：`do_exit` 把进程变成僵尸并唤醒父进程，`do_wait` 负责最终回收；`init_main` 里的 `do_wait` 循环还承担回收孤儿进程的职责。

---

#### 3.2.3 功能模块三：复制用户进程功能

**为什么需要。** 第一个用户进程只能靠 `kernel_execve` 从内核线程变出来，其他所有用户进程都应当由已有进程通过 `fork` 复制得到。`fork` 要让子进程"在逻辑上继承父进程，在物理上拥有独立的执行实体"，所以不仅要复制 PCB、内核栈和 trapframe，还要复制用户地址空间：按 VMA 复制合法区间，再按页复制内容并建立新的页表映射。

**与其他模块的关联。** 它通过模块一的 `sys_fork` 被用户程序触发，子进程从 `fork` 返回 0 靠的是模块一的 trapframe 返回路径。复制出来的子进程拥有的 `mm`、页表、内核栈和 PCB，都要由模块四的 `do_exit`（地址空间）和模块二的 `do_wait`（内核栈、PCB）回收，所以创建时的引用计数、进程树关系必须和回收时的假设一致。`copy_mm` 里的出错回滚用到了模块四同样使用的 `exit_mmap`、`put_pgdir`、`mm_destroy`。

---

#### 3.2.4 功能模块四：其余系统调用功能

**为什么需要。** 进程的生命周期必须闭环：有创建就要有结束。`do_exit` 让进程释放用户地址空间、变成僵尸、通知父进程并把自己的子进程过继给 `initproc`；`do_yield` 让进程主动让出 CPU；`do_kill` 让一个进程在合适的时机退出。没有它们，进程一旦创建就无法正常结束，父进程的 `wait` 也永远等不到结果。

**与其他模块的关联。** 它们由模块一的 `sys_exit`、`sys_yield`、`sys_kill` 触发；`do_exit` 和模块二的 `do_wait` 通过 `PROC_ZOMBIE`、`wait_state`、`WT_CHILD` 配合工作，`do_kill` 设置的 `PF_EXITING` 由 `trap()` 在返回用户态前检查并调用 `do_exit(-E_KILLED)`。`do_exit` 释放地址空间的方式与模块二的 `do_execve` 一致（先切换到内核页表，再按引用计数释放），它把孤儿过继给 `initproc` 之后，由 `init_main` 的 `do_wait` 循环最终回收，形成完整闭环。

---

#### 3.2.5 Challenge: Copy on Write 机制

---

## 四、实验内容与实现

### 4.1 功能模块一：用户态系统调用功能

**负责人：** [2414007-王奕瑜]

#### 4.1.1 模块功能描述

**需要实现/修改的函数：**

```c
.macro RESTORE_ALL                                   // kern/trap/trapentry.S
void exception_handler(struct trapframe *tf)         // kern/trap/trap.c
void syscall(void)                                   // kern/syscall/syscall.c
static int sys_exit(uint64_t arg[])                  // kern/syscall/syscall.c
static int sys_fork(uint64_t arg[])
static int sys_wait(uint64_t arg[])
static int sys_exec(uint64_t arg[])
static int sys_yield(uint64_t arg[])
static int sys_kill(uint64_t arg[])
static int sys_getpid(uint64_t arg[])
static int sys_putc(uint64_t arg[])
static int sys_pgdir(uint64_t arg[])
void print_pgdir(void)                               // kern/mm/pmm.c（sys_pgdir 依赖，骨架中只有声明）
```

**功能说明：**

这一模块把用户态通过 `ecall` 发出的请求接入内核，再把结果写回用户态寄存器。整条链路由四部分组成：

1. **`RESTORE_ALL`：区分返回用户态和返回内核态。** lab3 的 `RESTORE_ALL` 只服务于纯内核系统，恢复完寄存器就 `sret`。lab5 约定：用户态运行时 `sscratch` 保存内核栈顶，内核态运行时 `sscratch` 为 0，`SAVE_ALL` 靠交换 `sp` 和 `sscratch` 判断陷入来源。因此返回时必须把 `sscratch` 设成正确的值。实现上先从 trapframe 取出 `sstatus` 和 `sepc`，用 `andi` 检查 `SSTATUS_SPP`：`SPP == 1` 说明要返回内核，直接跳过，`sscratch` 保持 `SAVE_ALL` 清零后的 0；`SPP == 0` 说明要返回用户态，把 `sp + 36 * REGBYTES`（本 trapframe 之上，即内核栈顶）写入 `sscratch`，下一次陷入时 `SAVE_ALL` 才能找到内核栈。这一步必须在 `sp` 被换成用户栈指针之前完成，所以放在恢复通用寄存器之前，`x2` 仍然最后一个恢复。宏内的跳转标号用 `LOCAL` 声明，避免多次展开冲突。
2. **`exception_handler`：识别用户态 `ecall`。** 在原有的非法指令、断点分支之外增加 `CAUSE_USER_ECALL` 分支：先把 `tf->epc` 加 4（`ecall` 是 32 位指令），再调用 `syscall()`。必须先前移 `epc`，否则 `sret` 回去会重新执行同一条 `ecall`，系统调用会无限重复。
3. **`syscall()`：取号、取参、分发、写回。** 从 `current->tf` 里读出 `a0` 作为系统调用号，`a1`~`a5` 作为 `arg[0]`~`arg[4]`（与用户库 `user/libs/syscall.c` 的寄存器约定一一对应），调用 `syscalls[num](arg)`，返回值写回 `tf->gpr.a0`，用户程序 `sret` 回去后就从 `a0` 读到结果。用户可以传入任意整数，所以要同时检查 `num >= 0`、`num < NUM_SYSCALLS` 和表项非空；不合法的编号不分发，直接把 `-E_INVAL` 写入 `a0` 并返回，不能 panic，也不能通过空指针调用。
4. **`sys_*`：把统一编号的请求翻译成内核语义。** 这些函数只负责解包参数并转交给进程管理层，真正的权限检查和资源更新都在对应的 `do_*` 里完成：`sys_exit` → `do_exit(arg[0])`；`sys_fork` 以 `clone_flags = 0`、当前 trapframe 的 `sp` 为子进程用户栈指针、当前 trapframe 为模板调用 `do_fork`；`sys_wait` 把 PID 和用户态地址转交 `do_wait`（指针由 `do_wait` 用 `user_mem_check` 检查）；`sys_exec` 把名字、长度、ELF 映像和大小转交 `do_execve`；`sys_yield`、`sys_kill` 对应 `do_yield`、`do_kill`；`sys_getpid` 返回 `current->pid`；`sys_putc` 用 `cputchar` 输出一个字符（用户态不能直接访问控制台，用户库的 `cprintf` 每个字符都经由这个调用）；`sys_pgdir` 调用 `print_pgdir`。
5. **`print_pgdir`：补上骨架缺失的定义。** 骨架里 `print_pgdir` 只有声明，`sys_pgdir` 和用户程序 `pgdir` 都需要它，不实现则链接失败。它利用 `pmm.c` 中已有的 `get_pgtable_items` 和 `perm2str`，从 `satp` 找到当前页表的根，按 Sv39 三级页表逐级把"权限相同的连续有效表项"打印成一行：带 R/W/X 的是叶子（大页或页），不再下降；不带的指向下一级页表，缩进后继续打印。虚拟地址按 Sv39 规则做符号扩展。

一次完整的系统调用就是：`ecall` 进入 `__alltraps`（`SAVE_ALL` 保存现场，`sscratch` 清零）→ `trap()` → `exception_handler` 前移 `epc` → `syscall()` 分发并写回 `a0` → 回到 `__trapret`（`RESTORE_ALL` 恢复现场并设置 `sscratch`）→ `sret` 回到用户态。

#### 4.1.2 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

````markdown
[PROMPT]
**任务**：补全 lab5 的"用户态系统调用"功能模块。涉及四个文件：`kern/trap/trapentry.S` 中的 `RESTORE_ALL` 宏（扩展为同时支持返回用户态和返回内核态）、`kern/trap/trap.c` 中的 `exception_handler`（增加用户态 `ecall` 的处理）、`kern/syscall/syscall.c` 中的 `syscall()` 分发函数和九个 `sys_*` 处理函数、`kern/mm/pmm.c` 中的 `print_pgdir`（`sys_pgdir` 依赖它，骨架里只有声明没有定义）。

**操作要求**：你必须在项目中修改实际文件，保留现有代码，只在标有 `lab5 function1 YOUR CODE` 的位置补全，并把其中的 `YOUR CODE` 替换为学号 `2414007`；`exception_handler` 里已有的 lab3 分支保持不变。所有新增代码的注释使用英文，函数使用 Doxygen 风格。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息，以工程真实代码为准，不要引入无关功能。

[RELY]
1. 系统调用约定（`user/libs/syscall.c`、`libs/unistd.h`）：用户态把系统调用号放在 `a0`，参数依次放在 `a1`~`a5`，执行 `ecall`，返回值从 `a0` 读取。编号：`SYS_exit=1, SYS_fork=2, SYS_wait=3, SYS_exec=4, SYS_yield=10, SYS_kill=12, SYS_getpid=18, SYS_putc=30, SYS_pgdir=31`。
2. 陷入框架：`struct trapframe`（`kern/trap/trap.h`）含 `gpr`（其中有 `a0`~`a5`、`sp`）、`status`、`epc`、`tval`、`cause`；`current->tf` 在 `trap()` 里指向本次陷入的 trapframe；`CAUSE_USER_ECALL`（值为 8）表示用户态 `ecall`；`SSTATUS_SPP`、`REGBYTES`、`STORE/LOAD` 来自 `riscv.h`。
3. 内核栈与 `sscratch` 的约定：用户态运行时 `sscratch` 保存内核栈顶，内核态运行时 `sscratch` 为 0。已有的 `SAVE_ALL` 通过交换 `sp` 与 `sscratch` 区分来源，并在栈上建立 36 个字（`36 * REGBYTES`）的 trapframe，随后把 `sscratch` 清零。`__trapret` 先执行 `RESTORE_ALL` 再执行 `sret`。
4. `syscall.c` 中已有分发表 `syscalls[]`（用 `[SYS_xxx] 处理函数` 的形式初始化）和宏 `NUM_SYSCALLS`。
5. 进程管理接口（`kern/process/proc.h`）：`int do_exit(int error_code)`、`int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)`、`int do_wait(int pid, int *code_store)`、`int do_execve(const char *name, size_t len, unsigned char *binary, size_t size)`、`int do_yield(void)`、`int do_kill(int pid)`；`current` 为当前进程。控制台输出函数 `void cputchar(int c)`；错误码 `E_INVAL`（`libs/error.h`）。
6. `pmm.c` 中已有静态辅助函数 `perm2str(int perm)`（把权限位格式化成 3 个字符）和 `get_pgtable_items(left, right, start, table, left_store, right_store)`（在页表 `table[start..right)` 中找下一段"有效且权限相同"的连续表项，返回权限位，找不到返回 0）；Sv39 三级页表常量 `NPDEENTRY`、`NPTEENTRY`、`PDX1SHIFT`、`PDX0SHIFT`、`PTXSHIFT`、`PTE_R/W/X/V/U`、`PDE_ADDR`、`KADDR`；`read_csr(satp)` 可读出当前页表根的物理页号（低 44 位）。

[GUARANTEE]
需要实现或修改以下接口，签名必须与现有声明一致：

```c
.macro RESTORE_ALL                                  // kern/trap/trapentry.S
void exception_handler(struct trapframe *tf)        // kern/trap/trap.c
void syscall(void)                                  // kern/syscall/syscall.c
static int sys_exit(uint64_t arg[])                 // kern/syscall/syscall.c
static int sys_fork(uint64_t arg[])
static int sys_wait(uint64_t arg[])
static int sys_exec(uint64_t arg[])
static int sys_yield(uint64_t arg[])
static int sys_kill(uint64_t arg[])
static int sys_getpid(uint64_t arg[])
static int sys_putc(uint64_t arg[])
static int sys_pgdir(uint64_t arg[])
void print_pgdir(void)                              // kern/mm/pmm.c
```

`sys_*` 必须定义在 `syscalls[]` 之前，保持 `static`。允许在 `pmm.c` 内新增 `static` 辅助函数，不得改变公开接口。

[SPECIFICATION]
**RESTORE_ALL**
- Pre-Condition：`sp` 指向一个完整的 trapframe（`SAVE_ALL` 或 `load_icode()` 构造的）；该 trapframe 的 `status` 与 `epc` 已是希望返回时使用的值。
- Post-Condition：除 `sp` 之外的通用寄存器、`sstatus`、`sepc` 都已按 trapframe 恢复；`sp` 等于被中断代码的 `sp`；`sscratch` 满足"返回用户态则保存内核栈顶，返回内核态则为 0"的约定。
- Case 1：trapframe 的 `sstatus.SPP == 0`（将返回用户态）。把本 trapframe 之上的地址（`sp + 36 * REGBYTES`，即内核栈顶）写入 `sscratch`，这一步必须在 `sp` 被换成用户栈指针之前完成。
- Case 2：`SPP == 1`（将返回内核态）。不改动 `sscratch`，它保持为 0，这样嵌套陷入时 `SAVE_ALL` 能判断出来源是内核。
- Requirements：`sp` 必须是最后一个被恢复的寄存器；`s0~s2` 只用作中转，之后会被 trapframe 中的值覆盖。宏内的标号用 `LOCAL` 声明，保证多次展开不冲突。

**exception_handler**
- Pre-Condition：`tf` 是本次异常的 trapframe，`tf->cause` 的最高位为 0。
- Post-Condition：对 `CAUSE_USER_ECALL`，系统调用已被执行，结果已写入 `tf->gpr.a0`，且 `tf->epc` 已越过那条 `ecall` 指令。
- Case 1：`cause` 为非法指令或断点。保持 lab3 的原有行为。
- Case 2：`cause == CAUSE_USER_ECALL`。先把 `tf->epc` 前移 4 字节（`ecall` 恒为 32 位指令），再交给 `syscall()` 处理；不得先分发后前移。
- Case 3：其他异常。保持原有行为（打印 trapframe）。

**syscall**
- Pre-Condition：由 `exception_handler` 调用，`current->tf` 是发起系统调用的进程的 trapframe。
- Post-Condition：`current->tf->gpr.a0` 中是本次系统调用的返回值。
- Case 1：`a0` 中的编号在 `[0, NUM_SYSCALLS)` 内且对应表项非空。依次取 `a1`~`a5` 作为 `arg[0]`~`arg[4]`，调用处理函数，把返回值写入 `a0`。
- Case 2：编号为负、不小于 `NUM_SYSCALLS`，或表项为空。不得调用任何函数，不得 panic，把 `-E_INVAL` 写入 `a0` 并返回（调用号 0 属于这一类）。
- Requirements：必须同时检查上界、下界和空表项，用户可以传入任意整数。

**各 sys_* 处理函数**（只负责解包参数并调用进程管理接口，真正的语义和检查在 `do_*` 中完成）
- `sys_exit`：`arg[0]` 为退出码，返回 `do_exit` 的结果（成功时不会返回）。
- `sys_fork`：以 `clone_flags = 0`，用户栈指针取当前 trapframe 的 `sp`，当前 trapframe 为模板调用 `do_fork`，返回子进程 PID。
- `sys_wait`：`arg[0]` 是 PID（0 表示任意子进程），`arg[1]` 是用户态地址，转交 `do_wait`；指针的合法性由 `do_wait` 检查。
- `sys_exec`：`arg[0]`、`arg[1]` 是程序名与长度，`arg[2]`、`arg[3]` 是 ELF 映像与大小，转交 `do_execve`。
- `sys_yield`、`sys_kill`：分别调用 `do_yield`、`do_kill(arg[0])`。
- `sys_getpid`：返回 `current->pid`。
- `sys_putc`：把 `arg[0]` 作为字符交给 `cputchar`，返回 0。
- `sys_pgdir`：调用 `print_pgdir()`，返回 0。

**print_pgdir**
- Pre-Condition：在内核态调用，`satp` 指向一个有效的 Sv39 页表。
- Post-Condition：在 BEGIN/END 横幅之间逐级打印当前生效页表，每行对应一段"权限相同的连续有效表项"；不修改任何页表。
- Case 1：表项带有 R/W/X 之一（大页或叶子），只打印该行，不再下降。
- Case 2：表项无 R/W/X（指向下一级页表），打印该行后下降到下一级，行首缩进体现层次。
- Requirements：虚拟地址按 Sv39 规则做符号扩展；通过 `satp` 找到页表根，不依赖 `current`。
````

#### 4.1.3 实现迭代过程

本模块的实现经历了 1 次迭代，过程如下：

---

##### 第一次迭代

提示词生成的代码一次编译通过，没有出现需要修复的问题。

**最终结果：**

经过 1 次迭代，该模块最终：

- ✅ 编译通过
- ✅ 注入的测试 `[Test Syscall] PASS` 通过：伪造的 `SYS_getpid` 的 `ecall` 使 `epc` 前进 4 字节并返回当前 PID，调用号 0 返回 `-E_INVAL`
- ✅ `hello`、`yield`、`pgdir` 等依赖系统调用的用户程序输出符合预期（`make grade` 中对应项全部通过）
- ✅ 功能符合预期

---

### 4.2 功能模块二：创建加载第一个用户进程功能

**负责人：** [2414007-王奕瑜]

#### 4.2.1 模块功能描述

**需要实现/修改的函数：**

```c
static struct proc_struct *alloc_proc(void)                                      // kern/process/proc.c（更新）
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)            // （核对，无需修改）
static int init_main(void *arg)                                                  // （更新）
static int user_main(void *arg)
static int kernel_execve(const char *name, unsigned char *binary, size_t size)
int do_wait(int pid, int *code_store)
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size)
static int load_icode(unsigned char *binary, size_t size)
```

**功能说明：**

这一模块让系统从"只有内核线程"推进到"第一个用户程序开始执行"，启动链是：`proc_init` → `init_main` → `user_main` → `kernel_execve` → `do_execve` → `load_icode` → `__trapret`/`sret` → 用户程序入口。

1. **`alloc_proc`（更新）。** 在 lab4 的初始化基础上，给 lab5 新增的字段赋空值：`exit_code = 0`、`wait_state = 0`、`cptr/yptr/optr = NULL`。`kmalloc` 返回的内存不清零，而 `set_links`、`do_wait` 会读这些字段，所以不初始化就是 bug。
2. **`kernel_thread`（核对）。** 它构造的临时 trapframe 使用 `SPP = 1`、`SPIE = 1`、`SIE = 0`，经过扩展后的 `RESTORE_ALL` 会走内核分支（`sscratch` 保持 0），`sret` 后以 S 态、开中断进入 `kernel_thread_entry`，不需要修改。`init_main` 也用它创建 `user_main`。
3. **`init_main`（更新）。** 记录入口处的 `nr_process`，用 `kernel_thread` 创建 `user_main`；然后循环 `do_wait(0, NULL)` 回收所有子进程，每成功一次调用一次 `schedule()`，直到没有子进程；最后打印 `all user-mode processes have quit.`，检查进程树已回到起点（`initproc` 无亲属，`nr_process` 与入口一致，`proc_list` 中只剩 `initproc`），打印 `init check memory pass.`。
4. **`user_main`。** 若构建时定义了 `TEST`（`make build-x`），用 `KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE)` 启动指定程序，否则用 `KERNEL_EXECVE(exit)`。`kernel_execve` 不返回，返回就 panic。
5. **`kernel_execve`。** 直接调用 `do_execve` 行不通：它只准备好新映像和 trapframe，执行流还在 S 态的内核线程里。当前没有真实的 trap 可以返回，所以用内联汇编手工进入返回路径：把 `sp` 指向 `current->tf`（`load_icode` 写好的、位于内核栈顶的 trapframe），再 `j __trapret`，由 `RESTORE_ALL` 恢复寄存器并设置 `sscratch`，`sret` 后落到用户程序入口。
6. **`do_execve`。** 先用 `user_mem_check` 检查名字在调用者地址空间中可读（内核线程 `mm` 为 NULL，只要求是内核地址），并把名字复制到内核局部缓冲区，因为旧地址空间随后会被销毁。如果有旧 `mm`：先切到内核页表 `boot_pgdir_pa`（同时更新 `current->pgdir`），避免继续使用即将被释放的页表；再把 `mm_count` 减 1，减到 0 时依次 `exit_mmap`（释放页和页表）、`put_pgdir`、`mm_destroy`。然后调用 `load_icode`；成功就更新进程名，失败则旧映像已不存在，只能 `do_exit`。
7. **`load_icode`。** 新建 `mm` 和页目录（`setup_pgdir` 会把内核映射复制进去）；检查 `e_magic`，不对则返回 `-E_INVAL_ELF`；遍历 `PT_LOAD` 段，按 `ELF_PF_X/W/R` 设置 VMA 标志，PTE 权限含 `PTE_U|PTE_V`（可写段同时带 `PTE_R`），用 `mm_map` 建立 VMA，再逐页分配并拷贝 `p_filesz` 字节；`p_memsz` 超出 `p_filesz` 的 bss 部分清零，其中包括文件数据最后一页的剩余部分（刚分配的页内容不是零）。随后建立 `VM_STACK` 用户栈 VMA（`USTACKTOP` 之下 `USTACKSIZE` 字节）；由于本实验没有缺页处理，不能依赖按需分配，所以只为栈顶 4 页分配物理页（`USTACKSIZE` 为 256 页，全部分配会让每次 `fork` 都复制 1 MiB）。接着把新 `mm` 设为当前进程的地址空间：引用计数设为 1，写 `current->mm`、`current->pgdir` 和 `satp`，并刷新 TLB。最后重写 trapframe：`sp = USTACKTOP`、`epc = e_entry`，`status` 清除 `SSTATUS_SPP`（`sret` 返回 U 态）、置位 `SSTATUS_SPIE`（返回后开中断），其余寄存器清零。任何一步失败，都按逆序释放已建立的页、页目录和 `mm_struct`。
8. **`do_wait`。** 若 `code_store` 非 NULL，先用 `user_mem_check` 确认当前进程对它可写，否则返回 `-E_INVAL`。然后在子进程中查找 `PROC_ZOMBIE`：`pid == 0` 遍历所有子进程，否则只看指定的、且 `parent == current` 的子进程。找不到子进程返回 `-E_BAD_PROC`；有子进程但都没退出时，置 `PROC_SLEEPING`、`wait_state = WT_CHILD` 并 `schedule()`，唤醒后若自己已被标记 `PF_EXITING` 就 `do_exit(-E_KILLED)`，否则重新查找。找到僵尸后（`idleproc` 和 `initproc` 绝不回收），保存退出码，关中断从哈希表和进程树中摘除，再释放内核栈和 PCB。这两样东西在子进程执行 `do_exit` 时还在使用，只有父进程能释放。

#### 4.2.2 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

````markdown
[PROMPT]
**任务**：补全 lab5 的"创建并加载第一个用户进程"功能模块，修改 `kern/process/proc.c`：更新 `alloc_proc`、`kernel_thread`、`init_main`，新增 `user_main`、`kernel_execve`、`do_wait`、`do_execve`、`load_icode`。目标是让内核线程 `init_main` 创建 `user_main`，`user_main` 通过 `kernel_execve` 把自己变成一个运行在用户态的进程，`init_main` 再通过 `do_wait` 等它退出。

**操作要求**：你必须在项目中修改实际文件，保留现有代码，只在标有 `lab5 function2` 的位置补全或更新，并把其中的 `YOUR CODE` 替换为学号 `2414007`；lab4 版本的 `alloc_proc`、`kernel_thread`、`init_main` 是基线，在其上更新，不要删除原有逻辑。所有新增代码的注释使用英文，函数使用 Doxygen 风格。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息，以工程真实代码为准，不要引入无关功能。

[RELY]
1. 进程控制块 `struct proc_struct`（`proc.h`）：除 lab4 的字段外新增 `exit_code`、`wait_state`、`cptr`、`yptr`、`optr`；状态 `PROC_SLEEPING`、`PROC_ZOMBIE`；等待标志 `WT_CHILD`、`PF_EXITING`；`PROC_NAME_LEN`。`kmalloc` 返回的内存不清零。
2. 地址空间（`vmm.h`、`pmm.h`）：`struct mm_struct`（`pgdir`、`mm_count`）、`mm_create()`、`mm_destroy()`、`mm_map(mm, addr, len, vm_flags, vma_store)`、`exit_mmap()`、`mm_count_inc/dec()`、`user_mem_check(mm, addr, len, write)`、`VM_READ/WRITE/EXEC/STACK`；`setup_pgdir(mm)`、`put_pgdir(mm)`（`proc.c` 内的静态函数）、`pgdir_alloc_page(pgdir, la, perm)`、`page2kva`、`PADDR`、`boot_pgdir_pa`、`lsatp(pa)`、`flush_tlb()`、`PTE_U/R/W/X/V`、`PTE_USER`。
3. ELF（`libs/elf.h`）：`struct elfhdr`（`e_magic`、`e_entry`、`e_phoff`、`e_phnum`）、`struct proghdr`（`p_type`、`p_flags`、`p_offset`、`p_va`、`p_filesz`、`p_memsz`）、`ELF_MAGIC`、`ELF_PT_LOAD`、`ELF_PF_X/W/R`。
4. 用户空间布局（`memlayout.h`）：`USTACKTOP`、`USTACKSIZE`、`USER_ACCESS`。本实验没有缺页处理，页必须在装载时就分配好。
5. trapframe 与返回路径：`struct trapframe`（`gpr.sp`、`epc`、`status`）、`SSTATUS_SPP`、`SSTATUS_SPIE`、`SSTATUS_SIE`；汇编符号 `__trapret`（恢复寄存器并 `sret`，已按 `status.SPP` 区分用户/内核返回）；`current->tf` 指向内核栈顶的 trapframe。
6. 已有的宏 `KERNEL_EXECVE(x)`、`KERNEL_EXECVE2(x, xstart, xsize)`、`__KERNEL_EXECVE`；构建 `make build-x` 时会定义 `TEST`、`TESTSTART`、`TESTSIZE`。
7. 已有函数：`kernel_thread`/`do_fork`、`find_proc`、`unhash_proc`、`remove_links`、`put_kstack`、`set_proc_name`、`schedule`、`do_exit(int)`、`nr_process`、`proc_list`、`initproc`、`idleproc`。错误码 `E_INVAL`、`E_INVAL_ELF`、`E_NO_MEM`、`E_BAD_PROC`、`E_KILLED`。

[GUARANTEE]
需要实现或修改以下函数，签名必须与现有声明一致：

```c
static struct proc_struct *alloc_proc(void)
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
static int init_main(void *arg)
static int user_main(void *arg)
static int kernel_execve(const char *name, unsigned char *binary, size_t size)
int do_wait(int pid, int *code_store)
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size)
static int load_icode(unsigned char *binary, size_t size)
```

`user_main` 必须定义在 `init_main` 之前，`kernel_execve` 必须定义在 `user_main` 之前。允许新增 `static` 辅助函数，不得改变公开接口。

[SPECIFICATION]
**alloc_proc**
- Pre-Condition：无。
- Post-Condition：返回的 PCB 所有字段都处于确定的"空"状态：在 lab4 已有初始化的基础上，`exit_code` 为 0、`wait_state` 为 0、`cptr/yptr/optr` 为 NULL；仍为 `PROC_UNINIT`。
- Case：`kmalloc` 失败时返回 NULL。
- Requirements：`do_wait`、`set_links` 会读取新字段，不得留未初始化的值。

**kernel_thread**
- Post-Condition：沿用 lab4 构造临时 trapframe 的做法，使创建出的线程经 `sret` 以 S 态、开中断的状态进入 `kernel_thread_entry`；该函数也被 `init_main` 用来创建 `user_main`。需要确认它与扩展后的 `RESTORE_ALL` 兼容（`SPP=1` 走内核分支），一般无需改动。

**init_main**
- Pre-Condition：作为 `initproc` 的入口，由 `proc_init` 创建；参数不再使用。
- Post-Condition：`user_main` 对应的整棵子进程树都已被回收，打印 `all user-mode processes have quit.` 和 `init check memory pass.`，返回 0。
- Case 1：入口处记录 `nr_process`；用 `kernel_thread` 创建 `user_main`，创建失败则 panic。
- Case 2：循环调用 `do_wait(0, NULL)`，每成功回收一个子进程就调用一次 `schedule()`，直到没有子进程为止。
- Case 3：循环结束后检查进程表已回到起点：`initproc` 没有任何亲属指针，`nr_process` 等于入口处的值，`proc_list` 中只剩 `initproc`（idleproc 不在链表中）。
- Requirements：**不要**断言空闲物理页数与入口时相等。内核堆（SLOB）在同时存在很多 PCB/mm/VMA（如 forktest）时会向页分配器多要几页且永不归还，`kallocated()` 恒为 0，这样的断言会在没有真实泄漏时失败。

**user_main**
- Post-Condition：不返回；若返回则 panic。
- Case 1：定义了 `TEST` 时，用 `KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE)` 启动指定程序。
- Case 2：否则用 `KERNEL_EXECVE(exit)` 启动默认程序。

**kernel_execve**
- Pre-Condition：调用者是内核线程，`name` 是内核可读字符串。
- Post-Condition：成功时不返回，CPU 通过 `sret` 进入用户程序入口；失败时返回 `do_execve` 的负错误码。
- Requirements：不能只调用 `do_execve` 就返回，因为那样执行流仍在内核态。成功后必须把 `sp` 指向 `current->tf`（`load_icode` 写好的 trapframe）并跳转到 `__trapret`，复用异常返回路径；用内联汇编实现，并声明 `memory` 破坏。

**do_wait**
- Pre-Condition：`pid == 0` 表示任意子进程，否则指定的子进程；`code_store` 为 NULL 或用户态地址。
- Post-Condition：成功时已回收一个僵尸子进程：退出码写入 `code_store`，它已从哈希表、进程链表和进程树中摘除，内核栈与 PCB 已释放，返回 0。
- Case 1：`code_store` 非 NULL 但对当前进程不可写（`user_mem_check`），返回 `-E_INVAL`，不做任何其他事。
- Case 2：指定的 PID 不存在或不是当前进程的子进程，或当前进程没有任何子进程，返回 `-E_BAD_PROC`。
- Case 3：有子进程但都不是僵尸。当前进程置 `PROC_SLEEPING`、`wait_state = WT_CHILD`，调用 `schedule()`；被唤醒后若 `PF_EXITING` 已置位则 `do_exit(-E_KILLED)`，否则重新查找。
- Case 4：找到僵尸子进程。若它是 `idleproc` 或 `initproc` 则 panic；摘除链表时必须关中断。
- Requirements：内核栈和 PCB 只能由父进程在这里释放，子进程自己在 `do_exit` 中无法释放。

**do_execve**
- Pre-Condition：`name` 在调用者地址空间中可读（内核线程的 `mm` 为 NULL，此时只要求是内核地址）。
- Post-Condition：成功时当前进程拥有新的地址空间和一个可用于启动新程序的 trapframe，进程名已更新，返回 0。
- Case 1：名字不可读，返回 `-E_INVAL`。
- Case 2：当前进程有旧 `mm`。先把名字复制到内核局部缓冲区（旧地址空间随后会被销毁），先切换到内核页表（`boot_pgdir_pa`，同时更新 `current->pgdir`），再把 `mm_count` 减 1，减到 0 时依次 `exit_mmap`、`put_pgdir`、`mm_destroy`，并把 `current->mm` 置 NULL。
- Case 3：`load_icode` 失败。此时旧映像已经没有了，进程无法继续，调用 `do_exit` 退出。
- Requirements：必须先处理旧 `mm` 再装载新程序；名字长度超过 `PROC_NAME_LEN` 时截断。

**load_icode**
- Pre-Condition：`current->mm == NULL`（否则 panic）；`current->tf` 指向内核栈顶的 trapframe。
- Post-Condition：成功时 `current->mm` 为新建的、引用计数为 1 的地址空间，`current->pgdir` 与 `satp` 已指向它；trapframe 被重写为"从用户入口点以用户态开始执行"：除 `sp = USTACKTOP`、`epc = e_entry` 外所有寄存器清零，`status` 清除 `SSTATUS_SPP`、置位 `SSTATUS_SPIE`。
- Case 1：`e_magic` 不是 `ELF_MAGIC`，返回 `-E_INVAL_ELF`。
- Case 2：对每个 `PT_LOAD` 段：`p_filesz > p_memsz` 返回 `-E_INVAL_ELF`；否则按段权限建立 VMA（`PF_R/W/X` 映射为 `VM_READ/WRITE/EXEC`），PTE 权限含 `PTE_U|PTE_V`，写权限同时带 `PTE_R`；从映像逐页拷贝 `p_filesz` 字节，`p_memsz` 超出部分（bss）必须清零，包括文件数据最后一页的剩余部分。
- Case 3：建立 `VM_READ|VM_WRITE|VM_STACK` 的用户栈 VMA，范围是 `USTACKTOP` 之下 `USTACKSIZE` 字节；因为没有缺页处理，只为栈顶的 4 页分配物理页，权限为 `PTE_USER`。
- Case 4：任一步骤分配失败（`-E_NO_MEM`）或出错，必须按已完成的步骤逆序释放：已映射的页和页表（`exit_mmap`，此时 `mm_count` 为 0）、页目录、`mm_struct`。
- Requirements：切换 `satp` 之后要刷新 TLB；`status` 的修改要基于原值，只改 `SPP` 与 `SPIE` 两位。
````

#### 4.2.3 实现迭代过程

本模块的实现经历了 2 次迭代，过程如下：

---

##### 第一次迭代

提示词生成的代码一次编译通过，但在 `make grade` 中除 `forktest` 外都通过，`forktest` 一项失败：

**遇到的问题：**

- 问题1：`forktest` 提示 `missing 'init check memory pass.'`。手动运行 QEMU，`forktest pass.` 已经打印，随后内核 panic：`kernel panic at kern/process/proc.c: assertion failed: nr_free_pages_store == nr_free_pages()`。这是第一版提示词里要求 `init_main` 在回收完所有用户进程后断言空闲物理页数与入口时相等导致的。`exit`、`badarg` 等只创建一个子进程的程序都能通过，而 `forktest` 一次创建 32 个子进程时失败。
- 原因分析：先怀疑是 `copy_range`、`exit_mmap` 有真实的页泄漏，于是在调试副本里把断言临时改成打印"少了多少页"：`exit` 为 0 页，`forktest` 为 3 页，`forktree` 为 2 页。如果是用户页泄漏，32 个进程应当丢失几百页，不可能只差 2~3 页。再看 `kern/mm/kmalloc.c`：内核堆是 SLOB 分配器，同时存在很多 PCB、`mm_struct`、VMA 时会向页分配器多要几页，而 `slob_free` 不会把页还给页分配器；`kallocated()` 也恒为 0，无法用它扣除堆占用。所以差的几页是内核堆的增长，并非泄漏。

**问题解决策略：**

- 针对问题1：修改提示词中 `init_main` 的规格：不再断言空闲物理页数与入口相等，只检查进程表回到了起点（`initproc` 无亲属指针，`nr_process` 等于入口值，`proc_list` 中只剩 `initproc`），并在 Requirements 里写明原因，防止模型再次加入这条断言。

---

##### 第二次迭代

**最终结果：**

经过 2 次迭代，该模块最终：

- ✅ 编译通过
- ✅ 注入的测试 `[Test Process Create] PASS` 通过
- ✅ `hello`、`exit`、`badarg`、`forktest` 等用户程序都能被正确装载、运行并在退出后被 `init_main` 回收，打印 `all user-mode processes have quit.` 和 `init check memory pass.`
- ✅ 功能符合预期

**关键改进点总结：**

1. 在 [SPECIFICATION] 中去掉了 `init_main` 对空闲物理页数的相等断言，并说明了原因（SLOB 堆只增不减、`kallocated()` 恒为 0）。
2. 用调试输出量化"少了几页"而不是凭感觉判断，区分了真实泄漏和堆增长。

---

### 4.3 功能模块三：复制用户进程功能

**负责人：** [2414007-王奕瑜]

#### 4.3.1 模块功能描述

**需要实现/修改的函数：**

```c
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc)                          // kern/process/proc.c（更新）
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)                    // kern/process/proc.c（更新）
int dup_mmap(struct mm_struct *to, struct mm_struct *from)                                  // kern/mm/vmm.c
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share)          // kern/mm/pmm.c
```

**功能说明：**

这一模块把父进程复制成子进程，使子进程拥有自己的 PCB、内核栈、trapframe 和用户地址空间，并让用户程序看到"父进程得到子进程 PID，子进程从 `fork` 返回 0"的标准语义。调用链是 `sys_fork` → `do_fork` → `copy_mm` → `dup_mmap` → `copy_range`。

1. **`copy_mm`（更新）。** lab4 版本只处理内核线程（父进程无 `mm`）和 `CLONE_VM` 共享，lab5 补上普通 fork。父进程没有 `mm` 时子进程也没有，`pgdir` 保持 `alloc_proc` 给的内核页表。带 `CLONE_VM` 时直接共享父进程的 `mm`。普通 fork 时：`mm_create` 新建 `mm`，`setup_pgdir` 建页目录（并复制内核映射），在持有父进程 `mm_lock` 的情况下调用 `dup_mmap` 复制地址空间。成功后三种情况都一样：`mm_count_inc`，设置 `proc->mm`，并把 `proc->pgdir` 设为页目录的**物理地址**（`proc_run` 会把它写进 `satp`）。出错回滚按获取的相反顺序：复制了一部分时 `exit_mmap`（此时引用计数仍是 0，满足它的断言），再 `put_pgdir`，最后 `mm_destroy`，使用 `goto` 标号实现。
2. **`do_fork`（更新）。** 在 lab4 的基础上：先检查 `nr_process >= MAX_PROCESS`，返回 `-E_NO_FREE_PROC`；`alloc_proc` 之后立即设置 `proc->parent = current`，并断言父进程 `wait_state == 0`（正在运行的父进程不可能处在 `do_wait` 睡眠中）；`copy_mm` 现在会真正复制地址空间；`copy_thread` 在子进程内核栈顶放置 trapframe（`a0 = 0`，`sp` 为 `stack`）和初始上下文。关中断的临界区里依次完成 `get_pid`、`hash_proc`、`set_links`、`wakeup_proc`。用 `set_links` 取代 lab4 手工操作 `proc_list`，因为它还会维护父进程的子进程链（`cptr/optr/yptr`），`do_exit`、`do_wait` 都依赖这套关系，并且已经自带 `nr_process++`，所以不能再自己递增。`wakeup_proc` 放在临界区最后，保证子进程在所有链接完成之后才对调度器可见。
3. **`dup_mmap`。** 按父进程 `mmap_list` 的顺序遍历每个 VMA：`vma_create` 创建同区间、同标志的 VMA，`insert_vma_struct` 插入子进程（它保持有序并拒绝重叠），再对这个 VMA 的范围调用 `copy_range`，`share` 固定为 0。任何一步失败返回 `-E_NO_MEM`，已复制的部分由调用者 `copy_mm` 用 `exit_mmap` 清理。
4. **`copy_range`。** 逐页遍历 `[start, end)`：源页表某一级不存在时，跳到下一个 `PTSIZE` 边界；源 PTE 有效时，先 `get_pte(to, ..., 1)` 确保目标页表存在，取源 PTE 的 `PTE_USER` 位作为权限，`alloc_page` 分配新页、`memcpy` 复制完整的 4 KiB，再用 `page_insert` 以相同权限（含 `PTE_U`）映射到同一虚拟地址。`page_insert` 内部会设置 A/D 位并维护引用计数。`share != 0` 时不分配新页，直接映射同一物理页；本实验的 `fork` 不使用它，它是为后续 COW 预留的。分配或插入失败返回错误码（`page_insert` 失败时释放刚分配的新页）。循环条件写成 `start != 0 && start < end`，以免区间末尾地址回绕为 0 后死循环。

复制结束后，父子进程在虚拟地址上看到相同的内容，在物理上各自拥有一份，后续的写入互不影响。

#### 4.3.2 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

````markdown
[PROMPT]
**任务**：补全 lab5 的"复制用户进程"功能模块，涉及三个文件：`kern/process/proc.c` 中更新 `copy_mm` 和 `do_fork`，`kern/mm/vmm.c` 中实现 `dup_mmap`，`kern/mm/pmm.c` 中实现 `copy_range`。目标是让 `fork` 产生的子进程拥有自己的 PCB、内核栈、trapframe、进程树关系和一份独立的用户地址空间副本。

**操作要求**：你必须在项目中修改实际文件，保留现有代码，只在标有 `lab5 function3` 的位置补全或更新，并把其中的 `YOUR CODE` 替换为学号 `2414007`；lab4 版本的 `copy_mm` 和 `do_fork` 是基线，在其上更新，保留它们已有的出错回滚结构。所有新增代码的注释使用英文，函数使用 Doxygen 风格。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息，以工程真实代码为准，不要引入无关功能。

[RELY]
1. 进程相关（`proc.c`、`proc.h`）：`alloc_proc()`、`setup_kstack()`、`put_kstack()`、`copy_thread(proc, esp, tf)`、`get_pid()`、`set_links(proc)`（把进程挂入 `proc_list`、父进程的子进程链并 `nr_process++`，要求 `proc->parent` 已设置）、`hash_proc(proc)`、`wakeup_proc(proc)`、`setup_pgdir(mm)`、`put_pgdir(mm)`；`current`、`nr_process`、`MAX_PROCESS`；`CLONE_VM`；`struct proc_struct` 的 `parent`、`mm`、`pgdir`、`wait_state`。`local_intr_save/local_intr_restore`。
2. 地址空间（`vmm.h`）：`struct mm_struct`（`mmap_list`、`pgdir`、`mm_lock`）、`struct vma_struct`（`vm_start`、`vm_end`、`vm_flags`、`list_link`）、`le2vma`、`mm_create()`、`mm_destroy()`、`vma_create(start, end, flags)`、`insert_vma_struct(mm, vma)`（保持有序、重叠则断言失败）、`exit_mmap(mm)`（要求 `mm_count == 0`）、`mm_count_inc()`、`lock_mm/unlock_mm`（参数为 NULL 时忽略）。
3. 页表与页（`pmm.h`、`mmu.h`）：`get_pte(pgdir, la, create)`（`create` 为 0 时缺表返回 NULL）、`page_insert(pgdir, page, la, perm)`（成功返回 0，会增加引用计数，遇到不同页的旧映射会先释放）、`pte2page`、`alloc_page()`、`free_page()`、`page2kva`、`PADDR`、`PTE_USER`、`PTE_V`、`PGSIZE`、`PTSIZE`、`ROUNDDOWN`、`USER_ACCESS`、`memcpy`。
4. 错误码 `E_NO_MEM`、`E_NO_FREE_PROC`。

[GUARANTEE]
需要实现或修改以下函数，签名必须与现有声明一致：

```c
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc)             // kern/process/proc.c
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)       // kern/process/proc.c
int dup_mmap(struct mm_struct *to, struct mm_struct *from)                     // kern/mm/vmm.c
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share) // kern/mm/pmm.c
```

允许新增 `static` 辅助函数，不得改变公开接口。

[SPECIFICATION]
**copy_mm**
- Pre-Condition：`proc` 是 `do_fork` 刚创建的子进程 PCB，尚未有地址空间。
- Post-Condition：成功时 `proc->mm` 与 `proc->pgdir` 已设置（`pgdir` 是页目录的物理地址，`proc_run` 会把它写入 `satp`），`mm` 的引用计数已加 1。
- Case 1：父进程没有 `mm`（内核线程）。子进程也没有，`proc->mm` 为 NULL，`pgdir` 保持 `alloc_proc` 给的内核页表，返回 0。
- Case 2：带 `CLONE_VM`。子进程直接共享父进程的 `mm`，引用计数加 1，不复制任何内容。
- Case 3：普通 fork。新建 `mm` 和页目录，在持有父进程 `mm_lock` 的情况下调用 `dup_mmap` 复制地址空间。
- Case 4：任一步失败返回 `-E_NO_MEM`，并只释放本函数已经创建的东西：复制了一部分时用 `exit_mmap` 释放已映射的页和页表（此时引用计数为 0），然后释放页目录，最后销毁 `mm`；标号式回滚要与获取顺序相反。
- Requirements：普通 fork 与共享地址空间的线程创建不能混为一谈，二者的引用计数处理必须一致。

**do_fork**
- Pre-Condition：`tf` 不为 NULL，`current` 已设置；`stack` 为子进程的用户栈指针，内核线程时为 0。
- Post-Condition：成功时返回子进程 PID；子进程是 `PROC_RUNNABLE`，已挂入哈希表、进程链表和父进程的子进程链，`nr_process` 已加 1，父进程看到 PID，子进程从 `fork` 得到 0。
- Case 1：`nr_process >= MAX_PROCESS`，返回 `-E_NO_FREE_PROC`。
- Case 2：按顺序执行：分配 PCB 并设置 `parent = current`（此时父进程的 `wait_state` 应为 0，用断言保证）；分配内核栈；调用 `copy_mm`；调用 `copy_thread` 在子进程内核栈上放置 trapframe 和初始上下文。每一步失败都按相反顺序释放已获得的资源后返回错误码。
- Case 3：关中断，在同一个临界区内完成 PID 分配、哈希表插入、`set_links`（而不是手工操作 `proc_list`）和 `wakeup_proc`；之后恢复中断并返回 PID。
- Requirements：子进程必须只在临界区的最后一步才对调度器可见；`do_fork` 不得自己递增 `nr_process`，`set_links` 已经做了。

**dup_mmap**
- Pre-Condition：`to` 是新建的、页目录已建立的空地址空间，`from` 是父进程地址空间，调用者已持有 `from->mm_lock`。
- Post-Condition：`to` 中有与 `from` 相同区间、相同权限标志的 VMA，且每个 VMA 内父进程已映射的页都有子进程自己的物理副本。
- Case 1：按 `from->mmap_list` 的顺序遍历每个 VMA，创建同样的 VMA 并插入 `to`，然后调用 `copy_range` 复制该 VMA 的页，`share` 固定为 0（普通 fork 不共享）。
- Case 2：VMA 分配失败或 `copy_range` 失败，返回 `-E_NO_MEM`，已复制的部分留给调用者用 `exit_mmap` 清理。

**copy_range**
- Pre-Condition：`start`、`end` 页对齐且是用户地址；`to`、`from` 是页目录的内核虚拟地址。
- Post-Condition：`from` 在 `[start, end)` 内每个有效叶子表项，在 `to` 中的同一虚拟地址都有一个有效映射，权限位（含 `PTE_U`）与源表项相同。
- Case 1：`share == 0`。为每个有效页分配一个新物理页，拷贝完整的 4 KiB 内容，再用 `page_insert` 在 `to` 中映射。
- Case 2：`share != 0`。不分配新页，直接在 `to` 中映射同一个物理页（`page_insert` 会增加引用计数）。
- Case 3：源地址对应的页表不存在时，跳到下一个 `PTSIZE` 边界继续；源表项无效时在目标中保持不映射。
- Case 4：目标页表或新页分配失败，返回 `-E_NO_MEM`；`page_insert` 失败时释放刚分配的新页并返回其错误码。
- Requirements：循环必须能处理区间末尾地址回绕到 0 的情况；`page_insert` 后目标页的引用计数应为 1，父子之间不得互相影响。
````

#### 4.3.3 实现迭代过程

本模块的实现经历了 1 次迭代，过程如下：

---

##### 第一次迭代

提示词生成的代码一次编译通过，没有出现需要修复的问题。

**最终结果：**

经过 1 次迭代，该模块最终：

- ✅ 编译通过
- ✅ `exit`（父子进程 `fork` + `waitpid`，检查退出码）、`forktest`（连续创建 32 个子进程）、`badarg`、`yield`、`spin` 等依赖 `fork` 的用户程序全部通过
- ✅ 功能符合预期

---

### 4.4 功能模块四：其余系统调用功能

**负责人：** [2414007-王奕瑜]

#### 4.4.1 模块功能描述

**需要实现/修改的函数：**

```c
int do_exit(int error_code)    // kern/process/proc.c
int do_yield(void)
int do_kill(int pid)
```

**功能说明：**

这一模块实现用户程序生命周期中的"结束、让出和终止"三个动作。

1. **`do_exit`：释放能自己释放的，其余交给父进程。** 进程执行退出逻辑时仍在使用自己的内核栈和 PCB，所以这两样不能自己释放，只能由父进程在 `do_wait` 中回收。其余步骤如下：
   - `idleproc` 和 `initproc` 不允许退出，直接 panic。
   - 如果有 `mm`：先切换到内核页表 `boot_pgdir_pa`（并更新 `current->pgdir`），再把 `mm_count` 减 1；减到 0 才依次 `exit_mmap`（释放页和页表）、`put_pgdir`、`mm_destroy`，最后 `current->mm = NULL`。线程共享 `mm` 时，最后一个退出的才真正释放。
   - 置 `PROC_ZOMBIE`，记录 `exit_code`。
   - 关中断，使进程树不会被别的代码看到半更新的状态：若父进程的 `wait_state == WT_CHILD`，用 `wakeup_proc` 唤醒它；把当前进程的所有子进程过继给 `initproc`（摘出子进程链，设置新 `parent`，挂到 `initproc` 子进程链的头部），如果被过继的子进程已经是僵尸并且 `initproc` 正在等待，唤醒 `initproc`。
   - 恢复中断后调用 `schedule()`，僵尸不会再被调度；如果它竟然返回就 panic。
2. **`do_yield`：只设标志。** 只把 `current->need_resched` 置 1 并返回 0。真正的切换发生在系统调用返回时：`trap()` 在从用户态返回前检查 `need_resched`，调用 `schedule()`。所以不能在 `do_yield` 里直接 `schedule()`，那会让切换发生在内核态系统调用处理中途。
3. **`do_kill`：只发"请退出"的意图。** 用 `find_proc` 找目标进程：不存在返回 `-E_INVAL`；已经带 `PF_EXITING` 返回 `-E_KILLED`；否则置位 `PF_EXITING`，如果目标的 `wait_state` 带 `WT_INTERRUPTED`（例如正在 `do_wait` 里睡眠），用 `wakeup_proc` 唤醒它，让它尽快运行到 `trap()` 或 `do_wait` 的检查点，在那里 `do_exit(-E_KILLED)`。目标进程不会被立即停止。

三者与 `do_wait`、调度器和 `trap()` 一起闭合了进程生命周期：`exit` → 僵尸 → 父进程被唤醒 → `do_wait` 回收内核栈和 PCB；孤儿进程过继给 `initproc` 后由 `init_main` 的 `do_wait` 循环回收。

#### 4.4.2 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

````markdown
[PROMPT]
**任务**：补全 lab5 的"其余系统调用功能"模块，修改 `kern/process/proc.c`，实现进程退出 `do_exit`、让出 CPU `do_yield` 和终止进程 `do_kill`，使它们与 `do_wait`、调度器和 `trap()` 一起构成完整的进程生命周期闭环。

**操作要求**：你必须在项目中修改实际文件，保留现有代码，只在标有 `lab5 function4 YOUR CODE` 的位置补全，并把其中的 `YOUR CODE` 替换为学号 `2414007`。所有新增代码的注释使用英文，函数使用 Doxygen 风格。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息，以工程真实代码为准，不要引入无关功能。

[RELY]
1. 进程相关（`proc.h`、`proc.c`）：`current`、`idleproc`、`initproc`、`find_proc(pid)`；`struct proc_struct` 的 `state`、`exit_code`、`flags`、`wait_state`、`parent`、`mm`、`pgdir`、`cptr/yptr/optr`、`need_resched`；状态 `PROC_ZOMBIE`；标志 `PF_EXITING`；等待状态 `WT_CHILD`、`WT_INTERRUPTED`。
2. 调度（`sched.h`、`sched.c`）：`schedule()`（选出下一个可运行进程，僵尸不会被选中）、`wakeup_proc(proc)`（置 `PROC_RUNNABLE` 并清空 `wait_state`；对僵尸进程断言失败）。`local_intr_save/local_intr_restore`。
3. 地址空间：`exit_mmap(mm)`（要求 `mm_count == 0`）、`put_pgdir(mm)`、`mm_destroy(mm)`、`mm_count_dec(mm)`（返回减 1 后的值）；`boot_pgdir_pa`；`lsatp(pa)`、`flush_tlb()`。
4. `trap()` 在从用户态返回前会检查：`current->flags & PF_EXITING` 则调用 `do_exit(-E_KILLED)`；`current->need_resched` 则调用 `schedule()`。`do_wait` 在睡眠时设置 `wait_state = WT_CHILD`。
5. 错误码 `E_INVAL`、`E_KILLED`。`panic()`。

[GUARANTEE]
需要实现以下函数，签名必须与现有声明一致：

```c
int do_exit(int error_code)
int do_yield(void)
int do_kill(int pid)
```

允许新增 `static` 辅助函数，不得改变公开接口。

[SPECIFICATION]
**do_exit**
- Pre-Condition：当前进程不是 `idleproc`，也不是 `initproc`。
- Post-Condition：当前进程处于 `PROC_ZOMBIE`，`exit_code` 已记录；用户地址空间已在没有其他共享者时释放；它的子进程都已交给 `initproc`；等待它的父进程已被唤醒；函数不会返回。
- Case 1：当前进程是 `idleproc` 或 `initproc`，panic。
- Case 2：当前进程有 `mm`。先切换到内核页表（`boot_pgdir_pa`，同时更新 `current->pgdir`），再把 `mm_count` 减 1；减到 0 时依次释放页和页表（`exit_mmap`）、页目录、`mm_struct`；最后把 `current->mm` 置 NULL。内核线程没有 `mm`，跳过这一步。
- Case 3：置状态为 `PROC_ZOMBIE`、记录退出码，然后关中断完成以下两件事，使进程树不会被其他代码看到半更新的状态：若父进程的 `wait_state == WT_CHILD`，唤醒它；把当前进程的每个子进程过继给 `initproc`（摘出子进程链，设置新 `parent`，挂到 `initproc` 的子进程链头部）；若被过继的子进程已经是僵尸且 `initproc` 正在等待子进程，唤醒 `initproc`。
- Case 4：恢复中断后调用 `schedule()`；若返回则说明调度器出了问题，panic。
- Requirements：当前进程的内核栈和 PCB 不能在这里释放，它们仍在被使用，只能由父进程在 `do_wait` 中回收；必须在变成僵尸之前释放完用户地址空间。

**do_yield**
- Post-Condition：`current->need_resched == 1`，返回 0。
- Requirements：不要在函数里直接调用 `schedule()`，让出 CPU 发生在系统调用返回、`trap()` 检查 `need_resched` 的时候。

**do_kill**
- Pre-Condition：`pid` 为任意整数。
- Post-Condition：目标进程被标记为"请退出"，但它不会被立即停止。
- Case 1：`find_proc(pid)` 找不到，返回 `-E_INVAL`。
- Case 2：目标已设置 `PF_EXITING`，返回 `-E_KILLED`，不重复处理。
- Case 3：否则置位目标的 `PF_EXITING`；如果目标的 `wait_state` 带有 `WT_INTERRUPTED`（正在可中断的等待中），用 `wakeup_proc` 唤醒它，使它尽快在 `trap()` 或 `do_wait` 中发现该标志并退出；返回 0。
````

#### 4.4.3 实现迭代过程

本模块的实现经历了 1 次迭代，过程如下：

---

##### 第一次迭代

提示词生成的代码一次编译通过，没有出现需要修复的问题。

**最终结果：**

经过 1 次迭代，该模块最终：

- ✅ 编译通过
- ✅ 注入的测试 `[Test Process Control] PASS` 通过：`do_yield()` 返回 0 且置位 `need_resched`，`do_kill(-1)` 返回 `-E_INVAL`
- ✅ `exit`、`yield`、`spin`、`forktest` 等程序在 `make grade` 中全部通过，手动运行 `make build-waitkill` 验证了 `do_kill` 的唤醒与退出路径；进程退出后 `init_main` 的检查 `init check memory pass.` 成立
- ✅ 功能符合预期

---

### 4.5 Challenge: Copy on Write 机制

**负责人：** [2411622-张雨函]

#### 4.5.1 模块功能描述

#### 4.5.2 最终提示词

#### 4.5.3 实现迭代过程

#### 4.5.4 问题解答

> 说明该用户程序是何时被预先加载到内存中的？与我们常用操作系统的加载有何区别，原因是什么？

---

### 4.6 分支任务：gdb 调试系统调用以及返回

**负责人：** [2411622-张雨函]

---

## 五、测试与验证

**负责人：** [2414007-王奕瑜]

---

## 六、实验总结与收获

### 6.1 对操作系统的理解

### 6.2 AI 协作开发的经验
