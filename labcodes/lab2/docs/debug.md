这次调试直接停在 QEMU 8.2.7 的地址翻译函数里。机器上没有 QEMU 4.1.1 的源码树，下面的 TLB 查找对应的是正在跑的这份 8.2.7；4.1.1 里同一职责也在 `accel/tcg/cputlb.c` 的 load/store helper 中。

## 那段 `for` 就是地址翻译

`get_physical_address`（`target/riscv/cpu_helper.c`）里只有一个循环，不是三个循环：

```922:930:qemu-8.2.7/target/riscv/cpu_helper.c
    for (i = 0; i < levels; i++, ptshift -= ptidxbits) {
        target_ulong idx;
        if (i == 0) {
            idx = (addr >> (PGSHIFT + ptshift)) &
                           ((1 << (ptidxbits + widened)) - 1);
        } else {
            idx = (addr >> (PGSHIFT + ptshift)) &
                           ((1 << ptidxbits) - 1);
        }
```

Sv39 在上面的 `switch` 里把 `levels` 设成 3、`ptidxbits` 设成 9、`ptesize` 设成 8。这个循环最多转 3 次，一次对应页表的一级：`i = 0` 用 VPN[2] 查根页表，`i = 1` 用 VPN[1] 查中间页表，`i = 2` 用 VPN[0] 查末级页表。本实验的启动页表每一项都是 1 GiB 的叶子，所以循环体只执行 `i = 0` 这一次，随后就离开循环。

每次迭代先从当前页表算出一项的地址，再把那 8 个字节读出来。这两行就是在读当前页表的一项：

```952:968:qemu-8.2.7/target/riscv/cpu_helper.c
        } else {
            pte_addr = base + idx * ptesize;
        }
        ...
        } else {
            pte = address_space_ldq(cs->as, pte_addr, attrs, &res);
        }
```

`base` 一开始是 `satp` 里的根页表物理地址。`idx` 是这一级的虚页号。`ptesize` 在 RV64 上是 8，所以 `pte_addr` 是「当前页表物理基址 + 下标 × 8」。`address_space_ldq` 按这个物理地址读 64 位，结果放进 `pte`。上面的 `address_space_ldl` 只给 RV32 用。

读出来之后看权限位：

```992:1004:qemu-8.2.7/target/riscv/cpu_helper.c
        if (!(pte & PTE_V)) {
            return TRANSLATE_FAIL;
        }
        if (pte & (PTE_R | PTE_W | PTE_X)) {
            goto leaf;
        }
        /* Inner PTE, continue walking */
        ...
        base = ppn << PGSHIFT;
```

`R|W|X` 任一为 1，这一项就是叶子，`goto leaf`，循环不再继续，后面用 PPN 拼出物理地址。三项都是 0，说明它只是指向下一级页表，于是 `base` 改成下一级页表的物理地址，`i` 加 1，再读一次。

## 调用路径

客户机访存不会一上来就走进这个循环。慢路径的顺序是：

1. `probe_access_internal` 或 `mmu_lookup1`（`accel/tcg/cputlb.c`）用 `tlb_hit_page` 查 QEMU 自己的软件 TLB。
2. 主表没有，再扫一遍很小的 victim TLB。
3. 两处都没有，才调用 `tlb_fill` → `riscv_cpu_tlb_fill` → `get_physical_address`。
4. 翻译成功后 `tlb_set_page` 把结果填回软件 TLB。同一 4 KiB 页上之后的访问命中后，不再进入 `get_physical_address`。

第一次 Sv39 翻译停住时的调用栈是：

```text
get_physical_address          cpu_helper.c:967
riscv_cpu_tlb_fill            cpu_helper.c:1379
probe_access_internal         cputlb.c:1432    ← 这里 tlb_hit_page 失败后才 tlb_fill
get_page_addr_code_hostp
```

`cputlb.c:1430` 就是 miss 判断：

```1430:1433:qemu-8.2.7/accel/tcg/cputlb.c
    if (!tlb_hit_page(tlb_addr, page_addr)) {
        if (!victim_tlb_hit(cpu, mmu_idx, index, access_type, page_addr)) {
            if (!cpu->cc->tcg_ops->tlb_fill(cpu, addr, fault_size, access_type,
                                            mmu_idx, nonfault, retaddr)) {
```

比较本身在 `include/exec/cpu-all.h`：表项里缓存的页地址和这次访问的页地址相同才算命中。

```393:406:qemu-8.2.7/include/exec/cpu-all.h
static inline bool tlb_hit_page(uint64_t tlb_addr, vaddr addr)
{
    return addr == (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK));
}
static inline bool tlb_hit(uint64_t tlb_addr, vaddr addr)
{
    return tlb_hit_page(tlb_addr, addr & TARGET_PAGE_MASK);
}
```

