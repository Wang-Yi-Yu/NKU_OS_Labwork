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

**Challenge 1：在 ucore 中实现简化的死锁和重入探测机制**

````markdown
[PROMPT]
**任务**：在 lab7 里为互斥信号量加上简化的死锁与重入探测。探测对象只限 `sem_init(sem, 1)` 得到的互斥量。需要识别三类事件：同一进程再次进入自己已持有的互斥量（重入）、互斥量已被别的进程持有时又有进程取得它（多个进程进入同一临界区）、进程之间出现环形等待（死锁的环形等待条件，同时说明同时满足互斥、持有并等待、不可抢占）。发现任何一类时打印探测信息；默认处理是进入内核 monitor。另外提供一个自检，自检期间不得进入 monitor，并且返回后进程表与调用前一致。

**操作要求**：你必须在项目中修改实际文件。新增 `kern/sync/deadlock.h` 和 `kern/sync/deadlock.c`（`Makefile` 已把 `kern/sync` 列入 `KSRCDIR`，不必改编译脚本）。在 `kern/sync/sem.h` 的 `semaphore_t` 末尾增加字段，并在 `sem_init`、`__down`、`__up`、`try_down` 中调用本模块的钩子。不要改管程、哲学家就餐和 `check_sync()` 的逻辑。在 `init_main()` 里、`check_sync()` 之前调用 `deadlock_check()`。探测代码的注释使用英文。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 的信息，以工程中的真实代码为准。不要实现银行家算法，不要改调度类。

[RELY]

- `semaphore_t`（`kern/sync/sem.h`）现有字段只有 `int value` 和 `wait_queue_t wait_queue`。`value` 不会变成负数：没有资源时进程睡在 `wait_queue` 上。`sem_init(sem, value)`、`down`、`up`、`try_down` 的实现在 `kern/sync/sem.c` 的 `__down`、`__up`、`try_down` 中，这些操作已经用 `local_intr_save` / `local_intr_restore` 包住对 `value` 和等待队列的修改。
- 互斥量是 `sem_init(sem, 1)`。初值不是 1 的信号量（哲学家的叉子计数、条件变量内部的信号量）不参与探测。
- 进程：全局 `current`、`idleproc`、`proc_list`；`current->pid`。本实验是单 hart，一个时刻只有一个 `current`。
- 进入 monitor：`void kmonitor(struct trapframe *tf)`（`kern/debug/kmonitor.h`）。探测路径调用 `kmonitor(NULL)`。`kmonitor` 会读命令行并停住，所以启动路径上的自检不能走到这次调用。
- 打印使用 `cprintf`。创建内核线程使用已有的 `kernel_thread`。等待某个子进程结束使用已有的 `do_wait(pid, NULL)`，失败时再 `schedule()` 让出 CPU。`do_exit` 由 `kernel_thread_entry` 在线程函数返回后调用，自检线程返回即可。

[GUARANTEE]

- `semaphore_t` 末尾增加：`int is_mutex`（`sem_init` 的初值为 1 时置 1，否则 0）、`int owner`（持有者 pid，无人持有时为 0）。`sem_init` 必须给这两个字段赋初值。
- 下列钩子，声明在 `deadlock.h`，定义在 `deadlock.c`。`sem.c` 在关中断的临界区内调用它们：

```c
void deadlock_on_acquire(semaphore_t *sem);
int deadlock_on_wait(semaphore_t *sem);
void deadlock_on_release(semaphore_t *sem);
```

- `deadlock_on_acquire`：`__down` 在 `value > 0` 并完成 `value--` 之后调用；`try_down` 在成功取走一个单位之后调用。只对 `is_mutex == 1` 有动作。
- `deadlock_on_wait`：`__down` 在 `value == 0`、尚未 `wait_current_set` 之前调用。返回 1 表示已经报告违规，调用方不得把 `current` 放入等待队列，也不得 `schedule()`，本次 `down` 直接返回。返回 0 表示照常睡眠。
- `deadlock_on_release`：`__up` 在修改 `value` 或唤醒等待者之前调用。只对 `is_mutex == 1` 有动作。
- 默认报告函数打印一行并调用 `kmonitor(NULL)`。自检可以临时换掉这个报告函数，自检结束必须换回。
- `void deadlock_check(void);` 在 `init_main()` 中、`check_sync()` 之前调用。

