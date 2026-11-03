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

#### Trap 上下文恢复与 Trap 入口功能提示词

````markdown
[PROMPT]
**任务**：修改 `kern/trap/trapentry.S`，实现 Trap 上下文恢复与 Trap 入口功能：补全宏 `RESTORE_ALL`，并补全 `__alltraps` 中调用 C 函数 `trap()` 的部分。

**操作要求**：你必须在项目中修改实际文件，保留现有代码，只在标记 `lab3 function1` 的位置补全。不得修改 `SAVE_ALL`、`__trapret`、`struct trapframe` 以及 `trap()` 的函数签名。把标记注释中的 `YOUR CODE` 替换为学号 `2414007`，保留 `lab3 function1` 字样。对不显而易见的地方（例如恢复顺序的原因）添加简短的英文注释，不要逐行解释显而易见的指令。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息，以工程中的真实代码为准，不要引入无关功能，不要新增文件。

[RELY]

1. `struct trapframe` 在内存中是 36 个连续的机器字（每个字 `REGBYTES` 字节），按字序号排列：

```c
struct trapframe {
    struct pushregs gpr;  // words 0..31: x0, x1, ..., x31 in register-number order
    uintptr_t status;     // word 32: sstatus
    uintptr_t epc;        // word 33: sepc
    uintptr_t badvaddr;   // word 34: stval
    uintptr_t cause;      // word 35: scause
};
```

2. `libs/riscv.h` 中与寄存器宽度相关的汇编助记符和常量：`REGBYTES`（一个通用寄存器的字节数）、`LOAD`（按寄存器宽度加载，RV64 下为 `ld`）、`STORE`（按寄存器宽度存储）。汇编中的寄存器写法沿用 `x0`..`x31` 和 ABI 名（如 `a0`、`s1`）。

3. 已提供的 `SAVE_ALL` 的行为（只读参考，不得修改）：先把陷入前的 `sp` 暂存到 `sscratch`，再把 `sp` 减去 `36 * REGBYTES`，此时 `sp` 指向 trapframe 的起始地址；然后把 `x0`、`x1`、`x3`..`x31` 依次存入各自编号对应的字槽；字槽 2（`x2`）存放的是**陷入前**的 `sp`，而不是 trapframe 的地址；最后读取 `sstatus`、`sepc`、`stval`、`scause`，存入字槽 32..35。该过程用 `s0`..`s4` 作为临时寄存器，这些寄存器的原值在此之前已经保存。

4. 已提供的 `__alltraps` 和 `__trapret` 骨架：`__alltraps` 以 `SAVE_ALL` 开始，之后是待补全的标记位置，紧接着就是 `__trapret` 标签；`__trapret` 先执行 `RESTORE_ALL`，再执行 `sret`。`stvec` 已经指向 `__alltraps`。

5. C 语言处理入口，已在 `kern/trap/trap.c` 中定义，由汇编直接调用：

```c
void trap(struct trapframe *tf);
```

[GUARANTEE]
需要交付以下两项，均位于 `kern/trap/trapentry.S`：

1. 宏 `RESTORE_ALL`：与 `SAVE_ALL` 对应的上下文恢复宏，在 `__trapret` 中被使用。
2. `__alltraps` 中 `SAVE_ALL` 之后的部分：把 trapframe 交给 `trap()` 处理，`trap()` 返回后继续执行 `__trapret`。

不允许新增全局符号、标签或 C 函数；如需临时寄存器，只能使用 `RESTORE_ALL` 本身就要恢复的寄存器，并保证其最终值正确。

[SPECIFICATION]
**RESTORE_ALL**

Pre-Condition：`sp` 指向一个由 `SAVE_ALL` 建立的完整 trapframe；`trap()` 可能已经修改过 trapframe 中的任意字段，特别是 `epc`；`s0`..`s4` 在此刻可以被自由改写。

Post-Condition：

- `sstatus` 和 `sepc` 的值等于 trapframe 中对应字段的当前值，从而保证后续 `sret` 回到的是（可能被 `trap()` 调整过的）执行位置，并且特权级、中断使能位与陷入前一致。
- 除 `x0` 和 `x2` 以外的所有通用寄存器，其值等于 trapframe 中对应字槽的值；`x0` 保持为 0。
- `sp` 的值等于字槽 2 中保存的陷入前的 `sp`，trapframe 所占的栈空间随之释放，栈顶回到陷入前的位置。
- 不向 `stval`、`scause` 写回任何值。