命中并且是普通内存时，helper 用表项里的 `addend` 把客户机虚地址换成宿主机指针，直接读写，不再查页表。更常见的命中甚至不进这些 C 函数：TCG 把同样的比较编进了翻译块，命中时就是几条宿主机指令。所以在 C 里下断点，能看到的是 miss，以及 helper 被调用后的再次查找；快路径命中不会出现在调用栈里。

## 调试里看到的两次翻译

断点打在 Bare 短路（第 877 行）、读 PTE（第 967 行）、判定叶子（第 997 行）和写回物理地址（第 1153 行）。

写 `satp` 之前，OpenSBI 已经把 hart 放在 S 模式，但 `satp = 0`，模式是 Bare。取指 `0x80200000` 和 `0x80208000` 时函数走到：

```876:878:qemu-8.2.7/target/riscv/cpu_helper.c
    case VM_1_10_MBARE:
        *physical = addr;
        *ret_prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
```

这里直接 `return`。`for` 没有执行，没有读页表。物理地址就是虚地址。M 模式还有更早的一处同样处理：`mode == PRV_M` 时第 826 行就把 `*physical = addr` 后返回。

`csrw satp` 之后，`satp = 0x8000000000080207`，根页表物理地址是 `0x80207000`。第一条需要重新翻译的指令还在低地址 `0x80200038`：

| 量 | 值 |
|---|---|
| `i` | 0，只查根页表 |
| `idx` | 2，即 `0x80200038 >> 30` |
| `pte_addr` | `0x80207000 + 2×8 = 0x80207010` |
| `pte` | `0x200000cf`，V、R、W、X、A、D 都在 |
| 结果 | `goto leaf`，`i` 停在 0 |
| 物理地址 | `0x80200038`，恒等映射 |

接着 `jr` 到 `kern_init`，虚地址 `0xffffffffc02000d6`：

| 量 | 值 |
|---|---|
| `i` | 仍然是 0 |
| `idx` | 511，VPN[2] |
| `pte_addr` | `0x80207000 + 511×8 = 0x80207ff8` |
| `pte` | 还是 `0x200000cf`，1 GiB 叶子，PPN = `0x80000` |
| 物理地址 | `0x802000d6` |

偏移是 `0xffffffffc02000d6` 的低 30 位 `0x2000d6`，加上叶子给出的 `0x80000000`，得到 `0x802000d6`。再往后栈地址 `0xffffffffc0206ff8` 又 miss 了一次，读的仍是 `0x80207ff8` 这一项，物理地址是 `0x80206ff8`。三次都是 `i = 0` 就 `goto leaf`，因为根页表项本身已经是叶子，不存在第二、第三级页表。

## 和真实 CPU 的 TLB 差在哪里

真实 CPU 在 S 模式、`satp.MODE=Sv39` 时，每次访存硬件先查 TLB。命中就用其中的 PPN，不再读内存里的页表。未命中才做上面这种三级查找，然后把叶子装进 TLB。`satp` 为 Bare，或者当前是 M 模式时，翻译被旁路，页表和页表 TLB 都不参与，物理地址就是访存地址。`sfence.vma` 丢掉的是这些结构项。1 GiB 的叶子在硬件 TLB 里可以是一条覆盖整个 1 GiB 的表项，所以 `0xffffffffc02000d6` 和 `0xffffffffc0206ff8` 会命中同一条。

QEMU 的这张表是软件哈希表加一个很小的 victim 表，键是客户机虚页，值里除了客户机物理页，还有「加上这个数就得到宿主机指针」的 `addend`。它服务的是 TCG 快路径，不按 RISC-V 的 ASID、组相联和页大小来实现。因此：

- 写 `satp` 之前的 `0x80200000` 仍然会 miss 进 `get_physical_address`。区别只是函数在 Bare 分支把虚地址原样交回去，不进 `for`。真实 CPU 在 Bare 下不会为了这次取指去走页表代码。
- 写 `satp` 之后，miss 才进入上面的三级循环。`0x80200038` 所在的那一页填进软件 TLB 之后，同一页里后面的指令没有再碰到第 967 行。
- 两条高地址相距超过 4 KiB，却对应同一条 1 GiB 叶子。硬件 TLB 可以一条覆盖两者；QEMU 按 4 KiB 填表，所以 `0xffffffffc02000d6` 和 `0xffffffffc0206ff8` 各走了一次页表。第 1128 行的注释也说明了这一点：超大页会被折成软件 TLB 使用的 4 KiB 项。