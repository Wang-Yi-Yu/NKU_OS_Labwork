# 操作系统实验报告

## 实验基本信息

|     项目     |                      内容                      |
| :----------: | :--------------------------------------------: |
| **实验名称** |                 Lab8：文件系统                 |
| **小组成员** | 2414007-王奕瑜、2414015-鲁昕宁、2411622-张雨函 |
| **完成日期** |                   2026-12-20                   |

### 小组分工

练习如何分工？

|      成员      |                                                    负责的练习/模块                                                     |
| :------------: | :--------------------------------------------------------------------------------------------------------------------: |
| 2414007-王奕瑜 | 功能模块一：进程文件表与地址空间的必要更新、功能模块二：SFS 文件读写功能、功能模块三：基于文件系统的用户程序加载与执行 |
| 2414015-鲁昕宁 |                                                           -                                                            |
| 2411622-张雨函 |        Challenge1：完成基于“UNIX的PIPE机制”的设计方案、Challenge2：完成基于“UNIX的软连接和硬连接机制”的设计方案        |

实验报告如何分工？

|      成员      |  负责的实验报告内容   |
| :------------: | :-------------------: |
| 2414007-王奕瑜 | 3.1, 3.2, 4.1-4.3, 五 |
| 2414015-鲁昕宁 |        一, 六         |
| 2411622-张雨函 |        4.4,4.5        |

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

本章围绕**文件系统**这一核心主题展开：在 lab7 已有的进程、虚拟内存和同步机制之上，加入"通用文件访问接口 → 虚拟文件系统（VFS）→ Simple FS → 设备驱动"这条分层的持久存储通路，并让用户进程通过"文件"这一统一抽象访问磁盘文件和 `stdin`/`stdout` 等设备（"一切皆文件"），最终实现"从文件系统里加载用户程序并执行"，把系统推进到可以交互的 `sh` 终端。

整个实验解决的问题是：**如何让数据和程序能够持久地存放在磁盘上，并由内核统一、安全地管理起来**。它可以拆成三个层次：

1. **统一的文件读写通路。** 用户程序只调用 `open`、`read`、`write` 等接口，内核依次经过通用接口层（`sysfile_*`、`file_*`）、文件系统抽象层（`inode` 与 `inode_ops` 函数表，`vop_*`）、具体文件系统（SFS）和设备层（`disk0`、`stdin`、`stdout`）完成请求。上层不关心下层是磁盘还是键盘，下层也不关心请求来自哪个进程。SFS 把"文件内的字节区间"翻译成"磁盘块上的块号和块内偏移"，是这条通路里把字节流落到磁盘的位置。
2. **进程与文件的绑定。** 文件描述符是"进程"和"打开的文件"之间的关系，所以每个进程需要自己的文件表 `files_struct`，`fork` 要让子进程继承它，`exit` 要释放它；同时进程的地址空间不再从内存里的二进制映像建立，而是在 `exec` 时由新页表承载，进程切换也必须真正切到这套页表。文件表、页表和 TLB 三者必须与"当前进程"保持一致，否则就会出现用错文件表或用错地址空间的问题。
3. **从文件执行程序。** 以前的 `do_execve` 直接从内核镜像里的内存缓冲区装载 ELF，现在改为先通过文件系统打开程序文件，再由 `load_icode` 通过文件描述符读取 ELF 头、程序头和各段内容，建立 VMA、装载段、清零 BSS、建立用户栈并把命令行参数放上栈，最后借助 trapframe 返回用户态。有了这一步，`sh` 才能用 `fork` + `exec` 运行磁盘上的任意程序。

### 3.2 功能的逐步实现

三个功能模块按"先把文件系统接入进程，再打通磁盘读写，最后从文件执行程序"的顺序实现。模块一给每个进程配上文件表和正确的页表切换，是后两个模块的运行环境；模块二让 `read` 真正能读到磁盘上的数据；模块三用前两者读出 ELF 并启动用户程序。

#### 3.2.1 功能模块一：进程文件表与地址空间的必要更新

**为什么需要。** 文件系统本身（VFS、SFS、设备）由框架提供，但它必须挂在进程管理上才有意义：每个进程要有自己的文件描述符表，才能"打开文件、读写文件、继承文件"；每个进程要用自己的页表运行，才能让 `load_icode` 建立的用户地址空间真正生效。这一模块不是重新设计进程管理，而是在补回 lab4~lab6 代码的基础上补三个必要的更新点：`alloc_proc` 把 `filesp` 初始化为 `NULL`，保证新进程的文件表状态是确定的；`proc_run` 在切换进程时加载目标进程的页表并刷新 TLB；`do_fork` 在创建子进程时通过 `copy_files` 复制或共享文件表，并在失败时按相反顺序释放。

**与其他模块的关联。** 它是模块二、模块三的前提。模块三里 `do_execve` 一开始就要 `files_closeall(current->filesp)` 并 `sysfile_open`，如果新进程的 `filesp` 是未初始化的垃圾指针，或者子进程没有从父进程那里拿到文件表，这些调用都会崩溃；`load_icode` 建好新页表并写入 `current->pgdir` 之后，也要靠 `proc_run` 在下一次调度时把它写进 `satp`，用户代码才能运行在自己的地址空间里。模块二的 `sfs_io_nolock` 则是通过进程的文件表（`file_read` → `fd2file`）才被调用到的。

---

#### 3.2.2 功能模块二：SFS 文件读写功能

**为什么需要。** 用户程序的 `read`/`write`、`do_execve` 对 ELF 文件的读取，最终都落到 `sfs_io_nolock`：它把"文件内偏移 `offset` 开始的 `*alenp` 个字节"和"磁盘上若干个 4 KiB 块"之间建立对应，是 SFS 里唯一真正搬运文件内容的函数。其余部分（路径查找、inode 加载、块映射、磁盘块读写）框架都已提供，如果没有这个函数，文件系统可以挂载、可以查目录，但读不出也写不进任何内容，`sh` 和其他用户程序就无法从磁盘加载。因为读和写面对的是同一套块边界问题（首块不对齐、中间整块、尾块不对齐），所以用一个函数、只替换读写函数指针来共用实现。

**与其他模块的关联。** 它位于 `sys_read` → `sysfile_read` → `file_read` → `vop_read` → `sfs_read` → `sfs_io`（负责 inode 锁与 `iobuf` 位置更新）这条调用链的最底端，往下调用 `sfs_bmap_load_nolock` 做逻辑块到磁盘块的映射，再调用 `sfs_rbuf`/`sfs_wbuf`/`sfs_rblock`/`sfs_wblock` 通过 `disk0` 设备完成 I/O。它为模块三提供读取 ELF 文件的能力：`load_icode_read` 用 `sysfile_seek` 和 `sysfile_read` 读取 ELF 头、程序头和段内容，走的就是这条链；它依赖模块一，因为 `file_read` 要从当前进程的文件表里找到文件。

---

#### 3.2.3 功能模块三：基于文件系统的用户程序加载与执行

