# 操作系统实验报告

## 实验基本信息

|     项目     |                      内容                      |
| :----------: | :--------------------------------------------: |
| **实验名称** |                 Lab4：进程管理                 |
| **小组成员** | 2414007-王奕瑜、2414015-鲁昕宁、2411622-张雨函 |
| **完成日期** |                   2026-11-22                   |

### 小组分工

练习如何分工？

|      成员      |               负责的练习/模块               |
| :------------: | :-----------------------------------------: |
| 2414007-王奕瑜 |      功能模块三：进程调度、Challenge 1      |
| 2414015-鲁昕宁 | 功能模块一：虚拟内存页操作功能、Challenge 2 |
| 2411622-张雨函 |      功能模块二：进程创建及初始化功能       |

实验报告如何分工？

|      成员      |  负责的实验报告内容  |
| :------------: | :------------------: |
| 2414007-王奕瑜 | 3.2.3, 4.3, 4.4, 五  |
| 2414015-鲁昕宁 | 3.1, 3.2.1, 4.1, 4.5 |
| 2411622-张雨函 |  一, 3.2.2, 4.2, 六  |

---

## 一、实验目的

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

### 3.2 功能的逐步实现

#### 3.2.1 功能模块一：虚拟内存页操作功能

#### 3.2.2 功能模块二：进程创建及初始化功能

#### 3.2.3 功能模块三：进程调度

模块一、二结束时，系统里已经存在 `idleproc` 和 `initproc` 两个 PCB，但 `initproc` 只是一个"登记在册、状态为 RUNNABLE"的对象，没有任何机制能让 CPU 真正转到它上面。模块三补的就是这最后一环：`cpu_idle` 让 `idleproc` 持续检查是否需要重调度，`schedule` 在 `proc_list` 里挑出下一个可运行线程，`proc_run` 在关中断的临界区里更新 `current`、切换页表，最后由 `switch_to` 保存和恢复寄存器上下文。因此它放在最后实现，原因是它依赖前两个模块的结果。

与其他模块的关联如下：

- 与模块二：`schedule` 扫描的是 `do_fork` 挂入 `proc_list` 的 PCB，并依赖 `wakeup_proc` 设置的 `PROC_RUNNABLE`；`proc_init` 把 `idleproc->need_resched` 置 1，`cpu_idle` 才会第一次调用 `schedule`；`copy_thread` 预先把 `context.ra` 设为 `forkret`、`context.sp` 设为 trapframe，`switch_to` 的 `ret` 才能第一次进入新线程。
- 与模块一：所有线程共享的 `boot_pgdir_pa` 来自启动阶段建立的页表，`proc_run` 把它写进 `satp` 时沿用了 `pmm.c` 里 `enable_paging` 的 Sv39 写法。
- 与后续实验：lab4 的调度点只有 `cpu_idle`；后面的实验会让 `do_exit`、`do_wait` 等路径也调用 `schedule`，这时本模块的接口不用改动。

---

## 四、实验内容与实现

### 4.1 功能模块一：虚拟内存页操作功能

**负责人：** [2414015-鲁昕宁]

#### 4.1.1 模块功能描述

#### 4.1.2 最终提示词

#### 4.1.3 实现迭代过程

---

### 4.2 功能模块二：进程创建及初始化功能

**负责人：** [2411622-张雨函]

#### 4.2.1 模块功能描述

#### 4.2.2 最终提示词

#### 4.2.3 实现迭代过程

---

### 4.3 功能模块三：进程调度

**负责人：** [2414007-王奕瑜]

#### 4.3.1 模块功能描述

**需要实现的函数：**

```c
void cpu_idle(void);                                // kern/process/proc.c
void schedule(void);                                // kern/schedule/sched.c
void proc_run(struct proc_struct *proc);            // kern/process/proc.c
void switch_to(struct context *from, struct context *to);  // kern/process/switch.S
```

**功能说明：**

本模块负责把已经创建好的内核线程真正交给 CPU 执行。它不关心线程是怎么创建出来的，只关心"谁来决定切换、选谁、怎么切"。四个函数分成三层：

