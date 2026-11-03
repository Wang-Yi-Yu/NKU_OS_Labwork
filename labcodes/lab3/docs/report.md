# 操作系统实验报告

## 实验基本信息

|     项目     |                      内容                      |
| :----------: | :--------------------------------------------: |
| **实验名称** |               Lab3：断,都可以断                |
| **小组成员** | 2414007-王奕瑜、2414015-鲁昕宁、2411622-张雨函 |
| **完成日期** |                   2026-10-20                   |

### 小组分工

练习如何分工？

|      成员      |             负责的练习/模块             |
| :------------: | :-------------------------------------: |
| 2414007-王奕瑜 | 练习 1：Trap 上下文恢复与 Trap 入口功能 |
| 2414015-鲁昕宁 |         练习 2(a)：中断处理功能         |
| 2411622-张雨函 |         练习 2(b)：异常处理功能         |

实验报告如何分工？

|      成员      | 负责的实验报告内容 |
| :------------: | :----------------: |
| 2414007-王奕瑜 | 一, 3.2.1, 4.1, 五 |
| 2414015-鲁昕宁 |  3.1, 3.2.2, 4.2   |
| 2411622-张雨函 |   3.2.3, 4.3, 六   |

---

## 一、实验目的

本实验的主要目的是：

1. 理解 RISC-V 中 Trap 的概念与分类：中断（异步、由外部事件触发）与异常（同步、由当前指令触发），并理解 M/S/U 三个特权级、`mideleg`/`medeleg` 委托机制下 Trap 最终由哪一级处理。
2. 掌握 S 模式 Trap 相关的 CSR（`stvec`、`sepc`、`scause`、`stval`、`sstatus`、`sie`）的作用，以及 `ecall`、`ebreak`、`sret` 等特权指令的行为。
3. 掌握 Trap 前后执行上下文的保存与恢复：理解 `struct trapframe` 的布局、`SAVE_ALL` 与 `RESTORE_ALL` 的对应关系，以及 `sp` 在保存与恢复时的特殊处理。
4. 掌握 Trap 入口 `__alltraps` 与 C 语言 `trap()` 之间的衔接：按照 RISC-V 调用约定传递 `trapframe` 指针，并通过 `__trapret` 与 `sret` 返回被打断的执行流。
5. 掌握中断与异常的分发与处理：实现时钟中断（设置下一次时钟事件、`ticks` 计数、周期性输出、达到次数后关机）以及断点、非法指令异常的处理（按指令长度更新 `epc`），并对未识别的 Trap 给出调试输出。
6. 理解时钟中断的硬件支持（`rdtime`、`sbi_set_timer`）与中断的使能和屏蔽（`sstatus.SIE`、`sie`），以及内核在临界区中关闭、恢复中断的做法。

---

## 二、实验环境

你们使用的 AI 工具

|      成员      | AI 编程工具 |     底层模型      | 备注 |
| :------------: | :---------: | :---------------: | :--: |
| 2414007-王奕瑜 | Claude Code | Claude Sonnet 5.5 |  无  |
| 2414015-鲁昕宁 |      Cursor       |      Grok 4.7     |   无   |
| 2411622-张雨函 |     GPT     |        GPT-6       |  无  |

**说明：**

- **AI 编程工具**：指具体使用的终端工具、编辑器插件、桌面应用或浏览器界面
- **底层模型**：指该工具使用的大语言模型及版本

---

## 三、实验整体逻辑分析

### 3.1 本章节的逻辑主线

本章的主线是“内核怎样接住一次 Trap，处理完再回到原来的指令”。实验二结束时，内核已经能管理物理页，并用 Sv39 访问这些页，但时钟和异常指令还到不了 C 函数。实验三先做成一条所有 Trap 共用的进入和返回通路，再在这条通路上分别处理中断和异常。

这条链按依赖关系排开：