Requirements：

- `x2` 与其他寄存器的处理方式不同：它的字槽里是陷入前的 `sp`，而不是普通寄存器的备份。由于所有恢复操作都以当前 `sp`（trapframe 地址）为基址寻址，`x2` 必须在其他所有寄存器和 CSR 都恢复完毕之后、作为最后一步恢复。
- `sstatus` 和 `sepc` 必须借助通用寄存器才能写回，因此这一步必须在这些借用的寄存器自己被恢复**之前**完成，否则借用的寄存器会丢失恢复值或者破坏已恢复的值。
- 恢复结束后，除 `x0` 外的 31 个通用寄存器必须与 trapframe 中的值逐位一致；不得遗漏任何一个，也不得对 `x0` 执行写入。

**\_\_alltraps（SAVE_ALL 之后的部分）**

Pre-Condition：`SAVE_ALL` 已经执行完毕，`sp` 指向完整的 trapframe；此刻处于内核栈上，不允许使用 `sp` 以外的寄存器假定任何值。

Post-Condition：`trap()` 被调用，其唯一参数 `tf` 的值等于 trapframe 的起始地址，传参方式符合 RISC-V 调用约定。`trap()` 返回后 `sp` 与调用前相同，控制流自然继续进入 `__trapret`，由 `RESTORE_ALL` 和 `sret` 完成返回。

Requirements：

- 调用 `trap()` 之前不得再调整 `sp`，否则 trapframe 的位置会与 `RESTORE_ALL` 的假设不一致。
- 不得在 `trap()` 返回后用 `ret` 或其他跳转绕过 `__trapret`。
````

### 2.2 2414015-鲁昕宁

#### 中断处理功能提示词

````markdown
[PROMPT]
**任务**：修改 `kern/trap/trap.c`，实现中断处理功能：补全 `interrupt_handler()`。`trap_dispatch()` 已经按 `scause` 最高位把 Trap 分成中断和异常，本任务只负责中断这一支。时钟中断要能连续发生、按固定次数输出统计并在达到规定次数后关机；其余已列出的中断只做类型识别和一行提示；无法识别的中断输出规定的原因码。

**操作要求**：你必须在项目中修改实际文件，保留现有代码，只补全 `interrupt_handler()` 的函数体。不得修改 `exception_handler()`、`trap()`、`trap_dispatch()`、`idt_init()`、`print_trapframe()`、`print_ticks()` 以及 `TICK_NUM`、`SHUTDOWN_AFTER` 的定义。不得改函数签名，不得新增文件。把标记注释 `//lab3 function2 YOUR CODE` 中的 `YOUR CODE` 替换为学号 `2414015`，保留 `lab3 function2` 字样。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息，以工程中的真实代码为准，不要引入无关功能。

[RELY]

1. 待补全的函数已经在 `kern/trap/trap.c` 中声明并给出空函数体，由 `trap_dispatch()` 在判定为中断后调用：

```c
void interrupt_handler(struct trapframe *tf);
```

`trap_dispatch()` 的判定（只读参考，不得修改）：把 `tf->cause` 当作有符号数，小于 0 时调用 `interrupt_handler()`，否则调用 `exception_handler()`。因此进入本函数时，调用方认为这是一次中断。评分测试也会直接调用本函数，此时 `tf->cause` 的最高位可能已置位，也可能没有置位。与 `IRQ_*` 比较之前，必须先去掉最高位，只保留异常码。

2. `struct trapframe` 中本函数会读到的字段：

```c
uintptr_t epc;    /* sepc：被打断的那条指令的地址。中断返回后应重新执行它 */
uintptr_t cause;  /* scause：最高位为 1 表示中断，低位是异常码 */
```

3. `libs/riscv.h` 中的中断异常码（均不含最高位）：

```c
#define IRQ_U_SOFT   0
#define IRQ_S_SOFT   1
#define IRQ_H_SOFT   2
#define IRQ_M_SOFT   3
#define IRQ_U_TIMER  4
#define IRQ_S_TIMER  5
#define IRQ_H_TIMER  6
#define IRQ_M_TIMER  7
#define IRQ_U_EXT    8
#define IRQ_S_EXT    9
#define IRQ_H_EXT    10
#define IRQ_M_EXT    11
#define IRQ_COP      12
#define IRQ_HOST     13
```