**为什么需要。** lab5~lab7 的 `do_execve` 只能装载链接进内核镜像的 ELF，用户程序的集合在编译内核时就固定了，也谈不上终端。文件系统建立之后，程序应当以文件的形式放在磁盘上，由 `exec` 按路径打开并装载。`load_icode` 正是这个装载器：从文件描述符读取 ELF，创建 `mm` 和页目录，为可加载段建立 VMA 并装入内容，清零 BSS，建立用户栈并写入 `argc`/`argv`，更新 `satp`，最后重写 trapframe，让 `sret` 之后从 ELF 入口地址开始在用户态运行。同时它还要保证出错时不留下半初始化的地址空间。

**与其他模块的关联。** 它读取文件的方式依赖模块二（`sfs_io_nolock`）；它建立的 `current->mm`、`current->pgdir` 要靠模块一的 `proc_run` 在调度时生效，`fork` 后子进程继承的文件表（模块一）又让 `sh` 的子进程能带着 `stdin`/`stdout` 去执行程序。`kernel_execve` 启动的第一个用户进程 `sh`、`sh` 通过 `fork` + `exec` 启动的 `hello` 等程序，都走 `do_execve` → `load_icode`，所以它是整个实验最终能"进入 `sh`、执行 `exit`/`hello`"的最后一环。

---

## 四、实验内容与实现

### 4.1 功能模块一：进程文件表与地址空间的必要更新

**负责人：** [2414007-王奕瑜]

#### 4.1.1 模块功能描述

**需要实现/修改的函数：**

```c
static struct proc_struct *alloc_proc(void);                               // kern/process/proc.c（更新，lab8 function1-1）
void proc_run(struct proc_struct *proc);                                   // kern/process/proc.c（更新，lab8 function1-2）
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);  // kern/process/proc.c（更新，lab8 function1-3）
```

**功能说明：**

这一模块不重新设计进程管理，而是在补回 lab4~lab6 代码的基础上，为"用文件系统执行程序"补上三个必要的更新点。`copy_files()`、`put_files()`、`copy_mm()` 等框架辅助函数保持不动，只在三个函数里接入它们。

1. **`alloc_proc`（更新）：给 `filesp` 一个确定的初始值。** 前序实验已经让 `alloc_proc` 把 `state`、`pid`、`kstack`、`mm`、`context`、`pgdir`（内核页表）、`list_link`/`hash_link`（lab4）、`exit_code`、`wait_state`、`cptr`/`yptr`/`optr`（lab5）和调度字段（lab6）都设成确定的"空"值。lab8 的 `proc_struct` 新增了 `filesp`，指向进程的文件表，更新点只有一行：`proc->filesp = NULL;`。必须这样做的原因是 `kmalloc` 不清零内存：`do_fork` 失败路径上的 `put_files()` 用 `filesp != NULL` 判断"有没有表可放"，如果这里留着垃圾值，一次 `copy_mm` 失败就会对一个随机地址做 `files_count_dec` 和 `files_destroy`。这里也**不能**提前 `files_create()`：idle 进程的文件表由 `proc_init` 创建，普通子进程的文件表由 `do_fork` 里的 `copy_files` 创建或共享，在 `alloc_proc` 里建表会造成重复分配和泄漏。
2. **`proc_run`（更新）：切换进程时切换页表。** 补回的 lab4 版本已经有"关中断 → 更新 `current` → 加载 `proc->pgdir` → 刷新 TLB → `switch_to`"这套结构，页表是用手写的 CSR 访问装入 `satp` 的；但 lab4 里所有线程都共用 `boot_pgdir_pa`，这次写入实际上不改变任何东西。lab5 起用户进程有自己的页目录（`copy_mm` 和 `load_icode` 把它的物理地址保存在 `proc->pgdir`），lab8 的 `load_icode` 又会在 `exec` 时换一个新的页目录，这时这次写入就成了地址空间真正切换的地方。所以本模块的更新点是：把这一步统一为框架提供的 `lsatp(proc->pgdir)` 加 `flush_tlb()`，并在文档里明确它对用户进程的意义，而不是新增一个切换步骤。完整流程是：`proc == current` 直接返回；否则在 `local_intr_save`/`local_intr_restore` 之间记下 `prev`，把 `current` 设为 `proc`，`lsatp(proc->pgdir)`，`flush_tlb()`，最后 `switch_to(&prev->context, &proc->context)`。顺序有讲究：`current` 要先改，因为新线程一开始运行就会读它；页表要在 `switch_to` 之前切，因为 `switch_to` 返回后执行的第一条指令已经在新进程的内核栈上；全程关中断，否则时钟中断可能在 `current` 已更新、`satp` 尚未更新时触发调度，状态就不一致了。每个页目录都由 `setup_pgdir` 复制了内核映射，所以切换 `satp` 之后内核代码仍然可以继续执行。内核线程的 `pgdir` 就是 `boot_pgdir_pa`，写回去的值与当前相同，只多一次 TLB 刷新。
3. **`do_fork`（更新）：复制或共享文件表，并保证失败时不泄漏。** 在 lab5 的创建流程（检查 `MAX_PROCESS`、`alloc_proc`、设置 `parent`、`setup_kstack`、`copy_mm`、`copy_thread`、临界区内 `get_pid`/`hash_proc`/`set_links`/`wakeup_proc`）中，把 `copy_files(clone_flags, proc)` 放在 `setup_kstack` 之后、`copy_mm` 之前。设置 `CLONE_FS` 时子进程共享父进程的 `files_struct`，否则 `copy_files` 新建表并用 `dup_files` 复制打开的文件，两种情况都会把 `files_count` 加 1，所以成功路径上不能再自己加。错误处理按获取顺序的反方向：`copy_files` 失败时它已经销毁了自己建的表，`proc->filesp` 仍是 `alloc_proc` 给的 `NULL`，只需要释放内核栈和 PCB，不能再调用 `put_files`；`copy_mm` 失败时已经取得了文件表引用，所以依次 `put_files(proc)`、`put_kstack(proc)`、`kfree(proc)`。这里用 `put_files`（引用计数减 1，减到 0 才销毁）而不是直接销毁表，是为了不破坏 `CLONE_FS` 下与父进程共享的表。其余步骤和临界区保持 lab5 的写法不变，`wakeup_proc` 仍然是让子进程可运行的唯一步骤。

**与前序实验的关系。** 三个更新点都是"在原有代码上补一小段"：`alloc_proc` 在 lab4~lab6 的字段初始化末尾补 `filesp`；`proc_run` 在补回的 lab4 上下文切换代码上，把手写的页表加载改用统一的 `lsatp()` 接口；`do_fork` 在 lab4~lab6 已经成型的创建流程里插入文件表这一步。因此补回前序代码是前提，不能先写 lab8 的部分。

**文件表、页表和 TLB 为什么必须与"当前进程"保持一致。** 三者都是"当前进程执行环境"的一部分，任何一个错位都会出错：