1. **保存现场并返回。** 硬件在 Trap 时写下 `sepc`、`scause`、`stval`，然后跳到 `stvec`。`SAVE_ALL` 在内核栈上建立与 `struct trapframe` 一致的 36 个字；`__alltraps` 把这块内存的地址按调用约定传给 `trap()`；`RESTORE_ALL` 在 `trap()` 返回后装回通用寄存器以及 `sstatus`、`sepc`，最后 `sret`。没有这一段，后面的处理函数一返回，被打断的代码就无法继续。
2. **按 `scause` 分成两支。** `trap_dispatch()` 把 `cause` 当成有符号数。最高位为 1 时是中断，调用 `interrupt_handler()`；最高位为 0 时是异常，调用 `exception_handler()`。分发由框架给出，练习 2 补的是两个处理函数。
3. **中断：让时钟一直响，并认出其他中断。** `sbi_set_timer()` 一次只预约一个时刻。`clock_init()` 预约第一次；每次监督态时钟中断里再预约下一次，`ticks` 加 1，每 100 次打印一行 `100 ticks`，满 10 行后关机。其余已列出的中断各打印一行来源，认不出的打印原因码。中断处理不修改 `tf->epc`，`sret` 之后重新执行被打断的那条指令。
4. **异常：跳过触发异常的指令。** 非法指令和断点由当前指令引起，`sepc` 停在这条指令上。处理函数按指令长度前进 `tf->epc`，`RESTORE_ALL` 把新值写回 `sepc` 之后，`sret` 才会执行下一条。未单独处理的异常用 `print_trapframe()` 输出现场。这一支与中断处理分开实现。

因此，后面各节的顺序是：先完成 trapframe 的恢复和 `trap()` 入口，再实现时钟和其他中断的处理，最后实现异常处理。

### 3.2 功能的逐步实现

#### 3.2.1 Trap 上下文恢复与 Trap 入口功能

**为什么需要先实现这一部分：** Trap 发生时，硬件只会记录 `sepc`、`scause` 等少量信息并跳转到 `stvec`，而真正的处理逻辑是用 C 语言编写的，C 代码会随意使用和改写通用寄存器。如果不先把被打断程序的寄存器完整保存下来，处理结束后就无法让它从原来的位置、以原来的状态继续运行。因此，“保存上下文 → 进入 C 函数 → 恢复上下文 → `sret` 返回”是所有中断和异常共用的最底层通路，与具体的 Trap 类型无关，必须最先打通。骨架中已经提供了保存一侧的 `SAVE_ALL`，练习 1 需要补全与之对应的恢复一侧 `RESTORE_ALL`，以及把 `trapframe` 交给 `trap()` 的 `__alltraps`。

**对后续实验的作用：** 练习 2 中的时钟中断、断点异常、非法指令异常都是经由 `__alltraps` 进入 `trap()` 的；如果没有正确的 `RESTORE_ALL`，处理函数一旦返回系统就会崩溃，练习 2 的任何现象都无法验证。同时，异常处理中对 `tf->epc` 的修改，也只有在 `RESTORE_ALL` 把 `sepc` 写回之后才会真正生效。再往后，系统调用、用户态与内核态切换、进程切换等功能同样依赖这一套 `trapframe` 保存与恢复机制。

#### 3.2.2 中断处理功能

中断处理放在 Trap 入口之后。入口只保证能进入 `trap()`，再从原来的指令继续执行；这次进来该做什么，由 `scause` 的低位决定。

