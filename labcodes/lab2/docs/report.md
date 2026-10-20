# 操作系统实验报告

## 实验基本信息

| 项目         | 内容                                           |
| ------------ | ---------------------------------------------- |
| **实验名称** | Lab 2:物理内存和页表                           |
| **小组成员** | 2414007-王奕瑜、2414015-鲁昕宁、2411622-张雨函 |
| **完成日期** | 2026-10-20                                     |

### 小组分工

练习如何分工？

|      成员      |                 负责的练习/模块                 |
| :------------: | :---------------------------------------------: |
| 2414007-王奕瑜 |                                                 |
| 2414015-鲁昕宁 | 4.6 Challenge：任意大小的内存单元 slub 分配算法 |
| 2411622-张雨函 |                                                 |

实验报告如何分工？

|      成员      | 负责的实验报告内容 |
| :------------: | :----------------: |
| 2414007-王奕瑜 |                    |
| 2414015-鲁昕宁 |                    |
| 2411622-张雨函 |                    |

---

## 一、实验目的

---

## 二、实验环境

你们使用的 AI 工具

|      成员      | AI 编程工具 | 底层模型 | 备注 |
| :------------: | :---------: | :------: | :--: |
| 2414007-王奕瑜 |             |          |      |
| 2414015-鲁昕宁 |   Cursor    | Grok 4.7 |  无  |
| 2411622-张雨函 |             |          |      |

**说明：**

- **AI 编程工具**：指具体使用的终端工具、编辑器插件、桌面应用或浏览器界面
- **底层模型**：指该工具使用的大语言模型及版本

---

## 三、实验整体逻辑分析

### 3.1 本章节的逻辑主线

### 3.2 功能的逐步实现

#### 3.2.1 物理内存探测与页描述符初始化

本实验首先实现的是物理内存探测与页描述符初始化（`page_init()`），原因有以下几点：

1. **它是所有后续物理内存管理功能的前提。** 无论是 First-Fit、Best-Fit 还是 Buddy System，分配器管理的对象都是"以页为单位的空闲物理内存"。如果内核还不知道有多少物理内存、这些内存位于哪个地址范围，分配器就没有可以管理的对象，`alloc_pages()`/`free_pages()` 也就无从谈起。
2. **操作系统需要为每个物理页记录状态。** 分配算法依赖 `Page` 描述符中的引用计数、`PG_reserved`、`PG_property` 和 `property` 等信息来区分"已被占用"和"空闲"的页面，并把空闲块串成链表。这些描述符必须在分配器工作之前统一建立，并且初始状态应当是"全部保留"，避免尚未规划好的页面被提前分配出去。
3. **必须保护已经被占用的内存。** DRAM 的低地址部分被 OpenSBI 占用，随后是内核镜像，而 `Page` 描述符数组本身也要占用物理内存。如果不在初始化时把这些区域排除，分配器就可能把它们当作空闲页发出去，造成内核代码、数据或描述符被覆盖。同时还需要处理页面对齐、虚拟地址与物理地址的换算（`va_pa_offset`），否则空闲区的起止位置会算错。
4. **它把"硬件信息"和"分配算法"衔接起来。** `page_init()` 从设备树获取内存范围，再通过 `init_memmap()` 把初始空闲区交给当前的 `pmm_manager`，使上层分配算法不必关心内存信息来自何处，只需面向统一的接口实现。

因此，我们先完成物理内存初始化，建立"可管理物理页"的初始状态；之后才能在此基础上理解并实现各种页分配算法，并通过 `check_alloc_page()` 验证分配与释放的正确性。

#### 3.2.3 伙伴系统分配算法

在 First-Fit 和 Best-Fit 之后，我们继续实现伙伴系统（Buddy System），原因有以下几点：

1. **First-Fit/Best-Fit 的开销和碎片问题。** 二者都用一条按地址排序的空闲链表，分配时要线性扫描，释放时要按地址找位置才能合并，最坏情况下代价与空闲块个数成正比；而且长期运行后，大小不一的空闲块散布在内存里，外部碎片会越来越严重。伙伴系统把空闲块限定为 2 的幂大小，用按阶分开的链表使分配和释放的代价只与阶数（`log N`）有关，是对前两种算法缺点的直接改进。
2. **伙伴系统让"合并"变得简单且可靠。** 块的大小和对齐都是 2 的幂，一个块的伙伴地址只需把页号的某一位取反即可算出，不必再遍历链表找相邻块，这也是 Linux 内核物理页分配器采用它的原因。
3. **它是后续内存管理功能的页级基础。** 后面的 SLUB 等任意大小对象分配器，需要先从页级分配器取得整页，再在页内切分小对象；伙伴系统正好提供"按 2 的幂页数分配、自然对齐"的页级接口。
4. **便于对比不同分配策略。** 前两个算法的内部状态都是"一条链表 + `property`"，伙伴系统则需要多条链表、分裂与合并、区间边界等新的机制。把三种算法接到同一个`pmm_manager`框架上，可以体会到"同一接口、不同策略"的设计，也能通过它们各自的碎片表现比较其优缺点。

---

## 四、实验内容与实现

### 4.1 实现物理内存探测与页描述符初始化功能

**负责人：** 2414007-王奕瑜

#### 4.1.1 模块功能描述

**需要实现/修改的函数：**

```c
static void page_init(void);  // kern/mm/pmm.c
```

**功能说明：**

`page_init()`由`pmm_init()`在`init_pmm_manager()`之后调用，是物理内存管理中"硬件信息"与"页分配器"之间的桥梁。它不接收参数，也不返回结果，完成后需要保证：

- **地址换算与内存范围探测：** 设置`va_pa_offset`为内核线性映射的虚实偏移`PHYSICAL_MEMORY_OFFSET`；通过`get_memory_base()`和`get_memory_size()`读取设备树探测到的 DRAM 起始地址和大小，输出`physical memory map`信息；若设备树没有提供有效的内存信息，则`panic`终止初始化。
- **确定可管理上界`npage`：** 以设备树内存末端（不超过内核管理的上界`PHYSICAL_MEMORY_END`）对应的物理页号作为`npage`，描述符需要覆盖`[nbase, npage)`中的每个物理页。
- **建立`Page`描述符数组：** 把`pages`放在内核镜像末尾符号`end`之后的第一个页边界，先将整个数组清零，再把每个描述符都置为保留状态（`PG_reserved`），防止尚未规划好的页面被提前分配。
- **排除已占用内存、注册空闲区：** OpenSBI、内核镜像以及描述符数组本身占用的物理内存都不能分配。函数先用`PADDR()`把描述符数组末尾的虚拟地址换算成物理地址，向上按页对齐得到空闲区起点，把内存末端向下按页对齐得到终点（不足一页的尾部丢弃）；仅当两者之间存在完整页面时，才通过`pa2page()`取得起始描述符并调用`init_memmap()`，把这些页面交给当前`pmm_manager`管理。

初始化完成后的物理内存布局如下：

```text
0x80000000      0x80200000    end(物理)    pages(对齐)     mem_begin             mem_end(对齐)  0x88000000
|   OpenSBI    |  内核镜像    | 对齐空隙 | Page 描述符数组 | 对齐空隙 |   可分配的连续物理页   | 尾部不足一页 |
|<------------------- reserved ------------------------>|          |<---- init_memmap --->|
```

**与其他模块的交互：**

- 依赖`dtb_init()`（`kern/driver/dtb.c`）提前解析设备树并保存内存起点和大小；
- 通过`init_memmap()`调用当前`pmm_manager`的`init_memmap`接口，把初始空闲页交给 First-Fit / Best-Fit 等分配器；
- 之后的`check_alloc_page()`、`alloc_pages()`、`free_pages()`都以这里建立的`pages`、`npage`和空闲区为基础。

