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

#### 2.1.1 功能模块一：进程文件表与地址空间的必要更新

````markdown
[PROMPT]
**任务**：补全 lab8 的"进程文件表与地址空间的必要更新"功能模块，修改 `kern/process/proc.c` 中三个已经补回前序实验代码的函数：`alloc_proc`（标记 `lab8 function1-1`）、`proc_run`（标记 `lab8 function1-2`）、`do_fork`（标记 `lab8 function1-3`）。目标是：让每个新进程在进入文件系统相关路径前拥有确定的文件表初始状态；让进程切换时真正切换到目标进程自己的页表并刷新 TLB；让 `fork`/`clone` 创建的子进程正确继承或共享父进程的文件表，并在后续步骤失败时按获取的相反顺序释放。

**操作要求**：你必须在项目中修改实际文件，保留现有代码，只在标有 `lab8 function1-x YOUR CODE` 的位置补全或更新，并把其中的 `YOUR CODE` 替换为学号 `2414007`；lab4、lab5、lab6 补回的字段初始化、上下文切换和进程创建流程是基线，在其上更新，不得删除；框架提供的 `copy_files()`、`put_files()`、`copy_mm()`、`copy_thread()` 等辅助函数不要修改或重写。所有新增代码的注释使用英文，函数使用 Doxygen 风格。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息，以工程真实代码为准，不要引入无关功能。

[RELY]
1. 进程控制块 `struct proc_struct`（`proc.h`）：`state`、`pid`、`runs`、`kstack`、`need_resched`、`parent`、`mm`、`context`、`tf`、`pgdir`（页目录的**物理地址**，`proc_run` 把它写入 `satp`）、`flags`、`name`、`list_link`、`hash_link`、`exit_code`、`wait_state`、`cptr/yptr/optr`，以及 lab6 的调度字段；lab8 新增 `struct files_struct *filesp`，指向本进程的文件表。`kmalloc` 返回的内存不清零。
2. 文件表（`kern/fs/fs.h`）：`struct files_struct { struct inode *pwd; struct file *fd_array; int files_count; semaphore_t files_sem; }`。`proc.c` 中框架已提供：`static int copy_files(uint32_t clone_flags, struct proc_struct *proc)`——设置 `CLONE_FS` 时子进程共享父进程的表，否则新建表并复制打开的文件；成功时设置 `proc->filesp` 并把 `files_count` 加 1，返回 0；失败时返回 `-E_NO_MEM`，已创建的表由它自己销毁，`proc->filesp` 保持原值。`static void put_files(struct proc_struct *proc)`——`proc->filesp` 为 `NULL` 时什么都不做，否则把 `files_count` 减 1，减到 0 时销毁表并关闭其中仍打开的文件。`do_exit` 已经调用 `put_files(current)`；`proc_init` 已经用 `files_create()` 给 `idleproc` 建立文件表，所以 `alloc_proc` 不负责创建文件表。
3. 切换相关：`current`；`local_intr_save(flag)`、`local_intr_restore(flag)`（`sync.h`）；`lsatp(unsigned long pgdir_pa)`（`riscv.h`，把 Sv39 模式和页目录物理页号写入 `satp`）；`flush_tlb()`；`void switch_to(struct context *from, struct context *to)`（汇编，保存当前上下文并恢复目标上下文）；`boot_pgdir_pa`（内核页目录的物理地址）。
4. 进程创建相关：`alloc_proc()`、`setup_kstack(proc)`、`put_kstack(proc)`、`copy_mm(clone_flags, proc)`（成功时设置 `proc->mm` 和 `proc->pgdir`）、`copy_thread(proc, esp, tf)`、`get_pid()`、`hash_proc(proc)`、`set_links(proc)`、`wakeup_proc(proc)`、`kfree`、`nr_process`、`MAX_PROCESS`、`CLONE_VM`、`CLONE_FS`；错误码 `E_NO_MEM`、`E_NO_FREE_PROC`、`E_INVAL`。

[GUARANTEE]
需要修改以下函数，签名必须与现有声明一致：

```c
static struct proc_struct *alloc_proc(void)                                 // lab8 function1-1
void proc_run(struct proc_struct *proc)                                     // lab8 function1-2
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)    // lab8 function1-3
```