- **文件表**：所有 `sysfile_*` 都通过 `current->filesp` 找到 `fd_array`。子进程的 `filesp` 若为 `NULL`，`fd2file` 会解引用空指针；若多个进程共享同一张表却没有引用计数，先退出的进程会销毁别人还在用的表。`alloc_proc` 的 `NULL` 初值、`copy_files` 的计数和 `put_files` 的释放是一套完整的所有权规则。
- **页表**：`satp` 决定 CPU 怎样翻译用户虚拟地址，而 `current->mm` 的 VMA 决定内核认为哪些用户地址合法。`sysfile_read` 用 `current->mm` 检查用户缓冲区、再用当前 `satp` 写入数据，两者属于不同进程时，检查通过的地址会被写进别的进程的物理页。
- **TLB**：TLB 缓存"虚拟页到物理页"的翻译，本实验不使用 ASID，不同进程的同一虚拟地址会命中同一条缓存项。换了 `satp` 却不 `sfence.vma`，新进程就可能沿用旧进程的翻译，读写到别人的内存。

#### 4.1.2 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

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

#### 4.1.3 实现迭代过程

本模块的实现经历了 1 次迭代，过程如下：

---

##### 第一次迭代

提示词生成的代码一次编译通过，没有出现需要修复的问题。

**最终结果：**

经过 1 次迭代，该模块最终：

- ✅ 编译通过：`make` 链接成功，`proc.c` 没有新增警告
- ✅ `make grade` 中"fork / wait / file table"、"yield / scheduler"、"no kernel failures"等项全部通过，总分 100/100
- ✅ 额外编写的临时用户程序验证了文件表继承：`fork` 后子进程用继承来的文件描述符 `seek` 并读出了正确的内容，退出码为 0，父进程 `waitpid` 取到该退出码
- ✅ 功能符合预期

**说明：** `copy_files` 或 `copy_mm` 的失败回滚路径只有在内存耗尽时才会触发，没有做故障注入测试，释放顺序（`put_files` → `put_kstack` → `kfree`）通过代码审查确认与获取顺序相反。

---

### 4.2 功能模块二：SFS 文件读写功能

**负责人：** [2414007-王奕瑜]

#### 4.2.1 模块功能描述

**需要实现/修改的函数：**

```c
static int sfs_io_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, void *buf, off_t offset,
                         size_t *alenp, bool write);    // kern/fs/sfs/sfs_inode.c（lab8 function2，整个函数）
```

**功能说明：**

`sfs_io_nolock` 是 SFS 里唯一真正搬运文件内容的函数，位于 `sys_read`/`sys_write` → `sysfile_read`/`sysfile_write` → `file_read`/`file_write` → `vop_read`/`vop_write` → `sfs_read`/`sfs_write` → `sfs_io` 这条调用链的最底端。`sfs_io` 已经持有 inode 的信号量，并在返回后用 `*alenp` 推进 `iobuf` 的位置；本函数只负责"把文件字节区间 `[offset, offset + len)` 与磁盘块之间的数据搬一遍，并把实际搬运的字节数写回 `*alenp`"。函数从签名、局部变量、参数检查到 `out:` 收尾全部由生成的代码给出，实现分为以下几步：

1. **范围检查与终点计算。** 先取出 `din`，断言它不是目录（目录由上层拒绝），把请求长度保存到局部变量 `len`，并立即把 `*alenp` 置 0——这样任何一条提前返回的路径都表示"一个字节也没传"。`offset` 为负或不小于 `SFS_MAX_FILE_SIZE` 返回 `-E_INVAL`；`len == 0` 返回 0；请求区间越过上限时缩短到上限，缩短的判断写成 `len > SFS_MAX_FILE_SIZE - offset`，这样即使调用者传入极大的长度，`offset + len` 也不会溢出。随后 `endpos = offset + len`。读操作再多两条限制：`offset >= din->size` 时直接返回 0（EOF），`endpos > din->size` 时把终点收到 `din->size`；写操作不受文件大小限制，可以扩展文件。
2. **读和写共用同一份分块逻辑。** 两个函数指针 `sfs_buf_op`、`sfs_block_op` 按 `write` 选定：写用 `sfs_wbuf`/`sfs_wblock`，读用 `sfs_rbuf`/`sfs_rblock`。除了所用的函数、终点的限制和最后是否更新文件大小之外，读写在块边界上要处理的问题完全一样（首块不对齐、中间整块、尾块不对齐），共用实现能避免两份高度相似的代码在边界条件上出现偏差，也只需要修一处 bug。
3. **块号算术。** `blkno = offset / SFS_BLKSIZE` 是起始逻辑块，`nblks = endpos / SFS_BLKSIZE - blkno` 是区间跨过的块边界数。例如 `offset = 4090`、`len = 20`：`endpos = 4110`，`blkno = 0`，`nblks = 1`，则首块传 6 字节（块内偏移 4090 到块尾），中间没有整块，尾块传 `4110 % 4096 = 14` 字节，合计 20 字节。写操作时如果 `blkno` 已经大于 `din->blocks`（出现空洞），`sfs_bmap_load_nolock` 会断言失败，所以在这里先返回 `-E_INVAL`；框架的 `sfs_tryseek` 会在越过文件尾的 `seek` 时先把文件扩展，所以正常使用不会触发这个检查。
4. **首块（不对齐）。** `blkoff = offset % SFS_BLKSIZE` 非 0 时，先处理 `offset` 所在的块：`nblks != 0`（区间延伸到后面的块）则传 `SFS_BLKSIZE - blkoff` 字节，`nblks == 0`（区间整个在这一块里）则传 `endpos - offset` 字节。先 `sfs_bmap_load_nolock` 求磁盘块号，再调用 `sfs_buf_op(buf, size, ino, blkoff)`，成功后累加 `alen`、推进 `buf`。如果 `nblks == 0`，区间已经处理完，**必须**立即 `goto out`，否则后面的尾块处理会看到 `endpos % SFS_BLKSIZE != 0` 而把同一段字节再传一次。否则 `blkno++`、`nblks--`，继续处理后面的整块。首块和尾块必须用带块内偏移的函数：`sfs_rbuf` 先读整块到临时缓冲再拷出需要的部分，`sfs_wbuf` 是"读整块、改局部、写回整块"，其余字节保持不变。
5. **中间完整块。** 中间块用整块函数 `sfs_block_op` 传输，但磁盘上文件的逻辑块不保证连续（多个文件交替增长、或者删除文件后重用空闲块，都会让同一个文件的块散落各处），所以不能只查第一个块的位置然后一次读写 `nblks` 块。实现是：先求出第一个逻辑块的磁盘块号 `first`，再逐个向后探测相邻逻辑块的磁盘块号 `next`，只要 `next == first + run` 就并入同一次多块传输，遇到不连续就先把已经合并的这一段用 `sfs_block_op(buf, first, run)` 传完，再把这个不连续的 `next` 作为下一段的起点。这样既保留了"中间整块用连续块操作"的效率，又保证了不连续时的正确性。每一段传完后累加 `alen`、推进 `buf`、更新 `blkno`/`nblks`。
6. **尾块（不对齐）。** `endpos % SFS_BLKSIZE` 非 0 时，最后一个块只有前面一部分属于本次传输：同样先 `sfs_bmap_load_nolock` 取磁盘块号，再用 `sfs_buf_op(buf, size, ino, 0)` 传 `size = endpos % SFS_BLKSIZE` 字节。
7. **错误处理与收尾。** 每次 `sfs_bmap_load_nolock` 或 `sfs_buf_op`/`sfs_block_op` 返回非 0，都立即跳到 `out:`；`alen` 只在一段传输成功之后才累加，所以 `out:` 处写回 `*alenp = alen` 时保留的就是已经完整传输的长度。收尾处只有写操作才会检查 `offset + alen > din->size`，成立时把 `din->size` 更新为 `offset + alen` 并置 `sin->dirty`——数据已经落盘的部分也要体现在大小里，所以部分写失败时也会更新；读操作不会进入这个分支，所以不可能把文件大小扩大。读写中扩展文件所需的块由 `sfs_bmap_load_nolock` 在 `index == din->blocks` 时追加，由于三段处理是顺序向前推进的，传给它的 `index` 始终不超过 `din->blocks`，满足它的断言。

