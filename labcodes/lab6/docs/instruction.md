# lab6 进程调度

## 实验目的

-   理解操作系统的调度管理机制
-   熟悉 ucore 的系统调度器框架，实现缺省的Round-Robin 调度算法
-   基于调度器框架实现一个(Stride Scheduling)调度算法来替换缺省的调度算法

## 实验内容

在前两章中，我们已经分别实现了内核进程和用户进程，并且让他们正确运行了起来。同时我们也实现了一个简单的调度算法，FIFO调度算法，来对我们的进程进行调度,可通过阅读实验五下的 kern/schedule/sched.c 的 schedule 函数的实现来了解其FIFO调度策略。但是，单单如此就够了吗？显然，我们可以让ucore支持更加丰富的调度算法，从而满足各方面的调度需求。与实验五相比，实验六专门需要针对处理器调度框架和各种算法进行设计与实现，为此对ucore的调度部分进行了适当的修改，使得kern/schedule/sched.c 只实现调度器框架，而不再涉及具体的调度算法实现。而调度算法在单独的文件（default_sched.[ch]）中实现。

在本次实验中，我们在`init/init.c`中加入了对`sched_init`函数的调用。这个函数主要完成调度器和特定调度算法的绑定。初始化后，我们在调度函数中就可以使用相应的接口，切换你实现的不同的调度算法了。这也是在C语言环境下对于面向对象编程模式的一种模仿。这样之后，我们只需要关注于实现调度类的接口即可，操作系统也同样不关心调度类具体的实现，方便了新调度算法的开发。本次实验，主要是熟悉ucore的系统调度器框架，以及基于此框架实现Round-Robin（RR） 调度算法。然后进一步完成Stride调度算法。

### 练习

本实验建立在前面实验已经完成的物理内存、虚拟内存和进程管理基础上，重点理解这些基础如何与调度器框架衔接。学习过程可以沿着“理解调度框架、实现 Round Robin、切换并实现 Stride”的顺序展开：先认识调度类和运行队列如何组织一次调度，再通过时间片轮转观察进程交替运行，最后结合 Stride 调度理解优先级与 CPU 时间分配之间的关系。

本实验依赖实验2/3/4/5。请使用你此前设计的提示词生成相应实验代码，并将生成结果填写到本实验代码中标有“lab2”/“lab3”/“lab4”/“lab5”注释的对应位置，确保代码能够正确编译。

补回前序实验代码后，还需要按照本实验的更新标记完成 `alloc_proc()` 和 `interrupt_handler()` 的 Lab 6 更新。

#### 功能模块一：调度器框架准备、代码更新与理解（设计提示词借助 ai 完成）

##### 任务一：更新进程控制块初始化（设计提示词借助 ai 完成）

要求实现的函数有：

```c
static struct proc_struct *alloc_proc(void); (kern/process/proc.c)
```

在补回前序实验代码后，请设计 `prompt`，让 AI 在 `kern/process/proc.c` 中标有 `//lab6 function1-1 YOUR CODE update alloc_proc` 的位置完成 Lab 6 更新。需要初始化调度器使用的 `rq`、`run_link`、`time_slice`、`lab6_run_pool`、`lab6_stride` 和 `lab6_priority` 字段，使新建进程进入调度框架前具有确定的初始状态。

之所以需要更新 `alloc_proc()`，是因为 Lab 6 在进程控制块中加入了运行队列、时间片和 Stride 调度相关字段。若这些字段没有在创建进程时初始化，调度器后续进行入队、出队或选择进程时可能使用未定义的链表节点、斜堆节点或调度参数。更新思路是在保留前序实验初始化代码的基础上，分别将运行队列指针置空、初始化链表和斜堆节点，并将时间片、Stride 和优先级设置为合适的初始值。

整个任务的注意事项为：不能删除或改写前序实验对其他进程控制块字段的初始化；调度字段必须与 `struct proc_struct` 的实际定义一致，链表节点和斜堆节点不能保留未定义内容；初始化逻辑不能提前把进程加入运行队列。

##### 任务二：更新时钟中断处理（设计提示词借助 ai 完成）

要求实现的函数有：

```c
void interrupt_handler(struct trapframe *tf); (kern/trap/trap.c)
```

在保留实验三中断处理代码的基础上，请设计 `prompt`，让 AI 在 `kern/trap/trap.c` 中标有 `//lab6 function1-2 YOUR CODE update interrupt_handler` 的位置完成 Lab 6 更新。`IRQ_S_TIMER` 分支需要重新设置下一次时钟事件、更新时钟计数，并通过 `sched_class_proc_tick(current)` 将时钟节拍交给当前调度类处理，使 RR 或 Stride 能够更新进程时间片和 `need_resched`。

之所以需要更新 `interrupt_handler()`，是因为 Lab 6 的 RR 和 Stride 调度都依赖时钟中断驱动时间片消耗；如果时钟中断没有把节拍传递给调度框架，进程就不会及时触发重新调度。更新思路是在实验三已有的定时器分支中继续设置下一次时钟事件、递增时钟计数，并在 `current` 有效时调用统一的 `sched_class_proc_tick(current)`，由当前调度类决定如何处理时间片，而不是在中断处理函数中直接实现某一种调度算法。