#### 4.1.2 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

````markdown
[PROMPT]
**任务**：修改`kern/mm/pmm.c`，在标有`//lab2 function1 YOUR CODE`的位置实现`static void page_init(void)`，完成物理内存探测与`Page`描述符初始化：根据设备树提供的物理内存信息建立描述符数组，并把排除 OpenSBI、内核镜像和描述符数组占用空间后的完整空闲页面交给当前`pmm_manager`管理。

**操作要求**：你必须在项目中修改实际文件，保留现有代码，只在标记位置补全，不要改动`pmm_init()`等其他函数。将标记注释中的`YOUR CODE`替换为学号`2414007`（保留`lab2 function1`）。为新增代码添加英文 Doxygen 注释，`@author`写`Yiyu Wang(2414007)`。

**输出要求**：直接进行文件操作，使用下面[RELY]、[GUARANTEE]、[SPECIFICATION]部分的信息，以工程真实代码为准，不要引入无关功能。

[RELY]
地址与页面宏：

- `PGSIZE`（4096）、`PGSHIFT`（12）、`PPN(addr)`（`addr >> PGSHIFT`）：`kern/mm/mmu.h`
- `ROUNDUP(a, n)`、`ROUNDDOWN(a, n)`：`libs/defs.h`，结果类型与`a`相同
- `PHYSICAL_MEMORY_OFFSET`（`0xFFFFFFFF40000000`，内核线性映射的虚实偏移）、`PHYSICAL_MEMORY_END`（`0x88000000`，内核管理的物理内存上界）：`kern/mm/memlayout.h`
- `DRAM_BASE`（`0x80000000`）：`libs/riscv.h`
- `PADDR(kva)`：`kern/mm/pmm.h`，把内核虚拟地址转换为物理地址（`kva - va_pa_offset`），`kva`小于`KERNBASE`时 panic

全局变量（`kern/mm/pmm.c`）：

- `struct Page *pages;`——`Page`描述符数组的虚拟起始地址
- `size_t npage;`——内核可管理的物理页号上界（不含）
- `uint64_t va_pa_offset;`——虚拟地址与物理地址之差
- `const size_t nbase;`——DRAM 起始位置对应的物理页号，`DRAM_BASE / PGSIZE`

页描述符与状态宏（`kern/mm/memlayout.h`）：

- `struct Page { int ref; uint64_t flags; unsigned int property; list_entry_t page_link; };`
- `SetPageReserved(page)`、`PageReserved(page)`：设置/测试`PG_reserved`位；该位为 1 表示页面被内核占用，不能参与分配

已有函数：

- `uint64_t get_memory_base(void)`、`uint64_t get_memory_size(void)`（`kern/driver/dtb.h`）：返回设备树中的内存起始物理地址和字节数；设备树无效时返回 0
- `panic(fmt, ...)`（`kern/debug/assert.h`中的宏，输出信息并终止内核）、`int cprintf(const char *fmt, ...)`
- `void *memset(void *s, char c, size_t n)`（`libs/string.h`）
- `static void init_memmap(struct Page *base, size_t n)`（`kern/mm/pmm.c`）：把`base`开始的`n`个页面作为初始空闲区交给当前`pmm_manager`，要求这些页面此时都带有`PG_reserved`
- `static inline struct Page *pa2page(uintptr_t pa)`（`kern/mm/pmm.h`）：返回`&pages[PPN(pa) - nbase]`，`PPN(pa) >= npage`时 panic
- 链接符号`extern char end[];`：内核镜像末尾的**虚拟**地址

物理内存布局（默认 QEMU，DRAM 为`[0x80000000, 0x88000000)`）：

- `[0x80000000, 0x80200000)`被 OpenSBI 占用
- `[0x80200000, end 对应的物理地址)`被内核代码和数据占用
- `Page`数组紧接在内核镜像之后，按页对齐，覆盖物理页号区间`[nbase, npage)`，自身也占用物理内存
- 虚拟地址、物理地址、物理页号和`Page`描述符地址是四种不同的量，不能混用

[GUARANTEE]
必须实现：

```c
static void page_init(void);  // kern/mm/pmm.c
```

`page_init()`不接收参数，也不返回结果，由`pmm_init()`调用。允许新增辅助函数，但必须是`static`的，只在`pmm.c`内部使用，不得改变其他公开接口。

[SPECIFICATION]
**page_init**

- Pre-Condition：`init_pmm_manager()`已经执行，`pmm_manager`已指向某个分配器并完成其`init()`；`dtb_init()`已经执行；内核已通过启动页表在线性映射下运行。
- Post-Condition：`va_pa_offset`、`npage`和`pages`被正确设置；已向控制台输出设备树探测到的物理内存范围；`[nbase, npage)`内的每个页面都有描述符，内核镜像和描述符数组所在的页面保持保留状态且不会进入空闲区；描述符数组之后所有完整的空闲页面已通过`init_memmap()`注册给当前`pmm_manager`。
- Case 1：设备树没有提供有效的物理内存信息。此时无法确定可管理的范围，必须终止初始化，不得继续建立描述符。
- Case 2：设备树信息有效。依次完成：设置虚实偏移；输出探测到的内存范围；根据设备树内存末端和内核可管理的上界确定`npage`；把描述符数组放在内核镜像之后的第一个页边界，并使所有描述符在空闲区建立之前处于保留状态；换算出描述符数组末尾的物理地址，其向上按页对齐作为空闲区起点，设备树内存末端向下按页对齐作为空闲区终点，再把两者之间的页面注册为初始空闲区。
- Case 3：描述符数组之后已没有可用的完整页面（空闲区起点不小于终点）。此时不得调用`init_memmap()`。
- Requirements：
  - 不足一页的内存末尾不能交给分配器。
  - 描述符数组所在内存的初始内容不可假定为零，各描述符的状态必须由本函数显式确定。
  - 比较或传递地址前，必须明确它是虚拟地址还是物理地址，并做相应的转换。
  - 不得修改`best_fit_pmm.c`等其他文件。
````

#### 4.1.3 实现迭代过程

本模块的实现经历了 1 次迭代，过程如下：

---

##### 第一次迭代

使用上述提示词生成`page_init()`后，在 Best-Fit 模块尚未完成的状态下，用本实验提供的测试文件对该函数单独验证：把`tools/test_lab2.c`注入临时目录并运行`make grade`，同时用`make qemu`观察完整启动输出。

> 说明：由于`best_fit_pmm.c`中尚无`best_fit_pmm_manager`，仓库在这一阶段无法直接链接，因此测试在临时副本中进行，副本里仅为`best_fit_pmm.c`补了一个占位用的`best_fit_pmm_manager`（仅为通过链接，不含任何 Best-Fit 逻辑）。第一次运行`make grade`时，占位代码缺少`basic_check()`/`best_fit_check()`依赖的`free_list`和`nr_free`定义，编译报错；这是测试环境的问题，与`page_init()`的提示词无关，补全占位定义后重新运行即可。

**最终结果：**

测试结果：

- ✅ 编译通过
- ✅ `check page init`通过，输出`[Test 1] Page Init: PASS`
- ✅ `make qemu`输出了`physical memory map: memory: 0x0000000008000000, [0x0000000080000000, 0x0000000087ffffff].`，随后`check_alloc_page() succeeded!`，说明空闲区已被`pmm_manager`正确接收，First-Fit 的自检可以在其上正常分配和释放
- ⚠️ `[Test 2] Best-Fit`在这一阶段为`WRONG`，原因是 Best-Fit 属于任务二，尚未实现，不属于本模块

该测试从以下角度检验了`page_init()`：`va_pa_offset`是否等于`PHYSICAL_MEMORY_OFFSET`；`npage`是否等于设备树内存末端的物理页号；`pages`是否非空、页对齐且位于内核`end`之后；空闲页数是否合理；内核镜像尾页和描述符数组所在页面是否仍为保留状态；`page2pa()`与`pa2page()`是否互逆。