1. **时钟中断要自己接上下一次。** `sie` 里打开监督态时钟中断之后，硬件并不会按固定周期反复进入处理函数。`sbi_set_timer()` 传入的是 `time` CSR 上的一个绝对时刻，到点只产生一次。`clock.c` 里的 `timebase` 是 100000；QEMU 上 `time` 的频率是 10 MHz，所以间隔是 10 ms。`clock_init()` 预约第一次，并把 `ticks` 置 0。`interrupt_handler()` 在 `IRQ_S_TIMER` 里先调用 `clock_set_next_event()`，再把 `ticks` 加 1。若只加计数、不再预约，`kern_init()` 末尾的空循环里只会进来一次。
2. **计数和输出用框架里已有的间隔。** `TICK_NUM` 是 100，大约每秒调用一次 `print_ticks()`，输出一行 `100 ticks`。`SHUTDOWN_AFTER` 是 10，第 10 行输出之后调用 `sbi_shutdown()`。统计行走已有的 `print_ticks()`：`DEBUG_GRADE` 下它还会打印 `End of Test.` 并 `panic`，评分依赖这个行为。
3. **返回位置保持为被打断的指令。** 对中断来说，`sepc` 记录的是被打断的那条指令。本函数不写 `tf->epc`。`RESTORE_ALL` 把这个地址装回 `sepc`，`sret` 之后那条指令再执行一遍。把 `epc` 往前拨，会使被打断的代码少执行一条指令。
4. **原因码要去掉最高位，异常处理留在另一个函数里。** `IRQ_S_TIMER` 等宏只含低位的原因码，最高位只表示“这是中断”。`trap_dispatch()` 已经用最高位选过分支；评分也会不经过它、直接调用 `interrupt_handler()`，那时最高位可能置上，也可能没有。比较前先把最高位清掉。`exception_handler()` 由另一部分实现，中断处理不改它，认不出的中断也不调用 `print_trapframe()`，只打印 `Unknown interrupt: cause` 和原因码。

#### 3.2.3 异常处理功能

异常处理是练习 2-b 的实现内容。它和 2-a 的中断处理共用 trap()、trap_dispatch() 以及汇编入口和返回路径，但两者由 scause 的最高位分开选择：最高位为 1 时进入 interrupt_handler()，最高位为 0 时进入 exception_handler()。因此 2-b 不需要修改 2-a 的时钟逻辑，不过依赖 2.1 中已经正确实现的 SAVE_ALL、RESTORE_ALL 和 sret，否则对 tf->epc 的更新无法写回处理器。

本函数针对两类会在当前指令处停下的异常：

1. **非法指令异常。** 输出 Illegal instruction at ...，再根据 epc 指向的指令编码判断长度。RISC-V 指令低两位不为 11 时是 16 位压缩指令，否则是 32 位普通指令，分别让 tf->epc 增加 2 或 4。
2. **断点异常。** 输出 Breakpoint at ...，使用同样的指令长度判断，并把 tf->epc 移到断点指令之后。这样恢复现场并执行 sret 后，程序不会再次执行同一条 ebreak。
3. **其他异常。** 不猜测异常类型，直接调用已有的 print_trapframe() 输出完整现场，便于后续定位问题。

实现中只读取 epc 处的低 16 位来判断长度，不会把正在执行的指令整条解释成操作码；异常原因仍由 tf->cause 区分。这个处理满足 16 位和 32 位指令两种边界情况，也保持了未覆盖异常的调试信息。

---

## 四、实验内容与实现

### 4.1 Trap 上下文恢复与 Trap 入口功能

**负责人：** [2414007-王奕瑜]

#### 4.1.1 模块功能描述

**需要实现/修改的汇编功能**（均位于 `kern/trap/trapentry.S`）：

```armasm
.macro RESTORE_ALL    # 与 SAVE_ALL 对应的上下文恢复宏
__alltraps:           # SAVE_ALL 之后，把 trapframe 交给 trap()
```

**功能说明：**

本模块负责 Trap 从“进入内核”到“返回被打断程序”之间的底层上下文管理，一次 Trap 的完整路径如下：

```text
硬件：写 sepc/scause/stval，更新 sstatus，跳转到 stvec（= __alltraps）
__alltraps：SAVE_ALL            在内核栈上建立 trapframe
            move a0, sp         trapframe 地址作为 trap() 的第一个参数
            jal  trap           进入 C 语言的中断/异常处理
__trapret ：RESTORE_ALL         从 trapframe 恢复寄存器与 CSR
            sret                pc = sepc，特权级 = sstatus.SPP，SIE = SPIE
```

