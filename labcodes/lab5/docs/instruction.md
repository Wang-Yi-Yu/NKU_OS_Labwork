## lab5:用户程序

实验4完成了内核线程，但到目前为止，所有的运行都在内核态执行。实验5将创建用户进程，让用户进程在用户态执行，且在需要ucore支持时，可通过系统调用来让ucore提供服务。

## 实验目的

-   了解第一个用户进程创建过程
-   了解系统调用框架的实现机制
-   了解ucore如何实现系统调用sys_fork/sys_exec/sys_exit/sys_wait来进行进程管理

## 实验内容

实验4完成了内核线程，但到目前为止，所有运行都还停留在内核态。实验5开始构造用户进程，让用户程序真正运行在用户态，并在需要 ucore 支持时，通过系统调用进入内核获得服务。本实验需要完成系统调用入口、首个用户进程的创建与装载、用户进程的复制、以及退出、让出和终止等进程控制功能，从而把“启动内核线程”推进到“运行和管理用户程序”的完整阶段。

### 练习

本章围绕“用户态如何获得内核服务”“第一个用户进程如何被启动”“用户进程如何被复制”“进程如何退出”四条主线展开。阅读时可以把整章理解成一条完整链路：用户程序先通过系统调用进入内核，内核完成进程管理与虚拟内存管理后，再通过调度把结果送回用户态继续执行。

本实验依赖实验2/3/4。请使用你此前设计的提示词生成相应实验代码，并将生成结果填写到本实验代码中标有“lab2”/“lab3”/“lab4”注释的对应位置，确保代码能够正确编译。

注意：原来lab3的 `RESTORE_ALL` 宏，它仅能用于纯内核、无用户态的系统。请在此基础上修改，扩充为支持用户态 / 内核态双向异常返回的完整版上下文恢复宏。

#### 功能模块一：用户态系统调用功能（设计提示词借助ai完成）

需要更新和完善的函数有：

```c
void exception_handler(struct trapframe *tf) (kern/trap/trap.c)
void syscall(void) (kern/syscall/syscall.c)
static int sys_exit(uint64_t arg[]) (kern/syscall/syscall.c)
static int sys_fork(uint64_t arg[]) (kern/syscall/syscall.c)
static int sys_wait(uint64_t arg[]) (kern/syscall/syscall.c)  
static int sys_exec(uint64_t arg[]) (kern/syscall/syscall.c)  
static int sys_yield(uint64_t arg[]) (kern/syscall/syscall.c)  
static int sys_kill(uint64_t arg[]) (kern/syscall/syscall.c)  
static int sys_getpid(uint64_t arg[]) (kern/syscall/syscall.c)  
static int sys_putc(uint64_t arg[]) (kern/syscall/syscall.c) 
static int sys_pgdir(uint64_t arg[]) (kern/syscall/syscall.c)
```

整个功能模块的具体功能为：这一部分负责把用户态通过 `ecall` 发起的请求接入内核，再把内核的处理结果写回到用户态寄存器中。`exception_handler` 这里只需要关注 `CAUSE_USER_ECALL` 这一类情况，把系统调用异常转交给 `syscall`，`syscall` 再从当前进程的 `trapframe` 中取出系统调用号和参数并完成分发，而各个 `sys_*` 函数则把统一编号的系统调用请求转换成具体内核语义，例如进程退出、进程复制、等待子进程、程序替换、让出 CPU、终止进程、获取当前进程号、向控制台输出字符以及查看页表等。整条链路的关键点是，用户库只负责把参数放进约定寄存器并触发异常，真正的语义判断、权限检查和资源更新都在内核态完成，最后由异常返回路径把结果带回用户程序继续执行。

整个功能模块的注意事项为：系统调用编号和参数位置必须与用户态封装保持一致，通常由 `a0` 保存系统调用号，`a1` 到 `a5` 保存参数；`exception_handler` 在识别到用户态系统调用后必须把 `epc` 前移，避免返回后重复执行同一条 `ecall`；`syscall` 的分发表必须保证边界正确，未实现或非法的编号不能被当作有效请求处理。凡是涉及用户指针的参数，都不能直接信任，必须结合 `user_mem_check` 和当前进程的地址空间做合法性判断；凡是会改变进程状态、进程树关系或页表内容的处理，都要和当前进程的 `trapframe`、`mm_struct`、`proc_struct` 保持同步，避免出现返回值已经写回但进程上下文没有切换好的情况。

整个功能模块应该满足的对外的接口为：用户态需要看到的是一组语义清晰的库函数接口，例如 `exit`、`fork`、`wait`、`waitpid`、`exec`、`yield`、`kill`、`getpid`、`cprintf` 以及页表查看相关接口；内核态需要提供统一的系统调用入口 `exception_handler` 和 `syscall`，并保证每个系统调用都能在返回时把结果正确放入用户可见的返回寄存器。对外表现上，这一模块应该让用户程序像在普通操作系统中那样使用系统调用，而不需要关心异常入口、寄存器传参和 trap 返回的细节。

整个功能模块可以借助的接口函数、结构体、数据结构为：`current` 和 `current->tf` 提供当前进程及其 trapframe；`trapframe`、`pushregs`、`sstatus`、`sepc`、`scause` 等结构和寄存器状态用于异常处理；`do_exit`、`do_fork`、`do_wait`、`do_execve`、`do_yield`、`do_kill` 是系统调用真正落到进程管理层的入口；`cputchar` 负责最终的字符输出；`print_trapframe` 便于定位异常；`user_mem_check` 用于检查用户参数是否合法；`libs/unistd.h` 和 `user/libs/syscall.c` 中定义的系统调用号和用户态封装则决定了整条调用链的入口形式。

这组函数合在一起，完成的是“异常入口 + 系统调用分发 + 内核语义落地 + 返回用户态”这一完整闭环。`exception_handler` 和 `syscall` 负责控制流转接，`sys_*` 负责把用户请求翻译成内核动作，而后续各个 `do_*` 函数则承担真正的进程管理和资源管理工作。

#### 功能模块二：创建加载第一个用户进程功能（设计提示词借助ai完成）

需要更新和完善的函数有：

```c
static int init_main(void *arg) (kern/process/proc.c)  
static struct proc_struct *alloc_proc(void) (kern/process/proc.c)  
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags) (kern/process/proc.c)  
int do_wait(int pid, int *code_store) (kern/process/proc.c)  
static int user_main(void *arg) (kern/process/proc.c)  
static int kernel_execve(const char *name, unsigned char *binary, size_t size) (kern/process/proc.c)  
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size) (kern/process/proc.c)  
static int load_icode(unsigned char *binary, size_t size) (kern/process/proc.c)
```

