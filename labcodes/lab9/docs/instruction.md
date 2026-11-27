# 页面置换与内存映射

物理内存终有尽时，而应用对内存的需求却无止境。本章我们将借助磁盘，通过页面置换和内存映射技术，为进程提供超越物理内存限制的虚拟地址空间。

## 实验目的

通过完成本次实验，希望能够达到以下目标:

- 进一步了解现代操作系统中虚拟内存管理的基本概念与实现原理
- 理解页面置换算法的设计思想及其在实际系统中的应用
- 掌握缺页中断（Page Fault）的处理流程与实现机制
- 学习文件系统与内存管理协同工作的机制，深入理解mmap系统调用的实现

## 实验内容

在前一章中，我们实现了一个简单的文件系统，并构建了一个终端环境，使得用户能够在根目录下执行可执行程序。这标志着我们的操作系统已经具备了基本的文件管理和程序执行能力。然而，要让操作系统真正高效地管理内存资源并支持更复杂的应用场景，我们需要进一步探索文件系统与虚拟地址空间之间的协同机制。

本章我们将深入实现虚拟内存管理系统的核心组件。随着用户程序的增多和内存需求的增长，有限的物理内存资源将成为系统性能的瓶颈。为此，我们需要设计一种机制，能够让进程使用超过物理内存大小的地址空间，这就是虚拟内存管理的关键目标。我们将从两个维度来解决这个问题：一是通过页面置换算法在物理内存不足时智能地选择换出页面；二是通过内存映射机制让文件内容能够直接映射到进程的地址空间。

首先，我们将关注页面置换算法的实现。当物理内存无法容纳所有活跃页面时，操作系统需要选择一些页面暂时换出到磁盘上，这个过程称为页面置换。我们将从最简单的FIFO（先进先出）算法入手，通过分析和思考理解其工作原理和潜在缺陷。在此基础上，我们将实现更为复杂的Clock算法，这种算法通过近似模拟最近最少使用原则，能够显著提高页面命中的效率。实现这些算法的过程中，我们将深入理解缺页中断的处理流程，并学会如何与已有的文件系统协同工作，将磁盘作为内存的扩展存储。

接下来，我们将探索内存映射文件机制。通过实现mmap系统调用，我们可以让文件内容直接映射到进程的虚拟地址空间。这一机制不仅简化了文件访问的编程模型，更重要的是，它使得文件系统能够与内存管理无缝集成。当进程访问映射区域时，操作系统将按需将文件内容加载到物理页面中；当物理内存紧张时，这些页面又可以被置换到磁盘上。这种统一的管理方式极大地提升了系统的灵活性和效率。

通过本章的实验，我们将构建一个完整的虚拟内存管理框架。这个框架不仅能够处理物理内存不足的问题，还能够将文件系统与内存管理有机结合，为进程提供更大的地址空间和更高效的文件访问方式。这一机制是现代操作系统的核心特征，也是理解操作系统如何管理有限硬件资源支持无限应用需求的关键所在。

### 练习

本实验围绕页面置换与虚拟内存管理两条主线展开。 第一条主线是页面置换算法。学生在理解已有 FIFO 页面置换算法的基础上，实现 Clock 页面置换算法，使内核在物理内存不足时能够根据页面访问情况选择合适的页面进行换出。 第二条主线是虚拟内存映射。学生需要理解 VMA（Virtual Memory Area）的组织方式，并实现基于 VMA 的虚拟地址空间管理，包括虚拟地址区域分配、内存映射、取消映射以及缺页异常处理。

本实验依赖实验2/3/4/5/6/7/8。请使用你此前设计的提示词生成相应实验代码，并将生成结果填写到本实验代码中标有“lab2”/“lab3”/“lab4”/“lab5”/“lab6”/“lab7”/“lab8”注释的对应位置，确保代码能够正确编译。

#### 功能模块一：Clock 页面置换算法（设计提示词借助 AI 完成）

要求实现的函数

```c
static int _clock_init_mm(struct mm_struct *mm);(kern\mm\swap_clock.c)
static int _clock_map_swappable(struct mm_struct *mm,uintptr_t addr, struct Page *page,int swap_in);(kern\mm\swap_clock.c)
static int _clock_swap_out_victim(struct mm_struct *mm,struct Page **ptr_page, int in_tick);(kern\mm\swap_clock.c)
```

整个功能模块的具体功能：这个模块负责在已有页面置换框架基础上实现 **Clock 页面置换算法**。内核已经提供统一的 `swap_manager` 接口以及 FIFO 页面置换算法作为参考。Clock 算法需要维护一组当前可以被换出的页面，并利用页面的访问状态判断页面是否应该获得“第二次机会”。模块主要包含三个方面：（1）初始化 Clock 算法所需的数据结构；（2）将新进入可换出集合的页面登记到 Clock 管理结构中；（3）当系统需要换出页面时，根据访问标志进行扫描并选择 victim page。模块完成后，当物理内存不足触发 `swap_out()` 时，Clock 算法应该能够正确选择一个满足换出条件的页面。

整个功能模块需要满足的要求：Clock 管理结构必须在初始化时处于一致状态。新加入的可换出页面必须能够被 Clock 算法正确管理。页面访问标志应当用于实现“第二次机会”机制。当页面访问标志表明页面近期被访问过时，应清除该状态并继续寻找 victim。当找到符合换出条件的页面后，应将其从当前可换出集合中正确移除。不能返回一个已经被移除、已经失效或者不属于当前 `mm` 的页面。如果当前没有可换出的页面，应按照现有 `swap_manager` 接口约定返回失败。实现不能破坏原有 FIFO、swap 以及页面管理框架。

整个功能模块应该满足的对外的接口：本模块不需要新增公共 API，而是实现已有 `swap_manager` 所规定的接口。核心接口为：`init_mm()`，`map_swappable()`，`swap_out_victim()`。其中`init_mm()`：初始化当前地址空间对应的 Clock 管理状态；`map_swappable()`：将页面加入 Clock 管理范围；`swap_out_victim()`：选择一个应该被换出的页面。`swap_manager` 已经规定了这些函数的统一调用方式，因此学生不应修改接口定义。

整个功能模块可以借助的接口函数、结构体、数据结构为:`struct mm_struct`,`struct Page`,`struct swap_manager`,`list_entry_t`,`mm->sm_priv`,`page->pra_page_link`,`page->visited`,`list_init()`,`list_add()`,`list_del()`,`list_next()`,`le2page()`.`swap_out()`

#### 功能模块二：虚拟内存映射功能 （设计提示词借助 AI 完成）

要求实现的函数

```c
uintptr_t do_mmap(struct mm_struct *mm,
                  uintptr_t addr,
                  size_t len,
                  uint32_t vm_flags,
                  struct file *file,
                  off_t offset);(kern\mm\vmm.c)
```

整个功能模块的具体功能：`do_mmap` 是内核实现内存映射的核心函数。用户进程通过 `mmap` 请求一段虚拟地址空间后，系统调用层会将请求转换为内核所使用的 VMA 属性，并进一步调用 `do_mmap`。因此，本实验中不要求同学修改系统调用入口，而是重点完成 `do_mmap` 对虚拟地址空间和 VMA 的管理。

整个功能模块需要满足的要求：`len` 表示需要映射的虚拟地址空间大小，处理时需要考虑页边界对齐。映射区域必须位于合法的用户虚拟地址空间范围内。新创建的 VMA 不能与已有 VMA 发生重叠。当 `addr == 0` 时，需要在当前进程已有 VMA 之间寻找能够容纳映射区域的空闲空间。寻找空闲空间时，需要遍历所有可能的空闲区域，而不能找到第一个满足条件的区域后立即结束。空闲区域选择应遵循 **Best-Fit（最佳适配）策略**，即选择能够满足请求且剩余空间最小的区域。找到合适的虚拟地址后，需要创建并初始化 `vma_struct`。VMA 至少需要正确维护：`vm_start`,`vm_end`,`vm_flags`,`vm_pgoff`,`vm_file`。对于匿名映射，`vm_file` 应保持为空。对于文件映射，需要正确保存文件指针以及文件中的页偏移信息。文件映射涉及文件引用计数，不能在建立映射后丢失对文件对象的引用。创建完成的 VMA 必须加入当前进程的 VMA 链表，并正确维护 VMA 数量。如果执行过程中发生错误，需要避免留下不完整的 VMA 或错误的链表状态。`do_mmap` 不应该直接修改与本实验无关的系统调用接口和底层页表机制。不需要在 `do_mmap` 中一次性为整个映射区域分配物理页面。实验框架中的 `mmap` 同时支持匿名映射和文件映射；`MAP_SHARED`、`MAP_PRIVATE`、`MAP_ANONYMOUS` 等标志已经由实验框架定义。

整个功能模块可以借助的接口函数、结构体、数据结构为:`struct mm_struct`,`struct vma_struct`,`mm->mmap_list`,`mm->map_count`,`vma_create()`,`insert_vma_struct()`,`find_vma()`,`list_entry_t`,`list_entry()`,`list_next()`,`list_prev()`,`PGSIZE`,`USERTOP`,`USER_ACCESS`,`VM_READ`,`VM_WRITE`,`VM_EXEC`

#### 功能模块三：虚拟内存取消映射功能（设计提示词借助 AI 完成）

要求实现的函数：

```c
int do_munmap(struct mm_struct *mm,
          uintptr_t addr,
          size_t len);(kern\mm\vmm.c)
```

整个功能模块的具体功能：`do_munmap` 用于解除进程虚拟地址空间中的一段内存映射。它与 `do_mmap` 构成完整的映射生命周期，`do_munmap` 的核心任务不是简单地“删除一个 VMA”，而是保证**虚拟地址空间、页表以及 VMA 管理结构之间的状态保持一致**。

整个功能模块的注意事项：`addr` 和 `len` 需要按照页边界进行处理。取消映射的区域必须位于合法的用户地址空间。需要检查指定区域是否确实存在对应的 VMA。不能因为部分区域存在 VMA 就错误地删除整个不匹配的 VMA。解除映射前需要正确处理对应的页表映射。如果该 VMA 是文件映射，需要正确处理文件引用计数。从 VMA 链表删除 VMA 后，需要释放 VMA 对象。删除 VMA 后，需要正确更新 `mm->map_count`。操作过程中不能破坏其他 VMA。如果参数非法或找不到合法映射区域，应按照实验框架的错误处理方式返回失败。`do_munmap` 不应该修改用户态 `munmap` 接口。不需要重新设计页表底层释放机制，应优先使用实验框架已经提供的接口。

整个功能模块可以借助的接口函数、结构体、数据结构：`struct mm_struct`,`struct vma_struct`,`mm->mmap_list`,`mm->map_count`,`find_vma()`,`unmap_range()`,`vma_struct 的：``vm_start`,`vm_end`,`vm_flags`,`vm_file`,`vm_pgoff`,`list_entry_t`,`list_entry()`,`list_del()`

