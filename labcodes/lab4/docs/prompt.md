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

**功能模块二：进程创建及初始化功能**

本模块建议按“先阅读和确认接口，再分阶段实现，最后编译验证”的顺序向 AI 提问。这样可以先固定模块边界，再分别处理 PCB 初始化、首次运行现场和资源登记，避免一次生成代码时遗漏失败清理或误改调度模块。

#### 第一轮：只阅读代码，确认实现边界

```markdown
请先阅读当前 lab4 工程中与进程创建相关的真实代码，只做分析，不修改文件。

请重点检查：

1. kern/process/proc.c、proc.h、entry.S；
2. kern/trap/trap.h 和 trapentry.S 中的 struct trapframe、forkrets、__trapret；
3. kern/mm/pmm.h、memlayout.h、kmalloc.h 中的页分配、内核栈和 CLONE_VM 定义；
4. kern/schedule/sched.h 中的 wakeup_proc 声明；
5. proc.c 中 `//LAB4 function2 YOUR CODE` 和 `//LAB4 function3` 的边界。

请输出：

- 功能模块二需要实现的六个函数及其调用关系；
- idleproc、initproc、current、proc_list、hash_list 的关系；
- kernel_thread → do_fork → copy_thread → forkret → forkrets → __trapret → kernel_thread_entry 的首次运行链；
- 每个失败路径需要释放的资源；
- 不能修改的模块和文件。

本轮禁止修改任何文件，也不要根据记忆猜测宏名或结构体字段。
```

#### 第二轮：实现 PCB 初始化和两个基础线程

```markdown
在上一轮阅读结果与当前工程真实代码一致的前提下，只修改 kern/process/proc.c 中功能模块二的标记区域，先实现 alloc_proc() 和 proc_init()，暂不实现 kernel_thread、do_fork、copy_thread、copy_mm，也不要修改模块三代码。

要求：

- 保留现有代码和 Doxygen 注释；
- 将 `//LAB4 function2 YOUR CODE` 中的 YOUR CODE 改为 `2411622`，保留 `LAB4 function2`；
- alloc_proc() 返回的 PCB 必须处于 PROC_UNINIT，所有字段和两个链表节点都必须有确定的初始值；
- proc_init() 必须初始化 proc_list 和所有 hash_list 桶；
- idleproc 使用 PID 0、bootstack、PROC_RUNNABLE 和 need_resched=1，不通过普通 fork 分配内核栈；
- current 必须指向 idleproc；
- 先保留 initproc 的创建调用所需接口，不要自行实现调度或上下文切换。

完成后先检查函数签名、字段初始化和 idleproc 的特殊处理，再进行编译；不要修改 pmm.c、sched.c、switch.S 或 Trap 入口。
```

#### 第三轮：实现内核线程的首次运行现场

```markdown
继续在当前工程上工作，只修改 kern/process/proc.c 的功能模块二区域，实现 kernel_thread() 和 copy_thread()，不要修改其他模块。

请先对照 kern/process/entry.S、trapentry.S 和 struct trapframe 的真实布局，再写代码。

要求：

- kernel_thread() 在调用者栈上清零临时 trapframe；
- gpr.s0 保存线程函数 fn，gpr.s1 保存参数 arg，epc 指向 kernel_thread_entry；
- 从当前 sstatus 出发设置 SSTATUS_SPP 和 SSTATUS_SPIE，清除 SSTATUS_SIE；
- 调用 do_fork(clone_flags | CLONE_VM, 0, &tf)，返回 PID 或负错误码；
- copy_thread() 在新线程内核栈顶部放置 trapframe，复制模板并设置子线程 gpr.a0=0；
- esp 非零时把它写入子线程的栈指针，esp 为零时让栈指针指向新 trapframe；
- proc->tf 指向新 trapframe，proc->context.ra 指向 forkret，proc->context.sp 指向该 trapframe；
- 不要把 struct context 误放到内核栈中，context 是 PCB 的字段；
- 不要修改 kernel_thread_entry、forkrets 或 __trapret。

完成后检查首次运行链是否能够从 switch_to 恢复到 forkret，再到 kernel_thread_entry。
```

#### 第四轮：实现资源分配、登记和地址空间接口

```markdown
继续只修改 kern/process/proc.c 的功能模块二区域，实现 do_fork() 和 copy_mm()，并把 proc_init() 中的 initproc 创建流程接通。

do_fork() 必须严格按以下顺序执行：

1. alloc_proc；
2. setup_kstack；
3. copy_mm；
4. copy_thread；
5. 在关中断临界区内分配 PID、设置 parent、加入 proc_list 和 hash_list、更新 nr_process；
6. 调用 wakeup_proc 使子线程变为 PROC_RUNNABLE；
7. 返回新 PID。

失败处理要求：

- alloc_proc 失败返回 -E_NO_MEM；
- setup_kstack 失败时释放 PCB；
- copy_mm 失败时释放内核栈和 PCB；
- 不得把失败的 PCB 留在任何链表中；
- 只有完成链表登记后才能调用 wakeup_proc。