**与上下层的分工。** 本函数不加锁（调用者 `sfs_io` 持有 inode 信号量，块读写函数自己持有 io 信号量），不绕过 `sfs_bmap_load_nolock` 自行计算磁盘地址，也不改变 `iobuf`（由 `sfs_io` 根据 `*alenp` 推进）。

#### 4.2.2 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

````markdown
[PROMPT]
**任务**：补全 lab8 的"SFS 文件读写"功能模块，修改 `kern/fs/sfs/sfs_inode.c`：在标记 `lab8 function2 YOUR CODE` 的位置生成完整的 `sfs_io_nolock()` 函数定义（含函数签名、局部变量、参数检查、读写函数指针选择、首块/中间块/尾块三段处理、`out:` 收尾和文件大小更新）。该函数把文件偏移 `offset` 开始的 `*alenp` 字节在内存缓冲区与 SFS 磁盘块之间传输，**读和写共用同一份实现**。

**操作要求**：你必须在项目中修改实际文件，保留现有代码，只在标记位置补全，并把其中的 `YOUR CODE` 替换为学号 `2414007`；位于它之后的 `sfs_io()`、`sfs_read()`、`sfs_write()` 已经提供，不得删除或重新设计。生成的是完整的函数定义，不要只填写某一段。所有新增代码的注释使用英文，函数使用 Doxygen 风格。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息，以工程真实代码为准，不要引入无关功能。

[RELY]
1. 数据结构（`sfs.h`）：`struct sfs_disk_inode { uint32_t size; uint16_t type; uint16_t nlinks; uint32_t blocks; uint32_t direct[SFS_NDIRECT]; uint32_t indirect; }`（`size` 是文件字节数，`blocks` 是已分配的数据块数）；`struct sfs_inode { struct sfs_disk_inode *din; uint32_t ino; bool dirty; ... }`（修改 `din` 后必须置 `dirty`，inode 回收或同步时才会写回磁盘）；`struct sfs_fs`；常量 `SFS_BLKSIZE`（4096，等于页大小）、`SFS_MAX_FILE_SIZE`（`unsigned long`，128 MiB）、`SFS_TYPE_DIR`；`off_t` 是 64 位有符号整数，`size_t` 是 64 位无符号整数；错误码 `E_INVAL`。
2. 块映射（同文件中已定义，位于本函数之前）：`static int sfs_bmap_load_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, uint32_t index, uint32_t *ino_store)`——把文件的第 `index` 个逻辑块映射为磁盘块号并存入 `*ino_store`。`index` 必须不大于 `din->blocks`（断言）；`index == din->blocks` 表示在文件末尾追加一个新块（分配并清零，`din->blocks` 加 1）。**文件的逻辑块在磁盘上不保证连续。**
3. 块 I/O（`sfs_io.c`，声明在 `sfs.h`）：`int sfs_rbuf(struct sfs_fs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset)` 与 `int sfs_wbuf(...)`（同样的参数）——在**一个**磁盘块内、从块内偏移 `offset` 起读/写 `len` 字节，要求 `offset + len <= SFS_BLKSIZE`（断言）；写操作内部是"读整块、改局部、写回整块"。`int sfs_rblock(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks)` 与 `int sfs_wblock(...)`——从磁盘块 `blkno` 起**连续** `nblks` 个**整块**的读/写。以上函数成功返回 0，否则返回设备错误码。
4. 调用者约定：`sfs_io()` 已经持有该 inode 的信号量，并在返回前用 `*alenp` 推进 `iobuf` 的位置；因此本函数不得再加锁，且 `*alenp` 必须在所有返回路径上都表示"已经成功传输的字节数"。传入的 inode 一定是普通文件（目录由上层拒绝）。
5. 其他：`assert`、`ROUNDUP_DIV` 等通用宏。

[GUARANTEE]
需要实现以下函数，签名必须与下面一致，并放在 `sfs_io()` 之前：

```c
static int sfs_io_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, void *buf, off_t offset,
                         size_t *alenp, bool write);                      // lab8 function2
```

允许新增 `static` 辅助函数，不得改变公开接口，不得修改 `sfs_io`、`sfs_read`、`sfs_write`。

[SPECIFICATION]
**sfs_io_nolock**
- Pre-Condition：调用者持有 `sin` 的信号量；`sin->din` 描述一个普通文件；`*alenp` 是请求传输的字节数；`buf` 指向至少 `*alenp` 字节的内存（读时是可写的目的缓冲区，写时是源缓冲区）。
- Post-Condition：成功时返回 0；`*alenp` 等于实际传输的字节数（可能小于请求值）；若是写操作且传输区间越过了原来的文件末尾，则 `sin->din->size` 等于 `offset + *alenp` 并且 `sin->dirty` 为真；读操作绝不改变文件大小。失败时返回对应的错误码，`*alenp` 保留失败之前已经完整传输的字节数。
- Case 1（参数检查）：`offset` 为负数或不小于 `SFS_MAX_FILE_SIZE`，返回 `-E_INVAL`，`*alenp` 为 0；请求长度为 0，返回 0，`*alenp` 为 0；请求区间越过 `SFS_MAX_FILE_SIZE` 时把区间缩短到该上限，不报错。区间终点的计算不能发生整数溢出（请求长度可能极大）。
- Case 2（读到文件尾）：读操作的区间终点不能超过 `din->size`。`offset` 不小于 `din->size` 时返回 0，`*alenp` 为 0（EOF）；区间跨过文件尾时只传输到文件尾为止。
- Case 3（写）：写操作不受 `din->size` 限制，可以扩展文件，新增的块由块映射函数在文件末尾追加。起始块号大于 `din->blocks`（出现空洞）时返回 `-E_INVAL`，不得触发块映射函数的断言。
- Case 4（首块不对齐）：`offset` 不是块边界时，先处理它所在的块：区间延伸到下一块时，传输从块内偏移到该块末尾为止；整个区间都落在这一块内时，传输到区间终点为止。这一段必须用带块内偏移的缓冲区读/写函数，且此前必须通过块映射函数取得磁盘块号。若整个区间已在这一块内处理完，必须立即结束，不得再执行后面的尾块处理。
- Case 5（中间完整块）：区间内部完整的块用整块读/写函数传输。每个逻辑块都必须通过块映射函数求出磁盘块号，不得假定它们在磁盘上连续：磁盘块号恰好连续的相邻块可以合并为一次多块传输，遇到不连续处必须先传完已合并的部分，再从该处开始新的一段。
- Case 6（尾块不对齐）：区间终点不是块边界时，处理最后一块的前面部分，同样用带块内偏移（偏移为 0）的缓冲区读/写函数，先经过块映射函数。
- Case 7（错误）：块映射或磁盘读写任何一步出错，立即停止后续传输，返回该错误码；已经成功传输的字节数保留在 `*alenp` 中，并且对已经写入的部分仍然要更新文件大小和 `dirty`。
- Requirements：读和写必须共用一份分块逻辑，两者的差别只体现在所选用的缓冲区函数、整块函数，以及区间终点和大小更新的处理上；不得绕过块映射函数自行计算磁盘地址；不得在本函数内加锁；函数必须以一个 `out:` 收尾位置统一写回 `*alenp` 并处理大小更新。
````

