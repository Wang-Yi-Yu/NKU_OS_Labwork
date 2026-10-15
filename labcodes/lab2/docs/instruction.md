# lab2:物理内存和页表

实验一过后大家做出来了一个可以启动的系统，实验二主要涉及操作系统的物理内存管理。操作系统为了使用内存，还需高效地管理内存资源。在实验二中大家会了解并且自己动手完成一个简单的物理内存管理系统。

## 实验目的

- 理解页表的建立和使用方法
- 理解物理内存的管理方法
- 理解页面分配算法

## 实验内容

实验一过后大家做出来了一个可以启动的系统，实验二主要涉及操作系统的物理内存管理。操作系统为了使用内存，还需高效地管理内存资源。本次实验我们会了解如何发现系统中的物理内存，然后学习如何建立对物理内存的初步管理，即了解连续物理内存管理，最后掌握页表相关的操作，即如何建立页表来实现虚拟内存到物理内存之间的映射，帮助我们对段页式内存管理机制有一个比较全面的了解。本次的实验主要是在实验一的基础上完成物理内存管理，并建立一个最简单的页表映射。

### 练习

本实验围绕物理内存的发现、初始化和分配展开：内核首先选择并初始化物理内存管理器，再从设备树获取物理内存范围，由`page_init()`建立物理页描述符并将可用页面交给物理内存管理器；随后在理解 First-Fit 分配流程的基础上，实现并接入 Best-Fit 分配器，并通过 Buddy System 和 SLUB 扩展任务进一步理解页级分配与任意大小内存对象分配。整体实现链可以概括为`pmm_init() -> init_pmm_manager() -> page_init() -> init_memmap() -> alloc_pages()/free_pages()`，最终使内核能够识别、组织和分配可用物理内存。

#### 功能模块一：物理内存初始化功能

##### 任务一：实现物理内存探测与页描述符初始化功能（设计提示词借助 AI 完成）

要求实现的函数有：

```c
static void page_init(void); (kern/mm/pmm.c)
```

功能与接口说明：根据设备树提供的物理内存信息建立可管理物理页的描述符布局，并把排除 OpenSBI、内核镜像和描述符数组占用空间后的完整空闲页面交给当前`pmm_manager`管理，为后续页面分配提供正确的初始状态。`page_init()`不接收参数也不返回结果，由`pmm_init()`调用；成功返回时应正确设置`va_pa_offset`、`npage`和`pages`，输出探测到的物理内存范围，并完成当前物理内存管理器初始空闲区的注册。

实现约束与可用资源：必须区分虚拟地址、物理地址、物理页号和`Page`描述符地址，正确处理可管理上界和页面对齐，并确保内核及描述符数组不会进入空闲区；设备树信息无效时应终止初始化，没有可用完整页面时不得调用`init_memmap()`。实现过程中可以使用`get_memory_base()`、`get_memory_size()`、`panic()`、`cprintf()`、`init_memmap()`、`pa2page()`、链接符号`end`、`struct Page`及相关全局变量，并使用框架提供的内存上界、地址转换、页面对齐和页面状态宏完成初始化。

Tips：如果对设备树中的物理内存信息、`Page`描述符布局或虚拟地址与物理地址之间的转换关系还不熟悉，可以先仔细阅读实验手册中“物理内存管理的设计思路”下的“物理内存探测”一节。

##### 扩展任务 Challenge：硬件的可用物理内存范围的获取方法（思考题）

- 如果 OS 无法提前知道当前硬件的可用物理内存范围，请问你有何办法让 OS 获取可用物理内存范围？

#### 功能模块二：物理内存分配功能

##### 任务一：理解first-fit 连续物理内存分配算法（思考题）

first-fit 连续物理内存分配算法作为物理内存分配一个很基础的方法，需要同学们理解它的实现过程。请大家仔细阅读实验手册的教程并结合`kern/mm/default_pmm.c`中的相关代码，认真阅读default_init，default_init_memmap，default_alloc_pages， default_free_pages等相关函数，并掌握程序在进行物理内存分配的过程以及各个函数的作用。

请在实验报告中结合上述函数，用自己的话简要说明 First-Fit 分配器如何初始化空闲页、分配连续物理页、回收页面并合并相邻空闲块。同时请回答如下问题：

- 原有的first fit算法是否有进一步的改进空间？

##### 任务二：实现 Best-Fit 连续物理内存分配算法（设计提示词借助 AI 完成）

在完成任务一后，参考`kern/mm/default_pmm.c`中的完整 First-Fit 实现，设计`prompt`，让 AI 在`kern/mm/best_fit_pmm.c`中标有`lab2`和`YOUR CODE`的位置实现完整的 Best-Fit 物理内存管理模块。

该文件只提供`basic_check()`和`best_fit_check()`两个测试函数，不提供具体管理函数的声明和`best_fit_pmm_manager`注册结构。你可以让 AI 阅读`pmm.h`中的`struct pmm_manager`，根据各成员的职责自行设计 Best-Fit 分配器的内部状态和相关管理函数，并定义`best_fit_pmm_manager`完成注册；生成的实现必须符合测试函数使用的状态约定，不得修改两个测试函数。算法的时空复杂度不做要求，能通过测试即可。

完成 Best-Fit 模块后，还需要在`kern/mm/pmm.c`的`init_pmm_manager()`中将当前物理内存管理器切换为`best_fit_pmm_manager`，否则内核运行和检查的仍是 First-Fit 实现。

请在实验报告中提交本任务使用的`prompt`，并同时回答如下问题：

- 你的 Best-Fit 算法是否有进一步的改进空间？

##### 任务三：buddy system（伙伴系统）分配算法（设计提示词借助 AI 完成）

Buddy System算法把系统中的可用存储空间划分为存储块(Block)来进行管理, 每个存储块的大小必须是2的n次幂(Pow(2, n)), 即1, 2, 4, 8, 16, 32, 64, 128...

请分别设计两份`prompt`：第一份用于让 AI 参考现有 ucore 物理内存管理框架，从零生成 Buddy System 分配模块并接入内核；第二份用于让 AI 针对第一份提示词生成的分配算法设计相应的测试函数，以检验其实现是否正确。实验不提供 Buddy System 的代码框架和测试函数，具体实现方式和测试方案由学生自行分析和描述。最终生成的模块和测试应能够正常编译运行，算法的时空复杂度不做要求，能通过测试即可。

请在实验报告中提交两份`prompt`，并简要说明测试覆盖了哪些核心行为和边界情况，以及这些测试为什么能够有效检验生成的 Buddy System 实现。

