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

#### 2.1.1 实现物理内存探测与页描述符初始化功能

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

#### 2.1.2 伙伴系统分配算法

##### 2.1.2.1 设计阶段提示词

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

##### 2.1.2.2 测试阶段提示词

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

### 2.2 2414015-鲁昕宁

#### 2.2.1 实现任意大小的内存单元 SLUB 分配算法

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

#### 2.2.2 设计并实现 SLUB 分配算法的测试方案

````markdown
[PROMPT]
**任务**：为已实现的 SLUB 增加自测函数 `slub_check()`，在 `kern/init/init.c` 里于 `slub_init()` 之后调用。测试只通过 `slub_alloc()`、`slub_free()` 和空闲页计数观察行为，用来确认不同大小对象的分配、写入、释放和页归还。

**操作要求**：你必须在项目中修改实际文件。测试代码放在 `kern/mm/slub.c`，并在 `kern/mm/slub.h` 声明 `slub_check()`。不要修改 First-Fit、Best-Fit、Buddy System，也不要在测试里判断当前是哪一种页分配器。失败时用 `assert` 停住；全部通过时用 `cprintf` 打印 `slub_check() succeeded`。为 `slub_check()` 添加英文 Doxygen 注释，`@author` 写 `鲁昕宁(2414015)`。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息。测试结束前释放本次分配的全部分配对象，使空闲页数量回到进入测试时的值。

[RELY]
SLUB 接口（`kern/mm/slub.h`，行为以 2.2.1 的规格为准）：

- `void slub_init(void)`
- `void *slub_alloc(size_t size)`：按字节数分配，返回内核虚拟地址；失败或 `size` 为 0 时返回 `NULL`
- `void slub_free(void *obj)`：释放对象；`obj` 为 `NULL` 时不操作。调用者不再传入大小

页计数与断言：

- `size_t nr_free_pages(void)`（`kern/mm/pmm.h`）：当前页分配器的空闲页数。SLUB 领页时它减少，空页归还后它增加
- `PGSIZE` 为 4096
- `assert(x)`（`kern/debug/assert.h`）：条件为假时内核 panic
- `int cprintf(const char *fmt, ...)`
- 页内尺寸类是 2 的幂，最小 16 字节；大于页内最大尺寸类的请求按连续整页分配

[GUARANTEE]
必须提供并在 `slub_init()` 之后调用：

```c
void slub_check(void);
```

允许在 `slub.c` 内新增 `static` 辅助函数。不得改变 `slub_alloc()`、`slub_free()` 的公开语义，也不得改变 `pmm_manager`。

[SPECIFICATION]
**slub_check**

- Pre-Condition：`slub_init()` 已经执行，页分配器可用。
- Post-Condition：下列情况全部满足，否则 `assert` 失败。函数返回时空闲页数量与进入函数时相同，并已打印成功信息。
- Case 1：`slub_alloc(0)` 返回 `NULL`，空闲页数量不变。`slub_free(NULL)` 后空闲页数量仍不变。
- Case 2：对 16、32、64、128、256、512、1024、2048 字节各分配一个对象。每个地址非空，并按该尺寸对齐；在各自请求的字节范围内写入互不相同的内容后，读回仍然正确，且这些区域互不重叠。
- Case 3：再分配一个 16 字节对象。它与 Case 2 中的 16 字节对象位于同一页。此时释放其中一个，空闲页数量不变；两个都释放后，这一页被归还，空闲页数量比两者都存在时多 1。Case 2 中其余尺寸的对象内容保持不变。
- Case 4：16 字节与 32 字节各保留一个对象时，空闲页数量比两者都不存在时少 2。只释放 16 字节对象后少 1；再释放 32 字节对象后回到两者都不存在时的数量。
- Case 5：先分配两个 16 字节对象，再释放其中一个。随后再分配一个 16 字节对象。新对象与仍存活的对象不重叠，且这次分配不减少空闲页数量。
- Case 6：分配 `PGSIZE + 1` 字节。空闲页数量减少 2，这块区域可按请求的长度写入并读回。释放后空闲页数量增加 2。
- Case 7：连续分配 `PGSIZE + 1` 字节，直到返回 `NULL`。此前成功的对象内容仍然可读，且这次失败不改变空闲页数量。释放全部成功的对象后，空闲页数量回到本情况开始时的值。
- Requirements：断言只依据对象地址、对象内容和 `nr_free_pages()` 的变化，不依据某一种页分配算法选出的具体页号。
````