整个功能模块的具体功能为：这一部分负责把系统从“只有内核线程在运行”推进到“第一个用户程序真正开始执行”。`proc_init` 先建立 `idleproc`，再由它创建 `init_main` 所在的内核线程；`init_main` 继续创建 `user_main`，并通过 `do_wait` 等待后续用户进程退出，保证第一个用户进程建立后，内核仍然能够收束整个启动流程。`user_main` 的职责不是长期执行业务逻辑，而是充当一个过渡内核线程，它通过嵌入到镜像中的用户程序二进制符号，调用 `kernel_execve` 把自己从内核线程转换成用户进程。`kernel_execve` 负责搭出从内核态回到用户态所需的执行上下文，`do_execve` 负责回收旧地址空间并触发新程序加载，`load_icode` 则负责解析 ELF、建立新的虚拟内存布局、装入代码段和数据段、创建用户栈并设置 trapframe，最终让 CPU 能从用户程序入口开始执行。

整个功能模块的注意事项为：这一条链路最容易出问题的地方是“上下文没有接上”。`kernel_execve` 不能只调用 `do_execve`，因为那样只完成了内存和进程映像的准备，却没有真正进入 trap 返回路径；`load_icode` 必须把用户态入口地址、用户栈指针和 `sstatus` 中的特权级信息设置正确，否则即使程序已经装入内存，也可能无法从 `sret` 正常回到用户态；`do_execve` 在替换当前进程映像时必须先处理旧 `mm` 的回收，再继续装载新程序，避免旧地址空间残留；`do_wait` 则要保证父子进程关系、僵尸态回收和睡眠唤醒逻辑一致，否则 `init_main` 可能无法正确等到用户进程退出。

整个功能模块应该满足的对外的接口为：从启动路径上看，`proc_init`、`kernel_thread`、`init_main`、`user_main`、`kernel_execve`、`do_execve`、`load_icode` 和 `do_wait` 组成了从内核启动到用户程序启动的完整对外接口；从用户可见行为上看，系统最终应该表现为第一个用户程序能够被正确装入、正确切换到用户态、并且在退出后被父进程正确等待和回收。`init_main` 还应该持续作为系统启动后的收尾控制点，确保用户进程链路结束后内核状态稳定。

整个功能模块可以借助的接口函数、结构体、数据结构为：`proc_struct`、`mm_struct`、`vma_struct`、`trapframe`、`elfhdr`、`proghdr`、`KERNEL_EXECVE` 与 `KERNEL_EXECVE2` 宏、`mm_map`、`dup_mmap`、`exit_mmap`、`page_insert`、`pgdir_alloc_page`、`lcr3/lsatp`、`user_mem_check`、`set_proc_name`、`wakeup_proc`、`schedule` 等接口都可能参与其中。`KSTACKSIZE`、`USTACKTOP`、`USTACKSIZE`、`SSTATUS_SPP`、`SSTATUS_SPIE`、`PTE_U`、`PTE_R/W/X/V` 等常量则决定了用户栈、权限位和 trapframe 的最终形态。

这一组函数共同构成的是“从内核线程过渡到第一个用户程序”的启动链。`init_main` 负责把启动过程拉到用户程序入口，`user_main` 负责选择真正要执行的用户二进制，`kernel_execve` 负责把执行流送进 trap 返回路径，`do_execve` 和 `load_icode` 负责完成地址空间替换与程序装载，而 `do_wait` 负责让启动阶段的父进程在逻辑上闭合。

#### 功能模块三：复制用户进程功能（设计提示词借助ai完成）

需要更新和完善的函数有：

```c
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) (kern/process/proc.c)  
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc) (kern/process/proc.c)  
int dup_mmap(struct mm_struct *to, struct mm_struct *from) (kern/mm/vmm.c)  
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share) (kern/mm/pmm.c)
```

整个功能模块的具体功能为：这一部分负责把父进程的执行环境复制成一个新的子进程，使子进程拥有自己的进程控制块、内核栈、trapframe 和用户地址空间映像。`do_fork` 是整条链路的总入口，它需要把子进程创建出来并接入进程树，同时准备好后续调度所需的执行上下文；`copy_mm` 是在 `lab4` 已有实现基础上的更新点，它决定新进程是与父进程共享地址空间还是复制地址空间，在本实验的普通 fork 路径中主要体现为复制用户地址空间；`dup_mmap` 负责按 `vma` 粒度复制父进程的合法虚拟内存区域；`copy_range` 则负责按页粒度复制页面内容并建立新的页表映射。整体上，这条链路保证了子进程既能继承父进程看到的程序和数据，又能在之后独立运行、独立调度、独立退出。

整个功能模块的注意事项为：进程复制既要复制“看得见的地址空间”，也要复制“运行所需的上下文”，因此 `do_fork` 里不能只做页表复制，还要保证 `trapframe`、内核栈、进程关系和 PID 都正确建立。`copy_mm` 在处理 `mm_struct` 时要兼顾引用计数和复制路径，普通 fork 与共享地址空间的线程创建不能混为一谈；`dup_mmap` 复制 `vma` 时要保持区间顺序和权限一致，避免新进程得到非法或重叠的地址空间；`copy_range` 在复制页面时要确保源页内容完整、目标页映射有效、权限位与原页面语义一致，同时注意页表更新和引用计数的正确性，避免子进程和父进程在后续运行中互相破坏地址空间。凡是涉及进程树、哈希表、状态位和内存映射的修改，都应该在合适的临界区内完成。

整个功能模块应该满足的对外的接口为：从用户角度看，`fork` 最终应该表现为“父进程得到子进程 PID，子进程从 0 返回”的标准语义；从内核角度看，`do_fork`、`copy_mm`、`dup_mmap`、`copy_range` 必须把进程控制块、虚拟内存区域和实际页映射都复制完整，并保证新进程能够被调度器立即看到。对于内核线程场景，`do_fork` 还需要支持不复制用户地址空间的特殊路径，使得内核线程和用户进程的创建都能复用同一套框架。

整个功能模块可以借助的接口函数、结构体、数据结构为：`alloc_proc`、`setup_kstack`、`copy_thread`、`get_pid`、`set_links`、`hash_proc`、`wakeup_proc`、`mm_create`、`setup_pgdir`、`lock_mm/unlock_mm`、`find_vma`、`get_pte`、`page_insert`、`page2kva`、`pte2page`、`alloc_page`、`current`、`proc_list`、`hash_list`、`proc_struct`、`mm_struct`、`vma_struct`、`trapframe` 都会直接影响复制过程。它们共同提供了“创建新进程、复制地址空间、接入进程树、准备调度”的基础能力。

这组函数共同完成的是“从父进程生成子进程”的完整过程。`do_fork` 负责组织全局流程，`copy_mm` 负责决定是否共享或复制内存映像，`dup_mmap` 负责复制虚拟内存区间，`copy_range` 负责复制实际页内容并建立新的映射，最终让子进程在逻辑上继承父进程、在物理上拥有自己的执行实体。

#### 功能模块四：其余系统调用功能（设计提示词借助ai完成）

要求实现的函数有：

```c
int do_exit(int error_code) (kern/process/proc.c)  
int do_yield(void) (kern/process/proc.c)  
int do_kill(int pid) (kern/process/proc.c)
```