本次生成的代码在测试中一次通过，没有需要再调整提示词的问题。

### 4.2 理解first-fit 连续物理内存分配算法

**负责人：** [学号-姓名]

### 4.3 实现 Best-Fit 连续物理内存分配算法

**负责人：** [学号-姓名]

### 4.4 buddy system（伙伴系统）分配算法

**负责人：** [2414007-王奕瑜]

#### 4.4.1 模块功能描述

**需要实现/修改的函数：**

```c
extern const struct pmm_manager buddy_pmm_manager;  // kern/mm/buddy_pmm.h

static void buddy_init(void);                                 // kern/mm/buddy_pmm.c
static void buddy_init_memmap(struct Page *base, size_t n);
static struct Page *buddy_alloc_pages(size_t n);
static void buddy_free_pages(struct Page *base, size_t n);
static size_t buddy_nr_free_pages(void);
static void buddy_check(void);

static void init_pmm_manager(void);                           // kern/mm/pmm.c（条件编译接入）
```

##### 第一部分：伙伴系统算法简介

伙伴系统把可用内存划分为大小为`2^k`页的块，`k`称为阶（order），每一阶维护一条空闲链表，每个块的起始页号都是自身大小的整数倍（自然对齐）。若两个同阶块恰好是同一个`2^(k+1)`块的两半，它们互为**伙伴**，页号只在第`k`位不同，因此伙伴的页号可以直接用异或算出：`buddy_ppn = ppn ^ (1 << k)`。

- **分配**：把请求页数向上取整到`2^k`，从`k`阶开始向高阶找第一条非空链表；如果取到的是更高阶的块，就逐级对半分裂，一半留用，另一半挂入低一阶的链表。
- **释放**：把块放回对应阶的链表；如果它的伙伴也是同阶的空闲块，就把二者合并成高一阶的块，并重复检查，直到伙伴不空闲或达到最大阶。
- **例子**：64 页的块上申请 1 页，会依次分裂成 32+16+8+4+2+1+1，其中 1 页被分配，其余各阶各留一块；把这 1 页释放，会沿着相反的路径逐级合并回 64 页。

它的特点是分配和释放的时间只与阶数有关，合并只需要检查伙伴，不会因为长期运行而出现越来越多无法合并的小块；代价是请求大小要取整到 2 的幂，存在内部碎片。

##### 第二部分：设计思路

| 设计点   | 做法                                                                                                                       | 原因                                                                                                |
| -------- | -------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| 数据结构 | `buddy_area_t`含 16 条空闲链表（0 至 15 阶）和空闲页总数；块首页带`PG_property`，`property`记录块的页数                    | 完全复用`struct Page`，不需要位图或额外元数据，也与 First-Fit/Best-Fit 对`property`的约定一致       |
| 对齐基准 | 用**绝对物理页号**判断对齐并计算伙伴                                                                                       | `page_init()`交来的空闲区起点（如`0x80346`）并不是 2 的幂对齐，以绝对页号对齐才能得到正确的伙伴关系 |
| 初始化   | `init_memmap`把区间贪心切成最大的自然对齐块，每块调用同一个释放函数                                                        | 起点和页数任意时都不浪费页面，以 5.1 节中起点为`0x80346`、共 31930 页的空闲区为例，只会切成 10 个块 |
| 管理区间 | 记录`[zone_start, zone_end)`，合并时伙伴必须落在区间内                                                                     | 区间两端的块，其伙伴可能是保留页或根本没有描述符的地址，不加判断会越界访问或错误合并                |
| 分配     | 取目标阶或更高阶最小非空链表的第一个块；分裂时保留低地址的一半，高地址的一半挂入链表                                       | 取链表头是`O(1)`；总是保留低半，结果可预测，便于测试                                                |
| 释放     | 循环：算伙伴 → 检查区间 → 检查伙伴是同阶空闲块 → 摘下伙伴并取较低地址为新块首页 → 阶数加一                                 | 合并只依赖伙伴判断，不需要地址有序的链表                                                            |
| 请求取整 | `alloc_pages(n)`实际消耗`2^ceil(log2 n)`页，`free_pages(base, n)`须传相同的`n`；对齐、区间、保留页等前置条件用`assert`检查 | 与经典伙伴系统一致；非法释放立刻暴露，而不是悄悄破坏链表                                            |
| 接入内核 | `pmm.c`中用宏`PMM_BUDDY`选择管理器，`Makefile`的`make PMM=buddy`定义该宏，默认仍是 Best-Fit                                | 不改变任务二已通过的 Best-Fit 与`make grade`中的`[Test 2]`，同时可以单独运行伙伴系统                |

##### 第三部分：测试设计

测试分为两层：`buddy_check()`负责检验算法本身，`test_buddy_system()`负责把它接入`make grade`。

**隔离方式。** `buddy_check()`先从现有空闲内存中取出一个 64 页、自然对齐的块作为测试区，保存分配器状态，然后在空链表、管理区间收窄到该块的状态下运行场景，最后还原状态并归还该块。这样测试不依赖机器上的具体地址，也不会影响真实空闲内存，并且无论当前使用的是哪个管理器都可以运行（`make grade`中当前管理器是 Best-Fit，测试区由`test_buddy_system()`从中取出 256 页提供）。

**覆盖的核心行为和边界情况：**

| 场景           | 覆盖内容                                                                                        |
| -------------- | ----------------------------------------------------------------------------------------------- |
| 初始布局       | 对齐的 64 页恰好形成一个 6 阶块                                                                 |
| 分裂           | 申请 1 页后，0 至 5 阶各剩一块，返回的是低地址的一半，各块的`property`正确                      |
| 伙伴寻址与对齐 | 后续申请取到相邻页，互为伙伴的两页页号只在最低位不同                                            |
| 合并           | 伙伴未释放时不合并；伙伴释放后逐级链式合并回 6 阶块                                             |
| 取整与对齐     | 申请 1、2、3、4、5、8、9、16、17、33 页时，实际消耗的页数和块的自然对齐都正确                   |
| 耗尽与非法大小 | 整块耗尽后再申请返回`NULL`；65 页、`2^15`页、`2^15 + 1`页、`(size_t)-1`页都返回`NULL`且状态不变 |
| 外部碎片       | 两个互不为伙伴的 16 页块被释放时不合并，空闲 32 页却申请不到 32 页；补齐后才合并                |
| 随机压力       | 固定种子，3000 轮随机申请和释放，逐页检查已分配块互不重叠、自然对齐、不越界，最后恢复为整块     |
| 区间边界       | 区间外的空闲伙伴不被合并，区间末端之后和区间起点之前两个方向都测试                              |

**为什么能够有效检验实现。**

1. **精确的期望值。** 分裂、合并、取整等场景的布局是确定的，测试直接断言每个块的页号、阶数、`property`和各阶块数，而不仅仅是"没有崩溃"。
2. **全局不变式。** 每一步之后都用`verify_lists()`检查：块首页带`PG_property`且不是保留页、`property`等于块大小、页号自然对齐、位于管理区间内、伙伴不是同阶空闲块（即合并彻底）、各块页数之和等于空闲页数。这样即使某个场景没有直接断言，错误的链表状态也会被发现。
3. **随机压力测试补足人工场景的盲区。** 重叠分配、重复分配、遗漏合并等问题往往只在特定的分配释放序列下出现，逐页占用表可以直接发现这些问题。
4. **变异验证。** 我们向实现中逐个植入 9 类典型错误（伙伴地址、不合并、分裂留错半边、请求不取整、计数不更新、不检查区间、合并时不比较阶数、摘链不清标志、初始化不考虑对齐），测试全部能够发现，说明它不是"只要不崩溃就通过"的弱测试（详见 4.4.3）。