#### 扩展练习 Challenge1：实现不考虑实现开销和效率的LRU页替换算法（需要编程）

challenge部分不是必做部分，不过在正确最后会酌情加分。需写出有详细的设计、分析和测试的实验报告。完成出色的可获得适当加分。

#### 扩展练习 Challenge2：实现共享内存映射（mmap共享机制，需要编程）

在实际操作系统中，mmap 系统调用支持多种映射类型，其中共享映射（MAP_SHARED）和私有映射（MAP_PRIVATE）是最重要的两种。在之前的练习中，我们只实现了基本的匿名映射和文件映射，但缺少对共享机制的支持。

**共享映射（MAP_SHARED）** 允许多个进程映射同一文件区域，对映射区域的修改会反映到文件中，并且对其他映射同一文件的进程可见。 **私有映射（MAP_PRIVATE）** 则创建写时复制（Copy-On-Write）的映射，对映射区域的修改不会影响原始文件，也不会被其他进程看到。

在本练习中，你需要扩展原有的 do_mmap 和 do_munmap 函数，实现共享内存映射机制。具体任务包括：

- 修改VMA结构体：添加共享映射标志和支持写时复制的标志
- 扩展do_mmap函数：根据vm_flags中的MAP_SHARED/MAP_PRIVATE标志进行不同的处理
- 实现页面故障处理：修改缺页异常处理程序，支持写时复制（COW）机制
- 添加引用计数：为共享页面实现引用计数管理
- 修改do_munmap函数：正确处理共享页面的释放

### 练习

本实验围绕页面置换与虚拟内存管理两条主线展开。 第一条主线是页面置换算法。学生在理解已有 FIFO 页面置换算法的基础上，实现 Clock 页面置换算法，使内核在物理内存不足时能够根据页面访问情况选择合适的页面进行换出。 第二条主线是虚拟内存映射。学生需要理解 VMA（Virtual Memory Area）的组织方式，并实现基于 VMA 的虚拟地址空间管理，包括虚拟地址区域分配、内存映射、取消映射以及缺页异常处理。

本实验依赖实验2/3/4/5/6/7/8。请使用你此前设计的提示词生成相应实验代码，并将生成结果填写到本实验代码中标有“lab2”/“lab3”/“lab4”/“lab5”/“lab6”/“lab7”/“lab8”注释的对应位置，确保代码能够正确编译。

#### 功能模块一：Clock 页面置换算法（设计提示词借助 AI 完成）

要求实现的函数

```c
static int _clock_init_mm(struct mm_struct *mm);(kern\mm\swap_clock.c)
static int _clock_map_swappable(struct mm_struct *mm,uintptr_t addr, struct Page *page,int swap_in);(kern\mm\swap_clock.c)
static int _clock_swap_out_victim(struct mm_struct *mm,struct Page **ptr_page, int in_tick);(kern\mm\swap_clock.c)
```

整个功能模块的具体功能：这个模块负责在已有页面置换框架基础上实现 **Clock 页面置换算法**。内核已经提供统一的 `swap_manager` 接口以及 FIFO 页面置换算法作为参考。Clock 算法需要维护一组当前可以被换出的页面，并利用页面的访问状态判断页面是否应该获得“第二次机会”。模块主要包含三个方面：（1）初始化 Clock 算法所需的数据结构；（2）将新进入可换出集合的页面登记到 Clock 管理结构中；（3）当系统需要换出页面时，根据访问标志进行扫描并选择 victim page。模块完成后，当物理内存不足触发 `swap_out()` 时，Clock 算法应该能够正确选择一个满足换出条件的页面。

整个功能模块需要满足的要求：Clock 管理结构必须在初始化时处于一致状态。新加入的可换出页面必须能够被 Clock 算法正确管理。页面访问标志应当用于实现“第二次机会”机制。当页面访问标志表明页面近期被访问过时，应清除该状态并继续寻找 victim。当找到符合换出条件的页面后，应将其从当前可换出集合中正确移除。不能返回一个已经被移除、已经失效或者不属于当前 `mm` 的页面。如果当前没有可换出的页面，应按照现有 `swap_manager` 接口约定返回失败。实现不能破坏原有 FIFO、swap 以及页面管理框架。

整个功能模块应该满足的对外的接口：本模块不需要新增公共 API，而是实现已有 `swap_manager` 所规定的接口。核心接口为：`init_mm()`，`map_swappable()`，`swap_out_victim()`。其中`init_mm()`：初始化当前地址空间对应的 Clock 管理状态；`map_swappable()`：将页面加入 Clock 管理范围；`swap_out_victim()`：选择一个应该被换出的页面。`swap_manager` 已经规定了这些函数的统一调用方式，因此学生不应修改接口定义。

整个功能模块可以借助的接口函数、结构体、数据结构为:`struct mm_struct`,`struct Page`,`struct swap_manager`,`list_entry_t`,`mm->sm_priv`,`page->pra_page_link`,`page->visited`,`list_init()`,`list_add()`,`list_del()`,`list_next()`,`le2page()`.`swap_out()`

#### 功能模块二：虚拟内存映射功能 （设计提示词借助 AI 完成）

要求实现的函数

```c
uintptr_t do_mmap(struct mm_struct *mm,
                  uintptr_t addr,
                  size_t len,
                  uint32_t vm_flags,
                  struct file *file,
                  off_t offset);(kern\mm\vmm.c)
```

整个功能模块的具体功能：`do_mmap` 是内核实现内存映射的核心函数。用户进程通过 `mmap` 请求一段虚拟地址空间后，系统调用层会将请求转换为内核所使用的 VMA 属性，并进一步调用 `do_mmap`。因此，本实验中不要求同学修改系统调用入口，而是重点完成 `do_mmap` 对虚拟地址空间和 VMA 的管理。

整个功能模块需要满足的要求：`len` 表示需要映射的虚拟地址空间大小，处理时需要考虑页边界对齐。映射区域必须位于合法的用户虚拟地址空间范围内。新创建的 VMA 不能与已有 VMA 发生重叠。当 `addr == 0` 时，需要在当前进程已有 VMA 之间寻找能够容纳映射区域的空闲空间。寻找空闲空间时，需要遍历所有可能的空闲区域，而不能找到第一个满足条件的区域后立即结束。空闲区域选择应遵循 **Best-Fit（最佳适配）策略**，即选择能够满足请求且剩余空间最小的区域。找到合适的虚拟地址后，需要创建并初始化 `vma_struct`。VMA 至少需要正确维护：`vm_start`,`vm_end`,`vm_flags`,`vm_pgoff`,`vm_file`。对于匿名映射，`vm_file` 应保持为空。对于文件映射，需要正确保存文件指针以及文件中的页偏移信息。文件映射涉及文件引用计数，不能在建立映射后丢失对文件对象的引用。创建完成的 VMA 必须加入当前进程的 VMA 链表，并正确维护 VMA 数量。如果执行过程中发生错误，需要避免留下不完整的 VMA 或错误的链表状态。`do_mmap` 不应该直接修改与本实验无关的系统调用接口和底层页表机制。不需要在 `do_mmap` 中一次性为整个映射区域分配物理页面。实验框架中的 `mmap` 同时支持匿名映射和文件映射；`MAP_SHARED`、`MAP_PRIVATE`、`MAP_ANONYMOUS` 等标志已经由实验框架定义。

整个功能模块可以借助的接口函数、结构体、数据结构为:`struct mm_struct`,`struct vma_struct`,`mm->mmap_list`,`mm->map_count`,`vma_create()`,`insert_vma_struct()`,`find_vma()`,`list_entry_t`,`list_entry()`,`list_next()`,`list_prev()`,`PGSIZE`,`USERTOP`,`USER_ACCESS`,`VM_READ`,`VM_WRITE`,`VM_EXEC`

#### 功能模块三：虚拟内存取消映射功能（设计提示词借助 AI 完成）

要求实现的函数：

```c
int do_munmap(struct mm_struct *mm,
          uintptr_t addr,
          size_t len);(kern\mm\vmm.c)
```

整个功能模块的具体功能：`do_munmap` 用于解除进程虚拟地址空间中的一段内存映射。它与 `do_mmap` 构成完整的映射生命周期，`do_munmap` 的核心任务不是简单地“删除一个 VMA”，而是保证**虚拟地址空间、页表以及 VMA 管理结构之间的状态保持一致**。

整个功能模块的注意事项：`addr` 和 `len` 需要按照页边界进行处理。取消映射的区域必须位于合法的用户地址空间。需要检查指定区域是否确实存在对应的 VMA。不能因为部分区域存在 VMA 就错误地删除整个不匹配的 VMA。解除映射前需要正确处理对应的页表映射。如果该 VMA 是文件映射，需要正确处理文件引用计数。从 VMA 链表删除 VMA 后，需要释放 VMA 对象。删除 VMA 后，需要正确更新 `mm->map_count`。操作过程中不能破坏其他 VMA。如果参数非法或找不到合法映射区域，应按照实验框架的错误处理方式返回失败。`do_munmap` 不应该修改用户态 `munmap` 接口。不需要重新设计页表底层释放机制，应优先使用实验框架已经提供的接口。

整个功能模块可以借助的接口函数、结构体、数据结构：`struct mm_struct`,`struct vma_struct`,`mm->mmap_list`,`mm->map_count`,`find_vma()`,`unmap_range()`,`vma_struct 的：``vm_start`,`vm_end`,`vm_flags`,`vm_file`,`vm_pgoff`,`list_entry_t`,`list_entry()`,`list_del()` 根据这个规范 E:\vx\xwechat_files\wxid_mefss7puk13f12_2675\msg\file\2026-09\注入式评分测试规范(1).zip 注释用英文，格式就参考这个，格式尽量保持一致，注意（1）框架代码里面有些文件中本身就包含check_xx函数，所以check函数涵盖到的检查我们不需要再test （2）如果是框架中已经给出的函数，inject可以注入到任何地方；函数对于我们需要自己实现的函数，inject可能只适合注入到函数开头（函数末尾如果有return语句，放在末尾就检测不到） （3）跟第二点类似，inject在定位函数时，需要谨慎一点，要确保匹配到的字符串在学生们自己实现的代码中有 根据这些内容完成lab9的测试，别的不要修改，就先完成测试文件，确保练习中要求的点全部能够测试到，并且是根据规范中要求的

### 项目组成

#### Lab9 项目组成