整个功能模块的具体功能为：这一部分负责把用户态系统调用中最常见的三类进程控制动作落到内核中，分别是进程退出、主动让出 CPU 和终止指定进程。`do_exit` 负责处理当前进程结束时的资源回收与状态切换，它不仅要释放当前进程不再需要的用户地址空间，还要把进程标记成僵尸态，并通知父进程来完成最后的回收；`do_yield` 负责把当前进程标记为需要重新调度，让调度器尽快为它寻找新的运行机会；`do_kill` 则负责向目标进程发送退出意图，使其在合适的时机结束执行并进入退出流程。三者共同构成用户程序生命周期中的“结束、让出和终止”这条控制线。

整个功能模块的注意事项为：`do_exit` 不能让 `idleproc` 和 `initproc` 退出，且必须保证自己在变成僵尸前就已经完成必要的资源处理；进程退出时，父子关系、等待状态和僵尸回收都必须和 `do_wait` 的逻辑完全匹配，否则可能出现僵尸进程无人回收或者父进程永远睡眠的情况。`do_yield` 虽然实现简单，但它本质上是在影响调度器的决策，因此需要和 `need_resched`、当前进程状态以及中断时机保持一致；`do_kill` 在修改目标进程状态标志时也要考虑目标是否正在等待以及是否需要立即唤醒。凡是涉及进程状态转换的地方，都要避免在临界区之外留下半更新状态。

整个功能模块应该满足的对外的接口为：用户侧看到的是 `exit`、`yield`、`kill` 这类简单而清晰的进程控制接口；内核侧看到的是 `do_exit`、`do_yield`、`do_kill` 这三个与调度和资源回收紧密结合的处理入口。它们的共同目标不是单纯返回一个值，而是确保进程生命周期能在内核中闭合，并且和父进程等待、调度器切换、页面回收保持一致。

整个功能模块可以借助的接口函数、结构体、数据结构为：`current`、`schedule`、`wakeup_proc`、`user_mem_check`、`exit_mmap`、`put_pgdir`、`mm_destroy`、`unhash_proc`、`remove_links`、`local_intr_save/local_intr_restore`、`proc_struct`、`mm_struct`、`wait_state`、`PF_EXITING`、`PROC_ZOMBIE`、`WT_CHILD`、`WT_INTERRUPTED` 等接口和状态位都与这部分功能直接相关。它们共同保证退出、唤醒、等待和重新调度这几个动作能够在同一套进程状态机上协同工作。

这组函数共同完成的是“让进程结束并回到调度器”的收尾逻辑。`do_exit` 负责真正的退出与资源清理，`do_yield` 负责让出 CPU，`do_kill` 负责触发目标进程进入退出路径，三者与 `do_wait` 共同构成进程生命周期的闭环。

#### 扩展练习 Challenge

1.  实现 Copy on Write （COW）机制

    这个扩展练习涉及到本实验和上一个实验“虚拟内存管理”。在 ucore 操作系统中，当一个用户父进程创建自己的子进程时，父进程会把其申请的用户空间设置为只读，子进程可共享父进程占用的用户内存空间中的页面，这就是一个共享的资源。当其中任何一个进程修改此用户内存空间中的某页面时，ucore 会通过 page fault 异常获知该操作，并完成拷贝内存页面，使得两个进程都有各自的内存页面。这样一个进程所做的修改不会被另外一个进程可见了。请在 ucore 中实现这样的 COW 机制。

    由于 COW 实现比较复杂，容易引入 bug，请参考 https://dirtycow.ninja/ 看看能否在 ucore 的 COW 实现中模拟这个错误和解决方案。需要有解释。

    请提供实现代码、测试用例和设计报告（包括在 COW 情况下的各种状态转换，类似有限状态自动机的说明）。

    这是一个 big challenge。

2.  说明该用户程序是何时被预先加载到内存中的？与我们常用操作系统的加载有何区别，原因是什么？

### 项目组成

```text
├── boot
├── kern
│ ├── debug
│ │ ├── kdebug.c
│ │ └── ...
│ ├── mm
│ │ ├── memlayout.h
│ │ ├── pmm.c
│ │ ├── pmm.h
│ │ ├── vmm.c
│ │ └── vmm.h
│ ├── process
│ │ ├── proc.c
│ │ ├── proc.h
│ │ └── ...
│ ├── schedule
│ │ ├── sched.c
│ │ └── ...
│ ├── syscall
│ │ ├── syscall.c
│ │ └── syscall.h
│ └── trap
│   ├── trap.c
│   ├── trapentry.S
│   ├── trap.h
│   └── vectors.S
├── libs
├── tools
└── user
```

相对于实验4，lab5 主要增加了用户进程、系统调用和用户态到内核态的切换链路，因此改动重点集中在 `mm`、`process`、`syscall` 和 `trap` 这几部分。

`kern/mm/` 与本次实验关系最直接。`memlayout.h` 补充了用户虚拟地址空间相关的布局定义；`pmm.[ch]` 扩展了页表项管理和页复制/回收能力，支持 `do_fork` 的内存复制以及 `do_exit` 的地址空间释放；`vmm.[ch]` 则补齐了 `mm_struct`、`vma_struct` 相关操作，包括合法虚拟地址区间管理、父子进程地址空间复制，以及用户地址空间与内核地址空间之间的安全拷贝检查。

`kern/process/` 是用户进程生命周期的核心。`proc.[ch]` 扩展了 `proc_struct`，并补齐了进程创建、复制、执行、退出、等待和调度相关的逻辑，包括 `alloc_proc`、`proc_init`、`kernel_thread`、`do_fork`、`copy_mm`、`do_execve`、`load_icode`、`do_exit`、`do_yield`、`do_wait`、`do_kill`、`cpu_idle` 等函数。这些函数把“创建进程、装载程序、复制地址空间、回收资源、回到调度器”连成了一条完整链路。

`kern/syscall/` 负责把用户态请求转发到内核处理函数。`syscall.c` 读取系统调用号和参数，并分发到 `do_exit`、`do_fork`、`do_wait`、`do_execve`、`do_yield`、`do_kill`、`getpid`、`putc`、`pgdir` 等对应入口，使用户态能通过统一接口获得内核服务。

`kern/trap/` 负责异常和中断的入口与返回。`trap.c` 识别 `ecall` 等异常来源，并将用户态系统调用转发给 `syscall`；`trapentry.S` 则负责保存和恢复上下文，保证用户态与内核态切换时寄存器和栈状态保持一致。对于 lab5 来说，这部分是系统调用能否顺利返回用户态的关键。

`kern/schedule/` 负责调度。`sched.c` 通过 `schedule` 和 `wakeup_proc` 等接口，把处于可运行状态的进程选出来并切换到 CPU 上执行。`proc_run` 与底层上下文切换逻辑配合，完成进程之间的切换。

`user/` 目录下新增了用户程序和用户库。用户程序用于验证 `fork`、`exec`、`wait`、`exit`、`yield` 等系统调用是否正确；用户库则把这些接口包装成普通 C 函数，并通过系统调用进入内核。

## 用户进程管理

### 实验流程概述

我们在 lab1 中已经讲解过 RISC-V 的特权级。这里简要回顾一下：

