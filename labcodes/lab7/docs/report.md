# 操作系统实验报告

## 实验基本信息

|     项目     |                      内容                      |
| :----------: | :--------------------------------------------: |
| **实验名称** |                 Lab7：同步互斥                 |
| **小组成员** | 2414007-王奕瑜、2414015-鲁昕宁、2411622-张雨函 |
| **完成日期** |                   2026-12-13                   |

### 小组分工

练习如何分工？

|      成员      |                                                      负责的练习/模块                                                       |
| :------------: | :------------------------------------------------------------------------------------------------------------------------: |
| 2414007-王奕瑜 |                                                             -                                                              |
| 2414015-鲁昕宁 | Challenge 1：在 ucore 中实现简化的死锁和重入探测机制、Challenge 2 ：参考 Linux 的 RCU 机制，在 ucore 中实现简化的 RCU 机制 |
| 2411622-张雨函 |                功能模块一：条件变量功能、功能模块二：基于管程的哲学家就餐功能、功能模块三：内核同步基础设施                |

实验报告如何分工？

|      成员      |     负责的实验报告内容     |
| :------------: | :------------------------: |
| 2414007-王奕瑜 |         一, 五, 六         |
| 2414015-鲁昕宁 |   3.2.3, 3.2.4, 4.4, 4.5   |
| 2411622-张雨函 | 3.1, 3.2.1, 3.2.2, 4.1-4.3 |

---

## 一、实验目的

本实验的主要目的是：

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

#### 3.2.1 功能模块一：条件变量功能

---

#### 3.2.2 功能模块二：基于管程的哲学家就餐功能

---

#### 3.2.3 Challenge 1：在 ucore 中实现简化的死锁和重入探测机制

前面的模块解决的是「如何用信号量和管程让多个线程有序协作」；Challenge 1 关心的是**协作是否已越界**——既包括进程之间的资源僵局，也包括同一执行流对互斥约定的破坏。二者在教材里常并列讨论，但指向不同层次的现象，本实验的探测器需要同时覆盖。

**死锁：多执行流之间的循环等待**

死锁指多个线程或进程各自占有部分资源，又都在等待对方释放资源，形成**循环等待**，相关执行流都无法继续推进。经典 Coffman 四条件为：**互斥**（资源一次只被一个占用）、**占有并等待**（持有一部分资源的同时等待更多）、**不可剥夺**（只能由持有者主动释放）、**循环等待**（等待关系成环）。四者同时成立时，系统可能永久僵持；哲学家就餐若拿不到叉子的进程一直阻塞，就是需要靠同步协议避免死锁的典型场景。

**重入：两个层次上的含义**

1. **可重入函数（reentrant function）** 指函数在**嵌套调用或并发/中断上下文**中再次被调用时，仍不因共享可变状态而出错。可重入通常要求少依赖全局静态变量、临界区数据尽量落在栈上；`strtok` 因内部静态状态而不可重入，`strtok_r` 用调用者提供的缓冲区因而可重入。  
2. **可重入 ≠ 线程安全。** 线程安全强调多线程并发不出错，很多实现靠互斥锁达到，这样的函数往往**不可重入**——在同一线程或信号处理函数里嵌套调用时，会再次申请同一把**非递归**互斥锁，造成**自死锁**。

本实验所说的「**重入探测**」，针对的是互斥信号量语义上的违规：当前执行流在未 `up` 的情况下再次对**同一把互斥量** `down`，或在已持有该锁时又睡眠等待该锁。这与「函数是否可重入」不是同一个定义，但属于同一类根因——**普通互斥锁不支持同一线程重复获取**时，递归拿锁、函数 A 持锁调用 B、信号/中断里再抢同一把锁，都会把「重入问题」演化成死锁或互斥破坏。Linux 用户态的递归互斥量（如 `PTHREAD_MUTEX_RECURSIVE`）只是避免同一线程计数加锁时的自阻塞，**并不**自动使函数变成可重入函数。

