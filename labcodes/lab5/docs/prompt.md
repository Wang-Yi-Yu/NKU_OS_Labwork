# 本实验所使用的提示词汇总

## 1. 提示词格式要求

```
[PROMPT]
做什么、改哪里、怎么改

[RELY]
可以依赖什么——宏定义、数据结构、已有函数

[GUARANTEE]
必须实现或修改什么——需要交付的函数列表

[SPECIFICATION]
每个函数的详细行为规格
```

### 1.1 [PROMPT]：任务说明

[PROMPT]回答的是"你要做什么"。它包含三个字段：

**任务**：说清楚目标文件和模块的具体功能。如果涉及多个文件，每个文件负责什么，要在这里交代。不要让AI自己猜该改哪个文件。

**操作要求**：约束AI的行为方式。比较重要的一条是"必须在项目中修改实际文件"——如果不强调这一点，AI很可能只输出代码片段而不真正改文件。此外还可以说清楚：保留现有代码，只在标记位置补全，不要删除既有逻辑。

**输出要求**：告诉AI使用下面[RELY]、[GUARANTEE]、[SPECIFICATION]中提供的信息，以工程真实代码为准，不要引入无关功能。

以物理内存初始化任务为例：

```
**任务**：修改pmm.c文件，实现物理内存探测与Page描述符初始化功能。

**操作要求**：你必须在项目中修改实际文件，保留现有代码，
只在标记位置补全。

**输出要求**：直接进行文件操作，使用下面[RELY]、[GUARANTEE]、
[SPECIFICATION]部分的信息。
```

### 1.2 [RELY]：上下文信息

[RELY]提供AI完成任务所需的最小可信上下文——宏定义、数据结构、可调用的函数、相关全局变量。

关键词是"最小"和"可信"。

"最小"意味着不要把整个工程的头文件都贴进来，只放和当前任务直接相关的。无关信息太多，AI反而容易分心。

"可信"意味着这些信息必须和工程代码一致。去代码里把真实的定义复制过来，不要凭记忆自己写一个"差不多"的版本。如果工程里的宏叫`PGSIZE`，你写成`PAGE_SIZE`，AI生成的代码就会编译不过。

如果某个数据结构的布局对任务很关键（比如`struct Page`描述符数组的地址范围和对应的物理地址），应该把相关字段和地址关系明确列出来，而不是只给一个结构体名。

### 1.3 [GUARANTEE]：接口清单

[GUARANTEE]列出这次任务必须实现或修改的所有函数签名。它是一份"交付清单"：这些函数必须在最终代码中出现，签名必须和声明一致。

对于需要实现的新函数，明确指出函数签名和预期的接口行为；对于需要修改的现有函数，应说明修改的目标和预期效果。

除了主接口，通常还需要说明是否允许新增辅助函数。如果允许，加上约束：辅助函数应该是static的，只在本模块内部使用，不应该改变公开接口。

一个[GUARANTEE]块中的接口应该属于同一个功能模块。比如物理内存初始化相关的`page_init`、`get_memory_base`、`get_memory_size`、`init_memmap`和`pa2page`放在一起；它们可以跨文件，但应该归属同一个模块目标。

### 1.4 [SPECIFICATION]：行为规格

这是提示词最核心的部分。对每个主要函数，都要用统一的格式描述它的行为：

**Pre-Condition**：调用这个函数前，系统应该处于什么状态？参数有什么约束？

**Post-Condition**：函数执行完成后，系统应该处于什么状态？这里描述的是函数执行后的总体状态，包括数据结构、资源、全局状态等方面必须满足的条件。

**Case 1、Case 2……**：在Post-Condition下面，按照不同的执行情况分别描述函数的具体行为。每个Case都用一段话说明在什么条件下触发、需要执行什么操作，以及执行完成后需要满足什么状态。对操作系统代码来说，一个函数可能存在多种不同的执行路径，例如正常执行、参数异常、资源不足或特殊系统状态等。将这些情况分别列出，可以更准确地描述函数的完整行为。

**Requirements**：特殊的时序要求、并发约束、性能要求，或者其他特殊需求。

其中，**Case和Requirements都是可选的**，是否使用应根据具体函数的实际情况决定。如果函数只有单一执行路径或不存在需要单独描述的特殊执行路径，不需要设置Case；如果函数没有额外的特殊要求，不需要设置Requirements。