-   M态（Machine）：最高权限，运行固件OpenSBI，负责早期引导并向 S 态提供服务接口。
-   S态（Supervisor）：内核态，操作系统内核运行在此级别，管理内存、中断等。
-   U态（User）：用户态，运行普通用户程序，权限受限。

之前我们已经实现了内存的管理和内核进程的建立，但是那都是在内核态，接下来我们将在用户态运行一些程序。

用户程序，也就是我们在计算机系前几年课程里一直在写的那些程序，到底怎样在操作系统上跑起来？

首先需要编译器把用户程序的源代码编译为可以在CPU执行的目标程序，这个目标程序里，既要有执行的代码，又要有关于内存分配的一些信息，告诉我们应该怎样为这个程序分配内存。

我们先不考虑怎样在ucore里运行编译器（编译器其实也是用户程序的一种~~感兴趣的同学可以研究一下怎么把编译原理的课设项目运行在ucore中~~），只考虑ucore如何把编译好的用户程序运行起来。这需要给它分配一些内存，把程序代码加载进来，建立一个进程，然后通过调度让这个用户进程开始执行。

用户程序与内核程序有着本质区别：它们运行在受限制的用户态，无法直接分配内存、访问硬件或执行特权指令。这就产生了一个核心问题：用户程序如何安全地获取操作系统服务？

**系统调用**正是连接用户态与内核态的桥梁。它为用户程序提供了一套标准化的服务接口，使得用户程序能够通过受控的方式使用内核功能。

当用户程序需要操作系统提供服务时，比如一个 C 程序调用 `printf()` 函数进行输出，标准库会将输出请求转换为 `write` 系统调用。这个过程涉及从用户态到内核态的特权级切换，具体通过 `ecall` 指令实现。`ecall` 指令会触发一个异常事件，使 CPU 从用户态提升到内核态，并跳转到预设的中断处理程序 `trap` 中，在其中层层转发到系统调用函数 `write` 进行处理，之后再通过 `sret` 指令返回到用户态，到此，中断处理程序的纸飞机终于飞到系统调用手里。

当我们将视线转回到ucore的时候，就会遇到一个**鸡生蛋还是蛋生鸡**的问题，也就是，我们应该如何第一次从S态进入到U态的用户进程呢？

我们之前的内容提到的都是从用户态主动或被动地进入内核态，然后再从内核态返回到用户态的完整流程。但是在ucore的初始化进程中，我们始终处于内核态，因此，我们并不能像之后的用户进程一样完成这样一次完整的特权级切换循环，而是需要在**内核态**触发一个异常，从而借助异常处理机制的返回流程进行上下文的切换，从而第一次进入到用户进程。

关于用户进程的理论讲解可查看附录`用户进程的特征`。

### 用户进程

前面的内容已经把内核线程和调度流程搭起来了，接下来要解决的是：系统怎样第一次进入用户态，并让一个真正的用户程序开始执行。

先看一眼原先的 `init_main`。它还是一个很纯粹的内核线程入口，主要做的是打印信息和结束自身，不承担用户进程启动的职责：

```c
// kern/process/proc.c (原先的 init_main)
static int init_main(void *arg) {
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
    cprintf("To U: \"%s\".\n", (const char *)arg);
    cprintf("To U: \"en.., Bye, Bye. :)\"\n");
    return 0;
}
```

在 `proc_init()` 里，系统会先构造 `idleproc`，再由它创建 `init_main` 所在的内核线程。`init_main` 的作用不是直接完成业务逻辑，而是作为启动阶段的过渡点，负责继续创建 `user_main`，并在后续等待用户进程结束。这样一来，内核启动阶段就有了一个明确的“从内核线程过渡到用户程序”的入口。

`user_main` 的职责也很清晰，它会从编译进镜像的用户程序中取出目标二进制，然后调用 `kernel_execve`。这里的关键不在于“取到一个文件名”，而在于“把内核线程切换成用户进程”：一旦 `kernel_execve` 完成上下文构造并返回到 trap 的通用返回路径，`user_main` 就不再是普通内核线程，而会开始执行用户程序入口。

在进入 `user_main` 之前，`proc.c` 里还定义了一组用于发起 `kernel_execve` 的宏。它们的作用是把“用户程序名”和“镜像中对应的二进制符号”联系起来，从而让内核线程能够很方便地选中某个用户程序并启动它。它们本质上是 `kernel_execve` 的上层封装，供 `user_main` 直接调用。

```c
// kern/process/proc.c
#define __KERNEL_EXECVE(name, binary, size) ({                          \
            cprintf("kernel_execve: pid = %d, name = \"%s\".\n",        \
                    current->pid, name);                                \
            kernel_execve(name, binary, (size_t)(size));                \
        })

#define KERNEL_EXECVE(x) ({                                             \
            extern unsigned char _binary_obj___user_##x##_out_start[],  \
                _binary_obj___user_##x##_out_size[];                    \
            __KERNEL_EXECVE(#x, _binary_obj___user_##x##_out_start,     \
                            _binary_obj___user_##x##_out_size);         \
        })

#define __KERNEL_EXECVE2(x, xstart, xsize) ({                           \
            extern unsigned char xstart[], xsize[];                     \
            __KERNEL_EXECVE(#x, xstart, (size_t)xsize);                 \
        })

#define KERNEL_EXECVE2(x, xstart, xsize)        __KERNEL_EXECVE2(x, xstart, xsize)
```

这组宏的意义在于：`KERNEL_EXECVE(exit)` 这样的写法，会把用户程序 `exit` 对应的镜像符号传给 `kernel_execve`，从而把“选择哪个用户程序”这件事变得更自然。对学生来说，重点不是记住宏展开细节，而是理解 `user_main` 通过这层封装，最终会调用到真正的 `kernel_execve`。

用户程序本身通常通过 `exit`、`fork`、`wait` 等接口验证系统调用链路。用户态库函数的工作是把调用包装成系统调用请求，而不是直接碰内核接口；比如 `cprintf` 需要通过 `sys_putc` 输出字符，因为用户态不能直接调用内核里的控制台接口。这一点也说明了用户态和内核态之间的边界：用户程序只能通过系统调用获得服务，不能绕过权限检查直接访问内核资源。

`user/` 目录下存放的就是这类用户程序。它们会在构建镜像时被编译进去，运行时再由 `kernel_execve` 选中并加载。下面这个 `exit` 程序就是实验中最典型的测试用例之一，它会验证 `fork`、`waitpid` 和 `exit` 这些系统调用是否能够正确协作。

```c
// user/exit.c
#include <stdio.h>
#include <ulib.h>

int magic = -0x10384;

int main(void) {
    int pid, code;
    cprintf("I am the parent. Forking the child...\n");
    if ((pid = fork()) == 0) {
        cprintf("I am the child.\n");
        yield();
        yield();
        yield();
        yield();
        yield();
        yield();
        yield();
        exit(magic);
    }
    else {
        cprintf("I am parent, fork a child pid %d\n",pid);
    }
    assert(pid > 0);
    cprintf("I am the parent, waiting now..\n");

    assert(waitpid(pid, &code) == 0 && code == magic);
    assert(waitpid(pid, &code) != 0 && wait() != 0);
    cprintf("waitpid %d ok.\n", pid);

    cprintf("exit pass.\n");
    return 0;
}
```

