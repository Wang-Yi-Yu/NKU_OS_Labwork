# Lab2：物理内存和页表

本目录是小组的 lab2 工作区。实验内容是在 lab1 的最小内核基础上，探测物理内存、建立 `struct Page` 描述符，并实现 / 接入物理页分配器（First-Fit → Best-Fit，扩展任务：Buddy System、SLUB）。实验原始指导见 [docs/instruction.md](docs/instruction.md)，实验报告见 [docs/report.md](docs/report.md)，各任务使用的提示词见 [docs/prompt.md](docs/prompt.md)。

> 本 README 约定的是**小组共识**。如果要改动其中的规范，请先在群里确认，再同步修改本文件与对应的配置文件。

## 1. 环境与常用命令

| 工具          | 版本 / 说明                                                                                   |
| ------------- | --------------------------------------------------------------------------------------------- |
| QEMU          | 8.2.x（自带 OpenSBI v1.3）。**不要**退回指导书中的 4.1.1，原因见 [第 5 节](#5-与指导书的差异) |
| 交叉编译器    | `riscv64-unknown-elf-gcc` / `riscv64-unknown-elf-gdb`（需在 `PATH` 中）                       |
| clang-format  | ≥ 15（Ubuntu 24.04 自带 18：`sudo apt install clang-format`）                                 |
| Node.js / npm | 仅 `make format-md` 需要，通过 `npx` 临时拉取 Prettier 3.7.4                                  |

在本目录下执行：

| 命令                  | 作用                                                                    |
| --------------------- | ----------------------------------------------------------------------- |
| `make`                | 编译，生成 `bin/kernel` 与 `bin/ucore.img`                              |
| `make qemu`           | 编译并在 QEMU 中运行（退出：`Ctrl-A` 然后 `X`）                         |
| `make PMM=buddy qemu` | 以 Buddy System 作为物理内存管理器运行（切换前先 `make clean`）         |
| `make debug`          | 以 `-s -S` 启动 QEMU 等待 GDB；另开终端执行 `make gdb` 连接             |
| `make gdb`            | 用 `riscv64-unknown-elf-gdb` 连接 `localhost:1234`（配合 `make debug`） |
| `make grade`          | 在 `/tmp` 的拷贝中注入测试并评分（不会改动工作区源码）                  |
| `make format`         | 格式化全部 C/头文件（clang-format）与 Markdown（Prettier）              |
| `make format-check`   | 只检查格式，不修改文件（提交前自检用）                                  |
| `make clean`          | 删除 `bin/`、`obj/` 等编译产物                                          |

## 2. 复现与验证

以下命令均在本目录（`labcodes/lab2`）下执行，前提是第 1 节的工具已安装。

### 2.1 运行内核

| 目的             | 命令                                | 预期输出（末尾几行）                                                                                                          |
| ---------------- | ----------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| Best-Fit（默认） | `make clean && make qemu`           | `memory management: best_fit_pmm_manager` … `check_alloc_page() succeeded!` … `slub_check() succeeded`                        |
| Buddy System     | `make clean && make PMM=buddy qemu` | `memory management: buddy_pmm_manager`，若干行 `buddy_check: … OK`，`check_alloc_page() succeeded!`，`slub_check() succeeded` |

说明：

- 内核启动完成后进入死循环，不会自行退出；用 `Ctrl-A` 然后 `X` 退出 QEMU。需要在脚本里跑时可以用 `timeout 15 make qemu`（`timeout` 杀掉 QEMU 属正常现象）。
- 切换 `PMM=` 前必须 `make clean`：Makefile 的依赖不跟踪编译选项，不清理会沿用旧的目标文件。
- `kern_init()` 在 `pmm_init()` 之后调用 `slub_init()` 和 `slub_check()`，所以 SLUB 自检是**启动时自动运行**的，使用当前选中的页分配器；两种管理器下都应该看到 `slub_check() succeeded`。任何一处断言失败都会 panic，不会打印这一行。
- 默认管理器是 Best-Fit，因为 `make grade` 的 Test 2 要求 `pmm_manager == &best_fit_pmm_manager`。

### 2.2 评分脚本

```bash
make grade
```

脚本把工作区拷贝到 `/tmp/lab2_grade`，注入 `tools/test_lab2.c` 中的测试后在 QEMU 中运行（工作区源码不会被改动）。预期输出：

```text
  -check page init:                          OK
  -check best fit:                           OK
  -check buddy system:                       OK
Total Score: 30/30
```

评分脚本**不包含 SLUB**，SLUB 的验证看 2.1 中启动时的 `slub_check() succeeded`。

### 2.3 格式与注释检查

```bash
make format-check   # 只检查：clang-format（C/H）+ Prettier（Markdown），应输出 "clang-format: OK" 和 "All matched files use Prettier code style!"
make format         # 自动格式化上述文件
```

`make format-md` 通过 `npx` 临时拉取 Prettier 3.7.4，首次运行需要联网。

Doxygen 注释覆盖情况可以用 `doxygen` 自查（需要自备配置，仓库中不带）：设置 `EXTRACT_ALL = NO`、`EXTRACT_STATIC = YES`、`WARN_IF_UNDOCUMENTED = YES`、`ENABLE_PREPROCESSING = YES`、`MACRO_EXPANSION = NO`、`GENERATE_XML = YES`，`INPUT` 指向 `kern tools`，输出中不应再有 `... is not documented`。注意 `WARN_NO_PARAMDOC` 对 `static` 函数会误报，不要依赖它。

### 2.4 GDB 调试

```bash
make debug    # 终端 1：QEMU 暂停在第一条指令，等待 GDB
make gdb      # 终端 2：连接 localhost:1234，然后 b kern_init / c
```

## 3. 目录结构

> 不含 `.gitignore` 忽略的内容（`bin/`、`obj/`、`docs/private/`、`.gdb.in`、`.qemu.out`、`tags` 等）。

```
.
├── Makefile                 # 构建脚本；含 qemu/debug/gdb/grade/format 等目标
├── README.md                # 本文件
├── .clang-format            # C/H 代码格式规则
├── .prettierrc              # Markdown 格式规则（Prettier 默认值的显式声明）
├── .prettierignore
├── .editorconfig            # 编码、换行、缩进等基础约定
├── .vscode/                 # VS Code 工作区配置（格式化器、IntelliSense 头文件路径、推荐扩展）
├── docs/
│   ├── instruction.md       # 助教提供的实验指导书（只读参考）
│   ├── prompt.md            # 实验中使用的提示词汇总
│   ├── report.md            # 实验报告
│   ├── images/              # 报告用截图
│   └── private/             # 个人笔记（自带 .gitignore，不进入仓库）
├── kern/                    # 内核代码
│   ├── init/
│   │   ├── entry.S          # 内核入口 kern_entry：保存 hartid/dtb、开启 Sv39 分页、设置栈、跳转 kern_init
│   │   └── init.c           # kern_init：清 BSS → dtb_init → 打印内核信息 → pmm_init → slub_init → slub_check
│   ├── driver/
│   │   ├── console.[ch]     # 控制台输入输出（经 SBI）
│   │   └── dtb.[ch]         # 解析设备树，得到物理内存起点与大小（get_memory_base/size）
│   ├── debug/
│   │   ├── assert.h         # assert / panic / warn 宏
│   │   └── panic.c          # __panic / __warn 实现
│   ├── libs/
│   │   └── stdio.c          # cprintf / cputs / getchar 等高层输入输出
│   └── mm/                  # 物理内存管理（本实验主体）
│       ├── memlayout.h      # 虚拟地址空间布局常量；struct Page、页标志位、free_area_t
│       ├── mmu.h            # Sv39 页大小、页号划分、PTE 标志位
│       ├── pmm.[ch]         # pmm_manager 接口、alloc/free_pages、page_init、pmm_init
│       ├── default_pmm.[ch] # First-Fit 分配器（官方实现，作为参考）
│       ├── best_fit_pmm.[ch]# Best-Fit 分配器（练习 2，默认使用的管理器）
│       ├── buddy_pmm.[ch]   # Buddy System 分配器（练习 3，含自检 buddy_check）
│       └── slub.[ch]        # 简化 SLUB 对象分配器（Challenge，建立在当前 pmm_manager 之上，含自检 slub_check）
├── libs/                    # 内核通用库
│   ├── defs.h               # 基础类型、ROUNDUP/ROUNDDOWN、to_struct
│   ├── list.h               # 侵入式双向链表
│   ├── riscv.h              # RISC-V 寄存器 / CSR 常量与内联函数（已关闭自动格式化）
│   ├── sbi.[ch]             # SBI 调用封装
│   ├── stdio.h  string.[ch]  printfmt.c  readline.c  stdarg.h  error.h
└── tools/
    ├── kernel.ld            # 内核链接脚本（链接地址 0xFFFFFFFFC0200000）
    ├── function.mk          # Makefile 辅助函数
    ├── grade.sh             # 评分脚本（make grade 调用）
    ├── inject.py            # 评分时向临时拷贝注入测试代码
    ├── test_lab2.[ch]       # 评分用测试（page_init、Best-Fit、Buddy System；不含 SLUB）
    ├── gdbinit              # GDB 初始命令
    └── boot.ld  kernel_nopage.ld   # lab1 遗留脚本，当前 Makefile 未使用
```

## 4. 代码规范

### 4.1 自动格式化（必须遵守）

- **C / 头文件**：由 [.clang-format](.clang-format) 统一，基于 LLVM 风格，并贴合现有代码：4 空格缩进、K&R 花括号、`struct Page *p`（星号靠右）、`switch` 内 `case` 缩进、行宽 100、不对 `#include` 排序、不重排注释。
- **Markdown**：统一使用 VS Code 扩展 **Prettier**（`esbenp.prettier-vscode`），采用其**默认配置**，已在 [.prettierrc](.prettierrc) 中显式写出，避免个人设置造成差异。
- **编辑器**：VS Code 打开本目录后，[.vscode/settings.json](.vscode/settings.json) 会自动启用 _Format on Save_，并指定 C 使用 `ms-vscode.cpptools`（走 clang-format）、Markdown 使用 Prettier。提示安装推荐扩展时请接受；**不要同时启用 clangd 扩展来格式化 C 代码**，两者会抢格式化器。
- **命令行**：提交前执行 `make format-check`，不通过则执行 `make format`。
- **不要格式化的地方**：
  - `libs/riscv.h` 是体量很大的第三方寄存器头文件，文件内已用 `clang-format off` 排除；
  - 框架提供的测试函数（`default_pmm.c` 的 `basic_check`/`default_check`，`best_fit_pmm.c` 的 `basic_check`/`best_fit_check`）被 `// clang-format off/on` 包住，保证逐字不变，**禁止修改其函数体**；
  - 汇编 `.S` 文件不使用 clang-format，手工保持 4 空格缩进。

### 4.2 注释：Doxygen，使用英文

所有**新增的代码注释一律使用英文**，并采用 Doxygen 格式（`/** ... */`），这样 VS Code 悬浮提示可以直接显示函数说明。

文件头：

```c
/**
 * @file   pmm.c
 * @brief  One-sentence summary of what the file is responsible for.
 *
 * Optional details: relationship with other modules, usage constraints.
 */
```

函数 / 宏 / 结构体：

```c
/**
 * @brief Allocate n contiguous physical pages.
 *
 * Optional details: algorithm, side effects, edge cases (e.g. n == 0).
 *
 * @param[in]  n    number of pages, must be greater than 0
 * @param[out] out  receives ...            (only if there is an output parameter)
 * @return head page of the allocated range, or NULL if memory is insufficient
 * @note / @warning / @see as needed
 */
```

约定：

- 对外可见的函数：**完整文档写在头文件的声明处**；`.c` 的定义处写相同的 `@brief`，如有实现细节再补 `@details`。`static` 函数和没有头文件声明的函数：在定义处写完整文档。
- 结构体成员用 `/**< ... */`；成对的常量 / 标志位可用 `@name` + `@{ ... @}` 分组。
- 描述真实行为，包括边界情况与已知缺陷（用 `@warning`），不要写“该函数用于……”之类的空话。
- 函数体内部的行内注释使用普通 `//` 或 `/* */`，只解释“为什么”。
- 避免在注释里出现 `/*`，以及以 `\` 结尾的 `//` 注释行（会触发 `-Wcomment`，而 `-Werror` 会让编译失败）。
- **实验标记必须保留**：形如 `//lab2 function1 YOUR CODE` 的注释不要删除或改写；提交时按要求把 `YOUR CODE` 替换为学号。

### 4.3 命名与代码约定

- 函数、变量、结构体成员使用 `snake_case`；宏与常量使用 `UPPER_SNAKE_CASE`；类型沿用 ucore 习惯（`struct Page`、`list_entry_t`、`pte_t`）。
- 头文件保护宏形如 `__KERN_MM_PMM_H__`（路径全大写，`/` 与 `.` 换成 `_`，前后各两个下划线）。
- 编译选项为 `-std=gnu99 -Wall -Werror -nostdinc`：**任何警告都会导致编译失败**，提交前务必能干净编译。内核里没有标准库，只能使用 `libs/` 与 `kern/` 提供的函数。
- 严格区分四种“地址”，变量名要体现类型：

  | 名称           | 典型类型        | 含义                                         |
  | -------------- | --------------- | -------------------------------------------- |
  | 虚拟地址 `va`  | `uintptr_t`     | 内核代码实际使用的地址，位于 `KERNBASE` 以上 |
  | 物理地址 `pa`  | `uintptr_t`     | `va - va_pa_offset`，DRAM 从 `0x80000000` 起 |
  | 物理页号 `ppn` | `ppn_t`         | `pa >> PGSHIFT`                              |
  | 页描述符       | `struct Page *` | `pages[]` 的元素，下标为 `ppn - nbase`       |

  转换请用 `PADDR()`、`page2pa()`、`page2ppn()`、`pa2page()`，不要手写移位和偏移。

### 4.4 Git 与文档

- 提交信息沿用现有的前缀风格：`feat:`、`fix:`、`docs:`，后接简短描述，例如 `feat: Lab2 Initialized`。
- 一次提交只做一类事；纯格式化改动请单独提交，不要和功能修改混在一起。
- 提交前确认：`make format-check` 通过、`make` 无警告、`make qemu` 能跑到 `check_alloc_page() succeeded!`。
- `docs/private/` 用于个人草稿，内容已被忽略，不会进入仓库。

## 5. 与指导书的差异

指导书基于 QEMU 4.1.1 / OpenSBI v0.4，而小组使用 QEMU 8.2.x / OpenSBI v1.3，因此做了以下适配（已在补全了参考版 `page_init` 的临时副本中验证：能正常启动，`check_alloc_page()` 通过，`make grade` 的评分流程可以运行）：

1. **`Makefile` 与 `tools/grade.sh`**：启动参数由 `-device loader,file=bin/ucore.img,addr=0x80200000` 改为 `-kernel bin/ucore.img`。OpenSBI v1.3 的 `fw_dynamic` 需要通过 `-kernel` 才能得知内核入口，否则 `Domain0 Next Address` 为 0，内核根本不会被启动（与 lab1 的结论一致）。
2. **`kern/init/entry.S`**：启动页表 `boot_page_table_sv39` 增加了第 2 项，即 `0x80000000` 起 1GiB 的**恒等映射**。原因：`csrw satp` 之后紧接着的指令仍在低物理地址取指。QEMU 4.1 会把这几条指令放在同一个已翻译的 TB 中继续执行，因此只有高地址映射也能跑；QEMU 8.x 会按新页表重新取指，缺少恒等映射就会触发 `instruction page fault`，现象是内核**没有任何输出**。该映射不影响后续实验，内核仍运行在高地址。
3. 设备树解析（`dtb.c`）在 QEMU 8.2 上工作正常：能读到 `Base: 0x80000000`、`Size: 128 MB`。注意 QEMU 8.x 把 DTB 放在 RAM 末尾（约 `0x87e00000`），`page_init` 把它所在页也当作空闲页，这在 `dtb_init` 之后是安全的。
4. 输出中的 OpenSBI 横幅与指导书示例不同（v1.3 打印更多平台信息），属正常现象。

目前**没有**发现必须使用 QEMU 4.1.1 的地方。

## 6. 已知问题（暂未修改）

- `kern/mm/mmu.h`：`SV39_PTSIZE` 宏体里的 `SV39NENTRY` 缺少下划线；`SV39_PTE_ADDR` 沿用 32 位写法，与 Sv39 的 PTE 布局不符。两者当前都没有被使用，后续实验用到时需要修正。
- `kern/mm/pmm.h`：`flush_tlb()` 使用已废弃的 `sfence.vm` 助记符（应为 `sfence.vma`），当前未被调用；`pa2page()` 未检查低于 DRAM 起点的地址。
- `kern/driver/dtb.c`：只取第一个以 `memory` 开头的节点的第一个 `reg`，不处理 `#address-cells` / `#size-cells`。
