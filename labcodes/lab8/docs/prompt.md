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

#### 2.1.3 功能模块三：基于文件系统的用户程序加载与执行

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

### 2.2 2414015-鲁昕宁

### 2.3 2411622-张雨函