| 层次   | 函数        | 职责                                                                |
| ------ | ----------- | ------------------------------------------------------------------- |
| 触发层 | `cpu_idle`  | 空闲线程的主循环，发现 `need_resched` 就进入调度                    |
| 策略层 | `schedule`  | 选择下一个线程（FIFO + Round-Robin），不碰寄存器                    |
| 机制层 | `proc_run`  | 在关中断下更新 `current`、切换页表，然后调用 `switch_to`            |
| 机制层 | `switch_to` | 保存旧线程、恢复新线程的被调用者保存寄存器，用 `ret` 完成控制流转移 |

**1. `cpu_idle`：触发调度**

`cpu_idle` 是 `idleproc` 的函数体，是一个不返回的循环：每一轮读取 `current->need_resched`，非零就调用 `schedule()`。`need_resched` 声明为 `volatile`，每一轮都会重新读内存。`proc_init` 在构造 `idleproc` 时把它置 1，所以 `kern_init` 调用 `cpu_idle` 后第一轮就会进入调度，这也是 `initproc` 能第一次拿到 CPU 的原因。lab4 中调度点只有这一个。

```c
void cpu_idle(void) {
    while (1) {
        if (current->need_resched) {
            schedule();
        }
    }
}
```

**2. `schedule`：选择下一个线程**

`schedule` 在 `local_intr_save` / `local_intr_restore` 之间完成全部工作，这样扫描 `proc_list` 的过程中不会被时钟中断打断：

1. 先把 `current->need_resched` 清零，避免同一次请求被重复处理；
2. 确定扫描起点 `last`：`current == idleproc` 时取链表头 `&proc_list`，否则取 `&current->list_link`。这是因为 `idleproc` 不在 `proc_list` 中，它调度时应从链表第一个 PCB 开始，让所有线程有平等的机会；其他线程调度时从自己的下一个位置开始，形成 Round-Robin；
3. 从 `last` 的下一个节点开始，环形走一圈，回到 `last` 为止。链表头结点 `proc_list` 不是内嵌在 `proc_struct` 里的，遇到它必须跳过，不能调用 `le2proc`；
4. 找到第一个 `state == PROC_RUNNABLE` 的 PCB 就选中；整圈都没有就选 `idleproc`；
5. `next->runs++`，并且只有 `next != current` 才调用 `proc_run`。

```c
last = (current == idleproc) ? &proc_list : &(current->list_link);
le = last;
while ((le = list_next(le)) != last) {
    if (le == &proc_list) continue;               // skip the list head
    next = le2proc(le, list_link);
    if (next->state == PROC_RUNNABLE) break;
    next = NULL;
}
if (next == NULL) next = idleproc;                // nothing runnable: fall back to idle
next->runs++;
if (next != current) proc_run(next);
```

**3. `proc_run`：切换的临界区**

`proc == current` 时直接返回。否则先保存旧线程指针 `prev`，然后在关中断区间内依次做三件事，顺序是固定的：

1. `current = proc`：必须在切换栈之前更新，这样新线程一醒来读到的 `current` 就是它自己；
2. 切页表：`write_csr(satp, SATP_MODE_SV39 << 60 | proc->pgdir >> RISCV_PGSHIFT)`，再 `flush_tlb()`。写法对照 `pmm.c` 的 `enable_paging`，没有使用 Sv32 遗留的 `lsatp()`。lab4 中所有线程共用 `boot_pgdir_pa`，写入的值与当前相同，因此这一步不改变行为，但保证了以后进程有自己页表时这个函数仍然正确；
3. `switch_to(&prev->context, &proc->context)`。

`switch_to` 返回时，CPU 上跑的已经是另一个线程。对于旧线程，`switch_to` 的"返回"发生在它下一次被调度回来的时刻，这时才会执行 `local_intr_restore`，把中断状态还原成进入前的样子。对于一个从未运行过的新线程，它不会从 `proc_run` 返回，而是直接从 `forkret` 开始，退出内核时由 `sret` 根据 trapframe 里的 `sstatus.SPIE` 恢复中断。