整个任务的注意事项为：不能删除其他中断原因的处理逻辑，不能在中断处理函数中直接写死某一种调度算法；应保持时钟事件能够持续触发，并在 `current` 有效时调用调度框架提供的节拍接口。

##### 任务三：理解调度器框架的实现（理解题）

本任务用于理解 ucore 如何把通用调度流程与具体调度策略分开。请仔细阅读调度器框架的实现，并在实验报告中分别完成以下分析：

-   **调度类结构体分析**：解释 `struct sched_class` 中每个函数指针的作用和调用时机，并分析为什么需要使用函数指针组织这些调度接口，而不是直接实现固定的调度函数。
-   **运行队列结构体分析**：比较 `lab5` 和 `lab6` 中 `struct run_queue` 的差异，说明为什么 `lab6` 的运行队列需要同时支持链表和斜堆两种数据结构。
-   **调度器框架函数分析**：分析 `sched_init()`、`wakeup_proc()` 和 `schedule()` 在 `lab6` 中的实现变化，说明这些函数如何与具体的调度算法解耦。
-   **调度类初始化流程**：描述从内核启动到调度器初始化完成的完整流程，并分析 `default_sched_class` 如何与调度器框架建立关联。
-   **进程调度流程**：绘制完整的进程调度流程图，包括时钟中断触发、`proc_tick` 被调用、`schedule()` 执行以及调度类各接口的调用顺序，并解释 `need_resched` 标志在调度过程中的作用。
-   **调度算法切换机制**：分析添加新的调度算法（如 Stride）时需要修改哪些代码，并说明当前的框架设计为什么便于切换调度算法。

#### 功能模块二：Round Robin 调度算法（设计提示词借助 ai 完成）

要求实现的函数有：

```c
static void RR_init(struct run_queue *rq);
static void RR_enqueue(struct run_queue *rq, struct proc_struct *proc);
static void RR_dequeue(struct run_queue *rq, struct proc_struct *proc);
static struct proc_struct *RR_pick_next(struct run_queue *rq);
static void RR_proc_tick(struct run_queue *rq, struct proc_struct *proc);
```

完成功能模块一后，请在理解调度器框架的基础上设计`prompt`，让 AI 在`kern/schedule/default_sched.c`中标有`//lab6 function2 YOUR CODE`的位置生成完整的时间片轮转（Round Robin）调度模块。设计提示词时，可以重点阅读`sched.h`中的`struct sched_class`，理解其成员及各调度接口的作用。

整个功能模块的具体功能为：生成的实现应完整处理运行队列初始化、可运行进程入队和出队、下一进程选择，以及时钟中断到来后的时间片更新和重新调度标记。`RR_init`负责初始化运行队列，`RR_enqueue`和`RR_dequeue`负责维护可运行进程，`RR_pick_next`负责选择队首进程，`RR_proc_tick`负责消耗当前进程的时间片。实现完成后还需要定义`default_sched_class`，将这些函数接入统一调度框架。

整个功能模块的注意事项为：实现时应正确维护运行队列、进程所属队列、进程数量和时间片等状态，并使用框架提供的链表、进程转换宏和断言等机制。需要妥善处理空队列、重复入队、时间片无效或耗尽以及空闲进程等边界情况；修改队列时，`run_link`、`rq`和`proc_num`必须保持一致，时间片耗尽时还应正确设置`need_resched`。最终生成的代码应能够正常编译并通过`make grade`测试。

整个功能模块应该满足的对外接口为：RR 调度器通过`default_sched_class`向通用调度框架提供初始化、入队、出队、选择下一进程和处理时钟节拍五项操作。`RR_pick_next`应在队列非空时返回下一个可运行进程，在队列为空时返回`NULL`；其余函数应在不破坏框架状态的前提下完成各自操作。

整个功能模块可以借助的接口函数、结构体、数据结构为：可以重点阅读`sched.h`中的`struct sched_class`和`struct run_queue`、`proc.h`中的`struct proc_struct`，并使用`list_init()`、`list_add_before()`、`list_del_init()`、`list_next()`、`list_empty()`、`le2proc()`和`assert()`等已有接口。`run_list`、`proc_num`、`max_time_slice`、`run_link`、`rq`、`time_slice`和`need_resched`等字段共同记录 RR 调度过程中的状态。

Tips：如果对 RR 调度器如何接入现有框架还不熟悉，可以仔细阅读实验手册“RR调度算法实现”一节，并留意`kern/schedule/sched.c`中`sched_init()`选择和初始化当前调度类的过程。

请在实验报告中完成：