| 项目 | 死锁（本实验：环形等待） | 函数可重入性 | 本实验：互斥量重入 |
| ---- | ------------------------ | ------------ | ------------------ |
| 对象 | 多进程/多线程之间的资源关系 | 函数能否安全嵌套、中断调用 | 同一进程对同一互斥量的二次进入 |
| 本质 | 等待有向图成环 | 是否使用不可保护的共享可变全局状态 | 违反「至多一个持有者」 |
| 典型后果 | 多方永久阻塞 | 数据错乱或崩溃 | 自等待或互斥失效 |
| 典型例子 | 持 A 等 B，持 B 等 A | `strtok` / `strtok_r` | `initproc` 对同一 `sem` 连续两次 `down` |

**本挑战在原理上做什么**

思路仍是**观测而不改写** `down` / `up` 语义：只针对初值为 1 的互斥量，在取得、释放和即将睡眠时记录持有者，并区分三类情形——（1）同进程再次进入已持有的互斥量，对应自死锁/重入类风险；（2）两个不同 pid 同时被认为持有，对应互斥语义被破坏；（3）等待边闭合成环，对应 Coffman 中的循环等待，并在报告中点明与本实验相关的互斥、持有并等待、不可剥夺（由持有者 `up`）、环形等待。计数信号量、哲学家叉子与条件变量内部的信号量不参与建图，以免把正常多等待误判为「多进临界区」。

Challenge 1 因而接在管程与哲学家实现之后：后者回答**怎样协作**，前者回答**协作时锁的使用是否已出现死锁条件或互斥重入**——与 RCU、管程分属不同同步议题。

---

#### 3.2.4 Challenge 2 ：参考 Linux 的 RCU 机制，在 ucore 中实现简化的 RCU 机制

Challenge 1 审视的是**锁与等待图**；RCU（Read-Copy-Update，读–复制–更新）面向的是另一类场景：**读多写少**的内核数据结构。设计目标是让读路径开销极低、尽量不阻塞读者，把同步代价主要放在写者一侧，并用**延迟回收**换取读侧的简洁与可扩展性。它不替代普通互斥锁，而是与锁解决不同维度的问题——典型用于路由表、进程凭证、哈希表与文件系统缓存等读远多于写的结构。

**读–复制–更新的三步语义**

1. **读（Read）。** 读者用 `rcu_read_lock()` / `rcu_read_unlock()` 标出读临界区；在该区间内访问共享指针指向的对象。读路径一般不阻塞写者，二者可以并发；读者在发布新版本之后仍可能合法地看到**旧版本**——这正是多版本并存的前提。Linux 还用 `rcu_dereference()` 以正确的内存序取得受保护的指针。
2. **复制（Copy）。** 写者**不在原地修改**仍可能被读者引用的共享对象，而是在副本上完成修改；多个写者之间通常需要另加互斥（如自旋锁）串行化，避免同时破坏同一份结构。
3. **更新与回收（Update + Reclaim）。** 写者通过原子地替换全局指针（Linux 中为 `rcu_assign_pointer`）**发布**新版本，此后新进入读临界区的读者看到新对象；早已进入读临界区、或通过本地副本继续持有旧指针的读者仍可使用旧对象。**移除**（对外不再可见）与 **free**（物理回收）是两步，中间隔着**宽限期（grace period）**：`synchronize_rcu()` 等待所有在替换之前已进入读临界区、且仍可能持有旧指针的读者**全部退出**读临界区；宽限期结束，旧版本不再被引用，写者才可安全释放。`call_rcu()` 则把回收登记为回调，使写路径不必阻塞在同步等待上。

**关键概念与权衡**

- **宽限期**等待的不是「全系统没有任何读者」，而是「不可能再引用旧版本的那批读者」已结束读临界区。
- **多版本**允许同一逻辑时刻存在新旧两个一致快照；读者始终看到某一版本的内部一致性，而非写了一半的半成品。
- **与读写锁对比**：读写锁在写时会阻塞或排斥读者；RCU 写者不阻塞读者，代价是写逻辑更重、回收滞后，内存占用会短暂升高，且不适合写极频繁的场景。
- **优点**：读侧轻量，多核下缓存行为友好，读者不被写者挡住。
- **缺点**：写路径复杂，需理解内存序与宽限期；不能单靠 RCU 解决多写者竞争，也不能直接当作通用锁使用。

**本实验在原理上的落点**