```
lab9
├── CMakeLists.txt
├── giveitatry.pyq
├── kern
│   ├── debug
│   │   ├── assert.h
│   │   ├── kdebug.c
│   │   ├── kdebug.h
│   │   ├── kmonitor.c
│   │   ├── kmonitor.h
│   │   ├── panic.c
│   │   └── stab.h
│   ├── driver
│   │   ├── clock.c
│   │   ├── clock.h
│   │   ├── console.c
│   │   ├── console.h
│   │   ├── dtb.c
│   │   ├── dtb.h
│   │   ├── ide.c
│   │   ├── ide.h
│   │   ├── intr.c
│   │   ├── intr.h
│   │   ├── kbdreg.h
│   │   ├── picirq.c
│   │   ├── picirq.h
│   │   ├── ramdisk.c
│   │   └── ramdisk.h
│   ├── fs
│   │   ├── devs
│   │   │   ├── dev.c
│   │   │   ├── dev_disk0.c
│   │   │   ├── dev.h
│   │   │   ├── dev_stdin.c
│   │   │   └── dev_stdout.c
│   │   ├── file.c
│   │   ├── file.h
│   │   ├── fs.c
│   │   ├── fs.h
│   │   ├── iobuf.c
│   │   ├── iobuf.h
│   │   ├── sfs
│   │   │   ├── bitmap.c
│   │   │   ├── bitmap.h
│   │   │   ├── sfs.c
│   │   │   ├── sfs_fs.c
│   │   │   ├── sfs.h
│   │   │   ├── sfs_inode.c
│   │   │   ├── sfs_io.c
│   │   │   └── sfs_lock.c
│   │   ├── swap
│   │   │   ├── swapfs.c
│   │   │   └── swapfs.h
│   │   ├── sysfile.c
│   │   ├── sysfile.h
│   │   └── vfs
│   │       ├── inode.c
│   │       ├── inode.h
│   │       ├── vfs.c
│   │       ├── vfsdev.c
│   │       ├── vfsfile.c
│   │       ├── vfs.h
│   │       ├── vfslookup.c
│   │       └── vfspath.c
│   ├── init
│   │   ├── entry.S
│   │   └── init.c
│   ├── libs
│   │   ├── readline.c
│   │   ├── stdio.c
│   │   └── string.c
│   ├── mm
│   │   ├── default_pmm.c
│   │   ├── default_pmm.h
│   │   ├── kmalloc.c
│   │   ├── kmalloc.h
│   │   ├── memlayout.h
│   │   ├── mmu.h
│   │   ├── pmm.c
│   │   ├── pmm.h
│   │   ├── swap.c
│   │   ├── swap_clock.c
│   │   ├── swap_clock.h
│   │   ├── swap_fifo.c
│   │   ├── swap_fifo.h
│   │   ├── swap.h
│   │   ├── vmm.c
│   │   └── vmm.h
│   ├── process
│   │   ├── entry.S
│   │   ├── proc.c
│   │   ├── proc.h
│   │   └── switch.S
│   ├── schedule
│   │   ├── default_sched_c
│   │   ├── default_sched.h
│   │   ├── default_sched_stride.c
│   │   ├── sched.c
│   │   └── sched.h
│   ├── sync
│   │   ├── check_sync.c
│   │   ├── monitor.c
│   │   ├── monitor.h
│   │   ├── sem.c
│   │   ├── sem.h
│   │   ├── sync.h
│   │   ├── wait.c
│   │   └── wait.h
│   ├── syscall
│   │   ├── syscall.c
│   │   └── syscall.h
│   └── trap
│       ├── trap.c
│       ├── trapentry.S
│       └── trap.h
├── libs
│   ├── atomic.h
│   ├── defs.h
│   ├── dirent.h
│   ├── elf.h
│   ├── error.h
│   ├── hash.c
│   ├── list.h
│   ├── printfmt.c
│   ├── rand.c
│   ├── riscv.h
│   ├── sbi.h
│   ├── skew_heap.h
│   ├── stat.h
│   ├── stdarg.h
│   ├── stdio.h
│   ├── stdlib.h
│   ├── string.c
│   ├── string.h
│   └── unistd.h
├── Makefile
├── tools
│   ├── boot.ld
│   ├── function.mk
│   ├── gdbinit
│   ├── grade-rv64-patch.sh
│   ├── kernel.ld
│   ├── mksfs.c
│   ├── sign.c
│   ├── user.ld
│   └── vector.c
└── user
    ├── badarg.c
    ├── badsegment.c
    ├── divzero.c
    ├── exit.c
    ├── faultread.c
    ├── faultreadkernel.c
    ├── forktest.c
    ├── forktree.c
    ├── hello.c
    ├── libs
    │   ├── dir.c
    │   ├── dir.h
    │   ├── file.c
    │   ├── file.h
    │   ├── initcode.S
    │   ├── lock.h
    │   ├── mmap.h
    │   ├── panic.c
    │   ├── stdio.c
    │   ├── syscall.c
    │   ├── syscall.h
    │   ├── ulib.c
    │   ├── ulib.h
    │   └── umain.c
    ├── matrix.c
    ├── pgdir.c
    ├── priority.c
    ├── sh.c
    ├── sleep.c
    ├── sleepkill.c
    ├── softint.c
    ├── spin.c
    ├── testbss.c
    ├── testmmap.c
    ├── waitkill.c
    └── yield.c

20 directories, 162 files
```

简单说明如下： Lab9 在 Lab8 的基础上增加了内存映射（mmap）和页面替换算法的功能实现。项目主要围绕虚拟内存管理、文件系统与内存映射的集成展开。

kern/mm/swap_clock.c/h：新添加的文件，用于实现 Clock 页面替换算法。

kern/mm/vmm.c/h：需要修改的部分

user/testmmap.c：新添加的测试文件，用于测试内存映射功能的正确性。

kern/mm/swap.c/h：页面替换机制的主要框架，定义了交换管理器接口和基本操作函数。

kern/mm/swap_fifo.c/h：已有的 FIFO 页面替换算法实现，可作为参考实现。

kern/mm/vmm.c：修改了 do_pgfault 函数，添加了对缺页异常的处理逻辑

kern/trap/trap.c：修改了异常处理分发逻辑，确保缺页异常能正确调用 do_pgfault 函数。

## 页面置换机制的实现

### 页面置换后的驱动和存储

在之前的文件系统章节实验中，我们使用 `make qemu` 启动内核，你是否思考过这样一个问题：这个虚拟计算机所使用的文件系统，它对应的“硬盘”究竟挂载在哪里？当页面置换发生时，**被换出的页面到底交换到何处去了**？实际上，我们并没有直接使用物理硬盘，而是通过 Ramdisk 在内存中模拟了一个块设备，用作文件系统和交换分区的存储后端。你可能会觉得这不过是把数据从一个内存区域搬到另一个内存区域，看似没有实际意义。然而，通过构建一个完整的分层设备模型，整个页面置换过程不再是一次随意的内存拷贝，而是一系列标准化的设备操作流程。这就像我们虽然用纸板搭建了一个建筑模型，但它的结构、比例和连接方式完全参照真实建筑的标准，让我们能够在有限的条件下理解复杂的系统架构。

> **扩展** 仔细思考一下，内存和硬盘，除了一个掉电后数据易失一个不易失，一个访问快一个访问慢，其实并没有本质的区别。 理论上，我们完全可以把一块机械硬盘加以改造，写好驱动之后，插到主板的内存插槽上作为内存条使用，当然我们要忽视性能方面的差距。

这个 Ramdisk 的物理数据来源于一个在编译阶段就预先准备好的磁盘镜像文件。让我们深入构建系统的细节，看看这个"虚拟硬盘"是如何诞生的。在 `Makefile` 中，我们使用 `dd` 命令创建一个全零的文件 `swap.img`，其大小可以通过 `bs`（块大小）和 `count`（块数量）参数精确控制。例如，`bs=4kB count=8` 会创建一个 32KB 的文件，这个大小足以容纳多个内存页面。

```makefile
# Makefile (片段)
# 使用 dd 命令创建一个全零的 swap.img 文件，大小为 4KB * 8 = 32KB
$(SWAPIMG):
    $(V)dd if=/dev/zero of=$@ bs=4kB count=8
```

创建好镜像文件后，真正的魔法发生在链接阶段。通过 `ld` 命令的 `--format=binary` 参数，我们将 `swap.img` 的二进制内容直接嵌入到内核可执行文件中。这个操作使得镜像数据成为内核 `.data` 段的一部分，但又不占用编译时定义的静态数组空间。链接器会自动生成两个符号 `_binary_bin_swap_img_start` 和 `_binary_bin_swap_img_end`，它们分别指向这段嵌入数据在内核地址空间中的起始和结束位置。

```makefile
# Makefile (链接阶段)
# 使用 --format=binary 将 swap.img 直接链接进内核可执行文件
$(kernel): $(KOBJS) $(SWAPIMG) $(SFSIMG)
    # ...
    $(V)$(LD) ... --format=binary $(SWAPIMG) ...
```

有了存储介质，下一步就是为其提供访问接口。Ramdisk 驱动的核心任务是将这段嵌入的内存区域伪装成一个支持标准扇区读写的设备。驱动通过引用链接器生成的符号来定位数据区域，并在初始化时将其绑定到设备对象中。这种设计使得驱动完全不知道数据的具体来源，它只关心**一个基地址和一段连续的内存区域**。

```c
// Ramdisk 初始化：将内存地址与设备对象绑定
void ramdisk_init(int devno, struct ide_device *dev) {
    if (devno == SWAP_DEV_NO) {
        // 绑定数据源：让 iobase 指向 swap.img 的内存数据
        dev->iobase = (uintptr_t)_binary_bin_swap_img_start;

        // 计算容量：通过起止地址之差计算扇区数
        dev->size = (unsigned int)(_binary_bin_swap_img_end
                                 - _binary_bin_swap_img_start) / SECTSIZE;

        // 绑定行为：注册具体的读写函数
        dev->read_secs = ramdisk_read;
        dev->write_secs = ramdisk_write;
        dev->valid = 1; // 标记设备可用
    }
}
```

Ramdisk的读写逻辑虽然核心仍然是内存拷贝，但加入了关键的边界检查机制。这种检查确保任何读写请求都不会超出分配给Ramdisk的内存区域范围，从而防止意外的内存越界访问导致内核数据被破坏。

```c
// Ramdisk 写操作：带有边界检查的内存拷贝
static int ramdisk_write(struct ide_device *dev, size_t secno, const void *src,
                         size_t nsecs) {
    // 关键安全检查：防止写入超过镜像大小
    nsecs = MIN(nsecs, dev->size - secno);

    // 执行真正的"写入"：内存拷贝
    memcpy((void *)(dev->iobase + secno * SECTSIZE), src, nsecs * SECTSIZE);
    return 0;
}
```

仅仅实现驱动还不够，操作系统需要一套统一的模型来管理多种可能存在的块设备。为此，内核引入了 IDE 设备抽象层。该层定义了一个通用的设备结构体 `struct ide_device`，其中不仅包含设备容量、基地址等属性，更重要的是包含两个函数指针 `read_secs` 和 `write_secs`。这种设计使得具体的读写逻辑与上层接口解耦，任何符合此接口的驱动都能被无缝集成。