`SAVE_ALL` 已由框架提供，它在栈上开辟 36 个机器字的空间，布局与 `struct trapframe` 完全一致：字 0 ~ 31 依次是 `x0` ~ `x31`，字 32 ~ 35 依次是 `sstatus`、`sepc`、`stval`、`scause`。练习 1 要实现的是这条路径上剩下的两块：

1. **`RESTORE_ALL`：** 它是 `SAVE_ALL` 的逆过程，输入是栈顶的 trapframe，输出是恢复后的处理器状态。具体做法是：
   - 先恢复 CSR。`sstatus` 和 `sepc` 不能直接从内存写入 CSR，需要先 `LOAD` 到 `s1`、`s2`，再用 `csrw` 写回。必须最先做这一步，因为 `s1`、`s2` 本身也是需要被恢复的寄存器，稍后会被覆盖。`sepc` 要从 trapframe 读回而不是沿用硬件当前值，是因为异常处理函数会通过修改 `tf->epc` 来跳过触发异常的指令，只有写回 `sepc` 这个修改才会在 `sret` 之后生效。`sstatus` 写回后，`SPP`、`SPIE` 等位保持陷入时的状态，`sret` 才能回到正确的特权级并恢复中断使能。`stval` 和 `scause` 只是提供给处理程序阅读的信息，不需要写回。
   - 再依次 `LOAD` 通用寄存器 `x1`、`x3`~`x31`。`x0` 恒为 0，不需要也不能恢复。
   - 最后才 `LOAD x2`。`SAVE_ALL` 在字槽 2 中保存的是**陷入前**的 `sp`，而不是 trapframe 的地址；而前面所有的 `LOAD` 都是以当前 `sp`（即 trapframe 地址）为基址寻址的。如果提前恢复 `x2`，`sp` 就变成了陷入前的栈顶，之后的访存地址全部错误。把 `x2` 放在最后一步，既保证了前面的寻址正确，也正好完成了“释放 trapframe 所占的 36 个字，让栈顶回到陷入前位置”这件事。
2. **`__alltraps` 中调用 `trap()`：** `SAVE_ALL` 结束时，`sp` 恰好指向 trapframe 的起始地址。根据 RISC-V 调用约定，函数的第一个参数通过 `a0` 传递，所以只需要 `move a0, sp`，再 `jal trap`，C 函数就拿到了 `struct trapframe *tf`。这里不需要再调整 `sp`，因为 `trap()` 作为普通 C 函数会在自己的栈帧内保存和恢复被调用者保存寄存器，返回后 `sp` 与调用前相同。`jal` 返回后不需要额外跳转，`__trapret` 标签就紧跟在后面，控制流会自然落入 `RESTORE_ALL` 和 `sret`。

最终的实现如下（中间的寄存器加载按编号顺序重复，此处省略）：

```armasm
.macro RESTORE_ALL
    #lab3 function1 2414007
    LOAD s1, 32*REGBYTES(sp)
    LOAD s2, 33*REGBYTES(sp)
    csrw sstatus, s1
    csrw sepc, s2

    LOAD x1, 1*REGBYTES(sp)
    LOAD x3, 3*REGBYTES(sp)
    # ... x4 ~ x30 ...
    LOAD x31, 31*REGBYTES(sp)

    LOAD x2, 2*REGBYTES(sp)    # last: every load above is addressed through sp
.endm

__alltraps:
    SAVE_ALL
    #lab3 function1 2414007
    move a0, sp
    jal trap
```

#### 4.1.2 最终提示词

以下是用于生成练习 1 代码的提示词（只包含代码修改部分）：

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

#### 4.1.3 实现迭代过程

本模块的实现经历了 1 次迭代，过程如下：

---

##### 第一次迭代

**最终结果：**

