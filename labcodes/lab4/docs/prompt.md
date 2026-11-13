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

**功能模块三：进程调度**

```markdown
[PROMPT]
**任务**：补全 lab4 的"进程调度"功能模块。涉及三个文件：`kern/process/proc.c`（空闲线程循环 `cpu_idle` 与切换入口 `proc_run`）、`kern/schedule/sched.c`（调度器 `schedule`）、`kern/process/switch.S`（寄存器级上下文切换 `switch_to`）。这一模块的目标是让 `idleproc` 能够把 CPU 交给已经创建好的 `initproc`。

**操作要求**：你必须在项目中修改实际文件，保留现有代码和 Doxygen 注释，只在带有 `LAB4 function3` 标记的位置补全。把标记中的 `YOUR CODE` 换成学号 `2414007`，并且 `proc.c` 中的 `cpu_idle` 与 `proc_run` 各保留一行独立的标记。新增的代码要写必要的英文 Doxygen / 行内注释。不要修改 `struct context` 的字段顺序，不要修改与本模块无关的函数。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息，以工程中的真实代码为准，不要引入抢占、优先级、时间片等本实验没有要求的功能。

[RELY]

- 数据结构（`kern/process/proc.h`）：
  - `struct context`，字段顺序固定为 `ra, sp, s0, s1, ..., s11`，共 14 个机器字；
  - `struct proc_struct`，本模块用到 `state`、`runs`、`need_resched`（volatile）、`pgdir`（页目录的物理地址）、`context`、`list_link`；
  - 进程状态 `PROC_RUNNABLE` 等。
- 全局变量：`current`（正在占用 CPU 的 PCB）、`idleproc`（第 0 个线程，**不在** `proc_list` 中）、`proc_list`（带头结点的循环双向链表，头结点不是 `proc_struct`，各 PCB 通过 `list_link` 挂入）。
- 已有函数与宏：
  - 链表：`list_next()`、`le2proc(le, member)`；
  - 临界区：`local_intr_save(flag)`、`local_intr_restore(flag)`（`kern/sync/sync.h`，嵌套安全）；
  - 页表相关：`write_csr(satp, v)`、`SATP_MODE_SV39`（模式字段在 satp 的最高 4 位）、`RISCV_PGSHIFT`、`flush_tlb()`（`libs/riscv.h`、`kern/mm/pmm.h`）；`kern/mm/pmm.c` 的 `enable_paging()` 展示了 Sv39 下 satp 的正确写法；`libs/riscv.h` 里的 `lsatp()` 是 Sv32 遗留，不能使用；
  - 汇编：`libs/riscv.h` 提供 `REGBYTES`、`STORE`、`LOAD`；`switch.S` 需要自己 `#include <riscv.h>`；
  - 声明：`void switch_to(struct context *from, struct context *to);` 已在 `proc.c` 中声明；`schedule()` 在 `sched.h`；`cpu_idle` 在 `proc.h` 中声明为 `noreturn`。
- 已由其他模块保证的前提：
  - 新线程第一次运行时，`context.ra` 是 `forkret`，`context.sp` 是它的 trapframe，所以第一次 `switch_to` 的 `ret` 会进入 `forkret -> forkrets -> __trapret -> kernel_thread_entry`；
  - 所有 PCB 的 `pgdir` 都是启动页表 `boot_pgdir_pa`；
  - `proc_init` 已把 `idleproc->need_resched` 置 1，`do_fork` 通过 `wakeup_proc` 把新线程置为 `PROC_RUNNABLE` 并挂入 `proc_list`。

[GUARANTEE]
必须实现的函数（签名不得改变）：

- `void cpu_idle(void);`（`kern/process/proc.c`）
- `void schedule(void);`（`kern/schedule/sched.c`）
- `void proc_run(struct proc_struct *proc);`（`kern/process/proc.c`）
- `void switch_to(struct context *from, struct context *to);`（`kern/process/switch.S`，汇编，全局符号）