还需要注意的是，各个字段描述的是**函数需要实现什么行为**，而不是**具体如何实现**。因此，应尽量避免堆积大量具体的函数调用、变量名和常量。规格应该足够抽象、清晰，不拘泥于过于具体的实现细节，从而更适合作为需求和设计说明。

## 2. 本实验所使用的提示词汇总

### 2.1 2414007-王奕瑜

#### 2.1.1 功能模块一：用户态系统调用功能

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

#### 2.1.2 功能模块二：创建加载第一个用户进程功能

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

#### 2.1.3 功能模块三：复制用户进程功能

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

#### 2.1.4 功能模块四：其余系统调用功能

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

### 2.2 2414015-鲁昕宁

### 2.3 2411622-张雨函

#### 2.3.1 Challenge：Copy on Write 机制

````markdown
[PROMPT]
**任务**：在现有 Lab5 用户进程和虚拟内存实现上完成 Challenge：Copy on Write（COW）。普通 fork 不再为每个用户页立即分配物理副本，而是让父子进程暂时共享已映射的物理页；对原本可写的共享页清除硬件写权限并设置软件 COW 标志，进程第一次写入时再通过写缺页异常获得私有可写页。实现必须保持普通 fork、waitpid、exit 和非法访问测试的原有语义。

涉及文件：

- kern/mm/mmu.h：定义一个不与硬件权限位冲突的软件 PTE_COW 位；
- kern/mm/pmm.c：更新 copy_range，完成共享映射、权限转换、引用计数和失败回滚；
- kern/mm/vmm.c、kern/mm/vmm.h：让 dup_mmap 使用共享复制，并提供 COW 写故障解析接口；
- kern/trap/trap.c：在用户或内核向合法 COW 用户页写入时解析 CAUSE_STORE_PAGE_FAULT，成功后重试原指令；
- user/cowtest.c：新增覆盖父子隔离、多子进程共享、退出回收和内核写用户缓冲区的测试；
- 必要时更新 docs/report.md，说明状态转换、Dirty COW 风险、测试结果和剩余限制。

**操作要求**：

1. 先阅读并核对 copy_mm → dup_mmap → copy_range、page_insert、page_remove_pte、exit_mmap、user_mem_check 和 exception_handler 的真实实现，再开始修改。
2. 只修改 COW Challenge 所需的路径和测试文件，保留 Lab5 四个基础模块的语义；不要把普通 fork 改回父子各复制一份物理页。
3. 新增或修改的注释使用英文，函数注释使用 Doxygen 风格；保留并补充 lab5 Challenge 2411622张雨函 标记。
4. 先完成共享映射和引用计数，再完成写缺页解析，最后加入测试。每个阶段都要编译验证。
5. COW 只针对用户地址空间中的有效叶子页表项。不能因为页面属于可写 VMA 就把原本只读的 PTE 升级为可写。
6. 不引入文件系统、按需装载、多 hart 调度或与 Challenge 无关的公开接口。

**输出要求**：直接修改实际文件并给出实现；使用下面 RELY、GUARANTEE、SPECIFICATION 作为约束，以工程真实代码为准。

[RELY]
1. do_fork 的普通 fork 路径调用 copy_mm(0, child)，随后调用 dup_mmap。
2. dup_mmap 按父地址空间的 VMA 顺序建立子 VMA，并调用 copy_range(to->pgdir, from->pgdir, start, end, share)。
3. copy_range 的签名必须保持为：
   int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share)
4. page_insert 建立映射并增加物理页引用；page_remove_pte、unmap_range 和 exit_mmap 解除映射并减少引用；引用数为零时才释放物理页。
5. 可用 page_ref、page_ref_inc、page_ref_dec、pte2page、page2kva、alloc_page、free_page 管理物理页。
6. 页表权限包括 PTE_V、PTE_R、PTE_W、PTE_X、PTE_U 和 PTE_USER；VMA 权限包括 VM_READ、VM_WRITE、VM_EXEC、VM_STACK。
7. find_vma 查找 VMA，get_pte 查找页表项，tlb_invalidate 使单页 TLB 失效，lock_mm 和 unlock_mm 保护地址空间修改。
8. exception_handler 已处理用户 ecall；写缺页原因为 CAUSE_STORE_PAGE_FAULT。copy_to_user 和 do_wait 可能从内核向用户缓冲区写数据。
9. 本实验为单 hart。mm_lock 可以保护同一地址空间的解析和替换，但不能据此声称多核并发安全。