这个程序的作用很直接：先制造父子进程，再通过 `yield` 让调度器来回切换，最后让父进程等待子进程退出并检查退出码。它是验证进程复制、调度和退出链路是否正常的核心测试之一。

这些用户程序依赖的系统调用接口都封装在 `user/libs/ulib.c` 里。它们的设计方式很朴素：用户代码调用的是普通 C 函数，底层真正做事的是 `sys_*` 系统调用封装。

```c
// user/libs/ulib.c
#include <defs.h>
#include <syscall.h>
#include <stdio.h>
#include <ulib.h>

void exit(int error_code) {
    sys_exit(error_code);
    // 执行完 sys_exit 后，按理说进程就结束了，后面的语句不应该再执行，
    // 所以执行到这里就说明 exit 失败了。
    cprintf("BUG: exit failed.\n");
    while (1);
}

int fork(void) { return sys_fork(); }
int wait(void) { return sys_wait(0, NULL); }
int waitpid(int pid, int *store) { return sys_wait(pid, store); }
void yield(void) { sys_yield(); }
int kill(int pid) { return sys_kill(pid); }
int getpid(void) { return sys_getpid(); }
```

其中 `exit` 这个封装尤其重要，因为它清楚地表达了一个事实：一旦系统调用成功，后面的语句就不应该继续执行，所以如果程序真的运行到了 `cprintf("BUG: exit failed.\n")`，就说明退出路径没有按预期完成。

在用户程序里使用的 `cprintf()` 也是在 `user/libs/stdio.c` 里重新实现的。和内核里的打印接口相比，它最大的不同是不能直接访问底层控制台，而必须通过 `sys_putc()` 进入内核完成输出。

```c
// user/libs/stdio.c
#include <defs.h>
#include <stdio.h>
#include <syscall.h>

/* *
 * cputch - writes a single character @c to stdout, and it will
 * increace the value of counter pointed by @cnt.
 * */
static void
cputch(int c, int *cnt) {
    sys_putc(c); // 系统调用
    (*cnt) ++;
}

/* *
 * vcprintf - format a string and writes it to stdout
 *
 * The return value is the number of characters which would be
 * written to stdout.
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want cprintf() instead.
 * */
int
vcprintf(const char *fmt, va_list ap) {
    int cnt = 0;
    vprintfmt((void*)cputch, &cnt, fmt, ap);
    // 注意这里复用了 vprintfmt, 但是传入了 cputch 函数指针
    return cnt;
}
```

之所以要这样做，是因为用户态没有权限直接调用 `sbi_console_putchar()`。用户程序如果想打印字符，只能先通过系统调用进入内核，再由内核帮它完成控制台输出。

这一章最重要的理解点，是把 `init_main`、`user_main`、`kernel_execve` 和用户程序本身连成一条清晰的启动链：`init_main` 先把系统从纯内核启动推进到准备运行用户程序的阶段，`user_main` 再选中镜像里的目标用户程序，`kernel_execve` 负责把执行流送入返回用户态的路径，最后由用户程序真正开始执行。这样看下来，这一章讲的其实就是“第一个用户程序是怎样被选中、装入并启动起来的”。

### 系统调用实现

系统调用是用户态程序获得内核服务的标准入口。用户程序并不直接操作内核数据结构，而是先把系统调用号和参数放入约定位置，再通过 `ecall` 进入异常处理流程，由内核根据当前进程的 trapframe 完成分发。

用户态系统调用的入口已经由我们实现好了，用户程序只要调用这些封装函数即可。

```c
// libs/unistd.h
/* syscall number */
#define SYS_exit            1
#define SYS_fork            2
#define SYS_wait            3
#define SYS_exec            4
#define SYS_clone           5
#define SYS_yield           10
#define SYS_sleep           11
#define SYS_kill            12
#define SYS_gettime         17
#define SYS_getpid          18
#define SYS_brk             19
#define SYS_mmap            20
#define SYS_munmap          21
#define SYS_shmem           22
#define SYS_putc            30
#define SYS_pgdir           31
// user/libs/syscall.c
#include <defs.h>
#include <unistd.h>
#include <stdarg.h>
#include <syscall.h>
#define MAX_ARGS            5
static inline int syscall(int num, ...) {
    // va_list, va_start, va_arg 都是 C 语言处理参数个数不定的函数的宏
    // 在 stdarg.h 里定义。
    va_list ap; // ap: 参数列表(此时未初始化)
    va_start(ap, num); // 初始化参数列表, 从 num 开始
    uint64_t a[MAX_ARGS];
    int i, ret;
    for (i = 0; i < MAX_ARGS; i ++) {
        a[i] = va_arg(ap, uint64_t);
    }
    va_end(ap);
    asm volatile (
        "ld a0, %1\n"
        "ld a1, %2\n"
        "ld a2, %3\n"
        "ld a3, %4\n"
        "ld a4, %5\n"
        "ld a5, %6\n"
        "ecall\n"
        "sd a0, %0"
        : "=m" (ret)
        : "m"(num), "m"(a[0]), "m"(a[1]), "m"(a[2]), "m"(a[3]), "m"(a[4])
        :"memory");
    return ret;
}
int sys_exit(int error_code) { return syscall(SYS_exit, error_code); }
int sys_fork(void) { return syscall(SYS_fork); }
int sys_wait(int pid, int *store) { return syscall(SYS_wait, pid, store); }
int sys_yield(void) { return syscall(SYS_yield); }
int sys_kill(int pid) { return syscall(SYS_kill, pid); }
int sys_getpid(void) { return syscall(SYS_getpid); }
int sys_putc(int c) { return syscall(SYS_putc, c); }
```

这一章的核心链路可以概括成三层。第一层是用户态封装，它把 `fork`、`wait`、`exec`、`exit`、`yield`、`kill`、`getpid`、`putc` 等接口统一转换成系统调用请求。第二层是异常入口，它识别当前异常是否来自用户态系统调用，如果是，就调整返回地址并进入系统调用分发。第三层是内核语义层，它把系统调用号映射成对应的内核处理函数，例如进程创建、进程退出、等待子进程、程序替换、输出字符以及查看页表等。

理解这部分时，最重要的是分清“参数传递”和“语义执行”这两件事。用户态只负责把请求送进内核，真正的权限检查、地址空间检查、进程状态变化和资源更新都在内核中完成。系统调用返回时，内核会把结果写回到 trapframe 中，用户程序随后继续执行，这就是 `ecall` 和 `sret` 共同形成的完整闭环。

这一部分的知识点也直接连接到后面的进程管理：`fork` 会走向进程复制，`exec` 会走向新程序加载，`exit` 会走向进程退出，`wait` 会走向僵尸回收。把这些接口放在同一条调用链里理解，能更清楚地看出用户进程生命周期是如何被内核组织起来的。

### 第一次进入用户态

`do_execve` 和 `load_icode` 解决的是“把新程序装进当前进程地址空间”的问题，而 `kernel_execve` 解决的是“装好以后，怎样真正切回用户态执行”的问题。这两件事必须配合起来，系统才能第一次从内核态进入用户态。