完整 Linux RCU 按 CPU 维护读端状态，宽限期算法要处理跨核、中断与抢占。ucore 实验环境只有**单 hart**，读临界区不会被另一核对同一数据结构并行执行，但进程仍会被**调度换出**。因此宽限期不能简化为「调用 `synchronize_rcu` 的瞬间无人持读锁」：读者可能在持读端标记时被抢占，旧对象在逻辑上仍被该读者引用。本实验采用 **QSBR（Quiescent-State-Based Reclamation）**：把「静止状态」定义为读端嵌套深度为零，且要么已睡眠在调度之外，要么在上下文切换时被判定为不在读临界区内；写者开启新一轮宽限期，等到各相关执行流都报告静止后再返回，由调用者在返回之后自行回收旧对象（本实验不实现 `call_rcu` 回调队列）。读端内禁止睡眠，否则无法把「仍可能在读旧数据」与「已静止」区分开。

从实验主线看，RCU 接在死锁探测之后，意在说明：**lab7 除管程与信号量外，还应理解一种以指针发布与宽限期为核心的读–写同步模型**，以及单 CPU 下抢占如何改变「何时算作安全回收」的判定。

---

## 四、实验内容与实现

### 4.1 功能模块一：条件变量功能

**负责人：** [2411622-张雨函]

#### 4.1.1 模块功能描述

#### 4.1.2 最终提示词

#### 4.1.3 实现迭代过程

---

### 4.2 功能模块二：基于管程的哲学家就餐功能

**负责人：** [2411622-张雨函]

#### 4.2.1 模块功能描述

#### 4.2.2 最终提示词

#### 4.2.3 实现迭代过程

---

### 4.3 功能模块三：内核同步基础设施

**负责人：** [2411622-张雨函]

> 阅读实验框架提供的信号量和等待队列相关代码。假设线程 A 执行 `down(&sem)` 时信号量资源不可用，线程 A 因此进入等待状态；随后线程 B 执行 `up(&sem)`。
>
> 请分析这一过程中：
>
> 1.  `semaphore_t` 在其中承担什么作用？
> 2.  `wait_queue_t` 在其中承担什么作用？
> 3.  `down()` 为什么可能导致线程阻塞？
> 4.  `up()` 为什么可能导致等待线程被唤醒？
> 5.  被唤醒的线程为什么还需要经过调度才能继续运行？
>
> 请结合以下接口进行说明：
>
> ```c
> void sem_init(semaphore_t *sem, int value);
> void up(semaphore_t *sem);
> void down(semaphore_t *sem);
> bool try_down(semaphore_t *sem);
>
> void wait_queue_init(wait_queue_t *queue);
> void wait_current_set(wait_queue_t *queue, wait_t *wait);
> void wakeup_wait(wait_t *wait, uint32_t wakeup_flags);
> void wakeup_first(wait_queue_t *queue);
> ```
>
> **要求：**
>
> 本题不要求修改实验框架代码。重点说明**信号量、等待队列、线程唤醒和调度之间的关系**，并能够根据函数接口解释一次线程从阻塞到重新获得 CPU 的完整过程。

---

### 4.4 Challenge 1：在 ucore 中实现简化的死锁和重入探测机制

**负责人：** [2414015-鲁昕宁]

#### 4.4.1 模块功能描述

**原理与实验探测的对应**

教材中的死锁强调多执行流之间的**循环等待**；函数可重入性强调**嵌套/中断下不出错**。本模块把二者在「锁使用不当」这一交叉点上落地为三类可观测事件：同一 `pid` 在未释放时又 `down` 同一互斥量（报告 reenter，对应自死锁/互斥重入）；两个 `pid` 同时进入同一互斥临界区（报告 multi-enter）；睡眠前形成的 waiter→holder 边闭合成环（报告 deadlock cycle，并打印 Coffman 相关条件）。默认发现后进入 `kmonitor` 便于人工诊断；这与哲学家就餐里用协议**预防**死锁不同，本模块侧重**运行中报告**。

| 概念 | 本实验中的体现 |
| ---- | -------------- |
| 互斥 | 只跟踪 `sem_init(sem, 1)`，`owner` 记录持有者 |
| 占有并等待 | 边上 waiter 已运行、holder 仍占锁 |
| 不可剥夺 | 约定只有持有者的 `up` 释放，环检测文案中说明 |
| 循环等待 | `dl_find_cycle` 沿等待边找环 |
| 互斥重入（非「可重入函数」） | `owner == current` 时再次 acquire 或 wait |