**4. `switch_to`：寄存器级切换**

`switch_to` 用汇编实现，`a0` 指向旧线程的 `struct context`，`a1` 指向新线程的。它的做法是：把 `ra`、`sp`、`s0~s11` 共 14 个寄存器依次存到 `n * REGBYTES(a0)`，再从 `n * REGBYTES(a1)` 依次取回，最后 `ret`。偏移量与 `struct context` 的字段顺序一一对应。

只保存这 14 个寄存器的原因是：`switch_to` 是被 `proc_run` 当作普通函数调用的，按照 RISC-V 调用约定，调用者保存寄存器（`a*`、`t*`）已经由编译器在调用前后保存，需要由 `switch_to` 负责保存的只剩被调用者保存寄存器和 `ra`、`sp`。`ret` 会跳转到新上下文里恢复出来的 `ra`：对运行过的线程，这是它上次调用 `switch_to` 之后的位置；对新线程，这是 `copy_thread` 设置的 `forkret`，从而接入 `forkret -> forkrets -> __trapret -> kernel_thread_entry -> init_main` 这条首次运行链路。

**整体执行流程：**

```text
kern_init -> cpu_idle (idleproc, need_resched = 1)
          -> schedule: 从链表头找到 initproc (RUNNABLE)
          -> proc_run(initproc): current = initproc, satp, switch_to
          -> switch_to: ret 到 initproc->context.ra = forkret
          -> forkret -> forkrets(tf) -> __trapret -> sret
          -> kernel_thread_entry -> init_main(arg)
```

#### 4.3.2 最终提示词

以下是最终用于实现该功能的提示词（同时汇总在 [prompt.md](./prompt.md) 的 2.1 节）：

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

#### 4.3.3 实现迭代过程

本模块的实现经历了 1 次迭代，过程如下：

---

##### 第一次迭代

**测试方式：** 模块一、二当时还没有合入，仓库里无法链接，所以我在一个 `/tmp` 临时副本里补了一份只用于本地验证的 `page_insert`、`page_remove` 和进程创建函数（不提交到仓库），再把本模块的代码放进去，依次进行了：

1. `make`：编译、链接；
2. `make grade`：`check PMM`、`check Process Create`、`check Schedule`；
3. `make qemu`：观察启动输出，确认 `initproc` 被调度。

**最终结果：**

经过 1 次迭代，该模块最终：

- ✅ 编译通过（无与本模块相关的 warning）
- ✅ `make grade` 中 `check PMM`、`check Process Create`、`check Schedule` 全部 OK，总分 30/30
- ✅ `make qemu` 输出了 `this initproc, pid = 1, name = "init"` 以及两条 `To U: ...`，说明 `idleproc -> initproc` 的切换以及 `forkret -> forkrets -> __trapret -> kernel_thread_entry` 链路都正确
- ✅ 功能符合预期：随后 `init_main` 返回、`do_exit` 按 lab4 的设计 panic 并进入 kernel monitor，这属于预期行为

第一次生成的代码没有出现需要修复的 bug，所以只有 1 次迭代。提示词的 [RELY] 和 [SPECIFICATION] 里预先写明了下面几个这类模块里常见的易错点（它们是否是“没有出 bug”的原因没有做对照实验，这里只是列出）：

- 链表头结点不是 PCB：`schedule` 的环形扫描必须跳过 `&proc_list`，否则 `le2proc` 会把头结点当成 `proc_struct` 读出垃圾的 `state`；
- 寄存器偏移必须与 `struct context` 一致：提示词中固定了字段顺序 `ra, sp, s0~s11`，并要求按机器字宽计算偏移；
- `idleproc` 不在 `proc_list` 中：提示词区分了"调用者是 `idleproc`"和"不是"的两种扫描起点，否则 `idleproc` 调度时会从 `idleproc->list_link`（全 0）开始走；
- 不能使用 `lsatp()`：提示词在 [RELY] 里点明它是 Sv32 遗留，并指向 `enable_paging` 作为参照写法；
- `proc_run` 的顺序和中断恢复：提示词要求 `current` 在切换栈之前更新，且整个过程关中断。