#### 4.2.3 实现迭代过程

本模块的实现经历了 1 次迭代，过程如下：

---

##### 第一次迭代

提示词生成的代码一次编译通过，没有出现需要修复的问题。

**最终结果：**

经过 1 次迭代，该模块最终：

- ✅ 编译通过，`sfs_inode.c` 没有新增警告，并通过项目的 `clang-format` 检查
- ✅ `make grade` 中"SFS read path"、"load_icode / sh runs hello"等项全部通过，总分 100/100（`sh` 和 `hello` 的 ELF 都是经过 `sfs_io_nolock` 读出来的）
- ✅ 额外编写了一个临时用户程序，在 `sh` 里运行，覆盖评分脚本没有涉及的边界情况，全部输出 `iotest pass.`：
  - 一次写入 10000 字节（跨 3 个块）后整体读回；
  - 11 组不同的 `(offset, len)` 读：跨块边界（4090/20）、块内（100/50）、整块对齐（4096/4096）、从块尾 1 字节（4095/1）、从块首 1 字节（4096/1）、跨两个边界（8191/2）、长跨度（5/9990）等；
  - 文件尾读：在 `size - 10` 处读 100 字节只返回 10，再读返回 0，并确认读操作没有改变文件大小；
  - 块内偏移的覆盖写（4000 处写 200 字节）：前后相邻字节不变、文件大小不变；
  - 扩展写：先在最后一块内扩展，再一次追加 20000 字节跨多个新块，`fstat` 得到的大小正确，整体读回一致；
  - 块对齐的追加写（原大小是 4096 的整数倍）；
  - **碎片文件**：两个文件交替各写 9 个块，使它们的磁盘块号交错，再分别整体读、非对齐读、非对齐整体覆盖写，并确认另一个文件的内容没被破坏——这条用例专门验证了"中间块不能假定在磁盘上连续"的处理；
  - 长度为 0 的读返回 0，负的 `seek` 位置返回错误。
- ✅ 功能符合预期

**说明：** 测试文件没有用 `O_CREAT` 新建，而是打开镜像里已有的程序文件并用 `O_TRUNC` 清空后当作临时文件使用，因为框架的 inode 操作表里没有 `vop_create`，带 `O_CREAT` 创建新文件会在 `vfs_open` 里触发断言。改动只发生在 QEMU 运行时内存中的磁盘镜像副本里，下一次启动就恢复原样。

---

### 4.3 功能模块三：基于文件系统的用户程序加载与执行

**负责人：** [2414007-王奕瑜]

#### 4.3.1 模块功能描述

**需要实现/修改的函数：**

```c
static int load_icode(int fd, int argc, char **kargv);   // kern/process/proc.c（lab8 function3，整个函数）
_start:                                                  // user/libs/initcode.S（取 argc/argv 的入口代码）
```

**功能说明：**

`load_icode` 把文件描述符 `fd` 对应的 ELF 用户程序装载到当前进程的新地址空间，并让进程从 ELF 入口返回用户态。调用关系是 `sys_exec`（或 `kernel_execve`）→ `do_execve`（复制参数、关闭旧文件、打开 ELF、释放旧 `mm`）→ `load_icode`。进入时 `current->mm` 为 `NULL`，`current->tf` 指向内核栈顶的 trapframe。所有从文件的读取都通过已提供的 `load_icode_read` 完成（它先 `sysfile_seek` 再 `sysfile_read`，必须恰好读满）；因为此时 `current->mm == NULL`，`sysfile_read` 只要求目的地址是内核地址，所以可以把文件内容直接读进新分配物理页的内核虚拟地址。函数按下面七步进行：

1. **创建地址空间。** `mm_create` 新建 `mm_struct`，`setup_pgdir` 分配页目录并把内核映射复制进去（保证切换到新页表后内核代码仍可执行）。
2. **读取并检查 ELF 头。** `load_icode_read(fd, &elf, sizeof(elf), 0)` 读出 ELF 头，读失败返回其错误码，`e_magic != ELF_MAGIC` 返回 `-E_INVAL_ELF`。
3. **遍历程序头，装载每个 `PT_LOAD` 段。** 第 `i` 个程序头在文件偏移 `e_phoff + i * sizeof(struct proghdr)` 处，同样用 `load_icode_read` 读出。非 `PT_LOAD` 或 `p_memsz == 0` 的跳过；`p_filesz > p_memsz` 或 `p_va + p_memsz` 回绕的是非法 ELF，返回 `-E_INVAL_ELF`。按 `ELF_PF_R/W/X` 设置 VMA 标志（`VM_READ/WRITE/EXEC`）和页表权限：总含 `PTE_U | PTE_V`，可读加 `PTE_R`，可写加 `PTE_W | PTE_R`（RISC-V 不允许只写页），可执行加 `PTE_X`；然后 `mm_map` 建立覆盖 `[p_va, p_va + p_memsz)` 的 VMA。接着**逐页**装入内容：从 `ROUNDDOWN(p_va, PGSIZE)` 起，每个页都 `pgdir_alloc_page` 分配并映射，立刻**整页清零**（`alloc_page` 不清零页内容），然后只把落在 `[p_va, p_va + p_filesz)` 内的部分读入。对每个页取 `lo = max(页起始, p_va)`、`hi = min(页末尾, p_va + p_filesz)`，若 `lo < hi`，就把文件中接下来的 `hi - lo` 字节读到页内偏移 `lo - 页起始` 处，并把文件偏移前进 `hi - lo`。这样一套规则同时解决了三个问题：段起始地址不页对齐时，第一页的前面部分保持为零、文件内容从正确的页内偏移开始放；包含文件末尾的那一页只读到 `p_filesz` 为止；`p_filesz` 到 `p_memsz` 之间的 **BSS**，无论落在同一页的后半部分还是后面的整页，都因为先清零而自然为零。关键是 `p_filesz` 和 `p_memsz` 的区分：文件里只有前者那么多字节，只读这么多，不会把 BSS 当作文件内容去读（那会读到文件之外）。
4. **关闭文件。** 所有段读完后文件已经不需要了，`sysfile_close(fd)`。失败路径上不用关，进程 `do_exit` 时会统一关闭。
5. **建立用户栈并准备参数。** `mm_map` 建立 `VM_READ | VM_WRITE | VM_STACK` 的栈 VMA，范围 `[USTACKTOP - USTACKSIZE, USTACKTOP)`。参数按进程入口的常见布局从 `USTACKTOP` 向下放：先是各个参数字符串（含结尾 NUL），然后是以 `NULL` 结尾的指针数组 `uargv[0..argc]`，最后是 `argc`；栈指针 `sp` 按 16 字节对齐并指向 `argc`，`uargv` 位于 `sp + 8`。先计算字符串总长 `str_size`，得到 `str_base = ROUNDDOWN(USTACKTOP - str_size, 16)`，`sp = ROUNDDOWN(str_base - (argc + 2) * 8, 16)`，其中 `argc + 2` 个字分别给 `argc` 和 `argc + 1` 个指针。若 `sp` 落到栈 VMA 最低的保护页以内，返回 `-E_INVAL`。因为本实验没有缺页处理（`do_pgfault` 没有定义），无法按需补页，所以要在装载时就为栈分配物理页：至少 4 页，并且能容纳整个参数块；这些页同样 `pgdir_alloc_page` 之后整页清零，避免把旧数据暴露给用户程序。不全部分配 256 页，是因为 `fork` 会逐页复制已映射的页。
6. **切换到新地址空间并写入参数。** 所有可能失败的步骤都完成后，才让新 `mm` 生效：`mm_count_inc`，设置 `current->mm` 和 `current->pgdir`（页目录物理地址），`lsatp(PADDR(mm->pgdir))` 并 `flush_tlb()`。切换之后，内核（`sstatus.SUM` 已置位）可以直接通过用户虚拟地址访问新栈，把字符串、`uargv` 数组（每项指向对应字符串在用户栈中的地址，最后一项为 `NULL`）和 `argc` 写进去。这一步之后不会再失败，所以不需要处理"页表已切换但出错"的回退。
7. **设置 trapframe。** 先保存原来的 `status`，再 `memset` 清空整个 trapframe，设置 `epc = e_entry`（ELF 入口）、`sp` 为上面算好的栈指针、`a0 = argc`、`a1 = uargv`；`status` 在原值上置位 `SSTATUS_SPIE`（`sret` 后开中断）并清除 `SSTATUS_SPP`（`sret` 返回 U 态），原值要保留，因为 `RESTORE_ALL` 会用它整体恢复 `sstatus`（其中包括内核依赖的 `SUM`）。函数返回 0 后，`do_execve` 返回，`kernel_execve` 跳到 `__trapret`（或用户态的系统调用返回路径）完成特权级切换。