使用 4.1.2 的提示词生成的代码在第一次测试中就满足要求，因此没有需要修复的问题。测试方法是：在不影响仓库的临时副本中，写一个最小的临时 `trap()` 处理逻辑（时钟中断时设置下一次事件并计数，断点异常时前进 `epc`），然后在 QEMU 8.2 中运行。结果如下：

- ✅ 编译通过，无警告。
- ✅ 时钟中断能够连续触发：`100 ticks` 按预期周期性输出，说明每次 Trap 都能正确返回被打断的循环。
- ✅ 在 `kern_init` 中先给 `s5`、`t3` 赋特定值，再执行 `ebreak` 进入 Trap 并返回：两个寄存器的值保持不变，`ebreak` 前后的 `sp` 完全相同，说明通用寄存器和 `sp` 的恢复都正确（`x2` 最后恢复的设计生效）。
- ✅ 功能符合预期，`SAVE_ALL`、`struct trapframe` 和 `trap()` 均未被修改。

---

### 4.2 中断处理功能

**负责人：** [2414015-鲁昕宁]

#### 4.2.1 模块功能描述

**需要实现的函数**（位于 `kern/trap/trap.c`）：

```c
void interrupt_handler(struct trapframe *tf);
```

**功能说明：**

`trap()` 把 trapframe 交给 `trap_dispatch()`。`cause` 的最高位为 1 时进入本函数。函数按去掉最高位之后的原因码分三支，然后正常返回；`tf` 的每个字段都保持进入时的值，后面的 `RESTORE_ALL` 和 `sret` 因此回到被打断的指令。

1. **`IRQ_S_TIMER`。** 先 `clock_set_next_event()`，再 `ticks++`。`ticks` 是 `TICK_NUM`（100）的整数倍时调用 `print_ticks()`。`ticks / TICK_NUM` 等于 `SHUTDOWN_AFTER`（10）时，也就是刚刚打出第 10 行之后，调用 `sbi_shutdown()`。这一支不另打印中断名称。`sbi_shutdown()` 的声明在 `libs/sbi.h`，所以 `trap.c` 增加了这一行头文件，其余函数没有改。
2. **其余已列出的中断。** `IRQ_U_SOFT` 到 `IRQ_HOST` 各打印一行固定文字后返回，不改 `ticks`，也不预约下一次时钟。监督态软件中断的那一行是 `Supervisor software interrupt`。
3. **认不出的原因码。** 打印 `Unknown interrupt: cause %d`，`%d` 是去掉最高位后的值。原因码 15 得到 `Unknown interrupt: cause 15`。

原因码这样取出：

```c
uintptr_t cause = tf->cause << 1 >> 1;
```

`uintptr_t` 是无符号的，左移一位再右移一位丢掉的是最高位，低位原样留下。`tf->cause` 已带最高位时，`IRQ_S_SOFT` 仍然对得上；测试直接传入 15 时，打印出来的也是 15。

时钟中断的处理是：

```c
case IRQ_S_TIMER:
    clock_set_next_event();
    ticks++;
    if (ticks % TICK_NUM == 0) {
        print_ticks();
    }
    if (ticks / TICK_NUM == SHUTDOWN_AFTER) {
        sbi_shutdown();
    }
    break;
```

`exception_handler()` 仍是空函数体。

#### 4.2.2 最终提示词

以下是用于生成练习 2(a) 代码的提示词：

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

#### 4.2.3 实现迭代过程

本模块的实现经历了 1 次迭代，过程如下：

---

##### 第一次迭代

**最终结果：**

使用 4.2.2 的提示词生成的代码在第一次测试中就满足中断处理的要求，因此没有需要修复的问题。测试方法是：在 `labcodes/lab3` 下执行 `make`，再用 `qemu-system-riscv64 -machine virt -nographic -bios default -kernel bin/ucore.img` 运行。结果如下：