[GUARANTEE]
需要实现或修改以下接口，签名必须与现有声明一致：

~~~c
#define PTE_COW ...
int copy_range(pde_t *to, pde_t *from,
               uintptr_t start, uintptr_t end, bool share)
int dup_mmap(struct mm_struct *to, struct mm_struct *from)
int cow_resolve(struct mm_struct *mm, uintptr_t addr)
void exception_handler(struct trapframe *tf)
~~~

还必须新增 user/cowtest.c。允许新增本模块内部使用的 static 辅助函数，不得改变既有公开接口签名。

[SPECIFICATION]
按以下顺序实施和验收，每一阶段完成后先编译，再进入下一阶段。

**阶段一：权限和状态表示**

- PTE_COW 使用软件保留位，不能覆盖 PTE_V/R/W/X/U。
- PTE_COW 只表示“该映射原本允许写入”，不能单独授予用户访问权限。
- 必须区分“原本可写、当前因共享而只读的 COW 页”和“原本只读的共享页”。

**阶段二：建立共享映射**

- dup_mmap 为每个父 VMA 建立相同范围和权限的子 VMA，并调用 copy_range(..., share=1)。
- share 为真且源 PTE 带 PTE_W：父子映射同一物理页，父子 PTE 都清除 PTE_W 并设置 PTE_COW。
- share 为真但源 PTE 不带 PTE_W：父子共享物理页并保持只读，不设置 PTE_COW。
- 子映射建立成功后才修改父 PTE，并使父页 TLB 失效。
- 任何目标页表或映射分配失败都必须返回错误并保持引用计数一致。

**阶段三：解析写缺页**

cow_resolve 的合法条件必须同时满足：找到 VMA；VMA 带 VM_WRITE；PTE 有效；PTE 带 PTE_COW 且当前没有 PTE_W。

- 引用计数大于 1：分配新页，复制完整 4 KiB 内容，替换当前映射为 PTE_W 且去掉 PTE_COW 的私有页。
- 引用计数等于 1：不复制，直接恢复 PTE_W 并清除 PTE_COW。
- 地址、VMA、PTE 或权限不合法：返回错误，不升级权限。
- 分配或替换失败：释放本次新资源，保留原映射和 COW 状态。
- 检查、引用计数读取和映射替换由 mm_lock 保护，修改 PTE 后使 TLB 失效。

**阶段四：接入异常路径**

- CAUSE_STORE_PAGE_FAULT 调用 COW 解析接口。
- 解析成功时直接返回，让 sret 重试原 store；不能前移 tf->epc。
- 解析失败时继续原有非法访问处理，不能把普通只读页、未映射页或非法地址变成可写页。
- 必须覆盖用户态写入和内核向用户缓冲区写入，例如 waitpid 写退出码。

**阶段五：引用计数和回收**

- 父子任一方解除映射时只减少自己的引用，另一方仍可访问共享页。
- 两方都解除映射后物理页才释放；页表页和 VMA 继续由原有 exit_mmap、mm_destroy 路径回收。
- COW 失败或非法访问退出时，不能泄漏新页、页表或引用计数。

**阶段六：测试和报告**

cowtest 至少覆盖：

1. 父子分别写同一全局数据页，确认写入不可见；
2. 连续多轮 fork 和回收，覆盖引用数降为 1 后恢复写权限；
3. 多个同时存在的子进程分别写入，确认互不覆盖；
4. waitpid 向页对齐的用户缓冲区写退出码，覆盖内核写 COW 页；
5. 子进程退出后父进程数据不变，并输出 cowtest pass.。

至少执行并记录：

~~~bash
make build-cowtest
make qemu
make build-forktest
make qemu
make build-exit
make qemu
~~~

报告必须说明独占可写页、共享 COW 页、写故障复制页和引用数为 1 后恢复可写页的状态转换；说明如何避免把原生只读页升级为可写页；说明 Dirty COW 的竞态风险、单 hart 限制和实际测试结果。
````

#### 2.3.2 分支任务：双重 GDB 调试系统调用与返回

````markdown
[PROMPT]
**任务**：完成 Lab5 分支任务“使用双重 GDB 调试系统调用以及返回”。使用 RISC-V GDB 调试 ucore，使用宿主机 GDB 附加 QEMU，观察用户态 ecall 触发、异常委托、S 态系统调用处理以及 sret 返回 U 态的完整流程。调试结果写入实验报告，不修改系统调用语义。