**失败清理。** 任一步出错（`mm_create`/`setup_pgdir` 失败、读取失败、ELF 魔数或段非法、`mm_map` 失败、物理页分配失败、参数放不进栈），都按获取顺序的反方向释放：如果已经建立过 VMA，先 `exit_mmap`（释放已映射的物理页和页表，此时 `mm_count` 仍为 0，满足它的要求），再 `put_pgdir` 释放页目录，最后 `mm_destroy` 释放 `mm_struct`；在 VMA 建立之前失败（`mm_create` 之后读 ELF 头失败）则跳过 `exit_mmap`。出错后 `current->mm` 仍为 `NULL`，`do_execve` 随后让进程 `do_exit`，不会留下半初始化的地址空间。

**入口代码 `_start`。** `initcode.S` 里的 `_start` 原来只有 `call umain`，参数靠 `a0`/`a1` 传递。这对 `kernel_execve` 启动的程序（直接 `j __trapret`）成立，但对用户态通过 `sys_exec` 启动的程序不成立：`syscall()` 在 `do_execve` 返回后会把返回值 0 写回 trapframe 的 `a0`，覆盖 `load_icode` 设置的 `argc`。因此 `_start` 改为从栈上读取：`ld a0, 0(sp)` 取 `argc`，`addi a1, sp, 8` 取 `argv`，再 `call umain`。`load_icode` 仍然设置 `a0`/`a1`，使 `kernel_execve` 路径和没有修改入口代码的程序也能正常工作。

#### 4.3.2 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

````markdown
[PROMPT]
**任务**：补全 lab8 的"基于文件系统的用户程序加载与执行"功能模块，涉及两个文件：`kern/process/proc.c` 中标记 `lab8 function3 YOUR CODE` 的位置，生成完整的 `static int load_icode(int fd, int argc, char **kargv)`（必须使用这个固定签名，`do_execve()` 直接调用它）；`user/libs/initcode.S` 中的 `_start`，让用户程序入口按 `load_icode` 约定的栈布局取得 `argc`/`argv`。`load_icode` 要把已经打开的 ELF 文件 `fd` 装载到当前进程的新地址空间，建立用户栈并写入命令行参数，设置好 trapframe，使进程从 ELF 入口返回用户态；任何一步失败都必须释放已创建的资源，不留下半初始化的地址空间。

**操作要求**：你必须在项目中修改实际文件，保留现有代码，只在标记位置补全，并把其中的 `YOUR CODE` 替换为学号 `2414007`；`load_icode_read()`、`do_execve()`、`copy_kargv()`、`put_kargv()` 已经提供，不要删除或重写；不要修改函数名、参数类型和返回值类型。所有新增代码的注释使用英文，函数使用 Doxygen 风格。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息，以工程真实代码为准，不要引入无关功能。

[RELY]
1. 文件读取：`static int load_icode_read(int fd, void *buf, size_t len, off_t offset)`（`proc.c`，已提供）——先 `sysfile_seek` 到 `offset` 再 `sysfile_read` 读 `len` 字节，**恰好读满**才返回 0，否则返回负的错误码（短读返回 -1）。`int sysfile_close(int fd)`。`sysfile_read` 通过 `copy_to_user(current->mm, ...)` 把数据交给 `buf`：`load_icode` 运行时 `current->mm` 为 `NULL`（`do_execve` 已经释放旧地址空间），此时只要求目的地址是内核地址，所以可以直接读到新分配的物理页的内核虚拟地址 `page2kva(page)`。
2. 进程与地址空间：`mm_create()`、`setup_pgdir(mm)`（`proc.c` 内的静态函数，分配页目录并复制内核映射）、`put_pgdir(mm)`、`mm_map(mm, addr, len, vm_flags, vma_store)`（按页对齐 `addr` 和 `addr + len`，区间超出用户空间或与已有 VMA 重叠时返回 `-E_INVAL`）、`exit_mmap(mm)`（释放 VMA 对应的页和页表，要求 `mm_count == 0`）、`mm_destroy(mm)`、`mm_count_inc(mm)`；`pgdir_alloc_page(pgdir, la, perm)`（分配一个物理页并映射到 `la`，**不会清零页内容**，失败返回 `NULL`）；`page2kva`、`PADDR`、`lsatp(pa)`、`flush_tlb()`、`ROUNDDOWN`、`ROUNDUP`、`PGSIZE`；`VM_READ`、`VM_WRITE`、`VM_EXEC`、`VM_STACK`；`PTE_U`、`PTE_V`、`PTE_R`、`PTE_W`、`PTE_X`、`PTE_USER`（可读可写可执行的用户页）。`current->mm`、`current->pgdir`（页目录物理地址）、`current->tf`（内核栈顶的 trapframe）。
3. ELF（`libs/elf.h`）：`struct elfhdr`（`e_magic`、`e_entry`、`e_phoff`、`e_phnum`）、`struct proghdr`（`p_type`、`p_flags`、`p_offset`、`p_va`、`p_filesz`、`p_memsz`）、`ELF_MAGIC`、`ELF_PT_LOAD`、`ELF_PF_X`、`ELF_PF_W`、`ELF_PF_R`。程序段的起始地址 `p_va` 可能不是页对齐的；文件中只有 `p_filesz` 字节，`p_memsz` 超出的部分是 BSS，必须为零。
4. 用户空间布局（`memlayout.h`）：`USTACKTOP`、`USTACKSIZE`（256 页）；`EXEC_MAX_ARG_LEN`（4095）、`EXEC_MAX_ARG_NUM`（32）。用户栈 VMA 的最低一页是保护页，`user_mem_check` 不允许写入。本实验没有定义缺页处理（`do_pgfault`），无法按需补页，所以用户栈上会被访问的页必须在装载时就分配好；但全部分配会让每次 `fork` 都复制整个栈，因此只分配栈顶需要的若干页。
5. 陷入帧与特权级：`struct trapframe`（`gpr.sp`、`gpr.a0`、`gpr.a1`、`epc`、`status`）、`SSTATUS_SPP`（为 1 表示 `sret` 返回 S 态）、`SSTATUS_SPIE`。内核在初始化时已经置位 `sstatus.SUM`，因此切换到新页表之后，S 态可以直接通过用户虚拟地址读写该页表中的用户页。`__trapret` 用 trapframe 中的 `status` 整体恢复 `sstatus`，所以必须保留原来的 `status` 值（含 `SUM`）。
6. 调用环境：`do_execve` 在关闭进程所有已打开文件（`files_closeall`）之后才打开 ELF 文件，随后释放旧 `mm`，再调用 `load_icode(fd, argc, kargv)`；`kargv[0..argc-1]` 是内核内存中以 NUL 结尾的字符串（长度不超过 `EXEC_MAX_ARG_LEN`）；`load_icode` 返回非 0 时 `do_execve` 让进程 `do_exit`，返回 0 时释放 `kargv`。`kernel_execve` 直接 `j __trapret`，而用户态通过 `sys_exec` 进入时，`syscall()` 会在返回后把系统调用的返回值（成功时为 0）写回 trapframe 的 `a0`，**覆盖**其中的 `argc`。
7. 用户库：`void umain(int argc, char *argv[])`（`user/libs/umain.c`）；`_start` 当前只有 `call umain`。