copy_mm() 要求：

- current->mm 为 NULL 时，子进程 mm 保持 NULL 并返回 0；
- current->mm 非 NULL 且 clone_flags 含 CLONE_VM 时共享父进程 mm；
- 当前实验不实现非共享用户地址空间复制，保持接口完整并返回合适的负错误码；
- 不要为内核线程额外创建用户页表。

proc_init() 要求：

- 使用 kernel_thread(init_main, "Hello world!!", 0) 创建 initproc；
- initproc 应能通过 find_proc 找到，名称设置为 init；
- current 仍为 idleproc，initproc 处于 PROC_RUNNABLE；
- 失败时 panic，不要静默留下空的 initproc。

不要修改 proc_run、cpu_idle、schedule 或 switch_to。
```

#### 第五轮：编译和边界验证

```markdown
请验证刚才完成的功能模块二，不要为验证目的修改其他任务代码。

依次检查：

1. `grep` 确认 `//LAB4 function2 2411622` 存在，`LAB4 function3` 区域未被改写；
2. 单独编译 `kern/process/proc.c`，确认没有隐式声明、静态函数声明冲突或类型错误；
3. 检查 `git diff --check`；
4. 如果完整 Lab4 链接因模块一或模块三未完成而失败，请准确列出缺失符号，不要把它归因于模块二；
5. 对照测试契约核对 idleproc 的 PID、bootstack、状态和 current，以及 initproc 的 PID、父进程、状态、context.ra 和 context.sp。

请输出实际命令和结果，区分“模块二已验证”和“依赖其他模块、当前无法验证”的部分。
```

#### 综合实现提示词

完成前面的分阶段确认后，可以使用下面的综合提示词生成最终代码：

````markdown
[PROMPT]
**任务**：修改 `kern/process/proc.c`，完成 Lab4 功能模块二“进程创建及初始化功能”。需要实现 `alloc_proc()`、`proc_init()`、`kernel_thread()`、`do_fork()`、`copy_thread()` 和 `copy_mm()`，使内核能够把启动执行流登记为 `idleproc`，创建运行 `init_main()` 的 `initproc`，并构造新内核线程的首次运行现场。

**循序渐进要求**：先阅读并核对当前工程的真实定义，输出函数依赖和资源生命周期；再按 PCB 初始化、首次运行现场、do_fork 资源登记三个阶段修改代码；每个阶段完成后先编译和检查，再继续下一阶段。不要在没有确认真实结构体、宏和汇编布局的情况下凭记忆生成代码。

**操作要求**：直接修改工程中的实际文件，保留已有代码和 Doxygen 注释，只在 `//LAB4 function2 YOUR CODE` 对应区域补全。将 YOUR CODE 替换为 `2411622`，保留 `LAB4 function2` 字样。不得修改功能模块一的 `kern/mm/pmm.c`、功能模块三的 `kern/schedule/sched.c`、`cpu_idle()`、`proc_run()`、`switch_to()`，也不得修改 `struct proc_struct`、`struct context`、`struct trapframe`、`kernel_thread_entry` 或 `forkrets` 的定义和签名。

**输出要求**：以当前仓库真实代码为准完成文件修改；不要新增无关文件、调度策略、用户地址空间复制、进程退出回收或抢占逻辑。完成后报告修改文件、编译命令和实际验证边界。

[RELY]

1. `struct proc_struct` 的字段包括：`state`、`pid`、`runs`、`kstack`、`need_resched`、`parent`、`mm`、`context`、`tf`、`pgdir`、`flags`、`name`、`list_link`、`hash_link`。
2. `struct context` 的字段顺序固定为 `ra`、`sp`、`s0`～`s11`；`struct trapframe` 包含完整通用寄存器以及 `status`、`epc`、`badvaddr`、`cause`。
3. `PROC_UNINIT` 表示 PCB 已分配但尚未可运行，`PROC_RUNNABLE` 表示可以被调度；`CLONE_VM` 表示共享地址空间。
4. `boot_pgdir_pa` 是共享内核页表根的物理地址；`bootstack` 是启动执行流使用的内核栈；`KSTACKPAGE` 和 `KSTACKSIZE` 描述普通线程的内核栈大小。
5. 可使用当前工程已有的 `kmalloc()`、`kfree()`、`alloc_pages()`、`free_pages()`、`page2kva()`、`kva2page()`、`memset()`、`memcpy()`、`list_init()`、`list_add()`、`find_proc()`、`get_pid()`、`hash_proc()`、`wakeup_proc()`、`local_intr_save()` 和 `local_intr_restore()`。
6. `kernel_thread_entry` 在 `entry.S` 中从 `s0` 取线程函数、从 `s1` 取参数；`forkret()` 调用 `forkrets(current->tf)`，`forkrets()` 再进入 `__trapret`。
7. `SSTATUS_SPP`、`SSTATUS_SPIE` 和 `SSTATUS_SIE` 的真实定义来自 `libs/riscv.h`；首次运行状态必须从当前 `sstatus` 读取后再修改。
8. `wakeup_proc()` 要求 PCB 在调用时不能已经是 `PROC_RUNNABLE` 或 `PROC_ZOMBIE`；因此新 PCB 必须一直保持 `PROC_UNINIT`，直到链表登记完成。