- ✅ 编译通过。
- ✅ `++ setup timer interrupts` 之后连续输出 10 行 `100 ticks`，大约 10 秒后 QEMU 退出。10 行对应 `SHUTDOWN_AFTER`，行与行之间大约 1 秒，对应 100 次、每次 10 ms 的时钟中断。这说明每次都预约了下一次，`ticks` 每 100 次打印一行，第 10 行之后 `sbi_shutdown()` 生效。
- ✅ `exception_handler()` 仍是空函数体，`trap()`、`trap_dispatch()`、`print_ticks()` 和两个宏都没有改。
- 非法指令、断点以及 `Supervisor software interrupt`、`Unknown interrupt: cause 15` 这几条输出，要等异常处理函数写好之后才能用 `make grade` 一起看。本模块这次只跑了时钟中断这一条实际会发生的路径。

---

### 4.3 异常处理功能

**负责人：** [2411622-张雨函]

#### 4.3.1 模块功能描述

**需要实现的函数**（位于 kern/trap/trap.c）：

    void exception_handler(struct trapframe *tf);

**功能说明**
本实验任务实现 kern/trap/trap.c 中的 exception_handler()，负责处理由 trap_dispatch() 分发的同步异常。该函数接收保存完整寄存器和控制状态的 trapframe，根据 tf->cause 判断异常类型，并在处理结束后通过 RESTORE_ALL 和 sret 返回内核执行流。

主要功能包括：
1. 非法指令处理
   当异常原因为 CAUSE_ILLEGAL_INSTRUCTION 时，输出非法指令地址。函数读取 tf->epc 处的 16 位指令编码，根据低两位判断指令长度：低两位为 0x3 时为 32 位指令，其他情况为 16 位压缩指令，然后将 tf->epc 增加 4 或 2，使返回后跳过非法指令。
2. 断点异常处理
   当异常原因为 CAUSE_BREAKPOINT 时，输出断点地址，并使用相同的指令长度判断规则推进 tf->epc。这样执行 sret 后会从断点指令之后继续运行，避免重复触发同一个断点异常。
3. 其他异常处理
   对未专门处理的异常调用已有的 print_trapframe()，输出寄存器、`epc`、`badvaddr` 和 cause 等现场信息，便于后续调试和定位问题。
该功能依赖实验一完成的 Trap 上下文保存与恢复机制，与时钟中断处理共同使用 Trap 入口和返回路径，但异常处理本身不修改中断处理逻辑。

#### 4.3.2 最终提示词

以下是用于生成练习 2-b 代码的提示词：

````markdown

[PROMPT]

任务：只修改 kern/trap/trap.c 中的 exception_handler()，完成 Lab3 练习 2-b。实现非法指令和断点异常的提示、指令长度判断以及 epc 更新；其他异常沿用已有的 print_trapframe()。不得修改练习 2-a 的中断处理和 Trap 分发框架。

操作要求：直接修改项目中的真实文件，不新增文件，不改变 exception_handler(struct trapframe *tf) 的函数签名。保留 //lab3 function2 YOUR CODE 的 lab3 function2 标记，并把 YOUR CODE 替换为 2411622。除本函数外，不要随意增删或改动代码。

[RELY]

    1. trap_dispatch() 已经按 scause 最高位分流；最高位为 0 时调用本函数。
    2. struct trapframe 中 epc 是发生异常的指令地址，cause 是异常原因。
    3. CAUSE_ILLEGAL_INSTRUCTION 和 CAUSE_BREAKPOINT 已在 riscv.h 中定义。
    4. print_trapframe() 已经实现，可用于输出其他异常的完整现场。
    5. RISC-V 指令编码的低两位不为 11 时表示 16 位压缩指令，为 11 时表示 32 位指令。

[GUARANTEE]

    只需交付 exception_handler()。非法指令和断点处理后必须正常返回；其他异常必须调用 print_trapframe()。不得在本函数中循环、关机或修改中断处理逻辑。

