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

### 2.2 2414015-鲁昕宁

**功能模块一：调度器框架准备、代码更新与理解**

````markdown
[PROMPT]
**任务**：补全 lab6 功能模块一。只改两处已有函数：`kern/process/proc.c` 的 `alloc_proc()`，以及 `kern/trap/trap.c` 的 `interrupt_handler()` 里 `IRQ_S_TIMER` 这一支。目标是让新进程的调度字段处于“尚未入队”的确定状态，并让每次时钟中断把时间片交给调度类，而不是在中断处理函数里写死某一种算法。

**操作要求**：你必须在项目中修改实际文件，保留现有代码和 Doxygen 注释，只在这两处补全。`alloc_proc()` 的标记写成 `//lab6 function1-1 2414015 update alloc_proc`，时钟中断分支的标记写成 `//lab6 function1-2 2414015 update interrupt_handler`，替换原来的 `YOUR CODE`。不要改 `sched.c` 里已经写好的 `sched_init()`、`wakeup_proc()`、`schedule()` 和 `sched_class_proc_tick()`，不要实现 RR 或 Stride，不要改其他中断号和异常处理。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息，以工程中的真实代码为准。

[RELY]

- `struct proc_struct`（`kern/process/proc.h`）中本模块要初始化的字段：`rq`（`struct run_queue *`，不在任何运行队列时为 `NULL`）、`run_link`（`list_entry_t`）、`time_slice`（`int`）、`lab6_run_pool`（`skew_heap_entry_t`）、`lab6_stride`（`uint32_t`）、`lab6_priority`（`uint32_t`）。`kmalloc()` 不清零。
- 链表与斜堆：`list_init()` 把结点变成自环，表示不在任何链表中；`skew_heap_init()` 把结点的 `parent`、`left`、`right` 置为 `NULL`。二者的声明分别在 `libs/list.h` 和 `libs/skew_heap.h`。`proc.c` 已经包含 `sched.h`，而 `sched.h` 包含这两个头文件。
- 时钟中断：`IRQ_S_TIMER`、`clock_set_next_event()`、全局 `ticks`。`trap.c` 已经包含 `sched.h`。`current` 是当前进程，可能为 `NULL`。
- 已有接口 `void sched_class_proc_tick(struct proc_struct *proc)`（`kern/schedule/sched.c`）：`proc` 不是 `idleproc` 时调用当前调度类的 `proc_tick`；`proc` 是 `idleproc` 时只把 `need_resched` 置 1。空闲进程不能入队。
- 编译宏 `DEBUG_GRADE` 下，`print_ticks()` 会 panic。因此时钟分支不能再调用 `print_ticks()`，也不能只在 `ticks % TICK_NUM == 0` 时才请求调度。

[GUARANTEE]

- 修改 `static struct proc_struct *alloc_proc(void)`：在现有 lab4、lab5 字段初始化之后补上 lab6 调度字段，签名、返回值和失败时返回 `NULL` 的行为不变。
- 修改 `void interrupt_handler(struct trapframe *tf)` 的 `IRQ_S_TIMER` 分支。其余 `case` 保持原样。
- 不新增公开函数。

[SPECIFICATION]

**alloc_proc**

- **Pre-Condition**：调用发生在进程尚未进入任何运行队列之前。`kmalloc()` 成功时返回的内存内容未定义。
- **Post-Condition**：成功时返回的 PCB 仍是 `PROC_UNINIT`，且调度字段表示“不在运行队列中”：`rq` 为 `NULL`，`run_link` 是自环结点，`time_slice` 为 0，`lab6_run_pool` 是一棵空斜堆结点，`lab6_stride` 为 0，`lab6_priority` 为 1。分配失败时返回 `NULL`，不泄漏。本函数不把 PCB 插入运行队列。
- **Requirements**：优先级初值必须是正数。0 会在 Stride 计算步长时做除数。

**interrupt_handler 的 IRQ_S_TIMER 分支**

- **Pre-Condition**：`tf->cause` 去掉中断标志位后等于 `IRQ_S_TIMER`。`sched_init()` 已经绑定调度类。
- **Post-Condition**：下一次时钟事件已经预约，`ticks` 增加 1。若 `current` 不是 `NULL`，这一拍交给 `sched_class_proc_tick(current)`。`tf` 不被修改。
- **Case 1**：`current` 为 `NULL`。只预约下一次时钟并增加 `ticks`。
- **Case 2**：`current` 有效。除 Case 1 的动作外，调用 `sched_class_proc_tick(current)`。时间片是否耗尽、是否置 `need_resched`，由调度类决定；空闲进程由 `sched_class_proc_tick()` 自己处理。
- **Requirements**：每一拍都要交给调度类，不能再按 `TICK_NUM` 节流，也不能调用 `print_ticks()`。
````

**功能模块二：Round Robin 调度算法**

````markdown
[PROMPT]
**任务**：在 `kern/schedule/default_sched.c` 实现 Round Robin 调度类，并导出名为 `RR_scheduler` 的 `default_sched_class`。运行队列使用 `struct run_queue` 的循环链表 `run_list`，进程通过 `proc_struct::run_link` 挂入。`sched_init()` 已经把 `sched_class` 指到 `&default_sched_class`，本任务不要改这个绑定。