**操作要求**：

1. 使用 make build-exit 构建可重复触发 SYS_putc 的用户程序，用 make debug 启动 QEMU，用 make gdb 连接。
2. 在 RISC-V GDB 中用 add-symbol-file obj/__user_exit.out 加载用户程序符号；先用源码断点或反汇编定位实际 ecall 地址，不要预先假定固定地址。
3. 宿主机 GDB 使用带调试信息的 QEMU 构建，观察 helper_raise_exception、riscv_cpu_do_interrupt 和 helper_sret。
4. 两个 GDB 交替控制运行时，明确记录哪个终端执行 continue、哪个终端执行 si；不要让两个调试器同时争夺运行控制。
5. 记录关键命令、寄存器值、调用栈和 QEMU 源码位置，整理到报告 4.6。

[RELY]
1. user/libs/syscall.c 将调用号放入 a0、参数放入 a1 到 a5，执行 ecall，结果从 a0 读取。
2. 用户程序符号位于 obj/__user_exit.out，需要手动加载。
3. ucore 的 __alltraps 保存现场，exception_handler 识别 CAUSE_USER_ECALL，syscall 分发，__trapret 恢复现场并执行 sret。
4. QEMU 的 trans_ecall 位于 target/riscv/insn_trans/trans_privileged.inc.c；helper_raise_exception 和 helper_sret 位于 target/riscv/op_helper.c；riscv_cpu_do_interrupt 位于 target/riscv/cpu_helper.c；TCG 执行路径包含 code_gen_buffer 和 cpu_tb_exec。
5. CAUSE_USER_ECALL 为 8；异常进入内核后 sepc 保存 ecall 地址；ucore 前移 trapframe 的 epc 后，sret 应返回 ecall 后的下一条指令。
6. 宿主机 GDB 可使用 exception、env->pc、env->priv 和 env->sepc 设置 QEMU helper 条件断点。

[GUARANTEE]
必须交付以下调试材料：

1. 用户态 ecall 前的反汇编和寄存器；
2. helper_raise_exception 命中信息和调用栈；
3. riscv_cpu_do_interrupt 完成委托后的 priv、pc、sepc、scause、SPP；
4. ucore exception_handler、syscall 或 sys_putc 的参数和调用栈；
5. helper_sret 命中信息、返回地址和特权级；
6. sret 后用户态 pc、sp、a0；
7. 对 QEMU 源码位置、TCG 作用和一次实际调试问题的解释。

[SPECIFICATION]
按以下顺序完成：

**阶段一：用户态 ecall**

- 加载用户符号；
- 在 user/libs/syscall.c 设置源码断点；
- 用 x/i $pc 或 disassemble /r syscall 找到 ecall；
- 记录 pc、a0、a1，确认 a0 为 30，a1 为当前输出字符。

**阶段二：QEMU 接收异常**

- 在 helper_raise_exception 设置 exception == 8 且 env->priv == 0 的条件断点；
- 由 RISC-V GDB 单步执行 ecall；
- 记录 exception、env->pc、env->priv 和包含 code_gen_buffer 的调用栈。

**阶段三：异常委托**

在 riscv_cpu_do_interrupt 中记录 cause、async、medeleg 委托位、env->priv、env->pc、env->sepc、env->scause、env->stvec 和 mstatus.SPP，说明 U 态异常如何进入 S 态。

**阶段四：ucore 系统调用**

在 RISC-V GDB 中记录 tf->cause、tf->epc、tf->gpr.a0、tf->gpr.a1；说明 epc 已跳过 ecall，并记录 syscall → sys_putc 的调用栈。

**阶段五：sret 返回**

在 __trapret 的 sret 处停下；在 helper_sret 设置 env->priv == 1 且 env->sepc 指向返回地址的条件断点；记录执行前 S 态、执行后 retpc 和 U 态，以及 sret 后的用户 pc、sp、a0。

**阶段六：TCG 总结**

说明 TCG 将 RISC-V 指令翻译为宿主机代码，code_gen_buffer 出现在调用栈中；helper_raise_exception、riscv_cpu_do_interrupt 和 helper_sret 分别对应异常产生、异常委托和特权返回；并记录用户符号未自动加载、QEMU stripped 版本没有函数符号或双 GDB 运行控制冲突等实际问题。
````