#### 4.4.2 最终提示词

##### 4.4.2.1 设计阶段

````markdown
[PROMPT]
**任务**：新增`kern/mm/buddy_pmm.c`和`kern/mm/buddy_pmm.h`，实现 Buddy System 物理内存管理器`buddy_pmm_manager`，并接入内核：修改`kern/mm/pmm.c`的`init_pmm_manager()`，在定义了宏`PMM_BUDDY`时选用`buddy_pmm_manager`，否则保持默认的`best_fit_pmm_manager`；修改`Makefile`，使`make PMM=buddy`能够定义该宏。

**操作要求**：你必须在项目中创建和修改实际文件，不要只输出代码片段。保留现有代码，不要修改`default_pmm.c`、`best_fit_pmm.c`、`struct pmm_manager`和`struct Page`。在`buddy_pmm.c`开头和`pmm.c`的接入处加上注释`//lab2 function3 2414007`。为新增代码添加英文 Doxygen 注释，`@author`写`Yiyu Wang(2414007)`。

**输出要求**：直接进行文件操作，使用下面[RELY]、[GUARANTEE]、[SPECIFICATION]部分的信息，以工程真实代码为准，不要引入无关功能。

[RELY]
物理内存管理器接口（`kern/mm/pmm.h`）：

- `struct pmm_manager { const char *name; void (*init)(void); void (*init_memmap)(struct Page *base, size_t n); struct Page *(*alloc_pages)(size_t n); void (*free_pages)(struct Page *base, size_t n); size_t (*nr_free_pages)(void); void (*check)(void); };`
- `pmm_manager`全局指针指向当前使用的分配器，`alloc_pages()`/`free_pages()`只是把调用转发给它

页描述符与地址换算（`kern/mm/memlayout.h`、`kern/mm/pmm.h`）：

- `struct Page { int ref; uint64_t flags; unsigned int property; list_entry_t page_link; };`
- `PG_reserved`：为 1 表示页面被内核占用，不能参与分配；`PG_property`：为 1 表示该页是某个空闲块的首页，此时`property`为该块的页数
- `SetPageProperty(p)`、`ClearPageProperty(p)`、`PageProperty(p)`、`PageReserved(p)`、`le2page(le, member)`
- `extern struct Page *pages;`、`extern const size_t nbase;`、`ppn_t page2ppn(struct Page *)`：`pages[i]`对应物理页号`i + nbase`
- `set_page_ref(page, val)`

其他：

- `libs/list.h`：`list_init`、`list_add`、`list_del`、`list_next`、`list_empty`，均为 O(1) 的双向循环链表操作
- `assert(x)`（`kern/debug/assert.h`）、`cprintf`

初始空闲区的特点（由`page_init()`调用`init_memmap()`一次）：

- 区间内每个页面都带有`PG_reserved`
- 起始物理页号可以是任意对齐（例如`0x80346`），页数通常也不是 2 的幂
- 默认 QEMU 只有 128MiB 内存，共 2^15 页，因此最大块不超过 2^15 页

[GUARANTEE]
必须实现：

```c
extern const struct pmm_manager buddy_pmm_manager;  // kern/mm/buddy_pmm.h
```

`buddy_pmm.c`中为`struct pmm_manager`的六个成员各提供一个`static`函数（`init`、`init_memmap`、`alloc_pages`、`free_pages`、`nr_free_pages`、`check`）。允许新增`static`辅助函数，只在`buddy_pmm.c`内部使用。`check`在本阶段只需提供一个只检查空闲链表一致性的最小实现，完整的测试由测试阶段的提示词生成。

[SPECIFICATION]
**数据组织**

- 空闲内存以 2^k 页的块为单位管理，k 称为阶，取值 0 到 15；每一阶有各自的空闲链表，块的首页通过`page_link`挂入。
- 每个块按自身大小自然对齐：块首页的物理页号（而不是相对某个基址的序号）是 2^k 的整数倍。
- 同一阶的两个块若恰好是同一个 2^(k+1) 块的两半，则互为伙伴，二者的物理页号只在第 k 位不同。
- 只有空闲块的首页带`PG_property`，其`property`记录块的页数，与其他分配器的约定一致；已分配页面的`PG_property`为 0，保留页面不得被修改。

**buddy_init**

- Post-Condition：所有阶的空闲链表为空，空闲页数为 0，没有任何受管理的区间。

**buddy_init_memmap**

- Pre-Condition：`base`起的`n`页都带`PG_reserved`，`n > 0`。
- Post-Condition：这些页面被清除保留标志、引用计数为 0，并全部进入空闲链表；记录受管理的物理页号区间，之后只有区间内的页面才可能被合并。
- Requirements：
  - 起点或页数不满足对齐或 2 的幂时，不得浪费页面，应把区间切成尽可能大的、自然对齐的块。
  - 允许被多次调用，管理区间取所有区间的并集。

**buddy_alloc_pages**

- Pre-Condition：`n > 0`。
- Post-Condition：返回一个自然对齐的块的首页，该块含 2^k 页，k 为能容纳`n`页的最小阶；空闲页数相应减少 2^k。
- Case 1：对应阶的空闲链表非空。直接取出一个块。
- Case 2：对应阶为空但更高阶有块。取最小的非空高阶块逐级分裂：每次分裂保留低地址的一半继续使用，高地址的一半挂入低一阶的空闲链表。
- Case 3：`n`超过最大块或没有足够大的空闲块。返回`NULL`，空闲状态不变。

**buddy_free_pages**

- Pre-Condition：`base`是之前用同一个`n`分配得到的块首页；该块自然对齐，位于受管理区间内，其页面既不是保留页也不是空闲块首页。
- Post-Condition：空闲页数增加 2^k；该块回到空闲链表，并尽可能与伙伴合并，合并后的块以较低地址的块为首页。
- Case 1：伙伴不在受管理区间内、不是空闲块首页、或阶数不同。不合并，直接挂入对应阶的链表。
- Case 2：伙伴是同阶空闲块。把它从链表摘下并合并成高一阶的块，再对新块重复上述判断，直到不能合并或达到最大阶。
- Requirements：违反前置条件时用`assert`终止，不得悄悄破坏空闲链表。

**buddy_nr_free_pages**

- Post-Condition：返回所有空闲链表中页面总数。
````

##### 4.4.2.2 测试阶段

````markdown
[PROMPT]
**任务**：为上一份提示词生成的 Buddy System 分配器设计测试：在`kern/mm/buddy_pmm.c`中实现`static void buddy_check(void)`（即`buddy_pmm_manager.check`），并在`tools/test_lab2.c`中新增`test_buddy_system()`，通过`tools/test_lab2.h`声明，`tools/inject.py`注入到`pmm_init()`末尾，`tools/grade.sh`新增一项`[Test 3] Buddy System: PASS`的检查（10 分）。

**操作要求**：你必须在项目中修改实际文件。不要修改分配算法本身的代码；如果测试发现了算法缺陷，只报告问题，不要自行改算法。测试失败时用`assert`终止内核，成功时每个场景输出一行`buddy_check: <场景名> OK`。不得破坏已有的`[Test 1]`和`[Test 2]`。为新增代码添加英文 Doxygen 注释，`@author`写`Yiyu Wang(2414007)`。

**输出要求**：直接进行文件操作，使用下面[RELY]、[GUARANTEE]、[SPECIFICATION]部分的信息，以工程真实代码为准，不要引入无关功能。完成后请对分配算法逐个植入至少 8 类典型错误（如伙伴地址算错、不合并、分裂留错一半、请求不取整、计数不更新、不检查区间边界、合并时不比较阶数、摘链后不清标志），重新运行评分，报告没有被测试发现的错误并补充用例。