```c
// IDE 设备抽象结构（关键字段）
struct ide_device {

    // 其他成员
    unsigned int size;   // 设备总扇区数
    uintptr_t iobase;    // 数据区基地址
    // 多态的关键：函数指针
    int (*read_secs)(struct ide_device *dev, size_t secno, void *dst, size_t nsecs);
    int (*write_secs)(struct ide_device *dev, size_t secno, const void *src, size_t nsecs);
};
```

> **扩展** 当前我们使用的是 Ramdisk，这只是一种在内存中模拟的简单设备。在实际的系统演进中，我们可以预见以下几种扩展场景：
>
> 当我们需要在虚拟化环境中获得更好的磁盘性能时，可以引入 Virtio 块设备驱动。Virtio 是一种半虚拟化标准，通过在宿主机和虚拟机之间定义清晰的接口，能够显著提升虚拟设备的 I/O 效率。实现时，我们只需编写一个符合 struct ide_device 接口的 Virtio 驱动，并将其注册到 IDE 抽象层中，上层所有代码（包括 SwapFS 和文件系统）就能无缝切换到更高效的虚拟磁盘。
>
> 更进一步，当系统需要部署到真实的物理硬件时，我们可以根据具体的硬件平台实现相应的驱动。无论是 SATA、NVMe 还是其他类型的块设备，只要按照 read_secs 和 write_secs 的接口要求实现具体的读写函数，并正确初始化设备结构体，就能将真实的物理硬盘接入到这个存储子系统中。这意味着整个页面置换机制和文件系统可以在不改动上层逻辑的情况下，从模拟环境迁移到真实硬件环境。
>
> 这种设计体现了操作系统开发中重要的抽象原则：通过定义清晰的接口，将硬件相关的具体实现与系统核心逻辑分离，使得系统既能在简单的教学环境中运行，又具备向生产环境演进的能力。

系统启动时，`ide_init` 函数负责将具体的驱动实现注册到 IDE 抽象层中。这里我们可以看到系统支持多种设备并存的设计：交换设备和文件系统设备虽然都是 Ramdisk，但被分配了不同的设备号，绑定到不同的内存区域。

```c
// IDE 层初始化：注册具体设备
void ide_init(void) {
    // 初始化交换设备 (SWAP_DEV_NO = 1)
    ramdisk_init(SWAP_DEV_NO, &ide_devices[SWAP_DEV_NO]);
    // 初始化文件系统设备 (DISK0_DEV_NO = 2)
    ramdisk_init(DISK0_DEV_NO, &ide_devices[DISK0_DEV_NO]);
}
```

当上层需要读写磁盘时，调用的是统一的 `ide_write_secs` 函数。这个函数并不直接操作硬件，而是作为一个分发器，根据设备号找到对应的设备对象，然后通过其函数指针调用具体的实现。这种设计使得上层代码完全不用关心底层是 Ramdisk 还是未来可能添加的真实硬盘驱动。

```c
// IDE 层的统一写接口：动态分发请求
int ide_write_secs(unsigned short ideno, uint32_t secno, const void *src,
                   size_t nsecs) {
    // 参数校验...
    // 动态分发：调用注册在具体设备中的函数
    return ide_devices[ideno].write_secs(&ide_devices[ideno], secno, src, nsecs);
}
```

对于页面置换算法而言，它并不直接与 IDE 层交互，而是通过一个称为 SwapFS 的中间层。SwapFS 的任务是处理内存页与磁盘扇区之间的转换。这里涉及一个关键的数据结构 `swap_entry_t`，它是一个 32 位整数，其中高 24 位表示在交换分区中的索引，低 8 位用于存储标志位。

```c
// swap_offset 宏：从 swap_entry_t 中提取交换区索引
#define swap_offset(entry) ({                                       \
               size_t __offset = (entry >> 8);                      \
               if (!(__offset > 0 && __offset < max_swap_offset)) { \
                    panic("invalid swap_entry_t = %08x.\n", entry); \
               }                                                    \
               __offset;                                            \
          })
```

SwapFS 的写操作将页面数据写入交换设备。这里需要处理单位转换：一个内存页（通常 4KB）对应多个磁盘扇区（每个 512B）。通过 `PAGE_NSECT`（值为 8）的乘法运算，SwapFS 将页索引转换为起始扇区号，然后调用 IDE 层的统一接口。

```c
// SwapFS 的写操作：将内存页写入交换设备
int swapfs_write(swap_entry_t entry, struct Page *page) {
    // 将页面的交换索引转换为起始扇区号
    // PAGE_NSECT 定义了每页所需的扇区数（通常为 8）
    return ide_write_secs(SWAP_DEV_NO, swap_offset(entry) * PAGE_NSECT,
                          page2kva(page), PAGE_NSECT);
}
```

至此，我们看到了一个完整的调用链条：当需要换出一个页面时，`Swap Manager` 调用 `swapfs_write`；SwapFS 将页面信息转换为磁盘扇区请求，调用 `ide_write_secs`；IDE 抽象层根据设备号找到已初始化的 Ramdisk 设备对象，通过其函数指针调用 `ramdisk_write`；最后，Ramdisk 驱动执行内存拷贝，将页面数据写入由链接器嵌入的那段镜像内存中。虽然底层操作依然是内存拷贝，但每一层都有明确的职责：`SwapFS` 负责**页与扇区的转换**，IDE层负责**设备抽象与多态**，Ramdisk负责**内存模拟磁盘区域的管理与保护**。这样的分层设计不仅让页面置换的功能得以实现，更展示了一个可扩展的存储子系统如何通过清晰的接口定义和职责分离来构建。如果未来需要支持真正的硬盘设备，我们只需要在驱动层实现相应的读写函数并注册到IDE抽象层，而SwapFS和页面置换算法完全无需改动。这正是操作系统设备管理架构的价值所在：通过适当的**抽象**，让上层功能与底层实现解耦，使系统既能在简单环境下运行演示，又具备向复杂环境演进的能力。

### 处理缺页异常（page fault异常）

什么是缺页异常？

简单来说，当CPU试图访问一个虚拟地址，但MMU（内存管理单元）无法找到对应的物理地址映射关系，或者当前访问权限与页表项的权限设置不一致时，CPU就会触发一个缺页异常。这就像你有一个巨大的图书馆借阅卡（虚拟地址空间），但图书馆的实际藏书（物理内存）有限，当你试图借阅一本不在书架上的书时，图书馆管理员（操作系统）就需要去仓库（磁盘）取书，或者协调其他读者归还书籍。

在之前的内存管理实验中，我们主要关注的是物理内存的分配和回收，像是在管理一个真实的物理图书馆。然而，程序运行时所感知的是虚拟内存空间，它们并不关心物理内存的具体布局。这种虚拟内存的"需求"与物理内存的"供给"之间并没有直接的对应关系，需要操作系统作为中介来进行协调。在ucore中，我们通过page fault异常处理来搭建这座桥梁。

> **须知：哪些页面可以被换出？**
>
> 在操作系统的设计中，一个基本的原则是：并非所有的物理页都可以交换出去的，只有映射到用户空间且被用户程序直接访问的页面才能被交换，而被内核直接使用的内核空间的页面不能被换出。这里面的原因是什么呢？操作系统是执行的关键代码，需要保证运行的高效性和实时性，如果在操作系统执行过程中，发生了缺页现象，则操作系统不得不等很长时间（硬盘的访问速度比内存的访问速度慢 2~3 个数量级），这将导致整个系统运行低效。而且，不难想象，处理缺页过程所用到的内核代码或者数据如果被换出，整个内核都面临崩溃的危险。
>
> 不过我们当前实现的换入换出机制中，仅仅通过执行 `check_swap` 函数在内核中分配一些页，模拟对这些页的访问，然后通过 `do_pgfault` 来调用 `swap_map_swappable` 函数来查询这些页的访问情况并间接调用相关函数，换出“不常用”的页到磁盘上。

当我们引入了虚拟内存，就意味着虚拟内存的空间可以远远大于物理内存，也意味着程序可以访问"不对应物理内存页帧的虚拟内存地址"，这时CPU应当抛出 Page Fault 异常。

现在，我们需要真正实现 Page Fault 的处理机制，让操作系统能够在页面缺失时动态地分配物理内存，甚至从磁盘中换入需要的页面。这样，程序就能够透明地使用超过物理内存大小的地址空间，而无需担心内存不足的问题。

```c
// kern/trap/trap.c

static int pgfault_handler(struct trapframe *tf) {
    extern struct mm_struct *check_mm_struct;
    if (check_mm_struct != NULL) {
        print_pgfault(tf);
    }

    struct mm_struct *mm;
    if (check_mm_struct != NULL) {
        // 测试环境：使用check_mm_struct
        assert(current == idleproc);
        mm = check_mm_struct;
    } else {
        // 真实环境：使用当前进程的内存管理结构
        if (current == NULL) {
            print_trapframe(tf);
            print_pgfault(tf);
            panic("unhandled page fault.\n");
        }
        mm = current->mm;
    }

    return do_pgfault(mm, tf->cause, tf->tval);
}
```

观察这段代码，我们可以看到缺页异常处理函数的精妙设计。它首先区分了两种不同的运行环境：在测试环境中，我们使用全局的`check_mm_struct`来模拟内存管理结构；而在真实进程环境中，则使用当前进程的`mm`结构。这种设计使得同一个处理逻辑既能用于内核自检，又能服务于真实的用户进程。

当异常发生时，`exception_handler`会将控制流转到`pgfault_handler`，进而调用`do_pgfault`函数。这个函数是内存管理的核心枢纽，它需要根据缺页的原因和地址，执行相应的处理逻辑：可能是分配一个新的物理页，也可能是从交换分区中换入一个已被换出的页。

```c
// kern/trap/trap.c

void exception_handler(struct trapframe *tf) {
    int ret;
    switch (tf->cause) {
        /* ... 其他异常处理 ... */
        case CAUSE_LOAD_PAGE_FAULT:
            cprintf("Load page fault\n");
            if ((ret = pgfault_handler(tf)) != 0) {
                print_trapframe(tf);
                panic("handle pgfault failed. %e\n", ret);
            }
            break;
        case CAUSE_STORE_PAGE_FAULT:
            cprintf("Store/AMO page fault\n");
            if ((ret = pgfault_handler(tf)) != 0) {
                print_trapframe(tf);
                panic("handle pgfault failed. %e\n", ret);
            }
            break;
        default:
            print_trapframe(tf);
            break;
    }
}
```

处理缺页异常不仅需要分配内存，还需要在虚拟地址和物理地址之间进行转换。内核提供了一系列宏和函数来完成这项工作，它们像是操作系统中的"翻译官"，确保虚拟世界和物理世界能够正确对应。这些转换函数是内核基础设施的重要组成部分，使得上层代码能够透明地访问物理内存。