不得修改 `copy_files`、`put_files`、`copy_mm`、`copy_thread` 的实现。允许新增 `static` 辅助函数，不得改变公开接口。

[SPECIFICATION]
**alloc_proc**
- Pre-Condition：无。
- Post-Condition：在 lab4、lab5、lab6 已有的字段初始化之上，返回的 PCB 的 `filesp` 为 `NULL`，表示该进程尚未拥有文件表；其余字段的初始状态保持不变。
- Case：`kmalloc` 失败时返回 `NULL`（保持原有行为）。
- Requirements：不得在这里创建或共享文件表，也不得调用 `files_create()`：idle 进程和普通子进程建立文件表的时机不同，分别由 `proc_init` 和 `copy_files` 负责。不能假定 `kmalloc` 返回的内存已清零，也不能让一个未初始化的文件表指针到达 `copy_files`、`files_closeall` 或 `put_files`。

**proc_run**
- Pre-Condition：`proc` 是一个已初始化、状态为可运行的进程，由调度器调用。
- Post-Condition：`current` 等于 `proc`；`satp` 指向 `proc->pgdir`，TLB 中不再保留上一个地址空间的翻译；CPU 已经切换到 `proc` 的内核上下文。被切换出去的线程再次被调度回来时，会在原来的位置继续执行并恢复进入时的中断状态。
- Case 1：`proc` 已经是 `current`。直接返回，不做任何切换。
- Case 2：否则，在关中断保护下依次完成：记下切换前的进程，把 `current` 更新为 `proc`，加载 `proc->pgdir`，刷新 TLB，再切换内核上下文；切换返回后恢复中断状态。内核线程的 `pgdir` 就是 `boot_pgdir_pa`（与正在使用的页表相同），用户进程的 `pgdir` 是 `copy_mm` 或 `load_icode` 为它建立的页目录。
- Requirements：`current` 必须在切换内核栈之前更新；页表切换和上下文切换必须处在同一个关中断区间内，不能被时钟中断打断；使用框架提供的 `lsatp()` 加载页表，不要手写 CSR 访问；不能把用户页表当作内核页表使用；保留 lab4 要求的上下文切换语义。

**do_fork**
- Pre-Condition：`tf` 是用作子进程模板的 trapframe，`current` 已设置且拥有文件表。
- Post-Condition：成功时返回子进程 PID，子进程拥有自己的 PCB（`parent` 为 `current`）、内核栈、文件表状态、地址空间、trapframe 和初始上下文，已经挂入哈希表、进程链表和进程树，状态为可运行。失败时返回负的错误码，并且没有任何残留：不泄漏 PCB、内核栈、文件表引用或地址空间。
- Case 1：`nr_process` 已达 `MAX_PROCESS`，返回 `-E_NO_FREE_PROC`（保持原有行为）。
- Case 2：`copy_files` 失败。此时尚未取得文件表，只释放内核栈和 PCB，不得调用 `put_files` 去释放一个不存在的表，返回其错误码。
- Case 3：`copy_mm` 失败。必须按获取的相反顺序释放：先 `put_files` 放掉文件表引用，再释放内核栈，最后释放 PCB，返回其错误码。
- Case 4：全部成功。保持前序实验已有的流程不变：`copy_thread`，然后在关中断的临界区内完成 `get_pid`、`hash_proc`、`set_links`、`wakeup_proc`，返回 PID。
- Requirements：`copy_files` 放在 `setup_kstack` 之后、`copy_mm` 之前，使 `copy_mm` 的失败路径需要释放文件表；文件表的引用计数已由 `copy_files` 增加，成功路径上不得重复增加；失败路径用 `put_files`（按引用计数递减，最后一个引用才销毁表）而不是直接销毁，以免破坏 `CLONE_FS` 共享的表；`wakeup_proc` 仍是使子进程可运行的唯一步骤，并保持在临界区的最后。
````

#### 2.1.2 功能模块二：SFS 文件读写功能

#### 2.1.3 功能模块三：基于文件系统的用户程序加载与执行

### 2.2 2414015-鲁昕宁

### 2.3 2411622-张雨函