[RELY]
设计阶段产物（`kern/mm/buddy_pmm.c`中的`static`标识符，测试与它们在同一文件内）：

- `BUDDY_MAX_ORDER`（15）、`buddy_area`（含`free_list[0..15]`和`nr_free`）、`zone_start`、`zone_end`（受管理区间，物理页号，左闭右开）
- `block_insert(p, order)`、`block_remove(p)`、`block_free(p, order)`、`range_free(base, n)`
- `buddy_init()`、`buddy_alloc_pages(n)`、`buddy_free_pages(base, n)`
- 块首页带`PG_property`且`property == 2^order`；`page2ppn()`、`pages`、`nbase`

现有测试框架：

- `tools/test_lab2.c`已有`test_page_init()`和`test_best_fit()`，`tools/inject.py`在临时副本的`pmm_init()`末尾依次调用它们；此时当前管理器是 Best-Fit
- `tools/grade.sh`用`quick_check '<名称>' '<输出行>'`在串口输出中查找固定字符串，每项 10 分
- 已有`assert`、`cprintf`、`memset`、`alloc_pages`、`free_pages`、`pmm_manager`全局指针

[GUARANTEE]
必须实现：

```c
static void buddy_check(void);   // kern/mm/buddy_pmm.c
void test_buddy_system(void);    // tools/test_lab2.c
```

允许新增`static`辅助函数。

[SPECIFICATION]
**buddy_check**

- Pre-Condition：分配器已初始化并管理着一段空闲内存，当前活动的管理器不一定是 Buddy System。
- Post-Condition：所有场景通过；分配器的空闲链表、空闲页数和管理区间与调用前完全一致。
- Requirements：
  - 测试不得依赖机器上的具体地址：先从现有空闲内存中取出一个 64 页、自然对齐的块作为临时测试区，保存分配器状态，在空链表且管理区间收窄到该块的状态下运行所有场景，最后还原状态并归还该块。
  - 直接调用`buddy_alloc_pages`等`static`函数，不经过`pmm_manager`指针。
  - 每个场景的每一步之后检查空闲链表的不变式：链表中的块首页带`PG_property`、不是保留页、`property`等于块大小、页号自然对齐、位于管理区间内、其伙伴不是同阶空闲块（即已完全合并），并且各块页数之和等于空闲页数。
- Case 1（初始布局）：对齐的 64 页形成一个 6 阶块。
- Case 2（分裂）：申请 1 页得到块首页，剩余形成 0 至 5 阶各一块，且得到的是低地址一半。
- Case 3（伙伴寻址与对齐）：继续申请时依次取到相邻页，互为伙伴的两页页号只在最低位不同。
- Case 4（合并）：以“伙伴未释放”“链式合并”两种顺序释放后，空闲链表恢复为一个 6 阶块。
- Case 5（取整与对齐）：申请 1、2、3、4、5、8、9、16、17、33 页，实际消耗 1、2、4、4、8、8、16、16、32、64 页，返回的块自然对齐。
- Case 6（耗尽与非法大小）：整块申请成功后再申请返回`NULL`；申请 65 页、2^15 页、2^15+1 页、`(size_t)-1`页均返回`NULL`且状态不变。
- Case 7（外部碎片）：4 个 16 页块全部分配后，释放两个互不为伙伴的块，不合并，此时空闲页数为 32 却申请不到 32 页；继续释放后才逐级合并。
- Case 8（随机压力）：固定种子，对 32 个槽位随机申请（1 至 20 页）和释放共 3000 轮，用逐页占用表检查已分配块互不重叠、自然对齐、不越出测试区；结束时全部释放并恢复为一个 6 阶块。
- Case 9（区间边界）：把管理区间收窄为测试区的一半，另一半手工挂为真实的空闲块，释放这一半时不得与区间外的空闲伙伴合并；区间末端之后和区间起点之前两个方向都要测试。

**test_buddy_system**

- Pre-Condition：`pmm_init()`已结束，当前管理器通常是 Best-Fit。
- Post-Condition：串口输出恰好一行`[Test 3] Buddy System: PASS`表示通过；失败时不输出该行（断言失败使内核终止，或输出以`FAIL`开头的原因行）。
- Case 1：当前管理器不是 Buddy System。从当前管理器申请 256 页作为测试区，把它们标成保留页，临时把`pmm_manager`指向`buddy_pmm_manager`，初始化并注册这 256 页，运行`check`，核对空闲页数仍为 256；然后恢复`pmm_manager`，清除测试区页面的标志位，还原给原管理器。
- Case 2：当前管理器就是 Buddy System（用`make PMM=buddy`构建）。直接运行`check`并输出通过。
- Requirements：测试结束后`pmm_manager`必须恢复原值，且不影响`[Test 2]`对管理器身份的检查。
````

#### 4.4.3 实现迭代过程

本模块的实现经历了 2 次迭代。每次迭代以一次完整的测试调用（`make grade`，以及以`make PMM=buddy`构建后启动内核观察自检输出）为界。

---

##### 第一次迭代

使用设计阶段的提示词生成`buddy_pmm.c`并接入内核，再用测试阶段的提示词生成`buddy_check()`、`test_buddy_system()`和评分脚本的修改后，首次运行测试：

- `make grade`：编译通过（开启`-Wall -Werror`），`check page init`、`check best fit`、`check buddy system`均为`OK`，总分 30/30；
- `make clean && make PMM=buddy qemu`：伙伴系统作为真实管理器启动，`buddy_check`的 7 个场景依次输出`OK`，随后`check_alloc_page() succeeded!`。

测试一次通过，但"通过"本身不能说明测试足够强。因此按提示词的要求做了变异验证：向`buddy_pmm.c`中逐个植入一类典型错误，每次都重新运行`make grade`，看测试是否会失败。

| 编号 | 植入的错误                            | `[Test 3]`结果   |
| ---- | ------------------------------------- | ---------------- |
| M1   | 伙伴地址算错（`ppn ^ 1`，与阶数无关） | 失败，已发现     |
| M2   | 释放时从不合并                        | 失败，已发现     |
| M3   | 分裂时留下高地址一半、挂入低地址一半  | 失败，已发现     |
| M4   | 请求页数不向上取整而是向下取整        | 失败，已发现     |
| M5   | 分配后不减少空闲页数                  | 失败，已发现     |
| M6   | 合并时不检查伙伴是否在管理区间内      | **通过，未发现** |
| M7   | 合并时不比较伙伴的阶数                | 失败，已发现     |
| M8   | 摘下伙伴后不清除`PG_property`         | 失败，已发现     |
| M9   | 初始化切块时不考虑自然对齐            | 失败，已发现     |

**遇到的问题：**

- 问题 1：M6 存活，说明测试存在缺口。原因分析：区间边界检查只有在"伙伴恰好是区间之外的、同阶的空闲块"时才会起作用。场景 1 至 8 都在测试区内部进行，测试区最顶层 6 阶块的伙伴在`make grade`的运行环境里恰好不是满足条件的空闲块，这个分支从未被触发。这个缺陷在真实内核中是有危害的：区间两端的块如果错误地与区间外的页面（保留页，甚至没有描述符的地址）合并，会破坏内核或越界访问`pages`数组，而测试对此毫无察觉。

**问题解决策略：**

- 针对问题 1：不依赖测试区周围"碰巧"是什么，而是由测试自己构造出"区间外的真实空闲伙伴"。在测试阶段提示词的 [SPECIFICATION] 中增加 Case 9（区间边界）：把管理区间收窄为测试区的一半，另一半手工挂为空闲块，释放这一半并断言两者没有合并；区间末端之后和区间起点之前两个方向各测一次。同时在 [PROMPT] 的输出要求中写明"植入至少 8 类典型错误做变异验证，报告未被发现的错误并补充用例"。