```c
// kern/mm/pmm.h

// 内核虚拟地址到物理地址的转换
#define PADDR(kva) ({                                   \
    uintptr_t __m_kva = (uintptr_t)(kva);               \
    if (__m_kva < KERNBASE) {                           \
        panic("PADDR called with invalid kva %08lx", __m_kva); \
    }                                                   \
    __m_kva - va_pa_offset;                             \
})

// 物理地址到内核虚拟地址的转换
#define KADDR(pa) ({                                    \
    uintptr_t __m_pa = (pa);                            \
    size_t __m_ppn = PPN(__m_pa);                       \
    if (__m_ppn >= npage) {                             \
        panic("KADDR called with invalid pa %08lx", __m_pa); \
    }                                                   \
    (void *)(__m_pa + va_pa_offset);                    \
})

// Page结构体到内核虚拟地址的转换
static inline void *page2kva(struct Page *page) {
    return KADDR(page2pa(page));
}
```

当`do_pgfault`函数被调用时，它面临的核心挑战是：如何处理那些不在物理内存中的页面？这就是页面置换算法发挥作用的地方。如果缺页的地址对应着一个已被换出到磁盘的页面，那么`do_pgfault`需要协调`swap`子系统，从磁盘中读回页面内容，并为其分配物理内存。同时，它还需要更新页表，建立虚拟地址到新物理页面的映射关系。

```c
// kern/mm/vmm.c
int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr) {
    int ret = -E_INVAL;
    // 1. 查找 vma 结构与地址合法性检查
    struct vma_struct *vma = find_vma(mm, addr);
    pgfault_num++;
    if (vma == NULL || vma->vm_start > addr) {
        return -E_INVAL;
    }

    // 2. 写权限检查（仅对写操作检查）
    // RISC-V: cause == 15 (CAUSE_STORE_PAGE_FAULT) 表示写操作
    bool is_write = (error_code == 15);
    if (is_write && !(vma->vm_flags & VM_WRITE)) {
        return -E_INVAL;
    }

    // 3. 设置页权限：RISC-V 要求必须设置 PTE_R
    uint32_t perm = PTE_U | PTE_R;
    if (vma->vm_flags & VM_WRITE) {
        perm |= PTE_W;
    }

    addr = ROUNDDOWN(addr, PGSIZE);
    // 4. 获取 PTE（必要时创建页表）
    pte_t *ptep = get_pte(mm->pgdir, addr, 1);
    if (ptep == NULL) {
        return -E_NO_MEM;
    }

    /*
     * 5. mmap 文件映射缺页处理
     * 如果 vma->vm_file 不为空，说明是文件映射
     */
    if (vma->vm_file != NULL) {
    }

    /*
     * 6. 匿名页 / swap 缺页处理
     */
    if (*ptep == 0) {
        // 匿名页：分配新页并清零
        if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL) {
            return -E_NO_MEM;
        }
    } else {
        // PTE不为0，可能是swap条目
        if (swap_init_ok) {
            struct Page *page = NULL;
            // 从 swap 分区换入页面
            if ((ret = swap_in(mm, addr, &page)) != 0) {
                return ret;
            }
            // 建立映射并更新可交换标记
            page_insert(mm->pgdir, page, addr, perm);
            swap_map_swappable(mm, addr, page, 1);
            page->pra_vaddr = addr;
        }
        // ... 其他情况（如 swap 未初始化时的错误处理） ...
    }

    return 0;
}
```

缺页异常处理机制识别了页面不在内存中的情况。但这自然地引出了一个新的问题：如果物理内存已满，没有空闲页面容纳新换入的页面，该怎么办？这就需要页面置换算法来做出选择，决定哪些页面应该被换出到磁盘。接下来，我们就去看看页面置换算法是如何设计和实现的。

### 页替换算法和机制实现

#### 页替换算法设计思路

操作系统为何要进行页面置换呢？这是由于操作系统给用户态的应用程序提供了一个虚拟的“大容量”内存空间，而实际的物理内存空间又没有那么大。所以操作系统就就“瞒着”应用程序，只把应用程序中“常用”的数据和代码放在物理内存中，而不常用的数据和代码放在了硬盘这样的存储介质上。如果应用程序访问的是“常用”的数据和代码，那么操作系统已经放置在内存中了，不会出现什么问题。但当应用程序访问它认为应该在内存中的的数据或代码时，如果这些数据或代码不在内存中，则根据上一小节的介绍，会产生页访问异常。这时，操作系统必须能够应对这种页访问异常，即尽快把应用程序当前需要的数据或代码放到内存中来，然后重新执行应用程序产生异常的访存指令。如果在把硬盘中对应的数据或代码调入内存前，操作系统发现物理内存已经没有空闲空间了，这时操作系统必须把它认为“不常用”的页换出到磁盘上去，以腾出内存空闲空间给应用程序所需的数据或代码。

操作系统迟早会碰到没有内存空闲空间而必须要置换出内存中某个“不常用”的页的情况。如何判断内存中哪些是“常用”的页，哪些是“不常用”的页，把“常用”的页保持在内存中，在物理内存空闲空间不够的情况下，把“不常用”的页置换到硬盘上就是页替换算法着重考虑的问题。容易理解，一个好的页替换算法会导致页访问异常次数少，也就意味着访问硬盘的次数也少，从而使得应用程序执行的效率就高。

本次实验涉及的页替换算法（包括扩展练习）：

- 先进先出(First In First Out, FIFO)页替换算法：该算法总是淘汰最先进入内存的页，即选择在内存中驻留时间最久的页予以淘汰。只需把一个应用程序在执行过程中已调入内存的页按先后次序链接成一个队列，队列头指向内存中驻留时间最久的页，队列尾指向最近被调入内存的页。这样需要淘汰页时，从队列头很容易查找到需要淘汰的页。FIFO 算法只是在应用程序按线性顺序访问地址空间时效果才好，否则效率不高。因为那些常被访问的页，往往在内存中也停留得最久，结果它们因变“老”而不得不被置换出去。FIFO 算法的另一个缺点是，它有一种异常现象（Belady 现象），即在增加放置页的物理页帧的情况下，反而使页访问异常次数增多。
- 最久未使用(least recently used, LRU)算法：利用局部性，通过过去的访问情况预测未来的访问情况，我们可以认为最近还被访问过的页面将来被访问的可能性大，而很久没访问过的页面将来不太可能被访问。于是我们比较当前内存里的页面最近一次被访问的时间，把上一次访问时间离现在最久的页面置换出去。
- 时钟（Clock）页替换算法：是 LRU 算法的一种近似实现。时钟页替换算法把各个页面组织成环形链表的形式，类似于一个钟的表面。然后把一个指针（简称当前指针）指向最老的那个页面，即最先进来的那个页面。另外，时钟算法需要在页表项（PTE）中设置了一位访问位来表示此页表项对应的页当前是否被访问过。当该页被访问时，CPU 中的 MMU 硬件将把访问位置“1”。当操作系统需要淘汰页时，对当前指针指向的页所对应的页表项进行查询，如果访问位为“0”，则淘汰该页，如果该页被写过，则还要把它换出到硬盘上；如果访问位为“1”，则将该页表项的此位置“0”，继续访问下一个页。该算法近似地体现了 LRU 的思想，且易于实现，开销少，需要硬件支持来设置访问位。时钟页替换算法在本质上与 FIFO 算法是类似的，不同之处是在时钟页替换算法中跳过了访问位为 1 的页。
- 改进的时钟（Enhanced Clock）页替换算法：在时钟置换算法中，淘汰一个页面时只考虑了页面是否被访问过，但在实际情况中，还应考虑被淘汰的页面是否被修改过。因为淘汰修改过的页面还需要写回硬盘，使得其置换代价大于未修改过的页面，所以优先淘汰没有修改的页，减少磁盘操作次数。改进的时钟置换算法除了考虑页面的访问情况，还需考虑页面的修改情况。即该算法不但希望淘汰的页面是最近未使用的页，而且还希望被淘汰的页是在主存驻留期间其页面内容未被修改过的。这需要为每一页的对应页表项内容中增加一位引用位和一位修改位。当该页被访问时，CPU 中的 MMU 硬件将把访问位置“1”。当该页被“写”时，CPU 中的 MMU 硬件将把修改位置“1”。这样这两位就存在四种可能的组合情况：（0，0）表示最近未被引用也未被修改，首先选择此页淘汰；（0，1）最近未被使用，但被修改，其次选择；（1，0）最近使用而未修改，再次选择；（1，1）最近使用且修改，最后选择。该算法与时钟算法相比，可进一步减少磁盘的 I/O 操作次数，但为了查找到一个尽可能适合淘汰的页面，可能需要经过多次扫描，增加了算法本身的执行开销。

#### 页面置换机制设计思路

现在我们来看看ucore页面置换机制的实现。

页面置换机制中， 我们需要维护一些”不在内存当中但是也许会用到“的页，它们存储在磁盘的交换区里，也有对应的虚拟地址，但是因为它们不在内存里，在页表里并没有对它们的虚拟地址进行映射。但是在发生Page Fault之后，会把访问到的页放到内存里，这时也许会把其他页扔出去，来给要用的页腾出地方。页面置换算法的核心任务，主要就是确定”把谁扔出去“。

页表里的信息大家都知道内容方面是比较有限的，基本上可以理解为"当前哪些数据在内存条里以及它们物理地址和虚拟地址的对应关系", 这里我们显然需要一些页表之外的数据结构来维护当前页表里没映射的页。也就是要存储以下这些信息：

- 有哪些虚拟地址对应的页当前在磁盘上，分别在磁盘上的哪个位置？
- 有哪些虚拟地址对应的页面当前放在内存里？

这两类页面（位于内存/磁盘）因为会相互转换(换入/换出内存)，所以我们将这两类页面一起维护，也就是维护”所有可用的虚拟地址/虚拟页的集合“（不论当前这个虚拟地址对应的页在内存上还是在硬盘上）。之后我们将要实现进程机制，对于不同的进程，可用的虚拟地址（虚拟页）的集合常常是不一样的，因此每个进程需要一个页表，也需要一个数据结构来维护“所有可用的虚拟地址”。

因此，我们在vmm.h定义两个结构体 (vmm：virtural memory management)。

- `vma_struct`结构体描述一段连续的虚拟地址，从`vm_start`到`vm_end`。 通过包含一个`list_entry_t`成员，我们可以把同一个页表对应的多个`vma_struct`结构体串成一个链表，在链表里把它们按照区间的起始点进行排序。
- `vm_flags`表示的是一段虚拟地址对应的权限（可读，可写，可执行等），这个权限在页表项里也要进行对应的设置。

我们注意到，每个页表（每个虚拟地址空间）可能包含多个`vma_struct`, 也就是多个访问权限可能不同的，不相交的连续地址区间。我们用`mm_struct`结构体把一个页表对应的信息组合起来，包括`vma_struct`链表的首指针，对应的页表在内存里的指针，`vma_struct`链表的元素个数。