- 参考资料：[伙伴分配器的一个极简实现](http://coolshell.cn/articles/10427.html)

##### 扩展任务Challenge：任意大小的内存单元 slub 分配算法（设计提示词借助 AI 完成）

slub算法，实现两层架构的高效内存单元分配，第一层是基于页大小的内存分配，第二层是在第一层基础上实现基于任意大小的内存分配。可简化实现，能够体现其主体思想即可。

请分别设计两份`prompt`：第一份用于让 AI 在没有现成代码框架的情况下，从零生成 SLUB 内存对象分配模块并接入现有内核；第二份用于让 AI 针对第一份提示词生成的分配算法设计相应的测试函数，以检验不同大小内存对象的分配和释放是否正确。实验不提供 SLUB 的代码框架和测试函数，具体实现方式和测试方案由学生自行分析和描述。最终生成的模块和测试应能够正常编译运行，算法的时空复杂度不做要求，能通过测试即可。

请在实验报告中提交两份`prompt`，并简要说明测试覆盖了哪些核心行为和边界情况，以及这些测试为什么能够有效检验生成的 SLUB 实现。

- 参考资料：[Linux 的 SLUB 分配算法](https://github.com/torvalds/linux/blob/master/mm/slub.c)

> Challenges是选做，完成Challenge并回答了助教问题的小组可获得本次实验的加分。

### 项目组成

表1：实验二文件列表

```
.
├── Makefile
├── kern
│   ├── debug
│   │   ├── assert.h
│   │   └── panic.c
│   ├── driver
│   │   ├── console.c
│   │   ├── console.h
│   │   ├── dtb.c
│   │   └── dtb.h
│   ├── init
│   │   ├── entry.S
│   │   └── init.c
│   ├── libs
│   │   └── stdio.c
│   └── mm
│       ├── best_fit_pmm.c
│       ├── best_fit_pmm.h
│       ├── default_pmm.c
│       ├── default_pmm.h
│       ├── memlayout.h
│       ├── mmu.h
│       ├── pmm.c
│       └── pmm.h
├── lab2.md
├── libs
│   ├── defs.h
│   ├── error.h
│   ├── list.h
│   ├── printfmt.c
│   ├── readline.c
│   ├── riscv.h
│   ├── sbi.c
│   ├── sbi.h
│   ├── stdarg.h
│   ├── stdio.h
│   ├── string.c
│   └── string.h
└── tools
    ├── boot.ld
    ├── function.mk
    ├── gdbinit
    ├── grade.sh
    ├── kernel.ld
    └── kernel_nopage.ld
```

**编译方法**

编译并运行代码的命令如下：

```bash
make qemu
```

完成实验任务并将物理内存管理器切换为 Best-Fit 后，可以得到类似如下的输出（具体地址可能因代码大小和运行环境而有所不同）：

```bash
+ ld bin/kernel
riscv64-unknown-elf-objcopy bin/kernel --strip-all -O binary bin/ucore.img

OpenSBI v0.4 (Jul  2 2019 11:53:53)
   ____                    _____ ____ _____
  / __ \                  / ____|  _ \_   _|
 | |  | |_ __   ___ _ __ | (___ | |_) || |
 | |  | | '_ \ / _ \ '_ \ \___ \|  _ < | |
 | |__| | |_) |  __/ | | |____) | |_) || |_
  \____/| .__/ \___|_| |_|_____/|____/_____|
        | |
        |_|

Platform Name          : QEMU Virt Machine
Platform HART Features : RV64ACDFIMSU
Platform Max HARTs     : 8
Current Hart           : 0
Firmware Base          : 0x80000000
Firmware Size          : 112 KB
Runtime SBI Version    : 0.1

PMP0: 0x0000000080000000-0x000000008001ffff (A)
PMP1: 0x0000000000000000-0xffffffffffffffff (A,R,W,X)
DTB Init
HartID: 0
DTB Address: 0x<runtime-dependent>
Physical Memory from DTB:
  Base: 0x0000000080000000
  Size: 0x0000000008000000 (128 MB)
  End:  0x0000000087ffffff
DTB init completed
(THU.CST) os is loading ...
Special kernel symbols:
  entry  0xffffffffc02000c0 (virtual)
  etext  0xffffffffc02011de (virtual)
  edata  0xffffffffc0205008 (virtual)
  end    0xffffffffc0205058 (virtual)
Kernel executable memory footprint: 20KB
memory management: best_fit_pmm_manager
physical memory map:
  memory: 0x0000000008000000, [0x0000000080000000, 0x0000000087ffffff].
check_alloc_page() succeeded!
satp virtual address: 0xffffffffc0204000
satp physical address: 0x0000000080204000
```

从输出可以看到，ucore依次显示入口地址、各段结束地址和内核镜像结束地址，然后输出设备树探测到的物理内存范围，并运行物理页分配检查。需要注意的是，`entry.S`在进入`kern_init()`之前已经建立启动页表并开启 Sv39 分页；这里的物理内存初始化是在分页模式下建立`Page`描述符和初始空闲页区间。

## 物理内存管理

### 基本原理概述

#### 物理内存管理

在计算机系统的最底层，物理内存由一系列具有实际地址的存储单元构成。每个内存单元都拥有唯一的物理地址，CPU通过地址总线直接发送这些物理地址来访问内存数据。在简单系统中，程序指令中的内存地址（如`MOV [0x80200000], %eax`）会直接被CPU当作物理地址发送到内存总线上，这种方式实现了最直接的内存访问。

然而，当多个程序需要共享内存空间时，这种直接访问物理地址的方式会面临严重问题。首先，如果内核和用户程序都试图访问相同的物理地址（如`0x80200000`），就会产生冲突，导致数据被意外修改或破坏。更重要的是，这种内存管理方式对内存连续性有着苛刻的要求——每个程序都需要获得连续的内存空间才能运行。

由于程序对连续内存空间的需求，物理内存很容易产生碎片化问题。即使系统中总的空闲内存容量足够，但如果这些空闲内存分散成多个不连续的小块，程序仍然可能因为找不到足够的连续空间而无法运行。这种对连续性的强烈依赖，使得简单物理内存管理在复杂系统中难以有效工作。

那么如何消除这种影响呢？大家显然可以想象得到，我们可以通过让用户程序访问的`0x80200000`和内核访问的`0x80200000`不是一个地址来解决这个问题。但是如果我们只有一块内存，那么为了创造两个不同的地址空间，我们可以引入一个”翻译“机制：程序使用的地址需要经过一步”翻译“才能变成真正的内存的物理地址。这个”翻译“过程，我们可以用一个”词典“实现。通过这个”词典“给出翻译之前的地址，可以在词典里查找翻译后的地址。而对每个程序往往都有着唯一的一本”词典“，而它能使用的内存也就只有他的”词典“所包含的。

”词典“是否对能使用的每个字节都进行翻译？我们可以想象，存储每个字节翻译的结果至少需要一个字节，那么使用1MB的内存将至少需要构造1MB的”词典“，这效率太低了。观察到，一个程序使用内存的数量级通常远大于字节，至少以KB为单位（所以上古时代的人说的是”640K对每个人都够了“而不是”640B对每个人都够了"）。那么我们可以考虑，把连续的很多字节合在一起翻译，让他们翻译前后的数值之差相同，这就是“页”。

#### 物理地址和虚拟地址

在本次实验中，我们使用的是RISCV的`sv39`页表机制，每个页的大小是4KB，也就是4096个字节。通过之前的介绍，相信大家对物理地址和虚拟地址有了一个初步的认识了，页表就是那个“词典”，里面有程序使用的虚拟页号到实际内存的物理页号的对应关系，但并不是所有的虚拟页都有对应的物理页。虚拟页可能的数目远大于物理页的数目，而且一个程序在运行时，一般不会拥有所有物理页的使用权，只会占用部分物理页，这些物理页再通过页表映射到虚拟地址空间中。

分页机制的一个重要优势在于，它带来了地址管理的灵活性。程序员只需要面对一个连续的虚拟地址空间，而不需要关心底层物理内存是否连续。不同程序使用的虚拟地址空间可以相同，但通过页表映射到不同的物理地址，避免了冲突。操作系统还可以通过修改页表，方便地实现内存共享、隔离和换页。

在`sv39`中，定义**物理地址(Physical Address)有 56位，而虚拟地址(Virtual Address) 有 39位**。实际使用的时候，一个虚拟地址要占用 64位，只有低 39位有效，我们规定 63−39 位的值必须等于第 38 位的值（大家可以将它类比为有符号整数），否则会认为该虚拟地址不合法，在访问时会产生异常。不论是物理地址还是虚拟地址，我们都可以认为，最后12位表示的是页内偏移，也就是这个地址在它所在页帧的什么位置（同一个位置的物理地址和虚拟地址的页内偏移相同）。除了最后12位，前面的部分表示的是物理页号或者虚拟页号。

### 以页为单位管理物理内存

#### 页表项

一个页表项是用来描述一个虚拟页号如何映射到物理页号的。如果一个虚拟页号通过某种手段找到了一个页表项，并通过读取上面的物理页号完成映射，那么我们称这个虚拟页号通过该页表项完成映射。而我们的”词典“（页表）存储在内存里，由若干个格式固定的”词条“也就是页表项（PTE, Page Table Entry）组成。显然我们需要给词典的每个词条约定一个固定的格式（包括每个词条的大小，含义），这样查起来才方便。

那么在sv39的一个页表项占据8字节（64位），那么页表项结构是这样的：

| 63-54      | 53-28  | 27-19  | 18-10  | 9-8 | 7   | 6   | 5   | 4   | 3   | 2   | 1   | 0   |
| ---------- | ------ | ------ | ------ | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| _Reserved_ | PPN[2] | PPN[1] | PPN[0] | RSW | D   | A   | G   | U   | X   | W   | R   | V   |
| 10         | 26     | 9      | 9      | 2   | 1   | 1   | 1   | 1   | 1   | 1   | 1   | 1   |

我们可以看到 sv39 里面的一个页表项大小为 64 位 8 字节。其中第 53-10 位共44位为一个物理页号，表示这个虚拟页号映射到的物理页号。后面的第 9-0 位共10位则描述映射的状态信息。

介绍一下映射状态信息各位的含义：

- RSW：两位留给 S Mode 的应用程序，我们可以用来进行拓展。
- D：即 Dirty ，如果 D=1 ，表示从上次 D 被清零后开始算起，有虚拟地址通过这个页表项进行写入。
- A，即 Accessed，如果 A=1 ，表示从上次 A 被清零后开始算起，有虚拟地址通过这个页表项进行读、或者写、或者取指。
- G，即 Global，如果 G=1 表示这是一个全局映射，处理器缓存该映射时不需要使用 ASID 区分地址空间；这并不表示所有页表中都会自动包含这一项。
- U，即 user，U为 1 表示用户态 (U Mode)的程序 可以通过该页表项进映射。在用户态运行时，也只能够通过 U=1 的页表项进行虚实地址映射。 注意，S Mode 不一定可以通过 U=1 的页表项进行映射。我们需要将 S Mode 的状态寄存器 sstatus 上的 SUM 位手动设置为 1 才可以做到这一点（通常情况不会把它置1）。否则通过 U=1 的页表项进行映射也会报出异常。另外，不论sstatus的SUM位如何取值，S Mode都不允许执行 U=1 的页面里包含的指令，这是出于安全的考虑。
- R,W,X 为许可位，分别表示是否可读 (Readable)，可写 (Writable)，可执行 (Executable)。 以 W 这一位为例，如果 W=0 表示不可写，那么如果一条 store 的指令，它通过这个页表项完成了虚拟页号到物理页号的映射，找到了物理地址。但是仍然会报出异常，是因为这个页表项规定如果物理地址是通过它映射得到的，那么不准写入！ R,X也是同样的道理。 根据 R,W,X 取值的不同，我们可以分成下面几种类型：

| X   | W   | R   | Meaning                         |
| --- | --- | --- | ------------------------------- |
| 0   | 0   | 0   | 指向下一级页表的指针            |
| 0   | 0   | 1   | 这一页只读                      |
| 0   | 1   | 0   | _保留(reserved for future use)_ |
| 0   | 1   | 1   | 这一页可读可写（不可执行）      |
| 1   | 0   | 0   | 这一页仅可执行（不可读写）      |
| 1   | 0   | 1   | 这一页可读可执行                |
| 1   | 1   | 0   | _保留(reserved for future use)_ |
| 1   | 1   | 1   | 这一页可读可写可执行            |

- V 表示这个页表项是否合法。如果为 0 表示不合法，此时页表项其他位的值都会被忽略。

#### 多级页表

在实际使用中显然如果只有一级页表，那么我们构建出来的虚拟地址空间还是过于有限，因此我们需要引入多级页表以实现更大规模的虚拟地址空间。

但是相比于可用的物理内存空间，我们的虚拟地址空间太大，不可能为每虚个拟内存页都分配一个页表项。可以简单计算一下，在Sv39中，因为一个页表项占据8字节（64位），而虚拟地址有39位，后12位是页内偏移，那么还剩下27位可以编码不同的虚拟页号。

如果开一个大数组Pagetable[ ], 其中Pagetable[vpn]代表虚拟页号为vpn的虚拟页对应的的页表项，我们给个2^27 的虚拟页号都分配8字节的页表项，那就是整整1 GiB的内存。但这里面其实很多虚拟地址我们没有用到，会有大片大片的页表项的标志位为0（不合法），显然我们不应该为那么多非法页表项浪费宝贵的内存空间。

因此，我们可以对页表进行“分级”，让它变成一个树状结构。也就是把很多页表项组合成一个“大页”，如果这些页表项都非法（没有对应的物理页），那么只需要用一个非法的页表项来覆盖这个大页，而不需要分别建立一大堆非法页表项。很多个大页(megapage)还可以组合起来变成大大页(gigapage!)，继而可以有更大的页，以此类推，当然肯定不是分层越多越好，因为随着层数增多，开销也会越大。

在本次实验中，我们使用的sv39权衡各方面效率，使用三级页表。有4KiB=4096字节的页，大小为2MiB= 2^21 字节的大页，和大小为1 GiB 的大大页。

原先的一个39位虚拟地址，被我们看成27位的页号和12位的页内偏移。那么在三级页表下，我们可以把它看成9位的“大大页页号”，9位的“大页页号”（也是大大页内的页内偏移），9位的“页号”（大页的页内偏移），还有12位的页内偏移。这是一个递归的过程，中间的每一级页表映射是类似的。也就是说，整个Sv39的虚拟内存空间里，有512（2的9次方）个大大页，每个大大页里有512个大页，每个大页里有512个页，每个页里有4096个字节，整个虚拟内存空间里就有512∗512∗512∗4096个字节，是512GiB的地址空间。

那么为啥是512呢？注意，4096/8 = 512，我们恰好可以在一页里放下512个页表项！

我们可以认为，Sv39的多级页表在逻辑上是一棵树，它的每个叶子节点都对应内存的一页，它的每个内部节点都对应512个更低一层的节点，而每个内部节点向更低一层节点的链接都使用内存页中的页表项进行存储。

Sv39页表的根节点会占据一页4KiB的内存，存储512个页表项（一个页表项占据8字节），分别对应512个1 GiB的大大页。虚拟地址27位页号中的高9位用来在根节点中索引页表项，找到对应的大大页（注意只有合法的页表项才是根节点的儿子，可以跳转到一个物理页号，对应树中一个“大大页”的节点）。

大大页同样是4KiB的内存存储512个页表项，对应512个2MiB的大页，虚拟地址27位页号中的中9位用来在大大页中索引页表项，找到对应的大页的物理页号（同样需要是合法的）。同理，大页的配置与根节点、大大页如出一辙，通过虚拟地址27位页号中的低9位索引大页，便可以找到虚拟地址对应的4KiB物理页，接着用12位页内偏移索引物理页来找到具体的内容。

一级和二级页表项不一定要指向下一级页表。我们知道每个三级页表项控制一个虚拟页号，即控制 4KiB 虚拟内存；每个二级页表项则控制 9 位虚拟页号，总计控制 4KiB×2^9 =2MiB 虚拟内存；每个一级页表项控制 18 位虚拟页号，总计控制 2MiB×2^9 =1GiB 虚拟内存。我们可以将二级页表项的 R,W,X 设置为不是全 0 的许可要求，那么它将与一级页表项类似，只不过可以映射一个 2MiB 的大页 (Mega Page) 。同理，也可以将一级页表项看作一个叶子，来映射一个 1GiB 的大大页(Giga Page)。

#### 页表基址

那么在翻译的过程中，我们如何知道树状页表根节点的物理地址呢？对于 RISC-V 架构，页表相关信息保存在一个名为`satp`（Supervisor Address Translation and Protection Register）的特殊寄存器中。

实际上，`satp`里面存的不是最高级页表的起始物理地址，而是它所在的物理页号。除了物理页号，`satp`还包含其他信息。

| 63-60      | 59-44      | 43-0      |
| ---------- | ---------- | --------- |
| MODE(WARL) | ASID(WARL) | PPN(WARL) |
| 4          | 16         | 44        |

MODE表示当前页表的模式：

- 0000表示不使用页表，直接使用物理地址，在简单的嵌入式系统里用着很方便。
- 1000表示sv39页表，也就是我们使用的，虚拟内存空间高达`512GiB`。
- 1001表示Sv48页表，它和Sv39兼容。
- 其他编码保留备用

ASID（address space identifier）我们目前用不到。

OS 可以在内存中为不同的应用分别建立不同虚实映射的页表，并通过修改寄存器 satp 的值指向不同的页表，从而可以修改 CPU 虚实地址映射关系及内存保护的行为。

### 建立快表以加快访问效率

物理内存的访问速度要比 CPU 的运行速度慢很多, 去访问一次物理内存可能需要几百个时钟周期（带来所谓的“冯诺依曼瓶颈”）。如果我们按照页表机制一步步走，将一个虚拟地址转化为物理地址需要访问 3 次物理内存，得到物理地址之后还要再访问一次物理内存，才能读到我们想要的数据。这很大程度上降低了效率。 好在，实践表明虚拟地址的访问具有时间局部性和空间局部性。

- 时间局部性是指，被访问过一次的地址很有可能不远的将来再次被访问；
- 空间局部性是指，如果一个地址被访问，则这个地址附近的地址很有可能在不远的将来被访问。

因此，在 CPU 内部，我们使用快表 (TLB, Translation Lookaside Buffer) 来记录近期已完成的虚拟页号到物理页号的映射。由于局部性，当我们要做一个映射时，会有很大可能这个映射在近期被完成过，所以我们可以先到 TLB 里面去查一下，如果有的话我们就可以直接完成映射，而不用访问那么多次内存了。

### 分页机制的设计思路

#### 建立段页式管理中需要考虑的关键问题

为了实现分页机制，需要建立好虚拟内存和物理内存的页映射关系，即正确建立三级页表。此过程涉及硬件细节，不同的地址映射关系组合，相对比较复杂。总体而言，我们需要思考如下问题：

- 对于哪些物理内存空间需要建立页映射关系？
- 具体的页映射关系是什么？
- 页目录表的起始地址设置在哪里？
- 页表的起始地址设置在哪里，需要多大空间？
- 如何设置页目录表项的内容？
- 如何设置页表项的内容？

#### 实现分页机制

接下来我们就正式开始实验啦！

首先我们要做的是内核初始化的修改，我们现在需要做的就是把原本只能直接在物理地址空间上运行的内核引入页表机制。

具体来说，我们现在想将内核代码放在虚拟地址空间中以 `0xffffffffc0200000` 开头的一段高地址空间中。那怎么做呢？首先我们需要将下面的参数修改一下：

```
// tools/kernel.ld
BASE_ADDRESS = 0xFFFFFFFFC0200000;
//之前这里是 0x80200000
```

我们修改了链接脚本中的起始地址。但是这样做的话，就能从物理地址空间转移到虚拟地址空间了吗？大家可以分析一下现在我们相当于是在 bootloader 的 OpenSBI 结束后的现状，这样就可以更好的理解接下来我们需要干什么：

- 物理内存状态：OpenSBI 代码放在 `[0x80000000,0x80200000)` 中，内核代码放在以 `0x80200000` 开头的一块连续物理内存中。这个是实验一我们做完后就实现的效果。
- CPU 状态：处于 S Mode ，寄存器 `satp` 的 `MODE`被设置为 `Bare` ，即无论取指还是访存我们都通过物理地址直接访问物理内存。 `PC=0x80200000` 指向内核的第一条指令。栈顶地址 `SP` 处在 OpenSBI 代码内。
- 内核代码：这部分由于改动了链接脚本的起始地址，所以它会认为自己处在以虚拟地址 `0xffffffffc0200000` 开头的一段连续虚拟地址空间中，以此为依据确定代码里每个部分的地址（每一段都是从`BASE_ADDRESS`往后依次摆开的，所以代码里各段都会认为自己在`0xffffffffc0200000`之后的某个地址上，或者说编译器和链接器会把里面的符号/变量地址都对应到`0xffffffffc0200000`之后的某个地址上）

接下来，我们需要修改 `entry.S` 文件来实现内核的初始化，我们在入口点 `entry.S` 中所要做的事情是：将 `SP` 寄存器从原先指向OpenSBI 某处的栈空间，改为指向我们自己在内核的内存空间里分配的栈；同时需要跳转到函数 `kern_init` 中。

在之前的实验中，我们已经在 `entry.S` 自己分配了一块 `8KiB`的内存用来做启动栈：

```armasm
#include <mmu.h>
#include <memlayout.h>

    .section .text,"ax",%progbits
    .globl kern_entry
kern_entry:
    la sp, bootstacktop

    tail kern_init

.section .data
    # .align 2^12
    .align PGSHIFT
    .global bootstack
bootstack:
    .space KSTACKSIZE
    .global bootstacktop
bootstacktop:
```

通过之前的实验大家应该都明白：符号 `bootstacktop` 就是我们需要的栈顶地址, 符号 `kern_init` 代表了我们要跳转到的地址。之前我们直接将 `bootstacktop` 的值给到 `SP` ， 再跳转到 `kern_init` 就行了。看上去上面的这个代码也能够实现我们想要的初始化效果，但问题在于，由于我们修改了链接脚本的起始地址，编译器和链接器认为内核开头地址为 `0xffffffffc0200000`，因此这两个符号会被翻译成比这个开头地址还要高的某个虚拟地址。而我们的 CPU 目前还处于 `Bare` 模式，会将地址都当成物理地址处理。这样，我们跳转到 `kern_init`， 就意味着会跳转到比`0xffffffffc0200000`还大的一个物理地址。但物理地址显然不可能有这么多位！这就会出现问题。

于是，我们需要想办法利用刚学的页表知识，帮内核将需要的虚拟地址空间构造出来。也就是：构建一个合适的页表，让`satp`指向这个页表，然后使用地址的时候都要经过这个页表的翻译，使得虚拟地址`0xFFFFFFFFC0200000`经过页表的翻译恰好变成`0x80200000`，这个地址显然就比较合适了，也就不会出错了。

理论知识告诉我们，所有的虚拟地址有一个固定的偏移量。而要想实现页表结构这个偏移量显然是不可或缺的。而虚拟地址和物理地址之间的差值就可以当成是这个偏移量。

比如内核的第一条指令，虚拟地址为 `0xffffffffc0200000` ，物理地址为 `0x80200000` ，因此，我们只要将虚拟地址减去 `0xffffffff40000000` ，就得到了物理地址。所以当我们需要做到去访问内核里面的一个物理地址 `va` 时，而已知虚拟地址为 `va` 时，则 `va` 处的代码或数据就放在物理地址为 `pa = va - 0xffffffff40000000` 处的物理内存中，我们真正所要做的是要让 CPU 去访问 `pa`。因此，我们要通过恰当构造页表，来对于内核所属的虚拟地址，实现这种 `va` 到 `pa` 的映射。

还记得之前的理论介绍的内容吗？那时我们提到，将一个三级页表项的标志位 `R,W,X` 不设为全 `0` ，可以将它变为一个叶子，从而获得大小为 `1GiB` 的一个大页。

我们假定内核大小不超过 `1GiB`，通过一个大页将虚拟地址区间`[0xffffffffc0000000,0xffffffffffffffff]` 映射到物理地址区间 `[0x80000000,0xc0000000)`，而我们只需要分配一页内存用来存放三级页表，并将其最后一个页表项(也就是对应我们使用的虚拟地址区间的页表项)进行适当设置即可。对应的代码如下所示：

```armasm
#include <mmu.h>
#include <memlayout.h>

    .section .text,"ax",%progbits
    .globl kern_entry
kern_entry:
    # t0 := 三级页表的虚拟地址
    lui     t0, %hi(boot_page_table_sv39)
    # t1 := 0xffffffff40000000 即虚实映射偏移量
    li      t1, 0xffffffffc0000000 - 0x80000000
    # t0 减去虚实映射偏移量 0xffffffff40000000，变为三级页表的物理地址
    sub     t0, t0, t1
    # t0 >>= 12，变为三级页表的物理页号
    srli    t0, t0, 12

    # t1 := 8 << 60，设置 satp 的 MODE 字段为 Sv39
    li      t1, 8 << 60
    # 将刚才计算出的预设三级页表物理页号附加到 satp 中
    or      t0, t0, t1
    # 将算出的 t0(即新的MODE|页表基址物理页号) 覆盖到 satp 中
    csrw    satp, t0
    # 使用 sfence.vma 指令刷新 TLB
    sfence.vma
    # 从此，我们给内核搭建出了一个完美的虚拟内存空间！
    #nop # 可能映射的位置有些bug。。插入一个nop

    # 我们在虚拟内存空间中：随意将 sp 设置为虚拟地址！
    lui sp, %hi(bootstacktop)

    # 我们在虚拟内存空间中：随意跳转到虚拟地址！
    # 跳转到 kern_init
    lui t0, %hi(kern_init)
    addi t0, t0, %lo(kern_init)
    jr t0

.section .data
    # .align 2^12
    .align PGSHIFT
    .global bootstack
bootstack:
    .space KSTACKSIZE
    .global bootstacktop
bootstacktop:

.section .data
    # 由于我们要把这个页表放到一个页里面，因此必须 12 位对齐
    .align PGSHIFT
    .global boot_page_table_sv39
# 分配 4KiB 内存给预设的三级页表
boot_page_table_sv39:
    # 0xffffffff_c0000000 map to 0x80000000 (1G)
    # 前 511 个页表项均设置为 0 ，因此 V=0 ，意味着是空的(unmapped)
    .zero 8 * 511
    # 设置最后一个页表项，PPN=0x80000，标志位 VRWXAD 均为 1
    .quad (0x80000 << 10) | 0xcf # VRWXAD
```

总结一下，要进入虚拟内存访问方式，需要如下步骤：

1.  分配页表所在内存空间并初始化页表；
2.  设置好页基址寄存器（指向页表起始地址）；
3.  刷新 TLB。

到现在为止，看上去复杂无比的虚拟内存空间，我们终于得以窥视一二了。

### 物理内存管理的设计思路

#### 物理内存管理的实现

在管理虚拟内存之前，我们首先需要能够管理物理内存，毕竟所有虚拟内存页都要对应到物理内存页才能使用。

不妨把我们的内存管理模块划分为物理内存管理和虚拟内存管理两个模块。

物理内存管理应当为虚拟内存管理提供这样的接口：

- 检查当前还有多少空闲的物理页，返回空闲的物理页数目
- 给出n，尝试分配n个物理页，可以返回一个起始地址和连续的物理页数目，也可能分配一些零散的物理页，返回一个连起来的链表。
- 给出起始地址和n，释放n个连续的物理页

在`kern_init()`里，我们调用一个新函数：`pmm_init()`，`kern_init()`函数我们在之前就有学习过，这里我们只是新增一个调用`pmm_init()`的接口。

```c
// kern/init/init.c
int kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);
    cons_init();  // init the console
    const char *message = "(THU.CST) os is loading ...\0";
    cputs(message);
    print_kerninfo();

    idt_init();  // init interrupt descriptor table
    pmm_init();  // 新东西！
    clock_init();   // init clock interrupt
    intr_enable();  // enable irq interrupt
    /* do nothing */
    while (1)
        ;
}
```

那么`pmm_init()`究竟是用来干什么的呢？其实`pmm_init()`主要就是用来主要负责初始化物理内存管理，我们可以在pmm.c文件进行初始化操作。

```c
// kern/mm/pmm.c
/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[]; //我们把汇编里定义的页表所在位置的符号声明进来
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);//然后输出页表所在的地址
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}
```

`check_alloc_page()`是对物理内存分配功能的一个测试。我们重点关注`page_init()`

我们在lab2增加了一些功能，方便我们编程：

- libs/list.h：定义了通用双向链表结构以及相关的查找、插入等基本操作，这是建立基于链表方法的物理内存管理（以及其他内核功能）的基础。其他有类似双向链表需求的内核功能模块可直接使用 list.h 中定义的函数。

`list.h`里面实现了一个简单的双向链表。虽然接口很多，但是只要对链表熟悉，不难理解。如果理解不了，可以先去学学数据结构这门课。

```c
// libs/list.h
struct list_entry {
    struct list_entry *prev, *next;
};

typedef struct list_entry list_entry_t;

static inline void list_init(list_entry_t *elm) __attribute__((always_inline));
static inline void list_add(list_entry_t *listelm, list_entry_t *elm) __attribute__((always_inline));
static inline void list_add_before(list_entry_t *listelm, list_entry_t *elm) __attribute__((always_inline));
static inline void list_add_after(list_entry_t *listelm, list_entry_t *elm) __attribute__((always_inline));
static inline void list_del(list_entry_t *listelm) __attribute__((always_inline));
static inline void list_del_init(list_entry_t *listelm) __attribute__((always_inline));
static inline bool list_empty(list_entry_t *list) __attribute__((always_inline));
static inline list_entry_t *list_next(list_entry_t *listelm) __attribute__((always_inline));
static inline list_entry_t *list_prev(list_entry_t *listelm) __attribute__((always_inline));
//下面两个函数仅在内部使用，不对外开放作为接口。
static inline void __list_add(list_entry_t *elm, list_entry_t *prev, list_entry_t *next) __attribute__((always_inline));
static inline void __list_del(list_entry_t *prev, list_entry_t *next) __attribute__((always_inline));
```

看起来`list.h`里面定义的`list_entry`并没有数据域，但是，如果我们把`list_entry`作为其他结构体的成员，就可以利用C语言结构体内存连续布局的特点，从``list_entry`的地址获得它所在的上一级结构体。

于是我们定义了可以连成链表的`Page`结构体和一系列对它做操作的宏。这个结构体用来管理物理内存。此时，`page_link` 就是 `list_entry` 类型的节点，它把一批 `Page` 串起来形成空闲链表。但是当我们在链表中遍历时，手里只有 `list_entry*`，要怎么拿到它对应的 `Page*` 呢？

这里就用到了 `le2page` 宏。它的作用是，给定 `list_entry* le`，以及 `member = page_link`，利用 `to_struct` 宏，从 `le` 的地址向前偏移，得到 `Page` 结构体的首地址，最终返回 `Page*`。这种技巧本质上是 “container_of” 的用法（Linux 内核里大量使用），即从结构体中的某个成员指针，反推出整个结构体指针。

```c
// libs/defs.h

/* Return the offset of 'member' relative to the beginning of a struct type */
#define offsetof(type, member)                                      \
    ((size_t)(&((type *)0)->member))

/* *
 * to_struct - get the struct from a ptr
 * @ptr:    a struct pointer of member
 * @type:   the type of the struct this is embedded in
 * @member: the name of the member within the struct
 * */
#define to_struct(ptr, type, member)                               \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// kern/mm/memlayout.h
/* *
 * struct Page - Page descriptor structures. Each Page describes one
 * physical page. In kern/mm/pmm.h, you can find lots of useful functions
 * that convert Page to other data types, such as physical address.
 * */
struct Page {
    int ref;                 // page frame's reference counter
    uint64_t flags;          // array of flags that describe the status of the page frame
    unsigned int property;   // the num of free block, used in first fit pm manager
    list_entry_t page_link;  // free list link
};

/* Flags describing the status of a page frame */
#define PG_reserved                 0       // if this bit=1: the Page is reserved for kernel, cannot be used in alloc/free_pages; otherwise, this bit=0
#define PG_property                 1       // if this bit=1: the Page is the head page of a free memory block(contains some continuous_addrress pages), and can be used in alloc_pages; if this bit=0: if the Page is the the head page of a free memory block, then this Page and the memory block is alloced. Or this Page isn't the head page.
//这几个对page操作的宏用到了atomic.h的原子操作
#define SetPageReserved(page)       set_bit(PG_reserved, &((page)->flags))
#define ClearPageReserved(page)     clear_bit(PG_reserved, &((page)->flags))
#define PageReserved(page)          test_bit(PG_reserved, &((page)->flags))
#define SetPageProperty(page)       set_bit(PG_property, &((page)->flags))
#define ClearPageProperty(page)     clear_bit(PG_property, &((page)->flags))
#define PageProperty(page)          test_bit(PG_property, &((page)->flags))

// convert list entry to page
#define le2page(le, member)                 \
    to_struct((le), struct Page, member)

/* free_area_t - maintains a doubly linked list to record free (unused) pages */
typedef struct {
    list_entry_t free_list;         // the list header
    unsigned int nr_free;           // # of free pages in this free list
} free_area_t;
```

我们知道，物理内存通常是一片 RAM ，我们可以把它看成一个以字节为单位的大数组，通过物理地址找到对应的位置进行读写。但是，物理地址**并不仅仅**只能访问物理内存，也可以用来访问其他的外设，因此你也可以认为物理内存也算是一种外设。

这样设计是因为：如果访问其他外设要使用不同的指令（如 x86 单独提供了**in**, **out** 指令来访问不同于内存的**IO**地址空间），会比较麻烦，于是很多 CPU（如 RISC-V，ARM，MIPS 等）通过 MMIO(Memory Mapped I/O) 技术将外设映射到一段物理地址，这样我们访问其他外设就和访问物理内存一样啦！

我们先不管那些外设，目前我们只关注物理内存。

#### 物理内存探测

操作系统怎样知道物理内存所在的那段物理地址呢？在 RISC-V 中，这个一般是由 bootloader ，即 OpenSBI 来完成的。它来完成对于包括物理内存在内的各外设的扫描，将扫描结果以 DTB(Device Tree Blob) 的格式保存在物理内存中的某个地方。随后 OpenSBI 会将其地址保存在 `a1` 寄存器中，给我们使用。

这个扫描结果描述了所有外设的信息，当中也包括 Qemu 模拟的 RISC-V 计算机中的物理内存。

> 扩展 **Qemu 模拟的 RISC-V virt 计算机中的物理内存**
>
> 通过查看[virt.c](https://github.com/qemu/qemu/blob/master/hw/riscv/virt.c)的**virt_memmap[]**的定义，可以了解到 Qemu 模拟的 RISC-V virt 计算机的详细物理内存布局。可以看到，整个物理内存中有不少内存空洞（即含义为**unmapped**的地址空间），也有很多外设特定的地址空间，现在我们看不懂没有关系，后面会慢慢涉及到。目前只需关心最后一块含义为**DRAM**的地址空间，这就是 OS 将要管理的 128MB 的内存空间。
>
> | 起始地址   | 终止地址   | 含义                                                  |
> | :--------- | :--------- | :---------------------------------------------------- |
> | 0x0        | 0x100      | QEMU VIRT_DEBUG                                       |
> | 0x100      | 0x1000     | unmapped                                              |
> | 0x1000     | 0x12000    | QEMU MROM (包括 hard-coded reset vector; device tree) |
> | 0x12000    | 0x100000   | unmapped                                              |
> | 0x100000   | 0x101000   | QEMU VIRT_TEST                                        |
> | 0x101000   | 0x2000000  | unmapped                                              |
> | 0x2000000  | 0x2010000  | QEMU VIRT_CLINT                                       |
> | 0x2010000  | 0x3000000  | unmapped                                              |
> | 0x3000000  | 0x3010000  | QEMU VIRT_PCIE_PIO                                    |
> | 0x3010000  | 0xc000000  | unmapped                                              |
> | 0xc000000  | 0x10000000 | QEMU VIRT_PLIC                                        |
> | 0x10000000 | 0x10000100 | QEMU VIRT_UART0                                       |
> | 0x10000100 | 0x10001000 | unmapped                                              |
> | 0x10001000 | 0x10002000 | QEMU VIRT_VIRTIO                                      |
> | 0x10002000 | 0x20000000 | unmapped                                              |
> | 0x20000000 | 0x24000000 | QEMU VIRT_FLASH                                       |
> | 0x24000000 | 0x30000000 | unmapped                                              |
> | 0x30000000 | 0x40000000 | QEMU VIRT_PCIE_ECAM                                   |
> | 0x40000000 | 0x80000000 | QEMU VIRT_PCIE_MMIO                                   |
> | 0x80000000 | 0x88000000 | DRAM 缺省 128MB，大小可配置                           |

那么我们就可以很方便的从a1寄存器中读取设备树数据存储地址，在kern_entry的开头将设备树数据从a1寄存器中读取出来，并存入全局变量`boot_dtb`中（顺便读取了当前cpu核心号）

```armasm
# kern\init\entry.S
# a0: hartid
# a1: dtb physical address
# save hartid and dtb address
la t0, boot_hartid
sd a0, 0(t0)
la t0, boot_dtb
```

在kern_entry部分的初始化结束，我们正式进入到kern_init之后，会执行dtb_init函数来读取设备树结构中储存的相关信息。对设备树结构感兴趣的可以点击[链接](https://blog.csdn.net/Rank_d/article/details/106289183)了解

```c
// kern\init\init.c
int kern_init(void) {
    extern char edata[], end[];
    // 先清零 BSS，再读取并保存 DTB 中的内存信息
    memset(edata, 0, end - edata);
    dtb_init();
    // 其他初始化
}
// kern\driver\dtb.c
// 保存解析出的系统物理内存信息
static uint64_t memory_base = 0;
static uint64_t memory_size = 0;

void dtb_init(void) {
    cprintf("DTB Init\n");
    cprintf("HartID: %ld\n", boot_hartid);
    cprintf("DTB Address: 0x%lx\n", boot_dtb);

    if (boot_dtb == 0) {
        cprintf("Error: DTB address is null\n");
        return;
    }

    // 转换为虚拟地址
    uintptr_t dtb_vaddr = boot_dtb + PHYSICAL_MEMORY_OFFSET;
    const struct fdt_header *header = (const struct fdt_header *)dtb_vaddr;

    // 验证DTB
    uint32_t magic = fdt32_to_cpu(header->magic);
    if (magic != 0xd00dfeed) {
        cprintf("Error: Invalid DTB magic number: 0x%x\n", magic);
        return;
    }

    // 提取内存信息
    uint64_t mem_base, mem_size;
    if (extract_memory_info(dtb_vaddr, header, &mem_base, &mem_size) == 0) {
        cprintf("Physical Memory from DTB:\n");
        cprintf("  Base: 0x%016lx\n", mem_base);
        cprintf("  Size: 0x%016lx (%ld MB)\n", mem_size, mem_size / (1024 * 1024));
        cprintf("  End:  0x%016lx\n", mem_base + mem_size - 1);
        // 保存到全局变量，供 PMM 查询
        memory_base = mem_base;
        memory_size = mem_size;
    } else {
        cprintf("Warning: Could not extract memory info from DTB\n");
    }
    cprintf("DTB init completed\n");
}
```

由此，我们就已经将内存的起点和大小读取到了全局变量`memory_base`和`memory_size`中，我们会在物理内存管理初始化的时候用到这些信息

```c
// kern\mm\pmm.c
void pmm_init(void) {
    // other things
    page_init();
    // other things
}
```

`page_init()`连接了设备树探测结果和物理页分配器。它需要读取`get_memory_base()`与`get_memory_size()`保存的信息，确认内存范围有效并输出探测结果，再结合内核能够管理的地址上界确定物理页号范围。但是实现这个函数是本实验任务之一，这里就不展示其完整函数体，而是说明实现时必须满足的内存布局关系。

Qemu 规定的 DRAM 物理内存的起始物理地址为 `0x80000000` 。而在 Qemu 中，可以使用 `-m` 指定 RAM 的大小，默认是 `128MiB` 。因此，默认的 DRAM 物理内存地址范围就是 `[0x80000000,0x88000000)` 。

但是，有一部分 DRAM 空间已经被占用，不能用来存别的东西了！

- 物理地址空间 `[0x80000000,0x80200000)` 被 OpenSBI 占用；
- 物理地址空间 `[0x80200000,KernelEnd)` 被内核各代码与数据段占用；
- 其实设备树扫描结果 DTB 还占用了一部分物理内存，不过我们目前只在初始化的时读取其中的内存起点和长度信息，所以之后可以将它所占用的空间用来存别的东西。

暂不考虑管理数据本身的占用时，可供内核继续规划的物理内存候选范围是`[KernelEnd, 0x88000000)`。这里的`KernelEnd`为内核代码结尾的物理地址，而`kernel.ld`中定义的`end`符号表示内核代码结尾的虚拟地址，二者不能直接混用。

为了管理物理内存，内核需要使用`Page`结构体记录每个物理页的状态。所有`Page`描述符连续排列在内核镜像之后并形成数组，这个数组本身也要占用物理内存，因此内核镜像和描述符数组所在的页面都不能加入空闲页分配器。`page_init()`负责建立这种对应关系，并把描述符数组之后剩余的完整页面交给物理内存管理器。

可以把初始化后的物理内存布局理解为：OpenSBI 占用区之后依次是内核镜像、必要的对齐空隙、`Page`描述符数组、再次按页对齐产生的空隙，以及最终可以分配的连续物理页。物理内存末尾不足一页的部分同样不能交给分配器。

```c
// kern/mm/pmm.h

/* *
 * PADDR - takes a kernel virtual address (an address that points above
 * KERNBASE),
 * where the machine's maximum 128MB of physical memory is mapped and returns
 * the
 * corresponding physical address.  It panics if you pass it a non-kernel
 * virtual address.
 * */
#define PADDR(kva)                                                 \
    ({                                                             \
        uintptr_t __m_kva = (uintptr_t)(kva);                      \
        if (__m_kva < KERNBASE) {                                  \
            panic("PADDR called with invalid kva %08lx", __m_kva); \
        }                                                          \
        __m_kva - va_pa_offset;                                    \
    })

/* *
 * KADDR - takes a physical address and returns the corresponding kernel virtual
 * address. It panics if you pass an invalid physical address.
 * */
/*
#define KADDR(pa)                                                \
    ({                                                           \
        uintptr_t __m_pa = (pa);                                 \
        size_t __m_ppn = PPN(__m_pa);                            \
        if (__m_ppn >= npage) {                                  \
            panic("KADDR called with invalid pa %08lx", __m_pa); \
        }                                                        \
        (void *)(__m_pa + va_pa_offset);                         \
    })
*/
extern struct Page *pages;
extern size_t npage;
// kern/mm/pmm.c

// pages指针保存的是第一个Page结构体所在的位置，也可以认为是Page结构体组成的数组的开头
// 由于C语言的特性，可以把pages作为数组名使用，pages[i]表示顺序排列的第i个结构体
struct Page *pages;
size_t npage = 0;
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
const size_t nbase = DRAM_BASE / PGSIZE;
//(npage - nbase)表示物理内存的页数
```

`npage`表示内核可管理的物理页号上界，`nbase`表示 DRAM 起始位置对应的物理页号，因此描述符数组需要覆盖`[nbase, npage)`中的每个物理页。`pages`保存描述符数组的虚拟地址，数组起点应位于`end`之后并按页对齐；数组末尾则需要借助`PADDR`转换为物理地址，才能继续计算真正可用的物理内存起点。

在空闲区建立之前，所有描述符都应先处于保留状态，防止尚未完成布局的页面被提前分配。描述符数组之后的地址向上按页对齐，设备树报告的内存末端向下按页对齐，只有两者之间非空的完整页面区间才可以通过`pa2page()`转换为描述符起点，并交给`init_memmap()`。这样既能避开内核和描述符数组，也不会把不完整的尾页加入空闲区。

`page_init()`完成上述工作时会调用`init_memmap()`，这和另一个结构体`pmm_manager`有关。虽然 C 语言不直接支持面向对象，但我们可以把物理内存管理功能集中到一个包含函数指针的结构体中。这里的`init_memmap()`实际上会继续调用当前`pmm_manager`提供的初始化接口。

```c
// kern/mm/pmm.c

// physical memory management
const struct pmm_manager *pmm_manager;


// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}
// kern/mm/pmm.h
#ifndef __KERN_MM_PMM_H__
#define __KERN_MM_PMM_H__

#include <assert.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>

// pmm_manager is a physical memory management class. A special pmm manager -
// XXX_pmm_manager
// only needs to implement the methods in pmm_manager class, then
// XXX_pmm_manager can be used
// by ucore to manage the total physical memory space.
struct pmm_manager {
    const char *name;  // XXX_pmm_manager's name
    void (*init)(
        void);  // 初始化XXX_pmm_manager内部的数据结构（如空闲页面的链表）
    void (*init_memmap)(
        struct Page *base,
        size_t n);  //知道了可用的物理页面数目之后，进行更详细的初始化
    struct Page *(*alloc_pages)(
        size_t n);  // 分配至少n个物理页面, 根据分配算法可能返回不同的结果
    void (*free_pages)(struct Page *base, size_t n);  // free >=n pages with
                                                      // "base" addr of Page
                                                      // descriptor
                                                      // structures(memlayout.h)
    size_t (*nr_free_pages)(void);  // 返回空闲物理页面的数目
    void (*check)(void);            // 测试正确性
};

extern const struct pmm_manager *pmm_manager;

void pmm_init(void);

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void); // number of free pages

#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)
```

pmm_manager提供了各种接口：分配页面，释放页面，查看当前空闲页面数。但是我们好像始终没看见pmm_manager内部对这些接口的实现，其实是因为那些接口只是作为函数指针，作为pmm_manager的一部分，我们需要把那些函数指针变量赋值为真正的函数名称。

还记得最早我们在`pmm_init()`里首先调用了`init_pmm_manager()`, 在这里面我们把pmm_manager的指针赋值成`&default_pmm_manager`， 看起来我们在这里实现了那些接口。

```c
// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}
// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}
```

到现在，我们距离完整的内存管理， 就只差`default_pmm_manager`结构体的实现了，也就是我们要在里面实现页面分配算法。

#### 页面分配算法

我们在`default_pmm.c`定义了一个pmm_manager类型的结构体，并实现它的接口

```c
// kern/mm/default_pmm.h
#ifndef __KERN_MM_DEFAULT_PMM_H__
#define  __KERN_MM_DEFAULT_PMM_H__

#include <pmm.h>

extern const struct pmm_manager default_pmm_manager;

#endif /* ! __KERN_MM_DEFAULT_PMM_H__ */
```

较为关键的，是一开始如何初始化所有可用页面，以及如何分配和释放页面。大家可以学习下面的代码，其实现了First Fit算法。

```c
// kern/mm/default_pmm.c
free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void
default_init(void) {
    list_init(&free_list);
    nr_free = 0;//nr_free可以理解为在这里可以使用的一个全局变量，记录可用的物理页面数
}
static void
default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }
}