---

##### 第二次迭代

加入区间边界场景后重新运行测试，并再次植入 M6。

**最终结果：**

经过 2 次迭代，该模块最终：

- ✅ 编译通过（`-Wall -Werror`），`make format-check-c`通过
- ✅ `make grade`三项检查均为`OK`，总分 30/30，`[Test 1]`、`[Test 2]`不受影响
- ✅ `make PMM=buddy qemu`启动后`buddy_check`的 8 个场景（区间边界场景为新增）全部输出`OK`
- ✅ 9 类植入的错误全部被测试发现，其中 M6 在加入 Case 9 后由`[Test 3]`失败暴露
- ✅ 功能符合预期：分配器分裂、合并、取整和边界行为正确，且默认仍使用 Best-Fit

**关键改进点总结：**

1. 在测试阶段提示词的 [PROMPT] 中增加了变异验证的要求：测试通过并不等于测试有效，要主动植入典型错误来检验测试。
2. 在 [SPECIFICATION] 中增加了区间边界场景，要求由测试自己构造"区间外的空闲伙伴"，而不是依赖运行环境中的内存布局。
3. 在 [SPECIFICATION] 中明确了测试的隔离方式（临时测试区、保存并还原状态、收窄管理区间），使同一份`buddy_check()`既能在`make grade`中与 Best-Fit 共存，也能在`make PMM=buddy`下作为真实管理器的自检运行。
4. 在 [RELY] 中给出了评分脚本的匹配方式和`inject.py`的注入位置，避免生成的测试破坏已有的`[Test 2]`对管理器身份的检查。

---

### 4.5 Challenge：硬件的可用物理内存范围的获取方法

**负责人：** 2414007-王奕瑜

**问题：** 如果 OS 无法提前知道当前硬件的可用物理内存范围，请问你有何办法让 OS 获取可用物理内存范围？

本实验中，内核其实也没有把 128MB 硬编码进去，而是依赖 OpenSBI 通过`a1`寄存器传来的设备树（DTB）。若连这类信息都没有，可以从以下几个层次考虑，优先级从高到低：

1. **向固件/引导程序查询（首选）。** 内存的真实配置在系统上电时就由固件探测过，OS 只需要读取固件提供的结果：
   - **设备树（DTB/FDT）：** RISC-V、ARM 等平台由引导程序传入设备树，`/memory`节点的`reg`属性给出内存的起始地址和长度，`/reserved-memory`节点和 memory reservation block 给出需要避开的区域。这正是本实验使用的方式。
   - **BIOS 的 E820 内存映射：** x86 实模式下可通过`INT 15h, EAX=E820h`反复调用，每次返回一段地址范围及其类型（可用 RAM、保留、ACPI 可回收等）；更老的机器有`INT 15h, AX=E801h/88h`等只返回大小的接口。
   - **UEFI：** 通过`GetMemoryMap()`获得内存描述符数组，其中标明各段的类型和属性。
   - **Multiboot 等引导协议：** 引导器（如 GRUB）把内存映射以数据结构的形式传给内核。
   - **ACPI（如 SRAT/NFIT）：** 在多路、NUMA 或热插拔内存的机器上，补充描述各内存区域的归属和变化。
2. **读取内存控制器或硬件配置寄存器。** 有些嵌入式 SoC 没有设备树，但内存控制器的寄存器（如 DDR 控制器的行列、片选配置）或板级配置寄存器里记录了容量；也可以通过 DIMM 的 SPD 信息读出容量，再结合芯片手册得到地址映射。这种方式依赖具体硬件，通用性差。
3. **自行探测（最后手段）。** 如果前面的信息都拿不到，OS 可以从已知可用的地址（如内核镜像之后）开始，按固定步长（如每 1MB 或每页）向上逐段尝试"写入特征值再读回比较"：
   - 写入的值能读回，说明该地址有 RAM；读不回或总线出错，说明已经越过内存末端或进入空洞；
   - 地址线回绕（别名）时，高位地址会映射到低位地址的同一块 RAM，可以通过"修改一处后检查另一处是否随之改变"来发现容量上限；
   - 探测前应先保存原有内容，探测后恢复，并避开内核、固件和栈所在的区域。

   这种方法有明显的风险：向 MMIO 区域写入会触发外设副作用；访问不存在的地址可能产生访问异常或总线错误，因此需要提前设置异常处理程序，把这类异常当作"该地址无内存"；探测只能判断"能不能读写"，无法区分保留区与普通内存，也无法发现被固件占用的区域，所以通常只作为兜底。

4. **探测之后还要排除不可用的部分。** 无论用哪种方式得到物理内存范围，都要再扣除 OpenSBI/固件、内核镜像、`Page`描述符数组、设备树本身（读完后可回收）、initrd 等已被占用的区域，这与`page_init()`的处理一致。

综上，最可靠的做法是让固件/引导程序以标准格式（设备树、E820、UEFI memory map 等）告诉内核；只有在没有任何此类信息时，才退而采用带异常保护的写读探测。

### 4.6 Challenge：任意大小的内存单元 slub 分配算法

**负责人：** 2414015-鲁昕宁

#### 4.6.1 功能描述

SLUB 是 Linux 内核用来分配小块、固定大小内存对象的分配器。它不直接找一段任意长度的空闲内存，而是先向页分配器要整页，再把这些页切成同样大小的对象。

它分成两层：

- 页分配器按页提供内存。
- 对象分配器把一页或连续几页切成等大的槽。比如 64 字节一类，一页 4096 字节大约能放 64 个对象。申请 64 字节时，只从空闲槽里取一个；释放时把槽还回去。

同一大小（或同一类型）的对象归入一个缓存。缓存里的页有三种状态：

- 空闲：所有槽都没用，整页可以还给页分配器。
- 部分使用：还有空槽，下次优先从这里拿。
- 用满：没有空槽，暂时不再参与分配。

每个对象槽用空闲链表串起来。分配就是摘下一个空闲槽，释放就是挂回去。Linux 的 SLUB 还为每个 CPU 保留一条本地空闲链表，常用路径不用加锁；本地链表空了，再去部分使用的页里取，还不够才向页分配器要新页。

这样做是为了避免小对象直接按页分配。一个几十字节的对象若独占一页，页内大部分空间都浪费了。固定大小的槽把内部碎片限制在“对象实际大小到所在尺寸类”之间，释放也只是归还槽，不必频繁合并、拆开整页。

为了实现与验证的方便，降低各模块间的耦合度，我们的 SLUB 按页分配部分支持在之前已实现的 First-Fit / Best-Fit / Buddy-System 三种方法间切换，重点关注在页内按对象大小切分、分配和释放的部分。

#### 4.6.2 最终提示词

````markdown
[PROMPT]
**任务**：新增 `kern/mm/slub.h` 与 `kern/mm/slub.c`，实现一个简化的 SLUB 对象分配器，并在 `kern/init/init.c` 的 `pmm_init()` 之后调用它的初始化。SLUB 分为两层：第一层按页取得和归还内存，第二层在页内按对象大小切分、分配和释放。第一层不重新实现，只调用已经接入内核的页分配接口；当前 `pmm_manager` 是 First-Fit、Best-Fit 还是 Buddy System，由 `init_pmm_manager()` 的既有选择决定。本任务的重点是第二层。

**操作要求**：你必须在项目中修改和新增实际文件。保留 First-Fit、Best-Fit、Buddy System 及其 `pmm_manager` 实现，不要修改这些分配器，也不要在 SLUB 中引用它们的符号或判断当前用的是哪一种。`kern/mm` 下的 `.c` 会被 Makefile 自动编译，不要为了加入 `slub.c` 去改编译规则。为新增的公开接口添加英文 Doxygen 注释，`@author` 写 `鲁昕宁(2414015)`。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息，以工程真实代码为准。只实现对象分配所需的尺寸类、页内空闲槽和整页归还，不要实现每 CPU 缓存、锁或调试统计。