---

### 4.4 Challenge 1

**负责人：** [2414007-王奕瑜]

> 说明语句 `local_intr_save(intr_flag); ... local_intr_restore(intr_flag);` 是如何实现开关中断的。

---

### 4.5 Challenge 2

**负责人：** [2414015-鲁昕宁]

> 深入理解不同分页模式的工作原理（思考题）。
> `get_pte()` 函数位于 `kern/mm/pmm.c`，用于在页表中查找或创建页表项，从而实现对指定线性地址对应的物理页的访问和映射操作。这在操作系统的分页机制下，是虚拟内存与物理内存建立映射关系的关键基础。
>
> - `get_pte()` 函数中有两段形式类似的代码，结合 `sv32`、`sv39`、`sv48` 的异同，解释这两段代码为什么如此相像。
> - 目前 `get_pte()` 函数将页表项查找和页表页分配合并在一个函数里，你认为这种写法好吗？有没有必要把两个功能拆开？

Sv32、Sv39、Sv48 用的是同一套页表项，差别是虚拟地址有多长、中间要走几级。每一级页表都占一页。项的低 10 位是标志，`V` 为 0 表示这一项空着；`V` 为 1 且 `R`、`W`、`X` 都是 0，表示这一项指向下一级页表，物理页号在高位；`R`、`W`、`X` 有一位为 1，这一项才是叶子，指向数据页。页内偏移不参加查找，原样拼到物理地址末尾。

| 模式 | 虚拟地址 | 级数 | 每级下标 | 页表项 | 一页能放的项 |
| --- | --- | --- | --- | --- | --- |
| Sv32 | 32 位 | 2 | 10、10 位，加上 12 位页内偏移 | 4 字节 | 1024 |
| Sv39 | 39 位 | 3 | 9、9、9 位，加上 12 位页内偏移 | 8 字节 | 512 |
| Sv48 | 48 位 | 4 | 四个 9 位，加上 12 位页内偏移 | 8 字节 | 512 |

本实验是 Sv39。`get_pte()` 里那两段代码是根页表（`PDX1`，即 VPN[2]）和中间页表（`PDX0`，即 VPN[1]）。两段都做同一件事：用本级下标取出一项；`PTE_V` 已置位就沿着 `PDE_ADDR` 走到下一级；未置位且 `create` 非 0 就分配一页、清零、把引用计数设为 1，再写成只带 `PTE_V` 的非叶项。最后用 `PTX`（VPN[0]）返回叶子槽的地址，叶子本身不在这里分配。

它们长得一样，是因为非叶项在每一级的格式相同。Sv32 只有一级非叶页表，这样的代码只会出现一次；Sv48 有三级非叶页表，就会再抄一段，下标换成 VPN[3]。级数变了，每一级的动作没有变。

查找和分配放在一个函数里，靠 `create` 区分，对本实验是够用的。`page_insert()` 传入 1，缺的中间页表顺手建好；`page_remove()` 和 `get_page()` 传入 0，不会因为一次查询就领走物理页。调用处不用自己写三级循环。

它的问题是失败时会留下半截页表。第一级分配成功、第二级 `alloc_page()` 失败时，函数返回 `NULL`，已经挂上的那一页不会收回。`create == 0` 的路径也因此不再是纯粹的查询。两段几乎相同的代码也说明，Sv48 不能靠再复制一段来扩展。

更清楚的拆法是：沿页表往下走只负责读；“这一级不存在就分配一页并挂上”单独做一个内部函数，由 `create` 决定走不走。分配失败时由这个函数把本轮已经挂上的页表页释放掉。对外仍可以只留 `get_pte()`，不必再给调用者两个入口。叶子页仍由 `page_insert()` 填写，因为那一页是数据页，不是下一级页表。

---

## 五、测试与验证

**负责人：** [2414007-王奕瑜]

---

## 六、实验总结与收获

### 6.1 对操作系统的理解

### 6.2 AI 协作开发的经验

---