### 2.3 2411622-张雨函

#### 2.3.1 实现 Best-Fit 连续物理内存分配算法

````markdown
[PROMPT]
完成 `kern/mm/best_fit_pmm.c` 中的 **Best-Fit 连续物理内存分配器**，并在 `kern/mm/pmm.c` 中将当前物理内存管理器切换为 `best_fit_pmm_manager`。

Best-Fit 需要在现有 First-Fit 管理框架基础上，实现：

> 从所有能够满足请求的空闲块中，选择大小最小的空闲块进行分配。

同时需要正确维护：

- 空闲链表；
- 空闲页数量；
- 页面分割；
- 相邻空闲块合并。

---

[RELY]

可以依赖现有工程中的：

- `kern/mm/default_pmm.c` 中完整的 First-Fit 实现；
- `pmm.h` 中的 `struct pmm_manager`；
- `struct Page`、`free_area_t`、`free_list`、`nr_free`；
- `property` 和 `PG_property` 相关页面状态；
- `list_init()`、`list_add()`、`list_del()`、`list_next()`、`list_prev()` 等链表操作；
- 已经提供的 `basic_check()` 和 `best_fit_check()` 测试函数。

不得修改：

- `basic_check()`
- `best_fit_check()`

---

[GUARANTEE]

必须实现并注册：

- Best-Fit 管理器初始化；
- 空闲物理页区域初始化；
- Best-Fit 连续物理页分配；
- 连续物理页释放；
- 空闲页数量查询；
- `best_fit_pmm_manager`。

同时修改 `init_pmm_manager()`，使：

```c
pmm_manager = &best_fit_pmm_manager;
```

最终实现必须能够通过现有的：

- `basic_check()`
- `best_fit_check()`

---

[SPECIFICATION]

Best-Fit 分配器需要正确处理以下场景：

- **空闲区初始化**  
  使用链表维护连续空闲块，每个空闲块由首页记录 `property`，并正确维护 `nr_free`。

- **Best-Fit 分配**  
  申请连续 `n` 页时，遍历所有满足 `property >= n` 的空闲块，选择其中大小最小的块，而不是选择第一个满足条件的块。

- **空闲块分割**  
  当选中的空闲块大于请求大小时，从中分配 `n` 页，并将剩余部分重新作为空闲块加入管理。

- **页面释放**  
  释放连续页面后，将其重新加入空闲链表，并正确恢复 `property` 和页面状态。

- **相邻块合并**  
  释放后的空闲块如果与前一个或后一个空闲块在物理地址上连续，应进行合并，避免形成不必要的外部碎片。

- **空闲页计数**  
  初始化、分配和释放过程中均需正确维护 `nr_free`。

- **分配失败**  
  当不存在能够满足请求的连续空闲块时返回 `NULL`。

---

实现成功时应满足

- `free_list` 能正确表示当前所有连续空闲块；
- `nr_free` 与实际空闲物理页数量一致；
- 分配后的剩余空闲区域能够继续被正常管理；
- 释放页面能够与前后相邻空闲块正确合并；
- 当同时存在多个可满足请求的空闲块时，始终选择最小的那个；
- `best_fit_pmm_manager` 正确注册所有管理函数；
- 系统实际使用 Best-Fit，而不是原来的 First-Fit。

特别需要满足 `best_fit_check()` 中的 Best-Fit 行为：

> 当同时存在一个 2 页空闲块和一个 1 页空闲块，并申请 1 页时，应选择 1 页空闲块，而不是链表中更早出现的 2 页空闲块。
````