[RELY]
页分配只使用以下已有接口（`kern/mm/pmm.h`）。它们转发到当前 `pmm_manager`，因此更换 `init_pmm_manager()` 中的管理器后，SLUB 的页来源自动切换：

- `struct Page *alloc_pages(size_t n)`：分配 `n` 个连续物理页；无法满足时返回 `NULL`
- `void free_pages(struct Page *base, size_t n)`：释放从 `base` 开始的 `n` 个连续物理页
- `alloc_page()`、`free_page(page)`：上述二者在 `n == 1` 时的写法

页与地址（`kern/mm/pmm.h`、`kern/mm/mmu.h`）：

- `PGSIZE` 为 4096
- `page2pa(page)` 返回该页的物理地址
- `va_pa_offset` 是内核线性映射的虚实偏移；页的内核虚拟地址是 `page2pa(page) + va_pa_offset`
- `struct Page` 的 `property`、`flags`、`page_link` 属于页分配器。页在被 `alloc_pages()` 分配出去期间，SLUB 不得把这些字段当作自己的空闲链表或尺寸记录

[GUARANTEE]
必须提供：

```c
void slub_init(void);
void *slub_alloc(size_t size);
void slub_free(void *obj);
```

`slub_alloc()` 按字节数分配一个对象，返回其内核虚拟地址。`slub_free()` 释放该地址；调用者不再次传入大小，分配器自己要能判断这块内存属于哪个尺寸类，或它本身就是一整段按页分配的内存。允许在 `slub.c` 内新增 `static` 辅助函数，不得把它们放进公开头文件，也不得改变 `pmm_manager` 的接口。

[SPECIFICATION]
**slub_init**

- Pre-Condition：`pmm_init()` 已完成，`alloc_pages()` / `free_pages()` 可用。
- Post-Condition：各尺寸类的缓存为空，尚未向页分配器领取页面。重复初始化不得泄漏已经领出的页。

**slub_alloc**

- Pre-Condition：已经调用过 `slub_init()`。`size` 是本次要分配的字节数。
- Post-Condition：成功时返回一块至少 `size` 字节、按该对象尺寸对齐的内核虚拟地址，且不与任何尚未释放的对象重叠。失败时返回 `NULL`，已有对象和页分配器的空闲页数量都保持不变。
- Case 1：`size` 为 0。返回 `NULL`。
- Case 2：`size` 不超过一个页内对象的最大尺寸类。将其向上归入最小的、能装下它的尺寸类。尺寸类是 2 的幂，最小 16 字节，最大不超过 `PGSIZE` 去掉每页管理信息后还能至少放下一个对象的大小。优先从该尺寸类已有的、尚有空槽的页中取一个槽；没有这样的页时，用 `alloc_pages()` 领取新页并切成该尺寸的槽，再取出一个。页内管理信息和空闲槽链表都放在 SLUB 自己的数据或该页内容中。
- Case 3：`size` 大于页内对象的最大尺寸类。直接领取能够覆盖 `size` 的最少连续页数，把起始内核虚拟地址作为对象返回，不再切分。页分配器无法满足时返回 `NULL`。
- Requirements：同一次运行里，对象级分配结果不得依赖当前页分配器是 First-Fit、Best-Fit 还是 Buddy System。三种管理器只影响页从哪里来，不影响尺寸类、切槽和释放规则。

**slub_free**

- Pre-Condition：`obj` 为 `NULL`，或者是仍未释放的、由 `slub_alloc()` 返回的地址。
- Post-Condition：对应对象可以被再次分配。一个页内对象全部释放后，该页通过 `free_pages()` 归还页分配器；按整页分配的对象则按当初领取的页数归还。不得归还仍含有未释放对象的页，也不得把同一页归还两次。
- Case 1：`obj` 为 `NULL`。不执行任何操作。
- Case 2：`obj` 是页内对象。槽回到该页所属尺寸类的空闲槽中。该页上还有其他未释放对象时，页继续留在 SLUB。该页上已经没有未释放对象时，整页归还。
- Case 3：`obj` 是大于页内尺寸类、当初按连续页分配的对象。按分配时的页数归还。
````

#### 4.6.3 实现迭代过程

##### 4.6.3.1 测试用例

- Case 1：`slub_alloc(0)` 返回 `NULL`，空闲页数量不变。`slub_free(NULL)` 后空闲页数量仍不变。
- Case 2：对 16、32、64、128、256、512、1024、2048 字节各分配一个对象。每个地址非空，并按该尺寸对齐；在各自请求的字节范围内写入互不相同的内容后，读回仍然正确，且这些区域互不重叠。
- Case 3：再分配一个 16 字节对象。它与 Case 2 中的 16 字节对象位于同一页。此时释放其中一个，空闲页数量不变；两个都释放后，这一页被归还，空闲页数量比两者都存在时多 1。Case 2 中其余尺寸的对象内容保持不变。
- Case 4：16 字节与 32 字节各保留一个对象时，空闲页数量比两者都不存在时少 2。只释放 16 字节对象后少 1；再释放 32 字节对象后回到两者都不存在时的数量。
- Case 5：先分配两个 16 字节对象，再释放其中一个。随后再分配一个 16 字节对象。新对象与仍存活的对象不重叠，且这次分配不减少空闲页数量。
- Case 6：分配 `PGSIZE + 1` 字节。空闲页数量减少 2，这块区域可按请求的长度写入并读回。释放后空闲页数量增加 2。
- Case 7：连续分配 `PGSIZE + 1` 字节，直到返回 `NULL`。此前成功的对象内容仍然可读，且这次失败不改变空闲页数量。释放全部成功的对象后，空闲页数量回到本情况开始时的值。

##### 4.6.3.2 第一次迭代：在 Best-Fit 基础上实现 SLUB

**测试结果：**

- ✅ 编译通过
- ✅ `make qemu`输出了`physical memory map: memory: 0x0000000008000000, [0x0000000080000000, 0x0000000087ffffff].`，随后`check_alloc_page() succeeded!`，说明空闲区已被`pmm_manager`正确接收，Best-Fit 的自检可以在其上正常分配和释放
- ✅ `check alloc page`通过
- ✅ `slub check`通过
  上述测试说明 SLUB 已经可以向使用 Best-Fit 的页管理器申请页，并成功按各测试样例的设定对内存进行分配与释放。

##### 第二次迭代：在 Buddy-System 基础上实现 SLUB

> 等待Buddy-System的实现

## 五、测试与验证

### 5.1 物理内存探测、页描述符初始化与 Best-Fit 分配器综合测试

**负责人：** 2414007-王奕瑜

#### 5.1.1 测试目的

`make grade`使用的测试文件`tools/test_lab2.c`同时覆盖了本实验的两项核心功能，因此本节是对它们的综合验证：

- **Test 1（Page Init）：** 验证`page_init()`是否正确探测物理内存范围、建立`Page`描述符数组并划分出空闲区；
- **Test 2（Best-Fit）：** 验证在该空闲区之上，Best-Fit 分配器的分配、释放与合并是否符合"选择最小的足够大空闲块"的策略。

同时通过`make qemu`观察完整的启动输出，人工核对内存探测结果与内核自检结果。

#### 5.1.2 测试环境

- 运行环境：QEMU（`qemu-system-riscv64`）+ OpenSBI，默认 128MB 内存；
- 分配器：`best_fit_pmm_manager`（`init_pmm_manager()`中选用）；
- 代码状态：`page_init()`与 Best-Fit 均已实现，在完整仓库中直接构建。

#### 5.1.3 测试步骤与结果