[GUARANTEE]

必须实现以下函数，签名不得改变：

```c
static struct proc_struct *alloc_proc(void);
void proc_init(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
```

允许增加本文件内的 `static` 前置声明或辅助函数，但不得增加公开接口，不得修改模块三的标记区域。

[SPECIFICATION]

**alloc_proc**

- Pre-Condition：内核堆已经初始化，可以申请一个 PCB 大小的对象。
- Post-Condition：成功返回一个完整初始化但仍为 `PROC_UNINIT` 的 PCB；失败返回 NULL 且不泄漏资源。
- 必须设置 `pid=-1`、`runs=0`、`kstack=0`、`need_resched=0`，清空 `parent`、`mm`、`tf`、`flags`、`name` 和 `context`，设置 `pgdir=boot_pgdir_pa`，并初始化两个链表节点。

**proc_init**

- Pre-Condition：中断仍处于关闭状态，页表和内核堆已经初始化。
- Post-Condition：`proc_list`、`hash_list`、`idleproc`、`current` 和 `initproc` 均可用；`current == idleproc`。
- Case 1：创建 idleproc，PID 为 0，状态为 `PROC_RUNNABLE`，名称为 `idle`，内核栈为 `bootstack`，`need_resched=1`，不通过普通 `do_fork()` 分配栈，也不把它当作普通子线程加入 `proc_list`。
- Case 2：通过 `kernel_thread(init_main, "Hello world!!", 0)` 创建 initproc，设置名称为 `init`，使其成为 PID 1、父进程为 idleproc、状态为 `PROC_RUNNABLE` 的线程。
- Case 3：任一关键分配或创建失败时 panic，不能让初始化函数正常返回并留下不完整的进程表。

**kernel_thread**

- Pre-Condition：`fn` 是有效的内核线程入口，`current` 已建立。
- Post-Condition：返回新线程 PID 或负错误码；临时 Trap frame 不会被新线程直接引用，内容已经由 `do_fork()` 复制到新线程栈上。
- 必须清零临时 frame，将 `fn` 放入 `gpr.s0`、`arg` 放入 `gpr.s1`、将 `epc` 设为 `kernel_thread_entry`，从当前 `sstatus` 设置 SPP/SPIE 并清除 SIE，然后调用 `do_fork(clone_flags | CLONE_VM, 0, &tf)`。

**do_fork**

- Pre-Condition：`current` 有效，`tf` 指向可复制的 Trap frame 模板。
- Post-Condition：成功返回新 PID，新 PCB 已分配内核栈、完成首次运行现场、设置父进程、加入进程链表和 PID 哈希表，并处于 `PROC_RUNNABLE`；失败返回负错误码且已释放此前获得的资源。
- Case 1：PCB 分配失败，返回 `-E_NO_MEM`。
- Case 2：内核栈分配失败，释放 PCB 后返回错误。
- Case 3：`copy_mm` 失败，释放内核栈和 PCB 后返回错误。
- Case 4：所有前置步骤成功，在关中断临界区内完成 PID 分配、父进程设置、链表/哈希登记、进程数量更新和 `wakeup_proc()`。
- Requirements：新 PCB 在完成登记前不得变为 `PROC_RUNNABLE`；临界区必须覆盖 PID、父子关系、链表和唤醒操作。

**copy_thread**

- Pre-Condition：`proc->kstack` 已经指向一块有效的 `KSTACKPAGE` 连续页，`tf` 指向模板。
- Post-Condition：`proc->tf` 指向新栈中的 Trap frame，`proc->context.ra == forkret`，`proc->context.sp` 指向该 Trap frame。
- 必须复制完整模板并设置 `gpr.a0=0`；esp 非零时使用 esp，esp 为零时使用 Trap frame 地址作为内核线程的栈指针。
- Requirements：Trap frame 位于新线程内核栈中，context 仍保存在 PCB 字段中；不得破坏 `tf->epc`、`tf->status`、`s0` 或 `s1` 的线程入口信息。

**copy_mm**

- Pre-Condition：`current` 有效，子 PCB 已分配。
- Post-Condition：当前实验的内核线程不创建独立用户地址空间；父进程没有 mm 时子进程保持 `mm=NULL` 并成功返回。
- Case 1：`current->mm == NULL`，返回 0。
- Case 2：父进程有 mm 且 `clone_flags` 含 `CLONE_VM`，子进程共享父进程的 mm 并返回 0。
- Case 3：父进程有 mm 但不允许共享，当前实验不实现真正复制，应返回负错误码，且由 `do_fork()` 负责释放已分配资源。
````