`do_execve` 的核心职责是回收旧地址空间、建立新地址空间，并让当前进程的映像变成新的程序。它会清理旧的 `mm`，调用 `load_icode` 装入 ELF 程序，并更新进程名称和执行上下文。`load_icode` 则负责完成更细的工作：建立新的页表和用户栈，装入代码段与数据段，并把 trapframe 配置成一个可以从用户入口地址开始执行的状态。

但只做完这些还不够，因为内核此时仍然停留在 S 态。`kernel_execve` 的关键作用，就是把当前进程临时切换到一个适合 trap 返回的上下文，让 CPU 走通用的异常返回路径，最终通过 `sret` 进入 U 态。换句话说，`do_execve` 负责“准备好”，`kernel_execve` 负责“真正切过去”。

那么我们如何实现 `kernel_execve()` 函数？能否直接调用 `do_execve()`？

```c
static int kernel_execve(const char *name, unsigned char *binary, size_t size) {
    int64_t ret = 0, len = strlen(name);
    ret = do_execve(name, len, binary, size);
    cprintf("ret = %d\n", ret);
    return ret;
}
```

这样做行不通。原因在于，`do_execve()` 只负责把新的程序映像和执行上下文准备好，却没有让 CPU 真正走到返回用户态的那条路径。如果内核里只是直接调用 `do_execve()`，执行流仍然会停留在 S 态，新的用户程序并不会自动开始跑起来。

系统中的用户态切换，依赖的是中断处理的返回路径，也就是最终要走到 `sret`。对于正常的用户态 `exec()` 调用，这一步会在系统调用返回时自动完成；但这里不一样，我们是在内核线程上下文里启动第一个用户程序，并没有现成的用户态异常返回可以借用。

目前我们在 `S mode` 下，所以不能通过 `ecall` 重新触发一次系统调用进入返回路径。一个取巧的办法是人造一次 `ebreak` 异常，再借助异常处理里的系统调用返回路径完成切换。不过这种做法比较绕，理解上也不够直接。

另一种更清晰的方法，是手动构造一个新的 `trapframe`，把它放到当前进程的内核栈顶，再主动跳转到 `__trapret`。这样就能直接复用中断返回路径：先恢复寄存器，再执行 `sret`，最终顺利从内核态切回用户态。这个思路才是 `kernel_execve` 的核心。

理解这条链路时，最重要的是 trapframe 里的三个信息：入口地址决定从哪里开始执行，用户栈顶决定用户程序的栈空间，`sstatus` 里的特权级位决定 `sret` 返回到什么模式。只要这三项都正确，第一个用户程序就能从镜像中的入口点开始运行。

### 中断处理

由于用户进程比起内核进程多了一个"用户栈"，也就是每个用户进程会有两个栈，一个内核栈一个用户栈，所以中断处理的代码 `trapentry.S` 要有一些小变化。关注用户态产生中断时，内核栈和用户栈两个栈顶指针的移动。

```armasm
# kern/trap/trapentry.S

#include <riscv.h>

# 若在中断之前处于 U mode(用户态)
# 则 sscratch 保存的是内核栈地址
# 否则中断之前处于 S mode(内核态)，sscratch 保存的是 0

    .altmacro
    .align 2
    .macro SAVE_ALL
    LOCAL _restore_kernel_sp
    LOCAL _save_context

    # If coming from userspace, preserve the user stack pointer and load
    # the kernel stack pointer. If we came from the kernel, sscratch
    # will contain 0, and we should continue on the current stack.
    csrrw sp, sscratch, sp #这里交换了sp和sccratch寄存器
    #sp为0，说明之前是内核态，我们刚才把内核栈指针换到了sscratch, 需要再拿回来
    #sp不为0 时，说明之前是用户态，sp里现在存的就是内核栈指针，sscratch里现在是用户栈指针
    #sp不为0，就跳到_save_context, 跳过_restore_kernel_sp的代码
    bnez sp, _save_context     

_restore_kernel_sp:
    csrr sp, sscratch #刚才把内核栈指针换到了sscratch, 需要再拿回来
_save_context:
    #分配栈帧
    addi sp, sp, -36 * REGBYTES 
    # save x registers
    STORE x0, 0*REGBYTES(sp)
    STORE x1, 1*REGBYTES(sp)
    STORE x3, 3*REGBYTES(sp)
    STORE x4, 4*REGBYTES(sp)
    STORE x5, 5*REGBYTES(sp)
    STORE x6, 6*REGBYTES(sp)
    STORE x7, 7*REGBYTES(sp)
    STORE x8, 8*REGBYTES(sp)
    STORE x9, 9*REGBYTES(sp)
    STORE x10, 10*REGBYTES(sp)
    STORE x11, 11*REGBYTES(sp)
    STORE x12, 12*REGBYTES(sp)
    STORE x13, 13*REGBYTES(sp)
    STORE x14, 14*REGBYTES(sp)
    STORE x15, 15*REGBYTES(sp)
    STORE x16, 16*REGBYTES(sp)
    STORE x17, 17*REGBYTES(sp)
    STORE x18, 18*REGBYTES(sp)
    STORE x19, 19*REGBYTES(sp)
    STORE x20, 20*REGBYTES(sp)
    STORE x21, 21*REGBYTES(sp)
    STORE x22, 22*REGBYTES(sp)
    STORE x23, 23*REGBYTES(sp)
    STORE x24, 24*REGBYTES(sp)
    STORE x25, 25*REGBYTES(sp)
    STORE x26, 26*REGBYTES(sp)
    STORE x27, 27*REGBYTES(sp)
    STORE x28, 28*REGBYTES(sp)
    STORE x29, 29*REGBYTES(sp)
    STORE x30, 30*REGBYTES(sp)
    STORE x31, 31*REGBYTES(sp)

    # get sr, epc, tval, cause
    # Set sscratch register to 0, so that if a recursive exception
    # occurs, the exception vector knows it came from the kernel
    #如果之前是用户态产生的中断，用户栈指针从sscratch里挪到了s0寄存器， sscratch清零
    csrrw s0, sscratch, x0
    csrr s1, sstatus
    csrr s2, sepc
    csrr s3, 0x143 #stval
    csrr s4, scause

    STORE s0, 2*REGBYTES(sp) #如果之前是用户态发生中断，此时用户栈指针存到了内存里
    STORE s1, 32*REGBYTES(sp)
    STORE s2, 33*REGBYTES(sp)
    STORE s3, 34*REGBYTES(sp)
    STORE s4, 35*REGBYTES(sp)
    .endm

    .macro RESTORE_ALL
    LOCAL _save_kernel_sp
    LOCAL _restore_context

    LOAD s1, 32*REGBYTES(sp)
    LOAD s2, 33*REGBYTES(sp)

    andi s0, s1, SSTATUS_SPP #可以通过SSTATUS_SPP的值判断之前是用户态还是内核态
    bnez s0, _restore_context

_save_kernel_sp:
    # Save unwound kernel stack pointer in sscratch
    addi s0, sp, 36 * REGBYTES
    csrw sscratch, s0
_restore_context:
    csrw sstatus, s1
    csrw sepc, s2

    # restore x registers
    LOAD x1, 1*REGBYTES(sp)
    LOAD x3, 3*REGBYTES(sp)
    LOAD x4, 4*REGBYTES(sp)
    LOAD x5, 5*REGBYTES(sp)
    LOAD x6, 6*REGBYTES(sp)
    LOAD x7, 7*REGBYTES(sp)
    LOAD x8, 8*REGBYTES(sp)
    LOAD x9, 9*REGBYTES(sp)
    LOAD x10, 10*REGBYTES(sp)
    LOAD x11, 11*REGBYTES(sp)
    LOAD x12, 12*REGBYTES(sp)
    LOAD x13, 13*REGBYTES(sp)
    LOAD x14, 14*REGBYTES(sp)
    LOAD x15, 15*REGBYTES(sp)
    LOAD x16, 16*REGBYTES(sp)
    LOAD x17, 17*REGBYTES(sp)
    LOAD x18, 18*REGBYTES(sp)
    LOAD x19, 19*REGBYTES(sp)
    LOAD x20, 20*REGBYTES(sp)
    LOAD x21, 21*REGBYTES(sp)
    LOAD x22, 22*REGBYTES(sp)
    LOAD x23, 23*REGBYTES(sp)
    LOAD x24, 24*REGBYTES(sp)
    LOAD x25, 25*REGBYTES(sp)
    LOAD x26, 26*REGBYTES(sp)
    LOAD x27, 27*REGBYTES(sp)
    LOAD x28, 28*REGBYTES(sp)
    LOAD x29, 29*REGBYTES(sp)
    LOAD x30, 30*REGBYTES(sp)
    LOAD x31, 31*REGBYTES(sp)
    # restore sp last
    LOAD x2, 2*REGBYTES(sp) #如果是用户态产生的中断，此时sp恢复为用户栈指针
    .endm

    .globl __alltraps
__alltraps:
    SAVE_ALL

    move  a0, sp
    jal trap
    # sp should be the same as before "jal trap"

    .globl __trapret
__trapret:
    RESTORE_ALL
    # return from supervisor call
    sret

    .globl forkrets
forkrets:
    # set stack to this new process's trapframe
    move sp, a0
    j __trapret
```