不新增系统调用。允许在 `deadlock.c` 内使用 `static` 辅助函数。

[SPECIFICATION]

**deadlock_on_acquire**

- **Pre-Condition**：`sem` 是互斥量，本次 `down` 或 `try_down` 已经把 `value` 减 1，调用方持有关中断后的临界区。
- **Post-Condition**：未发现违规时 `sem->owner` 等于 `current->pid`。
- **Case 1**：`owner` 为 0。把 `owner` 设为 `current->pid`。
- **Case 2**：`owner` 等于 `current->pid`。这是重入。打印包含 pid 和 “reenter” 的一行，调用报告函数，不改 `owner`。
- **Case 3**：`owner` 是另一个非 0 pid。这是多个进程进入同一临界区。打印包含两个 pid 和 “multi-enter” 的一行，调用报告函数，不改 `owner`。

**deadlock_on_wait**

- **Pre-Condition**：`sem->value == 0`，`current` 尚未挂上 `sem->wait_queue`。
- **Post-Condition**：返回 1 时没有等待边被留下，调用方不睡眠。返回 0 时，若这是互斥量且 `owner` 非 0，则记下一条“`current` 等待 `owner`”的边。
- **Case 1**：不是互斥量，或 `owner` 为 0。返回 0，不记边。
- **Case 2**：`owner` 等于 `current->pid`。这是自己等待自己，按重入报告，删除本进程相关的等待边，返回 1。
- **Case 3**：`owner` 是其他进程。记下等待边后，若等待边组成环，则打印环上的 pid，并说明已观察到互斥（`is_mutex`）、持有并等待（当前进程的 `owner` 身份存在于别的互斥量上，或环长大于 1）、不可抢占（`up` 之前不会抢走 `owner`）、环形等待。然后删除本进程刚刚记下的边，调用报告函数，返回 1。没有环则返回 0。
- **Requirements**：等待边用定长数组保存，容量至少 32，不得按 `MAX_PROCESS` 分配邻接矩阵。数组满时打印 “deadlock table full” 并按发现事件处理（报告函数，返回 1）。

**deadlock_on_release**

- **Pre-Condition**：调用方即将执行 `__up` 的等待队列或 `value++` 逻辑。
- **Post-Condition**：若 `is_mutex` 为 1 且 `owner` 等于 `current->pid`，则 `owner` 变为 0，并删掉所有 holder 为该 pid 且信号量是 `sem` 的等待边。不是互斥量，或 `owner` 不是 `current` 时，不改 `owner`。

**deadlock_check**

- **Pre-Condition**：由 `init_main` 调用，中断已经开启，`current` 是 `initproc`。报告函数已被换成只记录事件、不调用 `kmonitor` 的函数。
- **Post-Condition**：返回前报告函数恢复为默认的 `kmonitor` 路径。本函数创建的内核线程都已退出并被 `do_wait` 回收。没有进程睡在自检用的信号量上。串口至少出现一行自检通过的信息，包含 “deadlock check”。
- **Case 1（重入）**：在 `initproc` 上对一个互斥量 `down` 两次。第二次必须走 `deadlock_on_wait` 的 Case 2 并返回，不得睡眠。然后 `up` 一次，使 `owner` 回到 0、`value` 回到 1。
- **Case 2（环形等待）**：两个内核线程和两个互斥量 A、B。线程 1 取得 A 后让出 CPU；线程 2 取得 B 后让出 CPU；线程 1 再 `down(B)`，此时没有环，必须真的睡眠；线程 2 再 `down(A)`，必须查出环、不睡眠。线程 2 随后 `up(B)` 唤醒线程 1，两边各自释放自己持有的互斥量后返回。
- **Requirements**：自检失败时 `panic`，不要调用 `kmonitor`。不得改动哲学家就餐用的信号量。
````

**Challenge 2：参考 Linux 的 RCU 机制，在 ucore 中实现简化的 RCU 机制**