**操作要求**：你必须在项目中修改实际文件，只在 `//lab6 function2 YOUR CODE` 处补全，并把标记改成 `//lab6 function2 2414015`。五个操作函数都是 `static`。不要改 `sched.c`、`sched.h` 和 Stride 文件。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息，以工程中的真实代码为准。类名必须是字符串 `"RR_scheduler"`。

[RELY]

- `struct sched_class`（`kern/schedule/sched.h`）的字段顺序：`name`、`init`、`enqueue`、`dequeue`、`pick_next`、`proc_tick`。`default_sched_class` 已在 `kern/schedule/default_sched.h` 中声明为 `extern`。
- `struct run_queue`：`run_list` 是带头结点的循环双向链表，`proc_num` 是队列中的进程数，`max_time_slice` 是时间片长度。宏 `MAX_TIME_SLICE` 的值是 5。
- 链表（`libs/list.h`）：`list_init()`、`list_empty()`、`list_next()`、`list_add_before(listelm, elm)`（插到 `listelm` 之前，对头结点调用就是插到队尾）、`list_del_init()`（摘下并恢复为自环）。`le2proc(le, run_link)` 从 `run_link` 回到 `struct proc_struct`。
- `schedule()` 的调用顺序已经固定：当前进程若仍是 `PROC_RUNNABLE` 则 `enqueue`，然后 `pick_next`，选中后再 `dequeue`。因此 `pick_next` 不能把进程从队列里摘掉。正在运行的进程不在队列中；`proc_tick` 针对的就是这个进程。
- 评分测试用一块清零的 `struct run_queue` 调用 `init`，随后断言 `proc_num == 0` 且 `max_time_slice == MAX_TIME_SLICE`。入队一个 `time_slice == 0` 的进程后，断言时间片变成 `MAX_TIME_SLICE`、`proc_num == 1`，`pick_next` 返回该进程；再把时间片设为 1 调用 `proc_tick`，断言时间片变为 0 且 `need_resched == 1`；`dequeue` 后 `proc_num == 0`。

[GUARANTEE]

- `static void RR_init(struct run_queue *rq);`
- `static void RR_enqueue(struct run_queue *rq, struct proc_struct *proc);`
- `static void RR_dequeue(struct run_queue *rq, struct proc_struct *proc);`
- `static struct proc_struct *RR_pick_next(struct run_queue *rq);`
- `static void RR_proc_tick(struct run_queue *rq, struct proc_struct *proc);`
- `struct sched_class default_sched_class`，`name` 为 `"RR_scheduler"`，五个函数指针分别指向上面的函数。
- 不新增公开接口。

[SPECIFICATION]

**RR_init**

- **Pre-Condition**：`rq` 非空。调用方可能还没有设置 `max_time_slice`。
- **Post-Condition**：`run_list` 是空的循环链表，`proc_num` 为 0，`max_time_slice` 等于 `MAX_TIME_SLICE`。

**RR_enqueue**

- **Pre-Condition**：`proc` 当前不在该运行队列中，`run_link` 是自环。`rq` 已初始化。
- **Post-Condition**：`proc` 接在 `run_list` 的队尾，`proc->rq` 指向 `rq`，`proc_num` 增加 1。若 `time_slice` 为 0 或大于 `rq->max_time_slice`，则把它设为 `rq->max_time_slice`；否则保留剩余时间片。
- **Requirements**：同一进程不能入队两次。可用 `assert(list_empty(&proc->run_link))` 表达这个前提。

**RR_dequeue**

- **Pre-Condition**：`proc` 正通过 `run_link` 挂在 `rq` 上，且 `proc->rq == rq`。
- **Post-Condition**：`proc` 从链表摘下，`run_link` 恢复为自环，`proc->rq` 为 `NULL`，`proc_num` 减少 1。不修改 `time_slice`。

**RR_pick_next**

- **Pre-Condition**：`rq` 已初始化。
- **Post-Condition**：队列为空时返回 `NULL`。否则返回队头进程，进程仍留在队列中，`proc_num` 不变。

**RR_proc_tick**

- **Pre-Condition**：`proc` 是当前正在运行的进程，不在运行队列中。
- **Post-Condition**：若 `time_slice` 大于 0，则减 1。减完后若 `time_slice` 为 0，则 `need_resched` 置 1。不把进程放回队列。
````

**功能模块三：Stride Scheduling 调度算法**

````markdown
[PROMPT]
**任务**：在 `kern/schedule/default_sched_stride.c` 实现 Stride 调度类，并导出名为 `stride_scheduler` 的 `stride_sched_class`。可运行进程放在 `rq->lab6_run_pool` 这棵斜堆里，按 `lab6_stride` 取最小者。本文件自己定义 `BIG_STRIDE`。不要改 `sched_init()`：它继续绑定 `default_sched_class`。两个调度类对象必须同时存在，评分程序按全局符号分别调用。