### 进程退出

进程结束时，退出动作并不是“一次性把所有资源都删掉”，而是要把能由自己处理的部分先处理掉，再把剩下必须由父进程完成的部分交出去。原因很简单：进程在执行退出逻辑时仍然依赖自己的内核栈和进程控制块，因此这两部分不能由自己直接释放，只能由父进程在 `wait` 阶段来完成最终回收。

用户态的 `exit` 只是把退出意图送进内核，真正的工作由内核中的退出路径完成。退出路径会先回收用户地址空间和相关页表，再把当前进程标记成僵尸态，并记录退出码。随后它会检查父进程是否正在等待，如果父进程处于等待子进程的状态，就把父进程唤醒；如果当前进程还有子进程，则要把这些子进程重新挂到 `initproc` 名下，避免父进程退出后留下无人管理的进程树分支。

`do_exit` 的最后一步是交给调度器。进程一旦进入僵尸态，继续运行已经没有意义，因此必须立即让出 CPU，由调度器选择下一个可以运行的进程。之后，父进程会在 `do_wait` 中完成真正的收尾回收，包括清除进程树关系、释放内核栈和进程控制块。

这部分内容最重要的理解点是：退出不是单独一个函数的事情，而是 `exit`、`do_exit`、`do_wait`、调度器和进程树管理共同协作的结果。只要这条链路任意一环处理不一致，就可能出现僵尸进程、孤儿进程或者父进程永远睡眠的问题。

## 分支任务：gdb 调试系统调用以及返回

之前，我们使用双重gdb的方案观测了qemu将一个虚拟地址翻译成物理地址的整个过程。那么这一次，我们温故而知新，使用同一套方案来观察操作系统中一个至关重要的机制——系统调用的完整流程。

系统调用是用户程序与操作系统内核交互的核心桥梁。当运行在受限的用户态(U mode)的程序需要获得内核服务时（如读写文件、分配内存），必须通过**系统调用**请求内核在更高特权的内核态(S mode)代为执行。这种**特权级的隔离**保障了系统的安全性和稳定性。

通过本次调试，我们将能够亲眼观察从用户态触发系统调用到返回用户态的完整过程，包括特权级切换、参数传递、内核处理等关键环节。

本次我们使用的双重gdb方案和lab2中调试地址翻译流程的操作流程都是基本相同的。我们先分析一下**调试思路**。

>   **思考**
>
>   回想一下我们之前调试地址翻译流程的调试思路，我们是找到了一个地址，并且知道这个地址一定会被访问，并且第一次访问的时候一定需要查找页表，我们直接在必经之路上打上**条件断点**来判断当前传入的地址是否是我们要观测的地址，然后直接启动内核执行就可以了，当ucore访问到这个地址的时候，qemu就会自动停下，我们就可以跟踪代码的执行。

那么系统调用的观测是不是也可以这么做呢？

应该是可以的，~~用大模型~~查看一下用户程序，可以发现，所有的系统调用接口最后都是对`syscall`这个函数的封装，在这个函数中，将参数放在指定的位置之后，使用内联汇编调用ecall，从而触发中断，进入到我们之前设置的中断入口点来进行中断处理流程。

那么很显然，这个地方的`ecall`就是一个相当合适的观测点，我们可以让ucore运行到这个`ecall`指令之前停住，然后为`qemu`设置合适的断点（同理，可以询问大模型：`qemu`的源码中是如何处理`ecall`指令的，给我找一下关键的代码和流程），随后单步执行这条指令，`qemu`就会及时打住执行，我们就可以继续跟踪`qemu`的代码执行逻辑，来观测它是如何处理`ecall`的。

等到`ecall`处理完之后，**移除掉原本的断点**，防止之后的执行被莫名打断，然后根据中断处理的流程找一下，系统调用执行结束之后，控制流是如何返回到用户态的，我们需要让ucore停在返回用户态之前的`sret`指令处，再次重复之前的流程，也就是找到`qemu`处理`sret`指令的关键代码，并设置断点跟踪执行。

### 实际操作

而当我们真正开始进行调试的时候，可能遇到的第一个问题就是，在前四章里面，我们都是处于内核态的，而这一次，我们调试的重点在于从**用户态进入内核态再返回**（也就是我们调试的实质上是一个运行在你的linux环境中的qemu上运行的内核上运行的用户程序），那么很自然的，我们按照先前的想法在用户态的syscall函数处打下一个断点，然后就会喜提一个奇怪的输出：

```bash
(gdb) b kern_entry
Breakpoint 1 at 0xffffffffc0200000: file kern/init/entry.S, line 10.
(gdb) b user/libs/syscall.c:26
No source file named user/libs/syscall.c.
<future shared library load? (y or [n]) y
Breakpoint 2 (user/libs/syscall.c:26) pending.
(gdb) b syscall
Breakpoint 3 at 0xffffffffc0205180: file kern/syscall/syscall.c, line 83.
```