````markdown
[PROMPT]
**任务**：在单 hart 的 ucore 上实现简化的 QSBR 型 RCU：读者用 `rcu_read_lock` / `rcu_read_unlock` 标记读端临界区，写者发布新数据后调用 `synchronize_rcu`，等到每个其他进程都经过一次静止状态再返回。静止状态定义为：该进程的读端嵌套深度为 0，并且要么正睡在调度之外，要么在一次上下文切换时被换出。读端临界区内不许睡眠，也不许调用 `synchronize_rcu`。提供一个会在启动时跑完的自检。

**操作要求**：你必须在项目中修改实际文件。新增 `kern/sync/rcu.h` 和 `kern/sync/rcu.c`。在 `struct proc_struct` 中增加本模块的两个字段，并在 `alloc_proc()` 里初始化。在 `proc_run()` 调用 `switch_to` 之前通知 RCU。在 `init_main()` 里、`deadlock_check()` 之后、`check_sync()` 之前调用 `rcu_check()`。不要改 `check_sync()`、调度类和信号量语义。注释使用英文。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 的信息，以工程中的真实代码为准。不要实现 `call_rcu` 回调队列，不要做每 CPU 的 `rcu_data`。

[RELY]

- 进程控制块在 `kern/process/proc.h` 的 `struct proc_struct`。`alloc_proc()` 在 `kern/process/proc.c`，`kmalloc` 不清零，新增字段必须在这里赋值。进程状态包含 `PROC_RUNNABLE`、`PROC_SLEEPING`、`PROC_ZOMBIE`。全局有 `current`、`idleproc`、`proc_list`。遍历用 `list_next` 和 `le2proc(le, list_link)`，跳过 `proc_list` 的头结点。
- `proc_run(struct proc_struct *proc)` 在切换前把 `current` 改成 `proc`，局部变量 `prev` 是被换出的进程。上下文切换函数是 `switch_to`。
- 让出 CPU：`schedule()`。`current->need_resched = 1` 后调用 `schedule()` 会选下一个进程。内核线程用 `kernel_thread` 创建，函数返回后进入 `do_exit`。父进程用 `do_wait(pid, NULL)` 回收；返回非 0 时先 `schedule()` 再等。
- 单 hart：读端临界区不会被另一个 hart 并行执行，但会被 `schedule()` 换出。因此宽限期必须在换出点观察读端深度，不能假设“调用 `synchronize_rcu` 时其他进程都已经不在读端”。

[GUARANTEE]

- `struct proc_struct` 增加 `int rcu_read_depth` 和 `unsigned rcu_gp_seen`。`alloc_proc()` 把二者设为 0。
- 下列接口声明在 `rcu.h`：

```c
void rcu_read_lock(void);
void rcu_read_unlock(void);
void rcu_note_context_switch(struct proc_struct *prev);
void synchronize_rcu(void);
void rcu_check(void);
```

- `proc_run()` 在 `switch_to` 之前调用 `rcu_note_context_switch(prev)`。
- `rcu.c` 内用一个 `static unsigned rcu_gp`，初值 0。允许 `static` 辅助函数。不新增系统调用。

[SPECIFICATION]

**rcu_read_lock**

- **Pre-Condition**：`current` 非空。
- **Post-Condition**：`current->rcu_read_depth` 比调用前大 1。可以嵌套。

**rcu_read_unlock**

- **Pre-Condition**：`current->rcu_read_depth > 0`。深度已经是 0 时 `panic`。
- **Post-Condition**：深度减 1。减到 0 时把 `current->rcu_gp_seen` 设为当前的 `rcu_gp`，表示该进程对目前已经开始的宽限期处于静止状态。

**rcu_note_context_switch**

- **Pre-Condition**：`prev` 是即将被换出的进程，由 `proc_run` 在 `switch_to` 之前调用。
- **Post-Condition**：仅当 `prev->rcu_read_depth == 0` 时，把 `prev->rcu_gp_seen` 设为当前 `rcu_gp`。深度大于 0 时不修改 `rcu_gp_seen`。

**synchronize_rcu**