**操作要求**：你必须在项目中修改实际文件，只在 `//lab6 function3 YOUR CODE` 处补全，并把标记改成 `//lab6 function3 2414015`。比较函数和五个操作函数都是 `static`。不要改斜堆实现，不要改 `lab6_set_priority()`。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息，以工程中的真实代码为准。类名必须是字符串 `"stride_scheduler"`。

[RELY]

- `struct sched_class` 与 `struct run_queue` 见 `kern/schedule/sched.h`。Stride 使用 `lab6_run_pool`（堆根，空堆为 `NULL`）和 `proc_num`；`run_list` 本算法不用，但 `init` 仍把它初始化为空链表。`stride_sched_class` 已在 `default_sched.h` 中声明。
- 斜堆（`libs/skew_heap.h`）：`skew_heap_insert(root, node, comp)` 会先对 `node` 做 `skew_heap_init`，再返回新的堆根；`skew_heap_remove(root, node, comp)` 返回删除后的堆根，结点不在堆中时不能调用。比较函数在 `a` 应比 `b` 更靠近根时返回 `-1`，否则返回 `0` 或 `1`。`le2proc(node, lab6_run_pool)` 从斜堆结点回到 PCB。
- 进程字段：`lab6_stride`、`lab6_priority` 都是 `uint32_t`，`time_slice` 是 `int`。步长是 `BIG_STRIDE / priority`。`BIG_STRIDE` 取 `0x7FFFFFFF`，保证任意两个 stride 的差值用 `int32_t` 解释时，环绕之后仍能比出谁更小：比较必须写成 `(int32_t)(stride_a - stride_b)`，不能直接比较两个无符号数。
- `schedule()` 先 `enqueue` 再 `pick_next` 再 `dequeue`。`pick_next` 在进程仍在堆中时加上它的步长，摘除由随后的 `dequeue` 完成。
- 评分测试：`init` 后 `proc_num == 0` 且 `lab6_run_pool == NULL`。先入队优先级 1、stride 0 的进程，再入队优先级 2、stride 100 的进程，`pick_next` 必须返回前者，且它的 `lab6_stride` 变为正数。`proc_tick` 对时间片为 1 的进程减到 0 并置 `need_resched`。两次 `dequeue` 后 `proc_num` 为 0，堆根为 `NULL`。

[GUARANTEE]

- `#define BIG_STRIDE 0x7FFFFFFF`
- `static int proc_stride_comp_f(void *a, void *b);`
- `static void stride_init(struct run_queue *rq);`
- `static void stride_enqueue(struct run_queue *rq, struct proc_struct *proc);`
- `static void stride_dequeue(struct run_queue *rq, struct proc_struct *proc);`
- `static struct proc_struct *stride_pick_next(struct run_queue *rq);`
- `static void stride_proc_tick(struct run_queue *rq, struct proc_struct *proc);`
- `struct sched_class stride_sched_class`，`name` 为 `"stride_scheduler"`，五个函数指针指向上面的操作函数。
- 不新增公开接口，不修改 `sched_init()`。

[SPECIFICATION]

**proc_stride_comp_f**

- **Pre-Condition**：`a`、`b` 都是某个 PCB 的 `lab6_run_pool` 结点。
- **Post-Condition**：用两个进程 `lab6_stride` 的有符号差比较。差为负返回 `-1`，差为 0 返回 `0`，差为正返回 `1`。

**stride_init**

- **Pre-Condition**：`rq` 非空。
- **Post-Condition**：`run_list` 为空链表，`lab6_run_pool` 为 `NULL`，`proc_num` 为 0，`max_time_slice` 等于 `MAX_TIME_SLICE`。

**stride_enqueue**

- **Pre-Condition**：`proc` 不在该斜堆中，`rq` 已初始化。
- **Post-Condition**：`proc->lab6_run_pool` 插入 `rq->lab6_run_pool`，堆根更新为插入后的根；`proc->rq` 指向 `rq`，`proc_num` 增加 1。`time_slice` 为 0 或大于 `max_time_slice` 时设为 `max_time_slice`。`lab6_priority` 为 0 时改为 1。不修改 `lab6_stride`。

**stride_dequeue**

- **Pre-Condition**：`proc` 在该堆中，且 `proc->rq == rq`。
- **Post-Condition**：结点从堆中删除，堆根更新；`proc->rq` 为 `NULL`，`proc_num` 减少 1。堆变空时根为 `NULL`。

**stride_pick_next**

- **Pre-Condition**：`rq` 已初始化。
- **Post-Condition**：堆空时返回 `NULL`。否则返回堆根对应的进程，进程仍留在堆中。设 `priority` 为该进程的 `lab6_priority`，0 按 1 处理，然后把 `BIG_STRIDE / priority` 加到它的 `lab6_stride` 上。加法使用 `uint32_t`，允许回绕。

**stride_proc_tick**

- **Pre-Condition**：`proc` 正在运行，不在斜堆中。
- **Post-Condition**：与 RR 相同：`time_slice` 大于 0 时减 1，减到 0 时把 `need_resched` 置 1。不修改 stride，也不把进程放回堆。
````

### 2.3 2411622-张雨函