4. 同一文件中已经定义、本函数必须使用而不得改写的宏和函数：

```c
#define TICK_NUM 100        /* 每累计这么多次时钟中断，输出一行统计 */
#define SHUTDOWN_AFTER 10   /* 统计行数达到这个值后关机 */
static void print_ticks(void); /* 输出 "%d ticks\n"，实参为 TICK_NUM */
```

5. 时钟与关机接口：

```c
extern volatile size_t ticks;          /* kern/driver/clock.h，clock_init() 已把它置 0 */
void clock_set_next_event(void);       /* 预约下一次时钟中断；不调用则只会收到一次 */
void sbi_shutdown(void);               /* libs/sbi.h，请求固件关机，成功时 QEMU 退出 */
```

[GUARANTEE]
需要交付的只有 `kern/trap/trap.c` 中的：

```c
void interrupt_handler(struct trapframe *tf);
```

函数签名保持不变。允许在本文件内新增 `static` 辅助函数，但不得增加公开接口，也不得把异常处理逻辑写进本函数。`exception_handler()` 仍保持空函数体。

[SPECIFICATION]
**interrupt_handler**

Pre-Condition：`tf` 指向一次中断的 trapframe。`tf->cause` 去掉最高位后是本次中断的异常码。`ticks` 记录的是自 `clock_init()` 以来已经处理过的监督态时钟中断次数。`tf->epc` 是被打断指令的地址，返回后要重新执行它。

Post-Condition：函数正常返回，不修改 `tf` 的任何字段，从而后续 `sret` 回到被打断的指令。控制流不在本函数内自旋，也不调用 `panic`。

Case 1：异常码等于 `IRQ_S_TIMER`。

先预约下一次时钟事件，再把 `ticks` 增加 1。增加之后，若 `ticks` 是 `TICK_NUM` 的整数倍，调用 `print_ticks()` 输出一行统计。若至此已经输出的统计行数达到 `SHUTDOWN_AFTER`（即 `ticks` 达到 `TICK_NUM * SHUTDOWN_AFTER`），在输出这行统计之后调用 `sbi_shutdown()`。这一支不另外打印中断名称。只计数而不预约下一次事件时，时钟中断只会到来一次。

Case 2：异常码等于下列值之一。各输出一行后返回，不修改 `ticks`，不预约时钟，不关机。

- `IRQ_U_SOFT`：`User software interrupt`
- `IRQ_S_SOFT`：`Supervisor software interrupt`
- `IRQ_H_SOFT`：`Hypervisor software interrupt`
- `IRQ_M_SOFT`：`Machine software interrupt`
- `IRQ_U_TIMER`：`User timer interrupt`
- `IRQ_H_TIMER`：`Hypervisor timer interrupt`
- `IRQ_M_TIMER`：`Machine timer interrupt`
- `IRQ_U_EXT`：`User external interrupt`
- `IRQ_S_EXT`：`Supervisor external interrupt`
- `IRQ_H_EXT`：`Hypervisor external interrupt`
- `IRQ_M_EXT`：`Machine external interrupt`
- `IRQ_COP`：`Coprocessor interrupt`
- `IRQ_HOST`：`Host interrupt`

Case 3：异常码不属于以上任何一种。输出一行 `Unknown interrupt: cause %d`，其中 `%d` 是去掉最高位后的异常码（异常码 15 必须得到 `Unknown interrupt: cause 15`），然后返回。不调用 `print_trapframe()`。

Requirements：

- 识别异常码时必须忽略 `cause` 的最高位。最高位已置位的监督态软件中断，其异常码仍是 `IRQ_S_SOFT`，输出 `Supervisor software interrupt`。
- 时钟中断不得改写 `tf->epc`。中断返回后应重新执行被打断的指令；把 `epc` 向前拨会使被打断的代码跳过一条指令。
- 统计行必须通过已有的 `print_ticks()` 输出，不要自行再写一条 `"%d ticks"`，否则 `DEBUG_GRADE` 下的结束行为会丢失。
````

### 2.3 2411622-张雨函

#### 异常处理功能提示词

````markdown
[PROMPT]
**任务**：修改 kern/trap/trap.c，实现 Lab3 练习 2-b 的异常处理功能，补全 exception_handler()。需要识别非法指令和断点异常，输出规定的提示，并根据当前 RISC-V 指令是 16 位压缩指令还是 32 位普通指令推进 epc；其他异常沿用已有的 print_trapframe()。