-   提交本任务使用的完整`prompt`。
-   比较一个在`lab5`和`lab6`中均存在但实现不同的函数，说明为什么需要这一改动，以及不修改可能产生的问题。可以参考`kern/schedule/sched.c`，也可以选择其他函数。
-   描述你的提示词让 AI 生成的 RR 调度模块如何实现各项调度接口，解释关键链表操作和**边界情况**的处理方式。
-   展示`make grade`的**输出结果**，并描述在 QEMU 中观察到的调度现象。
-   分析 Round Robin 调度算法的优缺点，并讨论时间片大小对系统性能的影响。
-   **拓展思考**：如果要让 AI 生成支持优先级的 RR 调度器，你会如何修改提示词？当前生成的实现是否支持多核调度？如果不支持，提示词还需要增加哪些要求？

#### 功能模块三：Stride Scheduling 调度算法（设计提示词借助 ai 完成）

要求实现的函数有：

```c
static int proc_stride_comp_f(void *a, void *b);
static void stride_init(struct run_queue *rq);
static void stride_enqueue(struct run_queue *rq, struct proc_struct *proc);
static void stride_dequeue(struct run_queue *rq, struct proc_struct *proc);
static struct proc_struct *stride_pick_next(struct run_queue *rq);
static void stride_proc_tick(struct run_queue *rq, struct proc_struct *proc);
```

请根据后续实验手册对 Stride Scheduling 的介绍设计`prompt`，让 AI 在`kern/schedule/default_sched_stride.c`中标有`//lab6 function3 YOUR CODE`的位置生成完整的 Stride 调度模块。

整个功能模块的具体功能为：`proc_stride_comp_f`负责比较两个进程的 stride，`stride_init`负责初始化运行队列，`stride_enqueue`和`stride_dequeue`负责维护可运行进程，`stride_pick_next`负责选择当前 stride 最小的进程并更新其 stride，`stride_proc_tick`负责更新时间片和重新调度标记。实现完成后还需要定义`stride_sched_class`，将这些函数接入现有调度框架。

整个功能模块的注意事项为：生成的实现应根据进程优先级维护 stride 和步长，并正确处理无符号整数溢出、优先级为零、空运行队列、时间片耗尽和运行队列状态同步等情况。使用斜堆时，应保持`lab6_run_pool`、`rq`和`proc_num`等状态一致；选择进程后，应按照优先级更新其 stride。完成后需要通过编译和运行测试，结合`priority.c`观察不同优先级进程获得的 CPU 时间是否大致符合优先级比例。

整个功能模块应该满足的对外接口为：Stride 调度器通过`stride_sched_class`提供初始化、入队、出队、选择下一进程和处理时钟节拍五项操作。`stride_pick_next`应返回当前 stride 最小的可运行进程，运行队列为空时返回`NULL`；其余接口应正确维护进程时间片及运行队列状态，使通用调度框架无须了解 Stride 的内部实现。

整个功能模块可以借助的接口函数、结构体、数据结构为：可以使用`sched.h`中的`struct sched_class`和`struct run_queue`、`proc.h`中的`struct proc_struct`，以及`skew_heap_insert()`、`skew_heap_remove()`、`skew_heap_init()`、`le2proc()`和`assert()`等已有接口。`lab6_run_pool`、`lab6_stride`、`lab6_priority`、`time_slice`、`need_resched`和`BIG_STRIDE`共同决定 Stride 调度器的队列组织与调度结果。

Tips：完成 Stride 调度模块后，还需要在`sched_init()`中切换调度方法，选择`stride_sched_class`作为当前调度类，否则系统运行的仍是 RR 调度器。如果对算法或数据结构还不熟悉，可以先阅读实验手册“stride调度算法”下的“基本思路”“使用优先队列实现 Stride Scheduling”和执行结果示例。

后面的实验文档部分给出了Stride调度算法的大体描述。这里给出Stride调度算法的一些相关的资料（目前网上中文的资料比较欠缺）。

-   [strid-shed paper location](http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.138.3502&rank=1)
-   也可 GOOGLE “Stride Scheduling” 来查找相关资料

请在实验报告中完成：

-   提交本任务使用的完整`prompt`。
-   说明提示词如何描述 Stride 调度规则、运行队列数据结构、`stride`溢出处理和调度框架接口，并分析 AI 生成结果是否满足这些要求。
-   简要证明或说明（不必特别严谨，但应当能够“说服你自己”），为什么 Stride 算法中，经过足够多的时间片之后，每个进程分配到的时间片数目和优先级成正比。

#### 扩展练习 Challenge 1：实现其他基本调度算法

在 ucore 上实现尽可能多的各种基本调度算法（FIFO、SJF 等），并设计各种测试用例，能够定量分析各种调度算法在不同指标上的差异，说明各调度算法的适用范围。

#### 扩展练习 Challenge 2：Linux 的 CFS 调度算法（感兴趣的同学可以学习并实现，不计入成绩）

在ucore的调度器框架下也可以实现Linux的CFS调度算法。可阅读相关Linux内核书籍或查询网上资料，这里给出CFS调度算法的一些相关的资料。