[GUARANTEE]
需要实现或修改以下内容，签名必须与现有声明一致：

```c
static int load_icode(int fd, int argc, char **kargv)    // kern/process/proc.c, lab8 function3
_start:                                                  // user/libs/initcode.S
```

允许新增 `static` 辅助函数，不得改变公开接口。

[SPECIFICATION]
**load_icode**
- Pre-Condition：`current->mm` 为 `NULL`；`current->tf` 指向内核栈顶的 trapframe；`fd` 是以只读方式打开的 ELF 文件，文件位置不作假设（每次读取都由 `load_icode_read` 先定位）；`1 <= argc <= EXEC_MAX_ARG_NUM`。
- Post-Condition：成功时返回 0，`current->mm` 是新建的地址空间（引用计数为 1），`current->pgdir` 和 `satp` 指向它的页目录，TLB 已刷新；ELF 的每个 `PT_LOAD` 段都已按 VMA 权限装入；用户栈已建立并写入参数；trapframe 使 `sret` 后在用户态从 ELF 入口开始执行。失败时返回错误码，本函数创建的 VMA、页表、物理页、页目录和 `mm_struct` 都已释放，`current->mm` 仍为 `NULL`。
- Case 1（ELF 头）：通过 `load_icode_read` 读取 ELF 头；读取失败返回其错误码；`e_magic` 不是 `ELF_MAGIC` 返回 `-E_INVAL_ELF`。
- Case 2（程序头与段）：对 `e_phnum` 个程序头逐个通过 `load_icode_read` 读取（位置为 `e_phoff` 加上序号乘以程序头大小）。非 `PT_LOAD` 的跳过；`p_memsz` 为 0 的跳过；`p_filesz > p_memsz`，或 `p_va + p_memsz` 回绕，返回 `-E_INVAL_ELF`。根据 `ELF_PF_R/W/X` 设置 VMA 标志，并据此设置页表权限：总含 `PTE_U` 和 `PTE_V`，可读加 `PTE_R`，可写加 `PTE_W | PTE_R`（RISC-V 不允许只写），可执行加 `PTE_X`。用 `mm_map` 建立覆盖 `[p_va, p_va + p_memsz)` 的 VMA。
- Case 3（装载一个段）：从 `p_va` 所在页起逐页处理，直到覆盖 `p_va + p_memsz`。每页先分配并**整页清零**，再只把落在 `[p_va, p_va + p_filesz)` 内的那部分字节从文件读入该页：首页要按 `p_va` 的页内偏移放置，包含文件尾的页只读到 `p_filesz` 为止，`p_filesz` 到 `p_memsz` 之间（BSS）和页内未被文件覆盖的部分保持为零。文件偏移从 `p_offset` 开始随已读字节数推进。
- Case 4（装载完毕）：所有段读完后关闭 `fd`（之后不再需要）。
- Case 5（用户栈与参数）：建立 `VM_READ | VM_WRITE | VM_STACK` 的栈 VMA，范围是 `USTACKTOP - USTACKSIZE` 到 `USTACKTOP`。参数从 `USTACKTOP` 向下依次存放：各参数字符串（含结尾 NUL）、以 NULL 结尾的指针数组 `uargv[0..argc]`、`argc`；栈指针 `sp` 按 16 字节对齐并指向 `argc`，`uargv` 位于 `sp + 8`（通常的进程入口布局）。分配并清零栈顶若干页：至少 4 页，并且足以容纳上面整个参数块；参数块放不进栈（越过保护页）时返回 `-E_INVAL`。
- Case 6（切换地址空间）：所有可能失败的步骤完成后，才使新 `mm` 成为当前进程的地址空间：引用计数加 1，设置 `current->mm`、`current->pgdir`，写 `satp` 并刷新 TLB。然后才通过用户虚拟地址把参数字符串、`uargv` 数组和 `argc` 写入新栈。
- Case 7（trapframe）：保存原 `status`，清空整个 trapframe，再设置：`epc` 为 ELF 入口，`sp` 为上面计算的栈指针，`a0` 为 `argc`，`a1` 为 `uargv`；`status` 在原值基础上置位 `SSTATUS_SPIE`、清除 `SSTATUS_SPP`，使 `sret` 返回用户态并开中断。其余寄存器为 0。
- Case 8（失败清理）：`mm_create` 失败、页目录分配失败、读取失败、`mm_map` 失败、物理页分配失败、栈放不下，都要按获取的相反顺序释放：已映射的页和页表（`exit_mmap`，此时 `mm_count` 为 0）、页目录、`mm_struct`；在 VMA 建立之前失败则不需要 `exit_mmap`。
- Requirements：所有磁盘读取都通过 `load_icode_read` 完成；只从文件读 `p_filesz` 字节，不得把整个 `p_memsz` 都当作文件内容；不得在切换到新页表之前通过用户虚拟地址访问新栈；不得在 `load_icode` 内部修改 `current->tf` 之外的执行现场。