**需要新增与修改的文件：**

| 路径 | 作用 |
| ---- | ---- |
| `kern/sync/deadlock.h` | 声明 `deadlock_on_acquire`、`deadlock_on_wait`、`deadlock_on_release`、`deadlock_check` |
| `kern/sync/deadlock.c` | 等待边表、环检测、默认报告与启动自检 |
| `kern/sync/sem.h` | `semaphore_t` 增加 `is_mutex`、`owner` |
| `kern/sync/sem.c` | 在 `__down` / `__up` / `try_down` 临界区内调用上述钩子 |
| `kern/process/proc.c` | `init_main()` 在 `check_sync()` 前调用 `deadlock_check()` |

**探测范围：** 仅 `sem_init(sem, 1)` 得到的互斥量。初值不是 1 的信号量（哲学家叉子计数、条件变量内部 `sem`）`is_mutex` 为 0，整条探测路径直接返回。

**三类事件：**

1. **重入。** 互斥量 `owner` 已是 `current->pid` 时再次成功 `down`，或 `value == 0` 时发现自己要等的正是自己持有的锁。
2. **多进程进入同一临界区。** 互斥量已被别的 pid 持有时仍走成功 `down` 路径（例如 `try_down` 或逻辑错误），`deadlock_on_acquire` 打印持有者与当前 pid。
3. **环形等待。** 在准备睡眠前插入一条边「`current` 等待 `owner`」，沿 waiter→holder 追链；若闭合成环，打印环上 pid 以及互斥、持有并等待、不可抢占、环形等待四条条件，**不**把当前边留在表里，并拒绝本次睡眠。

**默认处理：** `dl_default_report` 打印 `deadlock report: ...`，开中断后调用 `kmonitor(NULL)`，便于在 monitor 里查看现场。启动路径上的 `deadlock_check()` 把 `dl_report` 换成只设标志、不进 monitor 的 `dl_test_report`。

**等待边存储：** 定长数组 `dl_edges[32]`，每项为 `{waiter, holder, sem}`。释放互斥量时按 holder 与 sem 指针删掉相关边；重入或环检测失败时按 pid 批量删除。表满时报告并拒绝睡眠。

#### 4.4.2 最终提示词

以下是用于生成简化死锁与重入探测的提示词：

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

#### 4.4.3 实现迭代过程

**1. 互斥量字段与钩子挂接**

`sem_init` 根据初值设置 `is_mutex = (value == 1)`、`owner = 0`。`__down` 在 `value--` 之后调用 `deadlock_on_acquire`；若 `value == 0`，先调 `deadlock_on_wait`，返回 1 则恢复中断并返回 0，**不**调用 `wait_current_set`。`__up` 在唤醒或 `value++` 之前调用 `deadlock_on_release`：当前持有者对互斥量 `up` 时清 `owner` 并删以该 pid 为 holder 且 sem 指针匹配的边。`try_down` 成功路径同样调用 `deadlock_on_acquire`。

**2. 三类检测的具体行为**

- `deadlock_on_acquire`：`owner == 0` 时写入 `current->pid`；等于自己时报告 reenter；为其他 pid 时报告 multi-enter，均不改错乱的 `owner`。
- `deadlock_on_wait`：`owner == current` 时删本 pid 相关边、报告 reenter、返回 1；否则追加边并用 `dl_find_cycle` 从 `current->pid` 出发找环。成环则删掉刚加的边、打印 Coffman 相关条件行、报告 `deadlock cycle`、返回 1。
- `deadlock_on_release`：仅当 `owner == current` 时清 owner 并删边。

**3. 启动自检 `deadlock_check`**

- **重入用例：** 在 `initproc` 上对栈上互斥量 `once` 连续两次 `down`，第二次必须触发检测且不睡眠；一次 `up` 后 `owner == 0`、`value == 1`。
- **环用例：** 静态 `dl_sem_a`、`dl_sem_b`，线程 1 持 A 后等 B 入睡，线程 2 持 B 后对 A 的 `down` 应发现环且不睡眠，再 `up(B)` 唤醒线程 1，两边释放后父进程 `do_wait` 回收。串口出现 `deadlock check ok` 表示通过。