这个输出的结果是什么意思呢（**要不你问问大模型吧**），其实就是目前gdb找到的"调试信息"中找不到用户程序的`libs`等内容，也就是说，我们目前的`makefile`中编写的用于调试的`make debug`和`make gdb`这两个目标只加载了内核本身的调试信息，而用户程序的调试信息并没有加载进去。

**模型使用示例** 向一个你使用顺手的大模型提问，我目前的内核是如何将用户程序加载进去的，如果我想要调试内核上运行的用户程序，是不是需要将用户程序的符号表也都加载进去，我应该怎么操作，为什么使用makedebug和makegdb调试的时候就会自动将内核符号表加载进去。

之后大模型就会给出一些解释和方案，根据项目中的文件夹和代码进行分析，大概信息如下。

```text
在 Lab5 中，用户程序并不是像现代 Linux 那样存储在磁盘文件系统（如 ext4）中的。为了简化实验，ucore 采用了一种 "Link-in-Kernel" (链接进内核) 的方式。
...
如何加载用户程序符号表
如果你想调试 exit.c，你需要手动告诉 GDB 加载这个文件的符号表。
操作步骤：
启动调试：运行 make debug 或 make qemu + make gdb。
加载符号：在 GDB 提示符下输入：
(gdb) add-symbol-file obj/__user_exit.out
GDB 会提示确认，输入 y。
不需要指定地址，因为 user.ld 已经定义了它是静态链接到 0x800020 的，GDB 会自动从 ELF 头中读取这个地址。
打断点：
现在 GDB 认识用户程序的函数了。
(gdb) b user/exit.c:9  # 推荐：直接对源文件打断点
```

那么我们只需要在`make gdb`命令之后，在gdb命令行中执行对应的命令，就可以正确的加载用户程序的调试信息了，之后我们就可以在用户库函数的`syscall`处打上断点了：

```bash
(gdb) add-symbol-file obj/__user_exit.out
add symbol table from file "obj/__user_exit.out"
(y or n) y
Reading symbols from obj/__user_exit.out...
(gdb) break user/libs/syscall.c:18
Breakpoint 1 at 0x8000f8: file user/libs/syscall.c, line 19.
```

那么我们只需要让ucore继续执行，就会在第一次执行到用户态的`syscall`函数的时候停住，此时，可能会遇到第二个问题，因为`syscall`是一个c函数，而我们真正关注的是这个c函数的内联汇编中的`ecall`指令，看起来很奇怪，不过不要被吓住，毕竟本质上cpu上执行的都是汇编指令，我们可以直接使用`si`来单步执行汇编指令，gdb中也有一些命令来帮助我们显示当前指令之后的几条汇编指令，我们就可以找到那个我们真正关心的指令——`ecall`，我们控制ucore执行到`ecall`指令之前：

```bash
(gdb) si
0x0000000000800104      19          asm volatile (
1: x/7i $pc
=> 0x800104 <syscall+44>:       ecall  
   0x800108 <syscall+48>:
    sd  a0,28(sp)
   0x80010c <syscall+52>:
    lw  a0,28(sp)
   0x80010e <syscall+54>:
    addi        sp,sp,144
   0x800110 <syscall+56>:       ret    
   0x800112 <sys_exit>: mv      a1,a0  
   0x800114 <sys_exit+2>:
    li  a0,1
(gdb) i r $pc
pc             0x800104 0x800104 <syscall+44>
```

此时，我们就需要为`qemu`打上断点了，目前，我们执行`make debug`的终端中由于调试ucore的`gdb`打断了ucore的执行而卡住，而调试ucore的`gdb`在等待我们的下一步指令，而附加到`qemu`的`gdb`中显示为`Continuing`，我们应该如何在qemu中添加一个断点呢？只需要在这个显示`Continuing`的终端中按下`ctrl + C`即可。

这时我们就可以像之前一样为`qemu`打下断点，然后继续执行`qemu`，接着让ucore执行`ecall`指令，我们就会发现attach在`qemu`上的`gdb`卡在了我们打下的断点处，我们就可以从这里开始跟踪执行，当`ecall`的处理完成之后，我们可以让ucore继续执行，停在`sret`指令前一句，并执行类似的操作，跟踪`sret`的处理流程。

到此，我们就完整的观测了一个从U态触发系统调用进入到S态，并在S态进行系统调用的处理，处理结束之后返回U态的过程，尤其是，我们细致的观测了qemu是如何模拟硬件进行`ecall`和`sret`两个指令的处理的。

>   **最后的小建议**
>
>   调试过程中可能会遇到各种奇怪的问题，比如断点不触发、程序跑飞等等。别慌！ 这正是学习的机会。把错误信息、你的操作步骤、以及你的困惑一起扔给大模型，它会给你提供排查思路。记住，大模型不只是帮你写代码的工具，更是你学习的智能助手。用好它，你就能在复杂系统的探索路上走得更远。

### 调试要求

1.  在大模型的帮助下，完成整个调试的流程，观察一下ecall指令和sret指令是如何被qemu处理的，并简单阅读一下调试中涉及到的qemu源码，解释其中的关键流程。
2.  在执行ecall和sret这类汇编指令的时候，qemu进行了很关键的一步——指令翻译（TCG Translation），了解一下这个功能，思考一下另一个双重gdb调试的实验是否也涉及到了一些相关的内容。
3.  记录下你调试过程中比较~~抓马~~有趣的细节，以及在观察模拟器通过软件模拟硬件执行的时候了解到的知识。
4.  记录实验过程中，有哪些通过大模型解决的问题，记录下当时的情景，你的思路，以及你和大模型交互的过程。

## 实验报告要求

1.  基于 `markdown` 格式完成实验报告，以文本说明为主。
2.  说明自己对本章节整体逻辑线的理解，即本章节是按照什么顺序、围绕哪些功能逐步实现的。
3.  说明自己对每个功能的核心函数或功能模块的理解。
4.  说明每个练习的 `spec` 是怎么写的，`prompt` 是怎么写的。
5.  说明在使用提示词让 AI 生成代码的过程中出现了什么 bug，以及你是怎么解决的。
6.  列出你认为本实验中重要的知识点，以及与对应 OS 原理中的知识点，并简要说明二者的含义、关系和差异。
7.  列出你认为 OS 原理中很重要、但在本实验中没有对应上的知识点。

从 oslab 网站上取得实验代码后，进入目录 `labcodes/lab5`，完成实验要求的各个练习。在目录 `labcodes/lab5` 下存放实验报告，推荐使用 `markdown` 格式。每个小组建立一个 gitee 或 github 仓库，对于 `lab5` 中的编程任务，完成编写之后，再通过 `git push` 命令把代码和报告上传到仓库，最后请提前或按时提交到 git 网站。

注意有“lab5”的注释，代码中所有需要完成的地方（challenge 除外）都有“lab5”和“YOUR CODE”的注释，请在提交时特别注意保持注释，并将“YOUR CODE”替换为自己的学号，并且将所有标有对应注释的部分填上正确的代码。