static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            page = p;
            break;
        }
    }
    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));
        list_del(&(page->page_link));
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}
static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;

    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }

    list_entry_t* le = list_prev(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }

    le = list_next(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}

const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};
```

所谓**First Fit**算法就是当需要分配页面时，它会从空闲页块链表中找到第一个适合大小的空闲页块，然后进行分配。当释放页面时，它会将释放的页面添加回链表，并在必要时合并相邻的空闲页块，以最大限度地减少内存碎片。

完成页面分配算法后我们的物理内存管理算是基本实现了，接下来请同学们完成本次实验练习。

## 分支任务：gdb 调试页表查询过程

在之前，我们已经尝试使用gdb调试了内核启动流程，并且成功的观察到了cpu上电->opensbi初始化->进入到内核的入口点kern_entry->进入到内核c代码入口点kern_init，~~相信大家应该已经基本掌握gdb的使用方法了~~那么我们接下来整点俄罗斯套娃式花活，使用gdb调试一下**正在调试ucore代码**的qemu源码。

前面我们已经完成了物理内存管理，并进入到了**虚拟内存访问模式**（详见lab2：物理内存与页表）。在虚拟内存访问模式下，当CPU执行一条访存指令（如加载或存储）时，它产生的是一个虚拟地址。这个地址被立即送给内存管理单元（MMU）进行处理。

MMU的首要任务是完成虚拟地址到物理地址的转换。转换的第一步，MMU会查询其内部的转址旁路缓存（TLB）。TLB是一个高速缓存，保存着近期使用过的虚拟页号到物理页帧号的映射。如果当前虚拟地址的页号存在于TLB中（即TLB命中），MMU会立刻将对应的物理页帧号与虚拟地址中的页内偏移量组合，形成物理地址，并完成本次内存访问。

如果TLB中找不到该虚拟页号的映射（即TLB未命中），MMU就必须通过查询当前进程的页表来获取转换关系。在RISC-V的SV39多级页表架构下，这是一个硬件自动完成的查表过程：MMU会依据SATP寄存器中存储的页表基址，结合虚拟地址中的多级索引，逐级访问内存中的页表目录，最终在最后一级页表项中找到目标物理页帧号。

在通过页表成功获得物理页帧号后，MMU会执行两个关键操作：

- 使用获得的物理页帧号与页内偏移组合成物理地址，继续完成CPU请求的内存访问。
- 将这条新的“虚拟页号-物理页帧号”映射关系存入TLB中。这样，后续对该虚拟地址附近区域的再次访问，就有很高的概率可以直接在TLB中命中，从而极大地提升了地址转换的效率。

以上的内容大家应该在组成原理课上已经听过甚至实现过，大家都清楚的认识到这些功能都是由硬件完成的。而在我们的实验中，ucore运行在qemu通过软件模拟出来的开发板上。这原本可以降低开发的复杂度，但同时也带来了一定的困扰，会让同学们误以为似乎所有的工作都是软件在做。当然软件的实现还有另一个好处，就是我们可以借助源代码分析和调试的工具近距离的观察它。qemu是一个开源的、用软件模拟硬件的模拟器，因此，由硬件完成的代码，就是在qemu中完成的代码。我们本次实验的任务就是尝试观察一下qemu在接收到一个访存指令的时候，是如何一步步的操作的，从而帮助大家进一步理解这个过程。

> **重点** 我们本次实验有两个目的，其一是让各位同学深入的**对比模拟器的实现**，充分理解一下整个**地址翻译**的过程。 而另一个目的则是**培养大家使用大模型的能力**，希望大家不只是用大模型来做一些常见的工作~~比如只是用大模型来黑话加密一下自己的报告以及编写代码~~，而是利用大模型提高自己学习的效率，真正学到知识。 **以下的调试流程也是助教在大模型的帮助下学习并整理出来的**，大家也可以尝试当作没有这一小节指导书，考虑一下如何在大模型的帮助下从零捋出来一个调试的操作流程。

### 怎样调试qemu

#### 准备带调试信息的QEMU

> **须知**
>
> 其实大家应该明白，大模型的使用很简单，只需要**描述清楚自己的需求**就可以了，前提是你真的知道自己需要什么。在本流程中，附赠一下助教的思路和询问的逻辑，当然，不同的模型的结果也是不一样的，仅供参考。 我对gdb调试有一定了解，我询问的问题是：
>
> 1.  我需要调试一个正在运行我的操作系统内核源码的qemu源码，从而观察一下cpu在虚拟地址空间访问一个虚拟地址是如何查找tlb以及页表的，那么我应该怎么做，是不是需要两个gdb？（附加上指导书lab1中的gdb调试流程以及实验环境搭建的部分）
> 2.  （感觉重新编译一个带调试的qemu的流程没理解）我没有理解，你是如何重新编译一个新的带调试信息的qemu的，不需要执行make install吗，那么整个系统中是不是就有了两个qemu（附加了我们搭建环境时的qemu安装流程部分，也就是下面的几行）

还记得我们在环境搭建时安装QEMU的命令吗？

```sh
$ wget https://download.qemu.org/qemu-4.1.1.tar.xz
$ tar xvJf qemu-4.1.1.tar.xz
$ cd qemu-4.1.1
$ ./configure --target-list=riscv32-softmmu,riscv64-softmmu
$ make -j
$ sudo make install
```

这样安装的QEMU虽然能用，但缺少调试信息，就像一本被撕掉了目录和索引的书，GDB无法在其中**设置断点或查看源码**。我们需要重新编译一个"完整版"的QEMU：

```bash
# 进入QEMU源码目录
cd qemu-4.1.1