**4. 实现中需注意的局限**

`__up` 在有等待者时会把信号量直接交给被唤醒进程，且该路径不再调用 `deadlock_on_acquire`，因此刚被唤醒的线程进入临界区时 `owner` 可能仍为 0，直到它自己 `up` 时按普通 `value++` 处理。这是保持原信号量语义下的可接受盲区；环检测用例依赖的是「睡眠前」的边与 owner，不依赖唤醒后的 owner 立即更新。

**5. 验证**

在 WSL2 下 `make` 通过后，用 QEMU 启动内核，串口依次出现 `deadlock check ok`、`rcu check ok`，随后两组哲学家正常结束，说明探测未误伤管程版哲学家。

---

### 4.5 Challenge 2：参考 Linux 的 RCU 机制，在 ucore 中实现简化的 RCU 机制

**负责人：** [2414015-鲁昕宁]

#### 4.5.1 模块功能描述

**原理与实验范围的对应关系**

对照 Linux RCU 的读–复制–更新流程，本实验实现其中的骨架：读端用 `rcu_read_lock` / `rcu_read_unlock` 标临界区；写者在副本或新对象上完成修改后，通过改写全局指针完成**发布**，再调用 `synchronize_rcu` 等待宽限期。未实现 `rcu_dereference`、`rcu_assign_pointer` 的内存屏障封装、每 CPU `rcu_data` 与 `call_rcu` 异步回收，写者以同步方式等待宽限期结束后再改写或释放旧对象（自检里用哨兵值 `-1` 表示「已可安全复用」）。

| Linux RCU 概念 | 本实验中的体现 |
| -------------- | -------------- |
| 读临界区 | `rcu_read_lock` / `rcu_read_unlock`，PCB 上嵌套深度 |
| 发布新版本 | 写者修改共享指针（如 `rcu_shared` 指向新对象） |
| 宽限期 | 全局 `rcu_gp` 递增，`synchronize_rcu` 等待各进程静止 |
| 静止状态 | 读端深度 0 且睡眠或经上下文切换观测 |
| 延迟回收 | 宽限期返回后写者再写哨兵或 `free`（无 `call_rcu`） |

**设计目标（相对 Linux RCU 的简化）：**

单 hart 下采用 QSBR：写者递增全局宽限期编号，等每个相关进程的 `rcu_gp_seen` 达到本轮编号后，认为没有执行流仍依赖旧数据。

**需要新增与修改的文件：**

| 路径 | 作用 |
| ---- | ---- |
| `kern/sync/rcu.h` | 读锁、宽限期同步、切换通知、自检接口 |
| `kern/sync/rcu.c` | `rcu_gp`、扫描与 `synchronize_rcu`、`rcu_check` |
| `kern/process/proc.h` / `proc.c` | PCB 字段与 `alloc_proc` 初始化、`proc_run` 钩子、`init_main` 调用顺序 |
| `kern/schedule/sched.c` / `sched.h` | `sched_switch_idle()`，为 `idleproc` 补静止状态 |

**读端 API：**

- `rcu_read_lock()`：在关中断下给 `current->rcu_read_depth` 加 1，防止加深度过程中被换出并误报静止。
- `rcu_read_unlock()`：深度减 1；减到 0 时把 `rcu_gp_seen` 设为当前 `rcu_gp`。
- 读端内禁止睡眠；若 `synchronize_rcu` 在读端内被调用，只打印提示并返回，**不**递增 `rcu_gp`。

**写端 API：**

- `synchronize_rcu()`：`target = ++rcu_gp`，扫描 `idleproc` 与 `proc_list`（跳过 `current` 与 `PROC_ZOMBIE`）。`PROC_SLEEPING` 且深度为 0 的进程直接写入 `rcu_gp_seen = target`；深度大于 0 却睡眠则 `panic`。仍有进程未过关则 `schedule()` 循环，禁止忙等。返回前把 `current->rcu_gp_seen` 设为 `target`。旧对象的释放由调用者在返回之后完成。

**静止状态的两种观测点：**