（参考`libs/list.h`）

```c
// kern/mm/vmm.h
//pre define
struct mm_struct;

// the virtual continuous memory area(vma), [vm_start, vm_end),
// addr belong to a vma means  vma.vm_start<= addr <vma.vm_end
struct vma_struct {
    struct mm_struct *vm_mm; // the set of vma using the same PDT
    uintptr_t vm_start;      // start addr of vma
    uintptr_t vm_end;        // end addr of vma, not include the vm_end itself
    uint32_t vm_flags;       // flags of vma
    list_entry_t list_link;  // linear list link which sorted by start addr of vma
    int vm_pgoff;            // file offset in pages
    struct file *vm_file;    // mapped file pointer
};

#define le2vma(le, member)                  \
    to_struct((le), struct vma_struct, member)

#define VM_READ                 0x00000001
#define VM_WRITE                0x00000002
#define VM_EXEC                 0x00000004
#define VM_STACK                0x00000008

// the control struct for a set of vma using the same Page Table
struct mm_struct {
    list_entry_t mmap_list;        // linear list link which sorted by start addr of vma
    struct vma_struct *mmap_cache; // current accessed vma, used for speed purpose
    pde_t *pgdir;                  // the Page Table of these vma
    int map_count;                 // the count of these vma
    void *sm_priv;                   // the private data for swap manager
    int mm_count;                  // the number ofprocess which shared the mm
    semaphore_t mm_sem;            // mutex for using dup_mmap fun to duplicat the mm
    int locked_by;
};
```

除了以上内容，我们还需要为`vma_struct`和`mm_struct`定义和实现一些接口：包括它们的构造函数，以及如何把新的`vma_struct`插入到`mm_struct`对应的链表里。注意这两个结构体占用的内存空间需要用`kmalloc()`函数动态分配。

```c
// kern/mm/vmm.c
// mm_create -  alloc a mm_struct & initialize it.
struct mm_struct *
mm_create(void) {
    struct mm_struct *mm = kmalloc(sizeof(struct mm_struct));

    if (mm != NULL) {
        list_init(&(mm->mmap_list));
        mm->mmap_cache = NULL;
        mm->pgdir = NULL;
        mm->map_count = 0;

        if (swap_init_ok) swap_init_mm(mm);//我们接下来解释页面置换的初始化
        else mm->sm_priv = NULL;

        set_mm_count(mm, 0);
        sem_init(&(mm->mm_sem), 1);
    }
    return mm;
}

// vma_create - alloc a vma_struct & initialize it. (addr range: vm_start~vm_end)
struct vma_struct *
vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags) {
    struct vma_struct *vma = kmalloc(sizeof(struct vma_struct));
    if (vma != NULL) {
        vma->vm_start = vm_start;
        vma->vm_end = vm_end;
        vma->vm_flags = vm_flags;
        vma->vm_file = NULL; // 初始化文件映射指针
        vma->vm_pgoff = 0;   // 初始化页偏移
    }
    return vma;
}
```

在插入一个新的`vma_struct`之前，我们要保证它和原有的区间都不重合。

```c
// kern/mm/vmm.c
// check_vma_overlap - check if vma1 overlaps vma2 ?
static inline void
check_vma_overlap(struct vma_struct *prev, struct vma_struct *next) {
    assert(prev->vm_start < prev->vm_end);
    assert(prev->vm_end <= next->vm_start);
    assert(next->vm_start < next->vm_end);// next 是我们想插入的区间，这里顺便检验了start < end
}
```

我们可以插入一个新的`vma_struct`, 也可以查找某个虚拟地址对应的`vma_struct`是否存在。

```c
// kern/mm/vmm.c

// insert_vma_struct -insert vma in mm's list link
void
insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma) {
    assert(vma->vm_start < vma->vm_end);
    list_entry_t *list = &(mm->mmap_list);
    list_entry_t *le_prev = list, *le_next;

    list_entry_t *le = list;
    while ((le = list_next(le)) != list) {
        struct vma_struct *mmap_prev = le2vma(le, list_link);
        if (mmap_prev->vm_start > vma->vm_start) {
            break;
        }
        le_prev = le;
    }
    //保证插入后所有vma_struct按照区间左端点有序排列
    le_next = list_next(le_prev);

    /* check overlap */
    if (le_prev != list) {
        check_vma_overlap(le2vma(le_prev, list_link), vma);
    }
    if (le_next != list) {
        check_vma_overlap(vma, le2vma(le_next, list_link));
    }

    vma->vm_mm = mm;
    list_add_after(le_prev, &(vma->list_link));

    mm->map_count ++;//计数器
}

// find_vma - find a vma  (vma->vm_start <= addr <= vma_vm_end)
//如果返回NULL，说明查询的虚拟地址不存在/不合法，既不对应内存里的某个页，也不对应硬盘里某个可以换进来的页
struct vma_struct *
find_vma(struct mm_struct *mm, uintptr_t addr) {
    struct vma_struct *vma = NULL;
    if (mm != NULL) {
        vma = mm->mmap_cache;
        if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr)) {
                bool found = 0;
                list_entry_t *list = &(mm->mmap_list), *le = list;
                while ((le = list_next(le)) != list) {
                    vma = le2vma(le, list_link);
                    if (vma->vm_start<=addr && addr < vma->vm_end) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    vma = NULL;
                }
        }
        if (vma != NULL) {
            mm->mmap_cache = vma;
        }
    }
    return vma;
}
```

如果此时发生Page Fault怎么办？我们可以回顾之前的异常处理部分的知识。我们的`trapFrame`传递了`tf->tval`给`do_pgfault()`函数，而这实际上是`stval`这个寄存器的数值（在旧版的RISCV标准里叫做`sbadvaddr`)，这个寄存器存储一些关于异常的数据，对于`PageFault`它存储的是访问出错的虚拟地址。

```c
// kern/trap/trap.c
static int pgfault_handler(struct trapframe *tf) {
    extern struct mm_struct *check_mm_struct;//当前使用的mm_struct的指针，在vmm.c定义
    print_pgfault(tf);
    if (check_mm_struct != NULL) {
        return do_pgfault(check_mm_struct, tf->cause, tf->tval);
    }
    panic("unhandled page fault.\n");
}
// kern/mm/vmm.c
struct mm_struct *check_mm_struct;

// check_pgfault - check correctness of pgfault handler
static void
check_pgfault(void) {
    /* ...... */
    check_mm_struct = mm_create();
    /* ...... */
}
```

`do_pgfault()`函数在`vmm.c`定义，是页面置换机制的核心。如果过程可行，没有错误值返回，我们就可对页表做对应的修改，通过加入对应的页表项，并把硬盘上的数据换进内存，这时还可能涉及到要把内存里的一个页换出去，而`do_pgfault()`函数就实现了这些功能。如果你对这部分内容相当了解的话，那可以说你对于页面置换机制的掌握程度已经很棒了。

```c
// kern/mm/vmm.c
int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr)
{
    int ret = -E_INVAL;
    //try to find a vma which include addr
    struct vma_struct *vma = find_vma(mm, addr);
    //我们首先要做的就是在mm_struct里判断这个虚拟地址是否可用
    pgfault_num++;

    // 1. 地址合法性检查
    if (vma == NULL || vma->vm_start > addr)
    {
        cprintf("not valid addr %x, and can not find it in vma\n", addr);
        return -E_INVAL;
    }

    // 2. 写权限检查（仅对写操作检查）
    // RISC-V: cause == 15 (CAUSE_STORE_PAGE_FAULT) 表示写操作
    bool is_write = (error_code == 15);
    if (is_write && !(vma->vm_flags & VM_WRITE))
    {
        cprintf("do_pgfault: write to non-writable vma\n");
        return -E_INVAL;
    }

    // 3. 设置页权限
    // RISC-V 要求：必须设置 PTE_R 才能访问页面
    uint32_t perm = PTE_U | PTE_R; // 用户可访问 + 可读
    if (vma->vm_flags & VM_WRITE)
    {
        perm |= PTE_W;
    }

    addr = ROUNDDOWN(addr, PGSIZE);

    // 4. 获取 PTE（必要时创建页表）
    pte_t *ptep = get_pte(mm->pgdir, addr, 1);
    if (ptep == NULL)
    {
        cprintf("get_pte in do_pgfault failed\n");
        return -E_NO_MEM;
    }

    /*
     * =================================================
     * 5. mmap 文件映射缺页处理
     *    模仿 Linux 的处理方式：
     *    - 文件映射的页面在缺页时从文件读取
     *    - 使用 file_seek + file_read 来读取指定偏移的数据
     * =================================================
     */
    if (vma->vm_file != NULL)
    {
        // 文件映射：只处理"页尚未建立"的情况
        if (*ptep == 0)
        {
            // 分配物理页
            struct Page *page = pgdir_alloc_page(mm->pgdir, addr, perm);
            if (page == NULL)
            {
                cprintf("pgdir_alloc_page in mmap pgfault failed\n");
                return -E_NO_MEM;
            }

            void *kva = page2kva(page);
            // 先清零，防止文件末尾不足一页时有脏数据
            memset(kva, 0, PGSIZE);

            // 计算文件偏移
            // vm_pgoff 是映射起始的页偏移，加上当前页相对于映射起始的偏移
            size_t page_index = (addr - vma->vm_start) / PGSIZE;
            off_t file_offset = (vma->vm_pgoff + page_index) * PGSIZE;

            // 先 seek 到正确位置，再读取
            struct file *file = vma->vm_file;
            int fd = file->fd;
            off_t saved_pos = file->pos; // 保存当前游标

            ret = file_seek(fd, file_offset, LSEEK_SET);
            if (ret != 0)
            {
                cprintf("file_seek in mmap pgfault failed: %e\n", ret);
                // 即使 seek 失败，页面已分配，返回成功让进程继续
                // 页面内容为零
                return 0;
            }

            size_t read_size = 0;
            ret = file_read(fd, kva, PGSIZE, &read_size);

            // 恢复文件游标
            file_seek(fd, saved_pos, LSEEK_SET);

            if (ret != 0 && ret != -E_EOF)
            {
                cprintf("file_read in mmap pgfault failed: %e\n", ret);
                // 读取失败，但页面已分配，返回成功
                return 0;
            }
            // 读取成功（可能读取了 0 到 PGSIZE 字节）
        }
        // 如果 *ptep != 0，说明页面已存在，直接返回成功
        return 0;
    }

    /*
     * ==================================
     * 6. 匿名页 / swap 缺页处理
     * ==================================
     */
    if (*ptep == 0)
    {
        // 匿名页：分配新页并清零
        if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL)
        {
            cprintf("pgdir_alloc_page in do_pgfault failed\n");
            return -E_NO_MEM;
        }
    }
    else
    {
        // PTE不为0，可能是swap条目或者是已存在的页面
        if (swap_init_ok)
        {
            // swap 页：从交换区换入
            struct Page *page = NULL;
            if ((ret = swap_in(mm, addr, &page)) != 0)
            {
                cprintf("swap_in in do_pgfault failed\n");
                return ret;
            }
            page_insert(mm->pgdir, page, addr, perm);
            swap_map_swappable(mm, addr, page, 1);
            page->pra_vaddr = addr;
        }
        else
        {
            // swap未初始化，但PTE不为0
            // 检查是否是有效页面（PTE_V位）
            if (*ptep & PTE_V)
            {
                // 页面已经存在，可能是权限问题，直接返回成功
                return 0;
            }
            // 不是有效页面，可能是残留的swap条目或无效数据
            // 清除PTE并分配新页面
            *ptep = 0;
            if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL)
            {
                cprintf("pgdir_alloc_page in do_pgfault failed (swap not ready)\n");
                return -E_NO_MEM;
            }
        }
    }

    return 0;
}
```