# 清理之前的编译结果
make distclean

# 重新配置，这次要带上调试选项
./configure --target-list=riscv32-softmmu,riscv64-softmmu --enable-debug

# 重新编译
make -j$(nproc)
```

**重要提示**，~~这段重要提示也是大模型提醒的~~：编译完成后不要执行`sudo make install`！我们只是要生成一个带调试信息的QEMU可执行文件(编译完成之后，可以在`riscv64-softmmu`目录下找到一个`qemu-system-riscv64`)，而不是替换之前安装在系统里的QEMU。这样，系统里就有两个QEMU：一个是我们日常使用的"正式版"，另一个是我们专门用来调试的"调试版"。

**模型使用示例** 向一个你使用顺手的大模型提问，我要通过gdb来加断点单步调试`qemu-4.1.1`，从而理解在qemu中运行的riscv代码的地址转换逻辑，我需要注意哪些细节。你会得到跟下面差不多的信息。

接下来，我们需要修改ucore的Makefile，让它使用我们新编译的调试版QEMU：

```makefile
# 在Makefile中找到QEMU定义，修改为：
QEMU := /path/to/your/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64
```

请将`/path/to/your/qemu-4.1.1/`替换为你实际的QEMU源码路径。

#### 双重GDB调试策略

> **须知**
>
> 这段比较关键的问题是：两个gdb的作用是什么，按照我的makefile，调试内核使用的是riscv64专用的gdb，而调试qemu源码的gdb是不是应该是x86的gdb。以及我应该如何操作呢（附上我的makefile）

现在进入有趣的部分：我们将同时使用两个GDB会话，但它们的目标和用法完全不同：

- **GDB会话1**：使用普通的gdb命令，附加到QEMU进程本身，用于调试QEMU的源码
- **GDB会话2**：使用riscv64-unknown-elf-gdb，通过QEMU提供的调试接口连接，用于调试运行在模拟器中的ucore内核

这种"双重调试"的架构让我们能够同时观察**qemu模拟器**和**被模拟系统ucore**两个层面上的行为，而且这种模式可以让我们先在ucore当中停下来，然后看到那条要执行的riscv指令，然后再观察在qemu中如何完成了对这条指令的解析。

#### 调试步骤详解

我们需要三个终端窗口（多终端配置参考前文GDB调试一节）：

> **tips** 不难想象，用文字来解释三个终端的运行次序是很麻烦的，我已经能想象到各位因为命令的操作次序晕头转向，你也可以问问大模型来看看到底怎么控制这三个窗口的执行，比如：目前我有了三个终端，但是我没有理解，这三个终端分别是做什么用的，我应该怎么执行这个流程，先执行哪一个，这是在做什么，给我解释的详细一点（你甚至可以把这一节指导书发给他分析一下）。

##### 终端1：启动QEMU模拟器

在项目目录下执行：

```bash
make debug
```

这个命令会启动我们新编译的调试版QEMU，并暂停在初始状态。

##### 终端2：附加调试QEMU进程

首先，我们需要找到QEMU进程的PID：

```bash
pgrep -f qemu-system-riscv64 #这个命令怎么来的呢，应该不难猜吧
```

记住这个PID，然后启动GDB并附加到这个进程：

```bash
sudo gdb
```

在GDB会话中执行：

> **须知**
>
> 这里经过了很多的调整，比如，`handle SIGPIPE nostop noprint`这一句就是其中出现问题之后，我将命令行的输出提问给了大模型，它给我的回答。尝试一下，如果不运行这一句指令，会出现什么后果，让大模型为你分析一下这是怎么回事

```text
(gdb) attach <刚才查到的PID>
(gdb) handle SIGPIPE nostop noprint
(gdb) # 你可以在这里执行一些操作，设置一些断点等
(gdb) continue # 之后就启动执行
```

##### 终端3：调试ucore内核

在gdb已经设置好断点并启动qemu的执行（即执行了continue）之后，在项目目录下执行：

```bash
make gdb
```

这个GDB会话会连接到QEMU的`GDB stub`，就像我们平时调试内核一样。你可以在这里控制ucore的执行，比如**打断点让它在某个访存指令处暂停**、通过指令查看寄存器的值，某个符号的地址等。

> **须知**
>
> 这个地方有必要讲一下，我们之前尝试过在某个符号打断点，比如`b kern_entry`，最后就会显示为在0x80200000处打上了断点，也可以直接打在某个位置，**那么我们怎么知道任意一条指令的地址呢**，询问了LLM之后，它给我的答案是我可以直接调试ucore代码，单步执行的过程中，gdb会显示每条指令的地址，`$pc`中也是当前指令的地址，我们就可以找到某个访存指令用于测试。

#### 观察完整的地址转换流程

现在，三个终端已经建立了一个完整的调试环境：

在终端3中，让ucore执行到特定的访存指令。当访存发生时，终端2的GDB会在我们设置的断点处中断，在终端2中，你可以单步执行QEMU源码，观察TLB查询（**明示**）、页表遍历的具体过程

继续执行后，控制权回到终端3，可以继续调试ucore。

通过这种设置，我们实际上建立了一个从ucore用户程序发出访存请求，到QEMU模拟硬件MMU处理流程的完整观察链路。这种"透视"能力对于深入理解虚拟内存机制是极其宝贵的。

> **思考**
>
> 前文中，我们的思路是，执行ucore到某个访存指令，然后再在qemu的源码打上断点，去单步执行，有没有更简单的思路呢，应该是有的，~~好好思考一下，想不到就问问大模型~~我们可以这么想，既然是访存，哪个指令并不重要，访问的地址很重要，我只需要找到**某个一定会被访问的内存地址**，然后在你找到的`qemu`的关键代码处打上一个**条件断点**，比如，在某个关键函数入口点判断传入的地址参数addr是不是我们要观测的地址（当然你最好知道**这个地址是的的确确被访问到的**，比如，当我们将要跳转到内核的C入口点的时候，内核C入口点的地址就是我们一定要访存的地址）。那么我们就不用调试ucore代码，只要在一开始就在`qemu`中打好条件断点，然后启动ucore执行，就自然会在合适的时间停在合适的位置。

这种双重调试的方法虽然设置起来稍显复杂，但它为我们打开了一扇观察计算机系统底层运作的窗口。当你亲眼看到软件如何精确模拟硬件行为时，很多抽象的概念都会变得具体而清晰。

> **须知**
>
> 我们已经给出了基础的操作流程，以下是我们调试时可以参考的一些策略
>
> 1.  如何在qemu源码中找到和地址翻译有关的代码片段呢，如何开始调试？~~当然是慢慢找~~，**大模型或许可以给你一个好点的答案（明示）**，我们期望同学们在大模型的帮助下简单理解qemu的源码，当然，并不需要同学们在短时间内完全理解如此大的一个项目，那不现实，但是在大模型的帮助下找到一些**关键的代码位置**还是各位应当掌握的能力的，一些嵌入到ide的大模型可能也能发挥很大的作用。
> 2.  某个函数入口点（例如一个"翻译函数"）可能在每个地址都会在`qemu`运行的时候调用翻译一遍，如果每次都停下来再按c（continue），那真就是一个代码调一天了，`gdb`提供的**条件断点**功能或许可以帮到大家，我们可以在代码中找到某个访存指令的位置，然后查看一下这个指令的访问的地址，然后在"翻译函数"打断点的时候设置一个条件，也就是address是否等于"我们期望的地址"，这样可以节省很多时间，关于条件断点的更多复杂应用，~~我也不会~~同学们可以**让大模型当你的智能体**。
> 3.  我们本次的实验重在体验一下在大模型的帮助下调试的流程，正如助教所言，这是一个开发世界体验项目，助教的思路也**仅供参考**，各位的操作合理即可，或许能开发出新的玩法，学到新的知识。

### 调试要求（附赠部分助教使用的问题）

1.  尝试理解我们调试流程中涉及到的qemu的源码，给出关键的调用路径，以及路径上一些**关键的分支语句（不是让你只看分支语句）**，并通过调试演示某个访存指令访问的虚拟地址是**如何在qemu的模拟中被翻译成一个物理地址的**。
2.  单步调试页表翻译的部分，解释一下关键的操作流程。（这段是地址翻译的流程吗，我还是没有理解，给我解释的详细一点 / 这三个循环是在做什么，这两行的操作是从当前页表取出页表项吗，我还是没有理解）
3.  是否能够在qemu-4.1.1的源码中找到模拟cpu查找tlb的C代码，通过调试说明其中的细节。（按照riscv的流程，是不是应该先查tlb，tlbmiss之后才从页表中查找，给我找一下查找tlb的代码）
4.  仍然是tlb，qemu中模拟出来的tlb和我们真实cpu中的tlb有什么**逻辑上的区别**（提示：可以尝试找一条未开启虚拟地址空间的访存语句进行调试，看看调用路径，和开启虚拟地址空间之后的访存语句对比）
5.  记录下你调试过程中比较~~抓马~~有趣的细节，以及在观察模拟器通过软件模拟硬件执行的时候了解到的知识。
6.  记录实验过程中，有哪些通过大模型解决的问题，记录下当时的情景，你的思路，以及你和大模型交互的过程。

## 实验报告要求

1.  基于`markdown`格式完成实验报告，以文本说明为主。
2.  说明自己对本章节整体逻辑的理解，即本章节是按照什么顺序、围绕哪些功能逐步实现的。
3.  说明自己对每个功能的核心函数或功能模块的理解。
4.  说明每个练习的`spec`是怎么写的，`prompt`是怎么写的。
5.  说明在使用提示词让 AI 生成代码的过程中出现了什么 bug，以及你是怎么解决的。
6.  列出你认为本实验中重要的知识点，以及与对应的 OS 原理中的知识点，并简要说明二者的含义、关系和差异。
7.  列出你认为 OS 原理中很重要、但在本实验中没有对应上的知识点。

从oslab网站上取得实验代码后，进入目录labcodes/lab2，完成实验要求的各个练习。在实验报告中回答所有练习中提出的问题。在目录labcodes/lab2下存放实验报告，推荐用**markdown**格式。每个小组建一个gitee或者github仓库，对于lab2中编程任务，完成编写之后，再通过git push命令把代码和报告上传到仓库。最后请一定提前或按时提交到git网站。

注意有“lab2”的注释，代码中所有需要完成的地方（challenge 除外）都有“lab2”和“YOUR CODE”的注释，请在提交时特别注意保持注释，并将“YOUR CODE”替换为自己的学号，并把所有标有对应注释的部分填上正确的代码。