1. 进程调用 `rcu_read_unlock()` 且嵌套深度归零；
2. `proc_run()` 在 `switch_to` 前调用 `rcu_note_context_switch(prev)`，仅当 `prev->rcu_read_depth == 0` 时更新 `prev->rcu_gp_seen`。

单 hart 下，持锁读者被换出时深度仍大于 0，**不会**在换出点盖章；写者必须等到该读者再次运行并解锁。

#### 4.5.2 最终提示词

以下是用于生成简化 RCU 的提示词：

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

#### 4.5.3 实现迭代过程

**1. PCB 与切换钩子**

在 `struct proc_struct` 末尾增加 `rcu_read_depth`、`rcu_gp_seen`。`alloc_proc()` 将二者置 0，避免 `kmalloc` 残留值让宽限期提前结束。`proc_run()` 在写入 `satp`、刷新 TLB 之后、`switch_to` 之前调用 `rcu_note_context_switch(prev)`，与提示词中的「换出点观测深度」一致。

**2. `synchronize_rcu` 的扫描与等待**

`rcu_gp_pending()` 遍历 `idleproc` 和 `proc_list`。已睡眠且不在读端的进程视为静止，当场把 `rcu_gp_seen` 提到 `target`。仍有进程 `rcu_gp_seen < target` 时，在关中断区内记下状态后开中断、调用 `schedule()`，让读者有机会运行到 `rcu_read_unlock()`。

**3. `idleproc` 与 `sched_switch_idle`**

`idleproc` 不在 `proc_list` 上。当 `schedule()` 把除 `idleproc` 外的可运行进程都放进运行队列后，若只有写者或读者在转，调度器往往不会切换到 `idleproc`，其 `rcu_gp_seen` 会长期落后。实现上在每次 `schedule()` 之后，若 `idleproc` 仍未达到 `target` 且读端深度为 0，调用 `sched_switch_idle()`：把当前可运行进程重新入队，置 `idleproc->need_resched`，经 `proc_run(idleproc)` 切到 idle 再切回，从而让 idle 也经历一次带 `rcu_note_context_switch` 的换出。

**4. 启动自检 `rcu_check`**

全局指针 `rcu_shared` 初始指向 `rcu_obj_a`。读者线程：`rcu_read_lock`，读取 A，置 `rcu_reader_entered`，`schedule()` 让出 CPU；醒来后确认本地仍读到有效值、且 `rcu_writer_done == 0`，再解锁。写者线程：等到 `rcu_reader_entered`，把 `rcu_shared` 改为 `&rcu_obj_b`，调用 `synchronize_rcu()`（此时读者尚未解锁，宽限期不得结束），返回后将 `rcu_obj_a` 写成哨兵 `-1` 并置 `rcu_writer_done`。父进程 `do_wait` 回收两线程后打印 `rcu check ok`。

该用例同时覆盖三件事：读者持锁期间被抢占仍能看到旧对象；写者在读者解锁前不能结束宽限期；`synchronize_rcu` 返回后才允许改写 A。

**5. 与 Linux RCU 的差异（报告设计说明）**

| 项目 | Linux 内核 RCU（典型） | 本实验实现 |
| ---- | ---------------------- | ---------- |
| CPU 规模 | 每 CPU `rcu_data`，跨核宽限期 | 单 hart，进程级 QSBR |
| 读端标记 | 关抢占、per-CPU 计数等 | PCB 上 `rcu_read_depth` |
| 静止状态 | 调度器 tick、用户态/内核态切换等 | 解锁、深度 0 换出、读端外睡眠 |
| 回收旧数据 | `call_rcu` 延迟回调 | 写者在 `synchronize_rcu` 返回后自行处理 |
| 嵌套读锁 | 支持 | `rcu_read_lock` 可嵌套 |

**6. 验证**

`make -C labcodes/lab7` 链接通过后，QEMU 启动日志中在 `deadlock check ok` 之后出现 `rcu check ok`，再进入哲学家信号量版与管程版测试，最后 `init check memory pass.`，表明 `rcu_check` 回收线程后 `nr_process` 与进入 `check_sync` 前一致。

---

## 五、测试与验证

**负责人：** [2414007-王奕瑜]

---

## 六、实验总结与收获

### 6.1 对操作系统的理解

### 6.2 AI 协作开发的经验