接下来我们看看FIFO页面置换算法是怎么在ucore里实现的。

### FIFO页面置换算法

所谓FIFO(First in, First out)页面置换算法，相当简单，就是把所有页面排在一个队列里，每次换入页面的时候，把队列里最靠前（最早被换入）的页面置换出去。

换出页面的时机相对复杂一些，针对不同的策略有不同的时机。ucore 目前大致有两种策略，即积极换出策略和消极换出策略。

- 积极换出策略是指操作系统周期性地（或在系统不忙的时候）主动把某些认为“不常用”的页换出到硬盘上，从而确保系统中总有一定数量的空闲页存在，这样当需要空闲页时，基本上能够及时满足需求；
- 消极换出策略是指只有当试图得到空闲页时，发现当前没有空闲的物理页可供分配，这时才开始查找“不常用”页面，并把一个或多个这样的页换出到硬盘上。

目前的框架支持第二种情况，在`alloc_pages()`里面，如果此时试图得到空闲页且没有空闲的物理页时，我们才尝试换出页面到硬盘上。

```c
// kern/mm/pmm.c
// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE memory
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;

    while (1) {
        local_intr_save(intr_flag);
        { page = pmm_manager->alloc_pages(n); }
        local_intr_restore(intr_flag);
        //如果有足够的物理页面，就不必换出其他页面
        //如果n>1, 说明希望分配多个连续的页面，但是我们换出页面的时候并不能换出连续的页面
         //swap_init_ok标志是否成功初始化了
        if (page != NULL || n > 1 || swap_init_ok == 0) break;

        extern struct mm_struct *check_mm_struct;
        swap_out(check_mm_struct, n, 0);//调用页面置换的”换出页面“接口。这里必有n=1
    }
    return page;
}
```

类似`pmm_manager`, 我们定义`swap_manager`，它封装了一组函数指针接口，用来组合页面置换所需的各种操作：

```c
// kern/mm/swap.h

struct swap_manager
{
     const char *name;
     /* Global initialization for the swap manager */
     int (*init)            (void);
     /* Initialize the priv data inside mm_struct */
     int (*init_mm)         (struct mm_struct *mm);
     /* Called when tick interrupt occured */
     int (*tick_event)      (struct mm_struct *mm);
     /* Called when map a swappable page into the mm_struct */
     int (*map_swappable)   (struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in);
     /* When a page is marked as shared, this routine is called to
      * delete the addr entry from the swap manager */
     int (*set_unswappable) (struct mm_struct *mm, uintptr_t addr);
     /* Try to swap out a page, return then victim */
     int (*swap_out_victim) (struct mm_struct *mm, struct Page **ptr_page, int in_tick);
     /* check the page relpacement algorithm */
     int (*check_swap)(void);
};
```

这里的设计有点类似 **C++ 里的虚函数机制**。`swap_manager` 只定义了接口（函数指针），不关心具体实现。不同的页面置换算法（如 FIFO、LRU 等）只需要提供各自的函数实现，并把这些函数指针赋值到 `swap_manager` 中。当调用 `sm->swap_out_victim(mm, &page, in_tick)` 时，实际执行的是具体算法的版本（比如 FIFO 的实现）。这种基于函数指针的“虚函数表”设计，它让内核可以在运行时灵活切换不同的算法或策略，而无需修改框架代码。

我们来看`swap_in()`, `swap_out()`如何换入/换出一个页面.注意我们对物理页面的 `Page`结构体做了一些改动。

```c
// kern/mm/memlayout.h
struct Page {
    int ref;                        // page frame's reference counter
    uint64_t flags;                 // array of flags that describe the status of the page frame
    unsigned int property;          // the num of free block, used in first fit pm manager
    list_entry_t page_link;         // free list link
    list_entry_t pra_page_link;     // used for pra (page replace algorithm)
    uintptr_t pra_vaddr;            // used for pra (page replace algorithm)
};

// kern/mm/swap.c
int swap_in(struct mm_struct *mm, uintptr_t addr, struct Page **ptr_result)
{
     struct Page *result = alloc_page();//这里alloc_page()内部可能调用swap_out()
     //找到对应的一个物理页面
     assert(result!=NULL);

     pte_t *ptep = get_pte(mm->pgdir, addr, 0);//找到/构建对应的页表项
    //将物理地址映射到虚拟地址是在swap_in()退出之后，调用page_insert()完成的
     int r;
     if ((r = swapfs_read((*ptep), result)) != 0)//将数据从硬盘读到内存
     {
        assert(r!=0);
     }
     cprintf("swap_in: load disk swap entry %d with swap_page in vadr 0x%x\n", (*ptep)>>8, addr);
     *ptr_result=result;
     return 0;
}
int swap_out(struct mm_struct *mm, int n, int in_tick)
{
     int i;
     for (i = 0; i != n; ++i)
     {
          uintptr_t v;
          struct Page *page;
          int r = sm->swap_out_victim(mm, &page, in_tick);//调用页面置换算法的接口
          //r=0表示成功找到了可以换出去的页面
         //要换出去的物理页面存在page里
          if (r != 0) {
                  cprintf("i %d, swap_out: call swap_out_victim failed\n",i);
                  break;
          }

          // cprintf("SWAP: choose victim page 0x%08x\n", page);

          v=page->pra_vaddr;//可以获取物理页面对应的虚拟地址
          pte_t *ptep = get_pte(mm->pgdir, v, 0);
          assert((*ptep & PTE_V) != 0);

          if (swapfs_write( (page->pra_vaddr/PGSIZE+1)<<8, page) != 0) {
                      //尝试把要换出的物理页面写到硬盘上的交换区，返回值不为0说明失败了
                    cprintf("SWAP: failed to save\n");
                    sm->map_swappable(mm, v, page, 0);
                    continue;
          }
          else {
              //成功换出
                    cprintf("swap_out: i %d, store page in vaddr 0x%x to disk swap entry %d\n", i, v, page->pra_vaddr/PGSIZE+1);
                    *ptep = (page->pra_vaddr/PGSIZE+1)<<8;
                    free_page(page);
          }
          //由于页表改变了，需要刷新TLB
          //思考： swap_in()的时候插入新的页表项之后在哪里刷新了TLB?
          tlb_invalidate(mm->pgdir, v);
     }
     return i;
}
```

`kern/mm/swap.c`里其他的接口基本都是简单调用`swap_manager`的具体实现。值得一提的是`swap_init()`初始化里做的工作。

```c
// kern/mm/swap.c
static struct swap_manager *sm;
int swap_init(void)
{
     swapfs_init();

     // Since the IDE is faked, it can only store 7 pages at most to pass the test
     if (!(7 <= max_swap_offset &&
        max_swap_offset < MAX_SWAP_OFFSET_LIMIT))
     {
        panic("bad max_swap_offset %08x.\n", max_swap_offset);
     }

     sm = &swap_manager_fifo;//use first in first out Page Replacement Algorithm
     int r = sm->init();

     if (r == 0)
     {
          swap_init_ok = 1;
          cprintf("SWAP: manager = %s\n", sm->name);
          check_swap();
     }

     return r;
}

int swap_init_mm(struct mm_struct *mm)
{
     return sm->init_mm(mm);
}

int swap_tick_event(struct mm_struct *mm)
{
     return sm->tick_event(mm);
}

int swap_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
     return sm->map_swappable(mm, addr, page, swap_in);
}

int swap_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{
     return sm->set_unswappable(mm, addr);
}
```

在 `kern/mm/swap_fifo.c` 里，我们给 `swap_manager` 填上了 FIFO 算法的实现。核心思想是维护一个队列（用链表实现）：新换入的页面插入到队列尾部，换出时选择队列头部（最早换入）的页面。

```c
// kern/mm/swap_fifo.h
#ifndef __KERN_MM_SWAP_FIFO_H__
#define __KERN_MM_SWAP_FIFO_H__

#include <swap.h>
extern struct swap_manager swap_manager_fifo;

#endif
// kern/mm/swap_fifo.c
/*  Details of FIFO PRA
 * (1) Prepare: In order to implement FIFO PRA, we should manage all swappable pages, so we can
 * link these pages into pra_list_head according the time order. At first you should
 * be familiar to the struct list in list.h. struct list is a simple doubly linked list
 * implementation. You should know howto USE: list_init, list_add(list_add_after),
 * list_add_before, list_del, list_next, list_prev. Another tricky method is to transform
 *  a general list struct to a special struct (such as struct page). You can find some MACRO:
 * le2page (in memlayout.h), (in future labs: le2vma (in vmm.h), le2proc (in proc.h),etc.
 */

list_entry_t pra_list_head;
/*
 * (2) _fifo_init_mm: init pra_list_head and let  mm->sm_priv point to the addr of pra_list_head.
 *     Now, From the memory control struct mm_struct, we can access FIFO PRA
 */
static int
_fifo_init_mm(struct mm_struct *mm)
{
     list_init(&pra_list_head);
     mm->sm_priv = &pra_list_head;
     //cprintf(" mm->sm_priv %x in fifo_init_mm\n",mm->sm_priv);
     return 0;
}
/*
 * (3)_fifo_map_swappable: According FIFO PRA, we should link the most recent arrival page at the back of pra_list_head qeueue
 */
static int
_fifo_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *head=(list_entry_t*) mm->sm_priv;
    list_entry_t *entry=&(page->pra_page_link);

     assert(entry != NULL && head != NULL);
     //record the page access situlation
     //(1)link the most recent arrival page at the back of the pra_list_head qeueue.
     list_add_before(head, entry);
     return 0;
}
/*
 *  (4)_fifo_swap_out_victim: According FIFO PRA, we should unlink the  earliest arrival page in front of pra_list_head qeueue,
 *    then set the addr of addr of this page to ptr_page.
 */
static int
_fifo_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
     list_entry_t *head=(list_entry_t*) mm->sm_priv;
         assert(head != NULL);
      assert(in_tick==0);
      /* Select the victim */
      //(1)  unlink the  earliest arrival page in front of pra_list_head qeueue
      //(2)  set the addr of addr of this page to ptr_page
    list_entry_t* entry = list_next(head);
    list_del(entry);
    *ptr_page = le2page(entry, pra_page_link);
    return 0;
}
static int _fifo_init(void)//初始化的时候什么都不做
{
    return 0;
}

static int _fifo_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{
    return 0;
}

static int _fifo_tick_event(struct mm_struct *mm)//时钟中断的时候什么都不做
{ return 0; }
```