不需要新增辅助函数；如果确实需要，必须是 `static` 的，不能改变公开接口。

[SPECIFICATION]
**cpu_idle**

- Pre-Condition：当前执行流是 `idleproc`，`kern_init` 已经开启中断，`current == idleproc`。
- Post-Condition：函数永不返回；只要有线程请求重新调度，就一定会进入 `schedule`。
- Requirements：空闲线程自己不阻塞、不睡眠；判断重调度标志时必须每次都从内存重新读取。

**schedule**

- Pre-Condition：调用者是当前正在运行的线程（本实验中就是 `idleproc` 或某个内核线程），`current` 有效，`proc_list` 中的 PCB 状态一致。
- Post-Condition：调用者的重调度请求已被清除；如果存在其他可运行线程，CPU 已经被交给选中的线程，本函数在调用者下次被切换回来时才返回；如果没有任何可运行线程，调用者保持占用 CPU（退回 `idleproc`）。
- Case 1（调用者是 `idleproc`）：由于 `idleproc` 不在 `proc_list` 中，从链表的第一个 PCB 开始查找，让链表里的线程有平等的机会。
- Case 2（调用者不是 `idleproc`）：从调用者在链表中的下一个位置开始，环形查找一整圈（Round-Robin），最后才轮到调用者自己。
- Case 3（没有 `PROC_RUNNABLE` 线程）：选择 `idleproc` 作为下一个线程。
- Case 4（选中的线程就是 `current`）：不做切换，直接返回。
- Requirements：
  1. 整个"清标志 + 选择 + 切换"过程必须处于关中断的临界区内，且嵌套安全；
  2. 环形查找一圈即止，且必须跳过链表头结点，不能把头结点当成 PCB 解释；
  3. 被选中线程的 `runs` 计数加 1；
  4. 只负责选择策略（FIFO/轮转），不直接操作寄存器。

**proc_run**

- Pre-Condition：`proc` 是 `PROC_RUNNABLE` 的有效 PCB；调用者在 `current` 对应的线程里。
- Post-Condition：`current == proc`，页表根已指向 `proc->pgdir` 且旧的地址翻译缓存已失效，CPU 的寄存器上下文已从旧线程切换到 `proc`；本函数在旧线程被重新调度回来时才返回。
- Case 1（`proc == current`）：什么也不做，直接返回。
- Case 2（`proc != current`）：先保存旧线程的 PCB 指针，再依次完成"更新 `current`"、"切换页表"、"切换寄存器上下文"。
- Requirements：
  1. 整个切换过程必须关中断，并且在 `switch_to` 返回后（即旧线程被切换回来时）恢复进入前的中断状态；
  2. `current` 必须在切换栈之前更新，这样新线程里读到的 `current` 才是它自己；
  3. 页表只在 `pgdir` 的物理页号和 Sv39 模式位组合后写入，之后必须使旧映射的 TLB 失效；
  4. 新线程第一次运行不会从这里返回，它直接从 `forkret` 开始。

**switch_to**

- Pre-Condition：`a0` 指向旧线程的 `struct context`，`a1` 指向新线程的 `struct context`；调用发生在关中断状态下，调用方式与普通 C 函数调用相同。
- Post-Condition：旧线程的 `ra`、`sp`、`s0~s11` 已按 `struct context` 的字段顺序保存到 `*from`；这些寄存器已从 `*to` 加载；控制流已转移到新线程保存的 `ra`。
- Requirements：
  1. 只保存/恢复被调用者保存寄存器，不碰 `a0~a7`、`t0~t6`；
  2. 每个寄存器在结构体中的偏移必须与 `struct context` 一致，按机器字宽计算；
  3. 必须先保存全部旧寄存器再加载新寄存器，最后以 `ret` 结束；
  4. 不做任何调度决策，也不访问 `current`。
```

### 2.2 2414015-鲁昕宁

### 2.3 2411622-张雨函