**操作要求**：你必须在项目中修改实际文件，保留现有代码，只补全 exception_handler() 的函数体。不得修改 interrupt_handler()、trap()、trap_dispatch()、print_trapframe()、idt_init()、print_ticks() 或 Trap 汇编入口。不得改变函数签名，不得新增文件。保留标记注释 //lab3 function2 YOUR CODE 中的 lab3 function2，并把 YOUR CODE 替换为学号 2411622。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息，以工程中的真实代码为准，不要引入与异常处理无关的功能。

[RELY]

1. 待补全的函数位于 kern/trap/trap.c，函数声明和接口如下：

    void exception_handler(struct trapframe *tf);

trap_dispatch() 已经根据 tf->cause 的最高位完成分流：最高位为 1 时调用 interrupt_handler()，最高位为 0 时调用 exception_handler()。本任务只实现异常分支。

2. struct trapframe 中本函数需要使用的字段如下：

    struct trapframe {
        struct pushregs gpr;
        uintptr_t status;
        uintptr_t epc;        /* sepc：发生异常的指令地址 */
        uintptr_t badvaddr;
        uintptr_t cause;      /* scause：异常原因 */
    };

tf->epc 指向发生异常的指令，处理函数可以修改它；tf->cause 用于区分异常类型。

3. libs/riscv.h 已经定义异常原因宏：

    #define CAUSE_ILLEGAL_INSTRUCTION 2
    #define CAUSE_BREAKPOINT          3

4. print_trapframe(struct trapframe *tf) 已经实现。对于没有专门处理逻辑的异常，应调用该函数输出寄存器和异常现场，不要新增另一套现场打印格式。

5. RISC-V 指令的长度可以由指令编码低两位判断：低两位不等于 0x3 时为 16 位压缩指令，低两位等于 0x3 时为 32 位指令。读取 tf->epc 处的 16 位值即可完成长度判断。

6. cprintf() 已经可用，要求的两类提示格式为：

    Illegal instruction at 0x%08x
    Breakpoint at 0x%08x

[GUARANTEE]

需要交付的只有 kern/trap/trap.c 中的：

    void exception_handler(struct trapframe *tf);

函数签名保持不变。允许使用函数内部的局部变量，但不得增加公开接口，不得把中断处理逻辑写入本函数，也不得改变 Trap 的分发和返回框架。

[SPECIFICATION]

**exception_handler**

Pre-Condition：tf 指向一次由 SAVE_ALL 建立的异常 trapframe；tf->cause 的最高位为 0；tf->epc 指向触发异常的当前指令。函数返回后，汇编层会通过 RESTORE_ALL 恢复 trapframe，并执行 sret。

Post-Condition：

- 对非法指令和断点，输出对应提示，并将 tf->epc 更新为异常指令之后的下一条指令地址。
- 对其他异常，输出完整 trapframe 信息，不擅自修改异常原因或引入未要求的处理。
- 函数正常返回，不循环、不关机、不调用 panic。
- RESTORE_ALL 能够把更新后的 tf->epc 写回 sepc，使 sret 按新的地址继续执行。

Case 1：tf->cause == CAUSE_ILLEGAL_INSTRUCTION。

读取 tf->epc 处的 16 位指令前缀。若低两位为 0x3，按 32 位指令处理，将 tf->epc 增加 4；否则按 16 位压缩指令处理，将 tf->epc 增加 2。输出 Illegal instruction at 0x%08x，其中地址为发生异常时的原始 tf->epc。

Case 2：tf->cause == CAUSE_BREAKPOINT。

按照与 Case 1 相同的低两位规则判断指令长度。输出 Breakpoint at 0x%08x，其中地址为断点指令的原始地址；然后将 tf->epc 增加 2 或 4。必须推进 epc，否则返回后会重复执行同一条断点指令并再次陷入。

Case 3：其他异常原因。

调用已有的 print_trapframe(tf) 输出现场，然后返回。不得把其他异常误判为非法指令或断点，也不得修改 tf->epc。

Requirements：

- 只能修改 exception_handler()，保留其他 Trap 处理逻辑。
- 必须同时支持 16 位压缩指令和 32 位普通指令，不能固定使用同一个长度。
- 非法指令和断点的输出文字必须与评分脚本要求一致。
- 对中断原因不在本函数中处理；中断由 interrupt_handler() 负责。
````