这里通过结构体初始化，把一组函数地址绑定到 `swap_manager_fifo`，以后框架只要调用 `sm->swap_out_victim(...)` 这样的接口，实际上就会跳到 FIFO 的实现函数去执行。换句话说，框架只管调用接口，不关心背后是哪种算法。

```c
// kern/mm/swap_fifo.c
struct swap_manager swap_manager_fifo =
{
     .name            = "fifo swap manager",
     .init            = &_fifo_init,
     .init_mm         = &_fifo_init_mm,
     .tick_event      = &_fifo_tick_event,
     .map_swappable   = &_fifo_map_swappable,
     .set_unswappable = &_fifo_set_unswappable,
     .swap_out_victim = &_fifo_swap_out_victim,
     .check_swap      = &_fifo_check_swap,
};
```

我们通过`_fifo_check_swap()`, `check_swap()`, `check_vma_struct()`, `check_pgfault()`等接口对页面置换机制进行了简单的测试。具体测试的细节在这里不进行展示，同学们可以尝试自己进行测试。

## 内存映射机制的实现

### mmap的基本原理与设计

什么是mmap？简单地说，`mmap`（Memory Map）是一种内存映射机制，它允许将文件或其他对象映射到进程的虚拟地址空间中。通过mmap，进程可以像访问普通内存一样访问文件内容，而不需要使用传统的`read`/`write`系统调用。

在传统的文件I/O模型中，当进程需要读取文件内容时，需要调用`read`系统调用。这个过程涉及多次数据拷贝：首先，内核从磁盘读取数据到内核缓冲区（页缓存）；然后，内核将数据从内核缓冲区拷贝到用户空间的缓冲区。写操作同样需要经历相反的拷贝过程。这种方式存在几个问题：一是每次读写都需要系统调用的开销；二是数据需要在内核空间和用户空间之间拷贝，消耗CPU时间和内存带宽；三是对于随机访问模式，需要频繁调用`lseek`来定位文件位置。

mmap提供了一种不同的思路。它将文件直接映射到进程的虚拟地址空间，使得文件内容可以通过普通的内存访问指令（如load/store）来读写。当进程访问映射区域时，如果对应的物理页面不存在，会触发缺页异常，此时内核才从磁盘读取数据到物理内存，并建立页表映射。这种方式的优势在于：减少了数据拷贝（数据可以直接在用户空间访问，无需从内核缓冲区拷贝）；减少了系统调用次数（只需要在建立和解除映射时调用mmap/munmap）；对于随机访问模式更加友好（可以直接通过指针偏移访问任意位置）。

mmap的核心思想是**延迟加载**（Lazy Loading）和**按需分页**（Demand Paging）。当调用mmap时，内核只是建立虚拟地址到文件的映射关系，并不立即分配物理内存或读取文件内容。只有当进程首次访问映射区域时，才会触发**缺页异常**（Page Fault），此时内核才真正分配物理页面，并从文件中读取相应的数据。这种机制使得即使映射一个很大的文件，也不会立即占用大量物理内存，只有实际访问到的部分才会被加载。

mmap支持两种映射类型：**文件映射**（File-backed Mapping）将文件内容映射到虚拟地址空间，缺页时从文件读取数据，这是mmap最常见的用途；**匿名映射**（Anonymous Mapping）不关联任何文件，缺页时分配零页，常用于动态内存分配（如malloc在分配大块内存时会使用匿名mmap而不是brk）。

在之前的实验中，我们已经使用`vma_struct`来描述虚拟内存区域，`mm_struct`来管理进程的所有VMA。为了支持mmap的文件映射功能，我们需要在`vma_struct`中新增两个字段：

```c
struct vma_struct {
    // ... 原有字段保持不变 ...
    int vm_pgoff;             // 文件映射的页偏移
    struct file *vm_file;     // 映射的文件指针（匿名映射为NULL）
};
```

`vm_file`指向被映射的文件结构体，对于匿名映射则为NULL。`vm_pgoff`记录映射在文件中的起始页偏移，用于在缺页时计算应该从文件的哪个位置读取数据。在`vma_create`函数中，这两个新字段被初始化为空值。

### mmap系统调用的实现

用户态通过`mmap`和`munmap`系统调用来使用内存映射功能。在`user/libs/syscall.c`中，`sys_mmap`接收映射地址、长度、保护标志、映射标志、文件描述符和偏移量等参数，通过`ecall`指令陷入内核。

mmap使用两组标志位来控制映射的行为。保护标志（prot）定义在`libs/unistd.h`中，用于指定映射区域的访问权限：

```c
// libs/unistd.h

/* mmap protection flags */
#define PROT_NONE           0x0         // 页面不可访问
#define PROT_READ           0x1         // 页面可读
#define PROT_WRITE          0x2         // 页面可写
#define PROT_EXEC           0x4         // 页面可执行

/* mmap flags */
#define MAP_SHARED          0x01        // 共享映射，修改对其他进程可见
#define MAP_PRIVATE         0x02        // 私有映射，修改不影响原文件
#define MAP_FIXED           0x10        // 精确使用指定地址
#define MAP_ANONYMOUS       0x20        // 匿名映射，不关联文件
```

内核态的`sysfile_mmap`函数（位于`kern/fs/sysfile.c`）负责处理这个系统调用。它首先将用户态的保护标志转换为内核的VMA标志——`PROT_READ`对应`VM_READ`，`PROT_WRITE`对应`VM_WRITE`，`PROT_EXEC`对应`VM_EXEC`。这些VMA标志定义在`kern/mm/vmm.h`中，用于在缺页处理时设置页表项的权限位。对于文件映射，还需要验证文件描述符的有效性并获取对应的`file`结构体。最后调用`do_mmap`完成实际的映射工作。

### 缺页异常中的mmap处理

在前面的章节中，我们已经介绍了`do_pgfault`函数的整体框架，它负责处理各种类型的缺页异常。对于mmap映射的区域，缺页处理是其中的一个分支。当进程访问mmap映射的区域时，由于物理页面尚未分配，会触发缺页异常。在`do_pgfault`中，通过检查VMA的`vm_file`字段来判断是否为文件映射：如果`vm_file`不为NULL，则进入文件映射的缺页处理分支。

对于文件映射的缺页处理，函数分配一个物理页面，先将其清零，然后计算文件偏移量——文件偏移由VMA的页偏移（`vm_pgoff`）加上当前页相对于映射起始的偏移组成。接着通过`file_seek`定位到正确位置，用`file_read`读取一页数据到物理页面中。为了不影响其他文件操作，读取前后需要保存和恢复文件游标位置。对于匿名映射的缺页处理则与普通的缺页处理相同，只需分配一个物理页面即可，`pgdir_alloc_page`会自动将页面清零。

```c
// kern/mm/vmm.c

int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr) {
    struct vma_struct *vma = find_vma(mm, addr);
    if (vma == NULL || vma->vm_start > addr) {
        return -E_INVAL;
    }

    uint32_t perm = PTE_U | PTE_R;
    if (vma->vm_flags & VM_WRITE) perm |= PTE_W;
    addr = ROUNDDOWN(addr, PGSIZE);

    pte_t *ptep = get_pte(mm->pgdir, addr, 1);

    // 文件映射缺页处理
    if (vma->vm_file != NULL && *ptep == 0) {
        struct Page *page = pgdir_alloc_page(mm->pgdir, addr, perm);
        void *kva = page2kva(page);
        memset(kva, 0, PGSIZE);

        size_t page_index = (addr - vma->vm_start) / PGSIZE;
        off_t file_offset = (vma->vm_pgoff + page_index) * PGSIZE;

        off_t saved_pos = vma->vm_file->pos;
        file_seek(vma->vm_file->fd, file_offset, LSEEK_SET);
        size_t read_size = 0;
        file_read(vma->vm_file->fd, kva, PGSIZE, &read_size);
        file_seek(vma->vm_file->fd, saved_pos, LSEEK_SET);
        return 0;
    }

    // 匿名页缺页处理
    if (*ptep == 0) {
        pgdir_alloc_page(mm->pgdir, addr, perm);
    }
    return 0;
}
```

测试程序`user/testmmap.c`包含了几个测试用例来验证mmap实现的正确性：匿名映射测试创建一页可读可写的匿名内存，写入数据后验证读取结果；多页映射测试创建4页连续内存，在每页写入不同数据来验证跨页访问和按需分页机制；文件映射测试打开一个文件并映射到内存，通过检查ELF文件头来验证文件内容是否正确读取；只读映射测试创建只读的匿名映射，验证读操作的正确性。

本节实现了一个简化版的mmap内存映射机制，通过扩展VMA结构来支持文件映射，采用延迟分配策略在缺页时才分配物理页面。虽然相比Linux的实现省略了共享映射、写时复制、部分解映射等高级特性，但已经展示了mmap的核心原理，为理解操作系统的内存管理机制提供了良好的基础。

## 实验报告要求

1.  基于 `markdown` 格式完成实验报告，以文本说明为主。
2.  说明自己对本章节整体逻辑线的理解，即本章节是按照什么顺序、围绕哪些功能逐步实现的。
3.  说明自己对每个功能的核心函数或功能模块的理解。
4.  说明每个练习的 `spec` 是怎么写的，`prompt` 是怎么写的。
5.  说明在使用提示词让 AI 生成代码的过程中出现了什么 bug，以及你是怎么解决的。
6.  列出你认为本实验中重要的知识点，以及与对应 OS 原理中的知识点，并简要说明二者的含义、关系和差异。
7.  列出你认为 OS 原理中很重要、但在本实验中没有对应上的知识点。

从 oslab 网站上取得实验代码后，进入目录 `labcodes/lab9`，完成实验要求的各个练习。在目录 `labcodes/lab9` 下存放实验报告，推荐使用 `markdown` 格式。每个小组建立一个 gitee 或 github 仓库，对于 lab9 中的编程任务，完成编写之后，再通过 `git push` 命令把代码和报告上传到仓库，最后请提前或按时提交到 git 网站。

注意有“lab9”的注释，代码中所有需要完成的地方（challenge 除外）都有“lab9”和“YOUR CODE”的注释，请在提交时特别注意保持注释，并将“YOUR CODE”替换为自己的学号，并把所有标有对应注释的部分填上正确的代码。