- **Pre-Condition**：`current` 非空，且 `current->rcu_read_depth == 0`。深度大于 0 时打印 “synchronize_rcu in read-side” 并立即返回，不得递增 `rcu_gp`。
- **Post-Condition**：返回时，除 `current` 和 `PROC_ZOMBIE` 以外，`proc_list` 上的每个进程以及 `idleproc` 的 `rcu_gp_seen` 都不小于本次宽限期编号。`current` 自己的 `rcu_gp_seen` 也设为这个编号。
- **Case 1**：把 `rcu_gp` 加 1，得到本次目标编号 `target`。
- **Case 2**：扫描 `idleproc` 和 `proc_list`。跳过 `current` 和 `PROC_ZOMBIE`。若进程的 `rcu_read_depth == 0` 且状态是 `PROC_SLEEPING`，直接把它的 `rcu_gp_seen` 写成 `target`（它不在读端，也不占着 CPU）。若进程 `rcu_read_depth > 0` 且状态是 `PROC_SLEEPING`，打印 “sleep in rcu read-side” 并 `panic`。
- **Case 3**：若仍有进程的 `rcu_gp_seen < target`，置 `current->need_resched = 1` 并调用 `schedule()`，然后回到 Case 2。不得忙等。
- **Requirements**：不在本函数里释放调用者的旧对象。旧对象在本函数返回之后才可以由调用者释放。

**rcu_check**

- **Pre-Condition**：由 `init_main` 调用，此时中断已开。调用返回后 `nr_process` 与进入前相同，`check_sync()` 仍能按原样运行。
- **Post-Condition**：串口有一行包含 “rcu check ok”。自检创建的内核线程已被 `do_wait` 回收。
- **Case 1**：有一个全局指针，初始指向对象 A。读者线程 `rcu_read_lock` 后读到 A，置一个“读者已进入”的标志，再 `schedule()` 让出 CPU，醒回后仍能读到 A，然后 `rcu_read_unlock` 并返回。写者线程等到该标志后把指针改到对象 B，调用 `synchronize_rcu`，返回之后才把对象 A 写成哨兵值。读者在持锁期间观察到的值不得是哨兵值。写者在读者尚未 `rcu_read_unlock` 时不得从 `synchronize_rcu` 返回：读者让出 CPU 之后、解锁之前，写者完成标志仍为 0。
- **Requirements**：自检失败 `panic`。不要调用 `kmonitor`。
````

### 2.3 2411622-张雨函

以下提示词按依赖关系使用：先完成条件变量，再使用条件变量完成管程版哲学家就餐，最后整理信号量、等待队列和调度机制的理解题。三个提示词只覆盖 Lab7 基础任务，不涉及 Challenge 1 和 Challenge 2。

**功能模块一：条件变量功能**

~~~markdown
[PROMPT]
**任务**：修改 kern/sync/monitor.c，在 //lab7 function1 标记处实现 cond_wait() 和 cond_signal()。使用 monitor_t 的 mutex、next、next_count 和 condvar_t 的 sem、count、owner，实现 Hoare 风格条件变量。保留 monitor_init()、monitor_free() 和现有数据结构，不修改 sem.c、wait.c、哲学家代码和 Challenge。

**操作要求**：必须直接修改实际文件。先依据 monitor.h、sem.h、wait.h、sem.c 的真实定义设计；不得忙等待、不得直接维护等待队列、不得新增系统调用、不得改变死锁探测钩子语义。

**输出要求**：补全代码并说明空 signal、多个等待者、唤醒和通知者交接的行为。

[RELY]
condvar_t 的 sem 初值为 0，count 表示等待者数量，owner 指向所属管程。monitor_t 的 mutex 初值为 1，next 初值为 0，next_count 表示等待 next 的通知者数量。up() 和 down() 已封装睡眠、唤醒和调度。

[GUARANTEE]
实现：
void cond_wait(condvar_t *cvp);
void cond_signal(condvar_t *cvp);
不得修改结构体和公开接口。

[SPECIFICATION]
cond_wait 的前置条件是当前线程持有所属管程。它必须先增加 count，再根据 next_count 通过 next 或 mutex 释放管程，然后在 cvp->sem 上阻塞；被唤醒后减少 count 并返回，不能无条件重新获取 mutex。cond_signal 在 count 为 0 时必须完全不改变状态；count 大于 0 时只能唤醒一个等待者，增加 next_count，在 next 上等待被唤醒线程交还管程后恢复并减少 next_count。不得忙等待，释放管程和进入等待之间不得出现丢失通知窗口。
~~~

**功能模块二：基于管程的哲学家就餐功能**