[SPECIFICATION]

    - 非法指令：打印 Illegal instruction at 0x%08x，判断当前指令长度，并将 tf->epc 增加 2 或 4。
    - 断点：打印 Breakpoint at 0x%08x，判断当前指令长度，并将 tf->epc 增加 2 或 4。
    - 指令长度：读取 tf->epc 处的 16 位值；低两位为 0x3 时长度为 4，否则为 2。
    - 其他异常：调用 print_trapframe(tf)，不自行猜测原因。
    - 处理函数不能把 epc 留在异常指令上，否则 sret 会重复触发同一异常。
````

#### 4.3.3 实现迭代过程

本模块的实现经历了 1 次迭代。

##### 第一次迭代

根据 4.3.2 的约束补全 exception_handler()，实现了非法指令和断点两类异常的固定提示、16/32 位指令长度判断、epc 前进，以及其他异常的 print_trapframe() 回退处理。实现过程中只改动了指定函数，并将注释中的 YOUR CODE 替换为 2411622。

随后在 labcodes/lab3 下执行 make grade。评分脚本分三个用例验证 Trap 入口、异常返回和持续时钟中断：

- ✅ trap entry、寄存器恢复和断点返回：35/35；
- ✅ 非法指令 epc 前进和返回，同时检查 Supervisor software interrupt、Unknown interrupt: cause 15 以及 cause 0x00000014：25/25；
- ✅ 重新预约监督态时钟并重复返回：40/40。

最终结果为 Total Score: 100/100，说明 2-b 的异常处理可以和 2-a 的中断处理以及练习 1 的 Trap 上下文恢复协同工作。

---

## 五、测试与验证

---

## 六、实验总结与收获

### 6.1 对操作系统的理解

1. **Trap 是一条完整的软硬件协作链。** 硬件保存 sepc、scause 等控制状态并跳转到 stvec；SAVE_ALL 把通用寄存器保存成 struct trapframe；C 代码按原因分发；RESTORE_ALL 恢复现场后由 sret 返回。异常处理函数修改的 tf->epc，只有经过这条恢复路径才会真正写回 sepc。
2. **中断与异常的返回语义不同。** 时钟中断是外部事件，处理后应保留原 epc，让被打断的指令继续执行；非法指令和断点是当前指令造成的同步异常，处理后必须推进 epc，否则会无限重复陷入。这个差异体现了 scause 分流背后的体系结构语义。
3. **指令长度会影响异常恢复。** RISC-V 的 C 扩展允许 16 位压缩指令，因此不能固定让 epc += 4。通过低两位判断 16 位或 32 位长度，才能同时正确处理压缩指令和普通指令。
4. **框架化接口降低了模块之间的耦合。** trap_dispatch() 只负责分流，interrupt_handler() 和 exception_handler() 各自处理一类原因，汇编入口只负责保存和恢复。新增一种异常时可以集中修改异常处理函数，而不必破坏公共 Trap 入口。

### 6.2 AI 协作开发的经验

1. **先把约束写清楚，再让 AI 生成实现。** 提示词明确了只能修改 exception_handler()、必须保留 lab3 function2 注释、需要把 YOUR CODE 改成 2411622，生成结果才不会误改中断处理或 Trap 分发代码。
2. **把体系结构规则转成可检查的条件。** 不能笼统地说“跳过当前指令”，而要明确低两位为 11 时加 4，否则加 2，并要求非法指令和断点分别输出固定文本。这样代码和评分脚本的边界条件能够对应起来。
3. **用评分用例验证控制流，而不只看编译结果。** make grade 分别检查断点返回、非法指令 epc 前进、软件中断和未知中断输出，以及时钟中断持续触发。最终三项测试分别通过 35、25、40 分，总分 100/100，说明异常处理与已有 Trap 入口、中断处理可以协同工作。
4. **遇到未覆盖异常时保留现场比擅自处理更安全。** 对没有专门要求的 cause 调用 print_trapframe()，既满足调试需要，也避免 AI 根据不完整信息引入额外的异常语义。
---