**_start（initcode.S）**
- Pre-Condition：`sp` 指向 `load_icode` 写好的 `argc`，`argv` 数组紧随其后。
- Post-Condition：以 `argc` 为第一个参数、`argv` 为第二个参数调用 `umain`，保留其后的死循环作为兜底。
- Requirements：`argc` 和 `argv` 必须从栈上取（`argc` 在 `sp`，`argv` 为 `sp + 8`），不能依赖 `a0`/`a1`：用户态 `exec` 返回路径会把 `a0` 覆盖为 0；`load_icode` 仍应把 `a0`/`a1` 设置好，以便 `kernel_execve` 路径和未修改入口代码的程序可用。
````

#### 4.3.3 实现迭代过程

本模块的实现经历了 2 次迭代，过程如下：

---

##### 第一次迭代

第一版提示词生成的 `load_icode` 编译通过，`make grade` 也得到 100/100。但评分用例里没有一个带命令行参数的程序，所以为了验证"把 `argc` 和参数字符串写入用户栈"这一项，额外写了一个临时用户程序，在 `sh` 里以 `iotest a bb "c d"` 运行并打印自己的 `argc` 和 `argv`。

**遇到的问题：**

- 问题 1：从 `sh` 启动的程序拿到的 `argc` 为 0。程序的输出是 `iotest: argc=0 argv[argc]=NOT NULL`：没有任何 `argv[i]` 被打印，并且 `argv[argc]`（即 `argv[0]`）不是 `NULL`。第一版里 `load_icode` 把 `argc` 放进 trapframe 的 `a0`、`argv` 放进 `a1`（同时也按通常的布局写在了用户栈上），而 `_start` 只有一条 `call umain`，直接把 `a0`/`a1` 当作 `umain(argc, argv)` 的参数。`argv`（`a1`）确实是对的，错的是 `a0`。原因是用户态通过 `sys_exec` 进入内核时，`syscall()` 在 `do_execve` 成功返回后会执行 `tf->gpr.a0 = 0`，把系统调用的返回值写回 trapframe，**覆盖**了 `load_icode` 刚设置好的 `argc`。而 `sh` 本身是 `kernel_execve` 启动的，那条路径直接 `j __trapret`，不经过系统调用返回，所以 `a0` 没被覆盖，评分脚本里的 `sh`（`argc` 本来就是 1）和不带参数的 `hello` 等程序都看不出问题，只有这种"在 `sh` 里带参数运行"的场景才会暴露。

**问题解决策略：**

- 针对问题 1：不再依赖 `a0` 传 `argc`，改为让 `_start` 从栈上读取。这需要修改提示词的三处：在 [RELY] 中补充"用户态 `exec` 返回时 `syscall()` 会把 `a0` 覆盖为 0，而 `kernel_execve` 路径不会"这一条关键事实；在 [SPECIFICATION] 中把用户栈参数块的布局明确为"字符串 → 以 `NULL` 结尾的 `uargv` 数组 → `argc`，`sp` 16 字节对齐并指向 `argc`，`uargv` 位于 `sp + 8`"（这是通常的进程入口布局）；在 [GUARANTEE] 中把 `user/libs/initcode.S` 的 `_start` 也列入交付清单，规定它用 `ld a0, 0(sp)` 和 `addi a1, sp, 8` 取参数后再 `call umain`，并保留 `load_icode` 对 `a0`/`a1` 的设置，让 `kernel_execve` 路径和未修改入口代码的程序仍然可用。

---

##### 第二次迭代

按修改后的提示词，`load_icode` 的栈布局和 `a0`/`a1` 设置保持不变，新增了 `initcode.S` 中 `_start` 从栈上取 `argc`/`argv` 的改动。重新编译后一次通过，用同一个带参数的测试程序重跑，没有再发现需要修复的问题。

**最终结果：**

经过 2 次迭代，该模块最终：

- ✅ 编译通过，`proc.c` 没有新增警告，并通过项目的 `clang-format` 检查
- ✅ `make grade` 中"sh execve"、"user sh"、"load_icode / sh runs hello"、"fork / wait / file table"、"no kernel failures"等项全部通过，总分 100/100
- ✅ 在 `sh` 里运行带参数的临时测试程序 `iotest a bb "c d"`，输出 `argc=4 argv[0]="iotest" argv[1]="a" argv[2]="bb" argv[3]=" c d " argv[argc]=NULL`（`sh` 的词法分析把双引号替换成空格，所以第四个参数两侧各有一个空格，这是 `sh` 的行为），说明字符串、指针数组、`NULL` 终止和 `argc` 都正确
- ✅ 之后接着在 `sh` 里运行 `hello`，得到 `Hello world!!.`、`I am process 5.`、`hello pass.`，说明 `fork` + `exec` 从文件系统加载并运行程序的完整链路正常
- ✅ 失败路径：测试程序把镜像中的 `/exit` 文件内容覆盖成非 ELF 数据后，在 `sh` 里执行 `exit`，得到 `error: -8 - invalid elf file`（`-E_INVAL_ELF`），`sh` 继续可用，内核没有 panic，说明魔数检查和失败时的清理路径正常
- ✅ 功能符合预期

**关键改进点总结：**

1. 在 [RELY] 中补充了"`syscall()` 在 `exec` 返回后会覆盖 trapframe 的 `a0`，`kernel_execve` 路径则不会"这一调用环境事实，这是第一版提示词缺失、也最难凭空想到的信息。
2. 在 [SPECIFICATION] 中把用户栈上的参数布局写成可检查的规则（字符串、`uargv`、`argc` 的次序，`sp` 的对齐，`uargv = sp + 8`），不再只说"把参数放上栈"。
3. 在 [GUARANTEE] 中增加了 `user/libs/initcode.S`，让入口代码从栈上取 `argc`/`argv`，不依赖会被覆盖的 `a0`。
4. 验证方式上，用带参数的临时程序补上了评分脚本没有覆盖的场景，并顺带验证了 ELF 魔数错误时的失败路径。

---

### 4.4 Challenge1：完成基于“UNIX的PIPE机制”的设计方案

**负责人：** [2411622-张雨函]

> 如果要在ucore里加入UNIX的管道（Pipe）机制，至少需要定义哪些数据结构和接口？（接口给出语义即可，不必具体实现。数据结构的设计应当给出一个（或多个）具体的C语言struct定义。在网络上查找相关的Linux资料和实现，请在实验报告中给出设计实现”UNIX的PIPE机制“的概要设方案，你的设计应当体现出对可能出现的同步互斥问题的处理。）

---

### 4.5 Challenge2：完成基于“UNIX的软连接和硬连接机制”的设计方案

**负责人：** [2411622-张雨函]

> 如果要在ucore里加入UNIX的软连接和硬连接机制，至少需要定义哪些数据结构和接口？（接口给出语义即可，不必具体实现。数据结构的设计应当给出一个（或多个）具体的C语言struct定义。在网络上查找相关的Linux资料和实现，请在实验报告中给出设计实现”UNIX的软连接和硬连接机制“的概要设方案，你的设计应当体现出对可能出现的同步互斥问题的处理。）

---

## 五、测试与验证

**负责人：** [2414007-王奕瑜]

---

## 六、实验总结与收获

### 6.1 对操作系统的理解

### 6.2 AI 协作开发的经验