-   [CFS 调度器 - Linux 内核文档](https://docs.linuxkernel.org.cn/scheduler/sched-design-CFS.html)
-   [Linux完全公平调度(CFS)深度解剖（安卓流畅度核心）](https://www.cnblogs.com/16msyanjiusuo/articles/18720910)
-   [一文搞懂linux cfs调度器](https://zhuanlan.zhihu.com/p/556295381)

可通过这些相关资料了解CFS的细节，然后大致实现在ucore中。（可以作为Challenge 2的实现）

### 项目组成

```
lab6
├── CMakeLists.txt
├── Makefile
├── kern
│   ├── debug
│   │   ├── assert.h
│   │   ├── kdebug.c
│   │   ├── kdebug.h
│   │   ├── kmonitor.c
│   │   ├── kmonitor.h
│   │   └── panic.c
│   ├── driver
│   │   ├── clock.c
│   │   ├── clock.h
│   │   ├── console.c
│   │   ├── console.h
│   │   ├── dtb.c
│   │   ├── dtb.h
│   │   ├── intr.c
│   │   ├── intr.h
│   │   ├── kbdreg.h
│   │   ├── picirq.c
│   │   └── picirq.h
│   ├── init
│   │   ├── entry.S
│   │   └── init.c
│   ├── libs
│   │   ├── readline.c
│   │   └── stdio.c
│   ├── mm
│   │   ├── default_pmm.c
│   │   ├── default_pmm.h
│   │   ├── kmalloc.c
│   │   ├── kmalloc.h
│   │   ├── memlayout.h
│   │   ├── mmu.h
│   │   ├── pmm.c
│   │   ├── pmm.h
│   │   ├── vmm.c
│   │   └── vmm.h
│   ├── process
│   │   ├── entry.S
│   │   ├── proc.c
│   │   ├── proc.h
│   │   └── switch.S
│   ├── schedule
│   │   ├── default_sched.c
│   │   ├── default_sched.h
│   │   ├── default_sched_stride.c
│   │   ├── sched.c
│   │   └── sched.h
│   ├── sync
│   │   └── sync.h
│   ├── syscall
│   │   ├── syscall.c
│   │   └── syscall.h
│   └── trap
│       ├── trap.c
│       ├── trap.h
│       └── trapentry.S
├── libs
│   ├── atomic.h
│   ├── defs.h
│   ├── elf.h
│   ├── error.h
│   ├── hash.c
│   ├── list.h
│   ├── printfmt.c
│   ├── rand.c
│   ├── riscv.h
│   ├── sbi.h
│   ├── skew_heap.h
│   ├── stdarg.h
│   ├── stdio.h
│   ├── stdlib.h
│   ├── string.c
│   ├── string.h
│   └── unistd.h
├── tools
│   ├── boot.ld
│   ├── function.mk
│   ├── gdbinit
│   ├── grade.sh
│   ├── kernel.ld
│   ├── sign.c
│   ├── user.ld
│   └── vector.c
└── user
    ├── badarg.c
    ├── badsegment.c
    ├── divzero.c
    ├── exit.c
    ├── faultread.c
    ├── faultreadkernel.c
    ├── forktest.c
    ├── forktree.c
    ├── hello.c
    ├── libs
    │   ├── initcode.S
    │   ├── panic.c
    │   ├── stdio.c
    │   ├── syscall.c
    │   ├── syscall.h
    │   ├── ulib.c
    │   ├── ulib.h
    │   └── umain.c
    ├── matrix.c
    ├── pgdir.c
    ├── priority.c
    ├── softint.c
    ├── spin.c
    ├── testbss.c
    ├── waitkill.c
    └── yield.c

15 directories, 98 files
```

相对与实验五，实验六的主要改动简单说明如下：

-   libs/skew_heap.h:提供了基本的优先队列数据结构，为本次实验提供了抽象数据结构方面的支持。
-   kern/process/proc.[ch]：proc.h中扩展了proc_struct的成员变量，用于RR和stride调度算法。proc.c中实现了lab6_set_priority，用于设置进程的优先级。
-   kern/schedule/{sched.h,sched.c}: 定义了 ucore 的调度器框架，其中包括相关的数据结构（包括调度器的接口和运行队列的结构），和具体的运行时机制。
-   kern/schedule/{default_sched.h,default_sched.c}: default_sched.h提供调度类的对外声明，default_sched.c作为 RR 调度模块的代码生成区域，需要通过设计提示词让 AI 生成完整实现。
-   kern/schedule/default_sched_stride.c: Stride Scheduling调度模块的代码生成区域，需要通过设计提示词让 AI 生成完整实现并接入现有调度框架。
-   kern/syscall/syscall.[ch]: 增加了sys_gettime系统调用，便于用户进程获取当前时钟值；增加了sys_lab6_set_priority系统调用，便于用户进程设置进程优先级（给priority.c用）
-   user/{matrix.c,priority.c,. . . }: 相关的一些测试用户程序，测试调度算法的正确性，user目录下包含但不限于这些程序。在完成实验过程中，建议阅读这些测试程序，以了解这些程序的行为，便于进行调试。

## 调度框架和调度算法设计与实现

### 进程状态

在ucore中，进程有如下几个状态：

-   `PROC_UNINIT`：这个状态表示进程刚刚被分配相应的进程控制块，但还没有初始化，需要进一步的初始化才能进入`PROC_RUNNABLE`的状态。
-   `PROC_SLEEPING`：这个状态表示进程正在等待某个事件的发生，通常由于等待锁的释放，或者主动交出CPU资源（`do_sleep`）。这个状态下的进程是不会被调度的。
-   `PROC_RUNNABLE`：这个状态表示进程已经准备好要执行了，只需要操作系统给他分配相应的CPU资源就可以运行。
-   `PROC_ZOMBIE`：这个状态表示进程已经退出，相应的资源被回收（大部分），`almost dead`。

一个进程的生命周期一般由如下过程组成：

**1.** 刚刚开始初始化，进程处在`PROC_UNINIT`的状态

**2.** 进程已经完成初始化，时刻准备执行，进入`PROC_RUNNABLE`状态

**3.** 在调度的时候，调度器选中该进程进行执行，进程处在`running`的状态

**4.(1)** 正在运行的进程由于`wait`等系统调用被阻塞，进入`PROC_SLEEPING`，等待相应的资源或者信号。

**4.(2)** 另一种可能是正在运行的进程被外部中断打断，此时进程变为`PROC_RUNNABLE`状态，等待下次被调用

**5.** 等待的事件发生，进程又变成`PROC_RUNNABLE`状态

**6.** 重复3~6，直到进程执行完毕，通过`exit`进入`PROC_ZOMBIE`状态，由父进程对他的资源进行回收，释放进程控制块。至此，这个进程的生命周期彻底结束

下面我们来看一看如何实现内核对于进程的调度。

### 深入理解进程切换

在前面我们已经简单了解过内核启动过程中的进程切换，这一节将再次回顾这些内容，并对其中的一些关键细节进行更清晰的整理。

首先需要明确的是：进程切换究竟在什么情况下会发生？大体可以分为两类情形——**主动调度**与**被动调度**。

**主动调度**指的是进程自愿放弃 CPU。典型情况包括：

1.  进程主动放弃当前的CPU资源，显式调用 `wait`、`sleep` 等接口表示自己需要等待；
2.  进程尝试获取某个不可用的资源（比如尝试获得未被释放的锁，或进行磁盘I/O操作的时候），因此必须阻塞；
3.  内核线程在执行过程中基于逻辑需要显式调用 `schedule`。

这类调度都是由执行流主动触发的。在 ucore 中，由于内核线程是不可抢占的，它们无法被其他内核线程强行打断，因此必须依靠这种主动让出 CPU 的方式来避免死锁，维持系统整体的可调度性。类似的主动调度出现在如下位置：

| 函数               | 原因                                                 |
| ------------------ | ---------------------------------------------------- |
| `proc.c/do_exit`   | 用户进程退出，主动放弃 CPU                           |
| `proc.c/do_wait`   | 用户进程等待，主动放弃 CPU                           |
| `proc.c/init_main` | `init`线程等待其他用户线程结束，并调度 `kswapd` 执行 |
| `proc.c/cpu_idle`  | `idle`线程等待就绪线程，必要时主动调用 `schedule`    |
| `sync.c/lock`      | 获取锁失败时主动进入等待                             |

与主动调度相对应，**被动调度**发生在进程未主动让出 CPU 时。ucore 不信任用户进程会自动停下来，因此需要提供一种机制在用户态执行期间能够及时打断它，实现抢占式调度。被动调度主要通过两类途径完成：

1.  **系统调用陷入内核态。**

    用户进程在执行系统调用时会通过 `ecall` 进入内核态。虽然这是由用户显式触发的，但从调度角度看此时进程并没有主动想放弃 CPU，因此属于被动调度的触发点。内核在系统调用返回前会检查调度标记，从而决定是否切换到其他就绪进程。

2.  **外部中断（比如时钟中断）将用户态打断。**

    用户进程在正常执行过程中可能被外部硬件中断打断，其中最关键的是时钟中断。时钟中断使内核能够周期性获得控制权，用于更新时间片并在必要时触发抢占，以保证用户进程不会无限运行下去。

无论是哪一种情况，用户进程都会通过 trap 机制进入内核态。进入 trap 时，硬件会首先保存用户态的中断帧，CPU 切换到内核态并执行相应的中断或系统调用的处理函数。在处理完这些 trap 逻辑之后，内核会统一检查当前进程是否应当调度，比如时间片是否用尽或是否已被标记为需要调度。如果满足条件，内核便保存当前进程 A 的内核态上下文，切换到进程 B 的内核态。之后 B 将从内核态返回到用户态继续执行。如果将来 B 再次陷入 trap 并触发调度，则控制权可能重新回到 A，使得 A 能从先前的内核态继续执行并最终返回用户态。

综上，ucore 的调度机制同时依赖主动调度与被动调度：前者确保内核线程在不可抢占的环境下仍然保持系统活性，后者确保用户进程能够被及时打断，避免独占 CPU。下一节我们将继续讨论 ucore 调度器的框架和几种基本的调度算法。

### 调度算法框架

调度算法框架实现为一个结构体，其中保存了各个函数指针。通过实现这些函数指针即可实现各个调度算法。结构体的定义如下：

```c
struct sched_class {
    // 调度类的名字
    const char *name;
    // 初始化run queue
    void (*init)(struct run_queue *rq);
    // 把进程放进run queue，这个是run queue的维护函数
    void (*enqueue)(struct run_queue *rq, struct proc_struct *proc);
    // 把进程取出run queue
    void (*dequeue)(struct run_queue *rq, struct proc_struct *proc);
    // 选择下一个要执行的进程
    struct proc_struct *(*pick_next)(struct run_queue *rq);
    // 每次时钟中断调用
    void (*proc_tick)(struct run_queue *rq, struct proc_struct *proc);
};
```

所有的进程被组织成一个`run_queue`数据结构。这个数据结构虽然没有保存在调度类中，但是是由调度类来管理的。目前ucore仅支持单个CPU核心，所以只有一个全局的`run_queue`。

我们在进程控制块中也记录了一些和调度有关的信息：

```c
struct proc_struct {
    // ...
    // 表示这个进程是否需要调度
    volatile bool need_resched;
    // run queue的指针
    struct run_queue *rq;
    // 与这个进程相关的run queue表项
    list_entry_t run_link;
    // 这个进程剩下的时间片
    int time_slice;
    // 以下几个都和Stride调度算法实现有关
    // 这个进程在优先队列中对应的项
    skew_heap_entry_t lab6_run_pool;
    // 该进程的Stride值
    uint32_t lab6_stride;
    // 该进程的优先级
    uint32_t lab6_priority;
};
```

前面的几个成员变量的含义都比较直接，最后面的几个的含义可以参见Stride调度算法。这也是本次lab的实验内容。

结构体`run_queue`实现了运行队列，其内部结构如下：

```c
struct run_queue {
    // 保存着链表头指针
    list_entry_t run_list;
    // 运行队列中的线程数
    unsigned int proc_num;
    // 最大的时间片大小
    int max_time_slice;
    // Stride调度算法中的优先队列
    skew_heap_entry_t *lab6_run_pool;
};
```

有了这些基础，我们就来实现一个最简单的调度算法：Round-Robin调度算法，也叫时间片轮转调度算法。

### RR调度算法实现

时间片轮转调度（Round-Robin Scheduling）为每个可运行进程分配一个时间片，并按照运行队列中的顺序依次选择进程执行。当一个进程用完时间片后，调度器将其标记为需要重新调度；如果该进程仍处于可运行状态，它会重新回到队尾等待下一轮执行。该算法让各进程获得较为均等的 CPU 使用机会，但不会根据进程优先级分配不同份额。

在当前进程运行期间，时钟中断会周期性调用调度类的`proc_tick`接口更新剩余时间片。时间片耗尽时，`need_resched`会被设置，内核在适当的返回路径中调用`schedule()`重新选择进程。`schedule()`通过当前调度类完成进程入队、下一进程选择和出队，因此 RR 算法只需按照`struct sched_class`规定的接口实现自身策略，不需要修改通用调度流程。

为了把 RR 算法接入统一的调度框架，需要分别处理运行队列和时间片在不同调度阶段的变化：

-   初始化接口负责建立空的运行队列，并将队列中的进程数量初始化为零。
-   入队接口负责将可运行进程加入队尾，在剩余时间片无效时按照运行队列的最大时间片进行调整，同时维护进程所属队列和队列进程数量。
-   出队接口负责从运行队列移除指定进程，并同步更新相关队列状态。
-   选择接口负责返回队首进程；运行队列为空时应返回空指针。
-   时钟接口负责消耗当前进程的剩余时间片，并在时间片耗尽时设置重新调度标记。

`default_sched_class`是 RR 调度器与通用调度框架之间的连接点。它的类型是`struct sched_class`，其中的函数指针分别对应上述调度操作。在`sched_init()`中，框架把`sched_class`指向`default_sched_class`，初始化运行队列，并通过统一接口调用 RR 调度器；后续的进程唤醒、主动让出 CPU 和时钟中断也会沿着这些接口更新调度状态。

把这些操作串联起来，就形成了 RR 调度的完整循环：进程变为可运行状态后进入队尾，调度器从队首选择进程执行，时钟中断逐步消耗其时间片，时间片耗尽后触发重新调度，仍可运行的进程再次回到队尾。运行队列链表、`proc_num`、进程的`rq`、`time_slice`和`need_resched`共同记录这一过程中的状态，任何一步更新不完整都可能破坏后续调度。

### stride调度算法

#### 基本思路

**【提示】可以先阅读任务中给出的论文，理解 Stride Scheduling 的基本思想后再看下面的内容。**

考察 round-robin 调度器，在假设所有进程都充分使用了其拥有的 CPU 时间资源的情况下，所有进程得到的 CPU 时间应该是相等的。但是有时候我们希望调度器能够更智能地为每个进程分配合理的 CPU 资源。假设我们为不同的进程分配不同的优先级，则我们有可能希望每个进程得到的时间资源与他们的优先级成正比关系。Stride调度是基于这种想法的一个较为典型和简单的算法。除了简单易于实现以外，它还有如下的特点：

-   可控性：如我们之前所希望的，可以证明 Stride Scheduling对进程的调度次数正比于其优先级。
-   确定性：在不考虑计时器事件的情况下，整个调度机制都是可预知和重现的。该算法的基本思想可以考虑如下：
    1.  为每个runnable的进程设置一个当前状态stride，表示该进程当前的调度权。另外定义其对应的pass值，表示对应进程在调度后，stride 需要进行的累加值。
    2.  每次需要调度时，从当前 runnable 态的进程中选择 stride最小的进程调度。
    3.  对于获得调度的进程P，将对应的stride加上其对应的步长pass（只与进程的优先权有关系）。
    4.  在一段固定的时间之后，回到 2.步骤，重新调度当前stride最小的进程。

可以证明，如果令 P.pass =BigStride / P.priority 其中 P.priority 表示进程的优先权（大于 1），而 BigStride 表示一个预先定义的大常数，则该调度方案为每个进程分配的时间将与其优先级成正比。证明过程我们在这里略去，有兴趣的同学可以在网上查找相关资料。将该调度器应用到ucore 的调度器框架中来，则需要将调度器接口实现如下：

-   init:
    -   初始化调度器类的信息（如果有的话）。
    -   初始化当前的运行队列为一个空的容器结构。（比如和RR调度算法一样，初始化为一个有序列表）
-   enqueue
    -   初始化刚进入运行队列的进程 proc的stride属性。
    -   将 proc插入放入运行队列中去（注意：这里并不要求放置在队列头部）。
-   dequeue
    -   从运行队列中删除相应的元素。
-   pick next
    -   扫描整个运行队列，返回其中stride值最小的对应进程。
    -   更新对应进程的stride值，即pass = BIG_STRIDE / P->priority; P->stride += pass。
-   proc tick:
    -   检测当前进程是否已用完分配的时间片。如果时间片用完，应该正确设置进程结构的相关标记来引起进程切换。
    -   一个 process 最多可以连续运行 rq.max_time_slice个时间片。

在具体实现时，有一个需要注意的地方：stride属性的溢出问题，在之前的实现里面我们并没有考虑 stride 的数值范围，而这个值在理论上是不断增加的，在stride溢出以后，基于stride的比较可能会出现错误。比如假设当前存在两个进程A和B，stride属性采用16位无符号整数进行存储。当前队列中元素如下（假设当前运行的进程已经被重新放置进运行队列中）：

![](.\instruction.assets\image001.png)

此时应该选择 A 作为调度的进程，而在一轮调度后，队列将如下：

![](.\instruction.assets\image002.png)

可以看到由于溢出的出现，进程间stride的理论比较和实际比较结果出现了偏差。我们首先在理论上分析这个问题：令PASS_MAX为当前所有进程里最大的步进值。则我们可以证明如下结论：对每次Stride调度器的调度步骤中，有其最大的步进值STRIDE_MAX和最小的步进值STRIDE_MIN之差：

STRIDE_MAX – STRIDE_MIN <= PASS_MAX

提问 1：如何证明该结论？

有了该结论，在加上之前对优先级有Priority > 1限制，我们有STRIDE_MAX – STRIDE_MIN <= BIG_STRIDE,于是我们只要将BigStride取在某个范围之内，即可保证对于任意两个 Stride 之差都会在机器整数表示的范围之内。而我们可以通过其与0的比较结构，来得到两个Stride的大小关系。在上例中，虽然在直接的数值表示上 98 < 65535，但是 98 - 65535 的结果用带符号的 16位整数表示的结果为99,与理论值之差相等。所以在这个意义下 98 > 65535。基于这种特殊考虑的比较方法，即便Stride有可能溢出，我们仍能够得到理论上的当前最小Stride，并做出正确的调度决定。

提问 2：在 ucore 中，目前Stride是采用无符号的32位整数表示。则BigStride应该取多少，才能保证比较的正确性？

#### 使用优先队列实现 Stride Scheduling

在上述的实现描述中，对于每一次pick_next函数，我们都需要完整地扫描来获得当前最小的stride及其进程。这在进程非常多的时候是非常耗时和低效的，有兴趣的同学可以在实现了基于列表扫描的Stride调度器之后比较一下priority程序在Round-Robin及Stride调度器下各自的运行时间。考虑到其调度选择于优先队列的抽象逻辑一致，我们考虑使用优化的优先队列数据结构实现该调度。

优先队列是这样一种数据结构：使用者可以快速的插入和删除队列中的元素，并且在预先指定的顺序下快速取得当前在队列中的最小（或者最大）值及其对应元素。可以看到，这样的数据结构非常符合 Stride 调度器的实现。

本次实验提供了libs/skew_heap.h作为优先队列的一个实现，该实现定义相关的结构和接口，其中主要包括：

```c
   // 优先队列节点的结构
   typedef struct skew_heap_entry  skew_heap_entry_t;
   // 初始化一个队列节点
   void skew_heap_init(skew_heap_entry_t *a);
   // 将节点 b 插入至以节点 a 为队列头的队列中去，返回插入后的队列
   skew_heap_entry_t  *skew_heap_insert(skew_heap_entry_t  *a,
                                        skew_heap_entry_t  *b,
                                        compare_f comp);
   // 将节点 b 插入从以节点 a 为队列头的队列中去，返回删除后的队列
      skew_heap_entry_t  *skew_heap_remove(skew_heap_entry_t  *a,
                                           skew_heap_entry_t  *b,
                                           compare_f comp);
```

其中优先队列的顺序由比较函数`comp`决定。对于 Stride 调度器，比较函数需要根据进程的 stride 判断先后顺序，并结合前文介绍的方法正确处理无符号整数溢出。当使用优先队列实现 Stride 调度器后，运行队列结构也需要作相应改变，其中包括：

-   struct run_queue中的lab6_run_pool指针，在使用优先队列的实现中表示当前优先队列的头元素，如果优先队列为空，则其指向空指针（NULL）。
-   struct proc_struct中的lab6_run_pool结构，表示当前进程对应的优先队列节点。ucore 已经在进程和运行队列结构中预留了 Stride 调度需要的数据，使用优先队列组织这些数据时，可以将其工作过程概括如下：
-   init(rq):
    -   Initialize rq->run_list
    -   Set rq->lab6_run_pool to NULL
    -   Set rq->proc_num to 0
-   enqueue(rq, proc)
    -   Initialize proc->time_slice
    -   Insert proc->lab6_run_pool into rq->lab6_run_pool
    -   rq->proc_num ++
-   dequeue(rq, proc)
    -   Remove proc->lab6_run_pool from rq->lab6_run_pool
    -   rq->proc_num --
-   pick_next(rq)
    -   If rq->lab6_run_pool == NULL, return NULL
    -   Find the proc corresponding to the pointer rq->lab6_run_pool
    -   proc->lab6_stride += BIG_STRIDE / proc->lab6_priority
    -   Return proc
-   proc_tick(rq, proc):
    -   If proc->time_slice > 0, proc->time_slice --
    -   If proc->time_slice == 0, set the flag proc->need_resched

上述过程与 RR 调度器使用相同的调度类接口，区别主要在于运行队列的组织方式和下一进程的选择规则。优先队列使调度器能够直接取得 stride 最小的进程，从而避免每次调度都扫描整个可运行进程集合。

## stride调度算法执行`make qemu`的大致输出

```
$ make qemu
......
sched class: stride_scheduler
++ setup timer interrupts
kernel_execve: pid = 2, name = "priority".
set priority to 6
main: fork ok,now need to wait pids.
set priority to 5
set priority to 4
set priority to 3
set priority to 2
set priority to 1
child pid 7, acc 944000, time 2010 
child pid 6, acc 788000, time 2010 
child pid 5, acc 620000, time 2010 
child pid 4, acc 460000, time 2020 
child pid 3, acc 316000, time 2020 
main: pid 3, acc 316000, time 2020 
main: pid 4, acc 460000, time 2020 
main: pid 5, acc 620000, time 2030 
main: pid 6, acc 788000, time 2030 
main: pid 7, acc 944000, time 2030 
main: wait pids over
sched result: 1 1 2 2 3 
all user-mode processes have quit. 
init check memory pass.
kernel panic at kern/process/proc.c:<line>:
    initproc exit.
```

其中进程累计执行次数和时间会随运行环境有所波动，但各进程获得的 CPU 时间应大致符合其优先级比例。

## 实验报告要求

1.  基于`markdown`格式完成实验报告，以文本说明为主。
2.  说明自己对本章节整体逻辑的理解，即本章节是按照什么顺序、围绕哪些功能逐步实现的。
3.  说明自己对每个功能的核心函数或功能模块的理解。
4.  说明每个练习的`spec`是怎么写的，`prompt`是怎么写的。
5.  说明在使用提示词让 AI 生成代码的过程中出现了什么 bug，以及你是怎么解决的。
6.  列出你认为本实验中重要的知识点，以及与对应的 OS 原理中的知识点，并简要说明二者的含义、关系和差异。
7.  列出你认为 OS 原理中很重要、但在本实验中没有对应上的知识点。

从 oslab 网站取得实验代码后，进入目录`labcodes/lab6`，完成实验要求的各项任务，并在实验报告中回答任务提出的问题。实验报告存放在`labcodes/lab6`目录下，推荐使用`markdown`格式。每个小组建立一个 Gitee 或 GitHub 仓库，完成 Lab 6 的编程任务后，通过 Git 将代码和实验报告提交到仓库，并按要求及时提交。

注意有“lab6”的注释，代码中所有需要完成的地方（challenge 和要求自行创建文件的任务除外）都有“lab6”和“YOUR CODE”的注释，请在提交时特别注意保持注释，并将“YOUR CODE”替换为自己的学号，并且将所有标有对应注释的部分填上正确的代码。