**步骤 1：编译并运行内核，观察启动输出**

```bash
make qemu
```

与本功能相关的输出如下（QEMU 通过`Ctrl+A` 再按`X`退出）：

```text
DTB init completed
(THU.CST) os is loading ...
Special kernel symbols:
  entry  0xffffffffc02000d6 (virtual)
  etext  0xffffffffc0201654 (virtual)
  edata  0xffffffffc0205018 (virtual)
  end    0xffffffffc0205078 (virtual)
Kernel executable memory footprint: 20KB
memory management: best_fit_pmm_manager
physical memory map:
  memory: 0x0000000008000000, [0x0000000080000000, 0x0000000087ffffff].
check_alloc_page() succeeded!
satp virtual address: 0xffffffffc0204000
satp physical address: 0x0000000080204000
```

![make qemu 输出](./images/5.1_make_qemu.png)

**步骤 2：运行评分测试**

```bash
make grade
```

该命令把测试文件注入内核后重新编译并运行，输出如下：

```text
  -check page init:                          OK
  -check best fit:                           OK
Total Score: 20/20
```

![make grade 输出](./images/5.1_make_grade.png)

#### 5.1.4 结果分析

1. **内存范围探测正确。** `Physical Memory from DTB`与`physical memory map`两处输出一致：DRAM 起始地址为`0x80000000`，大小为`0x08000000`（128MB），末端为`0x87ffffff`，与 QEMU 默认配置相符。这说明`page_init()`通过`get_memory_base()`/`get_memory_size()`读到了`dtb_init()`保存的结果，并正确计算了范围末端。
2. **内存布局与手工计算一致。** 以本次运行为例：
   - `end = 0xffffffffc0205078`，向上按页对齐后`pages`位于虚拟地址`0xffffffffc0206000`，对应物理地址`0x80206000`；
   - `npage - nbase = 0x88000 - 0x80000 = 32768`个页面，每个`struct Page`占 40 字节，描述符数组共`0x140000`字节（1280KB），末尾为物理地址`0x80346000`，已按页对齐；
   - 因此空闲区为`[0x80346000, 0x88000000)`，共`0x7cba = 31930`页，约 124.7MB。
   - 内核镜像和描述符数组所在页面保持保留，没有进入空闲链表。
3. **Test 1 通过，说明初始化满足设计约束。** `[Test 1] Page Init: PASS`表示：`va_pa_offset`等于`PHYSICAL_MEMORY_OFFSET`、`npage`等于设备树内存末端的物理页号、`pages`页对齐并位于`end`之后、空闲页数大于 0 且不超过总页数、内核尾页和描述符数组所在页保持保留，以及`page2pa()`与`pa2page()`互逆。
4. **Test 2 通过，说明 Best-Fit 在该空闲区上工作正常。** `memory management: best_fit_pmm_manager`表明当前使用的是 Best-Fit；`check_alloc_page() succeeded!`说明其内置自检（`basic_check()`与`best_fit_check()`）通过；`check best fit: OK`进一步验证了 Best-Fit 的选择策略。Test 2 依赖`page_init()`建立的描述符与空闲链表，它的通过也从侧面印证了初始化的正确性。
5. **综合结论。** 两项评分均为 OK，总分 20/20，物理内存探测、页描述符初始化与 Best-Fit 分配器三者能够协同工作，本部分功能符合预期。

> 注：地址和空闲页数等具体数值会随内核镜像大小变化，代码修改后重新运行会略有不同，属于正常现象。

---

### 5.2 伙伴系统测试

**负责人：** 2414007-王奕瑜

#### 5.2.1 测试目的

验证伙伴系统分配器在真实内核中的正确性，包括两方面：

- 把`buddy_pmm_manager`作为内核的物理内存管理器启动，观察它自带的自检`buddy_check()`各场景是否通过；
- 通过`make grade`中新增的`[Test 3]`，验证在默认 Best-Fit 的内核中也能对伙伴系统做自动化评分，且不影响`[Test 1]`、`[Test 2]`。

#### 5.2.2 测试步骤与结果

**步骤 1：以伙伴系统为管理器启动内核**

```bash
make clean
make PMM=buddy qemu
```

`PMM=buddy`让`Makefile`定义宏`PMM_BUDDY`，`init_pmm_manager()`据此选用`buddy_pmm_manager`。由于目标文件不跟踪编译选项，切换前需要先`make clean`。输出如下（QEMU 通过`Ctrl+A` 再按`X`退出）：

```text
memory management: buddy_pmm_manager
physical memory map:
  memory: 0x0000000008000000, [0x0000000080000000, 0x0000000087ffffff].
buddy_check: initial layout OK
buddy_check: split OK
buddy_check: merge OK
buddy_check: rounding and alignment OK
buddy_check: exhaustion OK
buddy_check: fragmentation OK
buddy_check: random stress OK
buddy_check: zone boundary OK
check_alloc_page() succeeded!
satp virtual address: 0xffffffffc0206000
satp physical address: 0x0000000080206000
```

**截图指引：** 截取从`memory management: buddy_pmm_manager`到`satp physical address`的这一段终端输出，保存为`docs/images/5.2_make_qemu_buddy.png`。

![make PMM=buddy qemu 输出](./images/5.2_make_qemu_buddy.png)

**步骤 2：运行评分测试**

```bash
make clean
make grade
```

输出如下：

```text
  -check page init:                          OK
  -check best fit:                           OK
  -check buddy system:                       OK
Total Score: 30/30
```

**截图指引：** 截取上述三行`OK`和`Total Score`所在的终端输出，保存为`docs/images/5.2_make_grade.png`（5.1 节记录的是任务一、二完成时只有两项检查、20/20 的结果，此处是加入伙伴系统后的结果）。

![make grade 输出](./images/5.2_make_grade.png)

测试完成后，如要回到默认的 Best-Fit 内核，再执行一次`make clean`并用不带`PMM`的命令构建即可。

#### 5.2.3 结果分析

1. **伙伴系统可以作为真实的物理内存管理器运行。** 启动信息`memory management: buddy_pmm_manager`说明`PMM_BUDDY`接入生效；`page_init()`建立的空闲区被`buddy_init_memmap`正确接收并切成自然对齐的块，之后`check_alloc_page() succeeded!`说明自检全部通过，内核继续正常启动并打印页表地址。
2. **各场景逐项通过。** 8 行`buddy_check`输出对应 8 个场景：初始布局、分裂、合并、取整与对齐、耗尽、外部碎片、随机压力、区间边界。任何一个断言失败，内核会立即 panic，后面的输出和`check_alloc_page() succeeded!`都不会出现，因此这些行本身就是通过的证据。
3. **自检不改变真实内存。** `buddy_check()`在一个临时测试区上运行，结束时还原了状态，所以之后的启动流程不受影响，这一点也由自检最后断言"空闲页数与调用前一致"保证。
4. **`make grade`为 30/30。** 新增的`[Test 3]`在默认 Best-Fit 内核上通过：它从 Best-Fit 取出 256 页交给伙伴系统，跑完自检再归还；`[Test 1]`、`[Test 2]`依然为`OK`，说明伙伴系统的接入没有破坏任务一、二的结果，`pmm_manager`在测试结束后被正确恢复为 Best-Fit。
5. **测试有效性。** 对分配器植入 9 类典型错误后，`[Test 3]`都会变为`WRONG`（见 4.4.3），说明这里的`OK`并不是"只要不崩溃就通过"。

> 注：`satp`地址等数值随内核镜像大小而变化，伙伴系统版本的内核比 Best-Fit 版本更大（包含自检代码），所以与 5.1 节的数值略有不同，属于正常现象。

---

## 六、实验总结与收获

### 6.1 对操作系统的理解

### 6.2 AI 协作开发的经验

---