~~~markdown
[PROMPT]
**任务**：修改 kern/sync/check_sync.c，在 //lab7 function2 标记处实现 phi_test_condvar()、phi_take_forks_condvar() 和 phi_put_forks_condvar()，用 mtp->mutex 和 mtp->cv[i] 完成五名哲学家的管程同步。保留已有信号量版本、线程函数、check_sync() 和测试注入接口，不实现 Challenge。

**操作要求**：先阅读 phi_test_sema()、phi_take_forks_sema()、phi_put_forks_sema()。保持函数名、参数和 static 属性。所有 state_condvar[] 的读写必须受管程保护；条件不满足时必须 cond_wait()，不得忙等待。退出管程时 next_count 大于 0 则 up(next)，否则 up(mutex)。

**输出要求**：直接修改实际文件，说明状态转换、资源判定和左右邻居唤醒过程，并通过 tools/test_lab7.c 验证。

[RELY]
N 为 5；LEFT、RIGHT 宏依赖参数 i；状态为 THINKING、HUNGRY、EATING。state_condvar[N] 是共享状态，mtp->cv[i] 是哲学家 i 的条件变量。只有自身 HUNGRY 且左右邻居都不是 EATING 时才能进餐。

[GUARANTEE]
实现：
static void phi_test_condvar(int i);
static void phi_take_forks_condvar(int i);
static void phi_put_forks_condvar(int i);
允许增加本文件内的 static 管程退出辅助函数，不得新增全局同步原语。

[SPECIFICATION]
phi_test_condvar 必须在管程内检查自身为 HUNGRY 且两侧非 EATING，先写入 EATING 再 signal(cv[i])；否则不改变状态。phi_take_forks_condvar 必须进入管程、设为 HUNGRY、测试条件，失败时在 cv[i] 上阻塞，唤醒后以 EATING 状态退出管程。phi_put_forks_condvar 必须设自身为 THINKING，并在同一管程内依次检查 LEFT 和 RIGHT。相邻哲学家不得同时 EATING，等待不得通过轮询完成。
~~~

**功能模块三：内核同步基础设施理解题**

~~~markdown
[PROMPT]
**任务**：不修改内核代码，阅读 kern/sync/sem.h、sem.c、wait.h、wait.c、kern/schedule/sched.c 和进程状态定义，完成可放入 docs/report.md 的同步基础设施分析。说明线程 A 在 down(&sem) 资源不可用时阻塞、线程 B 执行 up(&sem)、线程 A 最终重新获得 CPU 并继续执行的完整过程。不要实现 Challenge 1/2。

**操作要求**：以当前仓库真实函数签名和控制流为准。当前接口是 wait_current_set(queue, wait, wait_state)、wakeup_wait(queue, wait, wakeup_flags, del)、wakeup_first(queue, wakeup_flags, del)；若教材原型不同，以当前头文件为准。不得修改 sem.c、wait.c 和调度器。

**输出要求**：按数据结构、阻塞路径、唤醒路径、重新调度、接口对应关系和并发保证组织说明，必须给出完整时间线，并解释关中断如何避免丢失唤醒。

[RELY]
semaphore_t 的基础字段是 value 和 wait_queue；is_mutex、owner 属于 Challenge 扩展。__down() 在关中断区检查 value，资源不足时调用 wait_current_set() 设置 PROC_SLEEPING、入队并在恢复中断后 schedule()。__up() 在关中断区取队首并调用 wakeup_wait()；wakeup_proc() 只使目标线程可运行，线程仍需等待调度器选择。__down() 从 schedule() 返回后调用 wait_current_del() 并检查 wakeup_flags。

[GUARANTEE]
报告必须说明 semaphore_t、wait_queue_t、wait_t、down()、up()、try_down()、wakeup_proc()、schedule() 的职责，并给出线程 A 的完整阻塞到恢复时序；不修改内核实现。

[SPECIFICATION]
资源可用时 down() 在关中断区减一并返回；资源不可用时必须先在关中断区入队并设置睡眠状态，再开中断和调度。up() 无等待者时增加 value，有等待者时移除并唤醒队首且不再增加 value。必须区分唤醒和获得 CPU，说明 local_intr_save() 覆盖检查到入队区间以防止 B 在 A 入队前错过等待者。try_down() 失败时返回 false，不入队、不睡眠、不调度。
~~~
