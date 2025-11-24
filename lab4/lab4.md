# lab4: 进程管理

在前面的实验中，我们已经完成了物理内存管理和基础页表机制的实现，使内核具备了对物理内存页的分配与管理能力，并能够建立起虚拟地址到物理地址的基本映射结构。本实验将在此基础上进一步扩展，完成以下两方面内容：

1.  首先，通过引入虚拟内存描述结构，管理进程或线程的虚拟地址空间布局，为每个执行实体提供逻辑上的运行空间；
2.  其次，本实验将实现线程控制块、上下文切换和调度器等内容，从而实现多线程并发执行，使内核能够调度多个执行实体轮流使用 CPU 运行。

## 实验目的

  * 了解虚拟内存管理的基本结构，掌握虚拟内存的组织与管理方式
  * 了解内核线程创建/执行的管理过程
  * 了解内核线程的切换和基本调度过程

## 实验内容

在前面的实验中，我们已经完成了物理内存管理和基础页表机制的实现，使内核具备了对物理内存页的分配与管理能力，并能够建立起虚拟地址到物理地址的基本映射结构。但当前系统仍然只能以单一执行流的方式运行，无法并发执行多个任务，也尚未体现虚拟内存机制在进程或线程隔离中的作用。

本实验将在此基础上进一步扩展，完成以下两方面内容：

**首先，进一步完善虚拟内存管理，实现基本的地址空间结构。** 通过引入虚拟内存描述结构，管理进程或线程的虚拟地址空间布局，为每个执行实体提供逻辑上的运行空间。与后续实验中的缺页异常和页面置换不同，本实验中的虚拟内存仍采用预映射方式，即在建立地址空间时一次性完成所有需要的页表映射，不涉及按需分配或页面置换。

**其次，引入内核线程机制，实现多执行流并发运行能力。** 内核线程是一种特殊形式的“进程”。当一个程序加载到内存中运行时，首先通过 ucore OS 的内存管理子系统分配合适的空间，然后就需要考虑如何分时使用 CPU 来“并发”执行多个程序，让每个运行的程序（这里用线程或进程表示）“感到”它们各自拥有“自己”的 CPU。本实验将实现线程控制块、上下文切换和调度器等内容，从而实现多线程并发执行，使内核能够调度多个执行实体轮流使用 CPU 运行。

**内核线程与用户进程的区别如下：**

| 比较项 | 内核线程 | 用户进程 |
| :--- | :--- | :--- |
| **运行模式** | 仅在内核态运行 | 在用户态和内核态之间切换 |
| **地址空间** | 共享内核地址空间 | 拥有独立的用户虚拟地址空间 |

通过本实验，系统将从“单一执行流的内核”发展为“支持多线程调度的内核”，并完成基本的虚拟内存环境框架搭建，为后续实现用户进程、系统调用、缺页异常处理和页面置换等功能奠定基础。

需要注意的是，在 ucore 的调度和执行管理中，对线程和进程做了统一的处理。且由于 ucore 内核中的所有内核线程共享一个内核地址空间和其他资源，所以这些内核线程从属于同一个唯一的内核进程，即 ucore 内核本身。

-----

## 练习

### 对实验报告的要求：

  * 基于 markdown 格式来完成，以文本方式为主
  * 填写各个基本练习中要求完成的报告内容
  * 列出你认为本实验中重要的知识点，以及与对应的 OS 原理中的知识点，并简要说明你对二者的含义，关系，差异等方面的理解（也可能出现实验中的知识点没有对应的原理知识点）
  * 列出你认为 OS 原理中很重要，但在实验中没有对应上的知识点

### 练习0：填写已有实验

本实验依赖实验 2/3。请把你做的实验 2/3 的代码填入本实验中代码中有“LAB2”,“LAB3”的注释相应部分。

### 练习1：分配并初始化一个进程控制块（需要编码）

`alloc_proc` 函数（位于 `kern/process/proc.c` 中）负责分配并返回一个新的 `struct proc_struct` 结构，用于存储新建立的内核线程的管理信息。ucore 需要对这个结构进行最基本的初始化，你需要完成这个初始化过程。

**请在实验报告中简要说明你的设计实现过程。请回答如下问题：**

  * 请说明 `proc_struct` 中 `struct context context` 和 `struct trapframe *tf` 成员变量含义和在本实验中的作用是啥？（提示通过看代码和编程调试可以判断出来）

### 练习2：为新创建的内核线程分配资源（需要编码）

创建一个内核线程需要分配和设置好很多资源。`kernel_thread` 函数通过调用 `do_fork` 函数完成具体内核线程的创建工作。`do_kernel` 函数会调用 `alloc_proc` 函数来分配并初始化一个进程控制块，但 `alloc_proc` 只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore 一般通过 `do_fork` 实际创建新的内核线程。`do_fork` 的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。因此，我们实际需要 "fork" 的东西就是 stack 和 trapframe。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。

你需要完成在 `kern/process/proc.c` 中的 `do_fork` 函数中的处理过程。它的大致执行步骤包括：

1.  调用 `alloc_proc`，首先获得一块用户信息块。
2.  为进程分配一个内核栈。
3.  复制原进程的内存管理信息到新进程（但内核线程不必做此事）
4.  复制原进程上下文到新进程
5.  将新进程添加到进程列表
6.  唤醒新进程
7.  返回新进程号

**请在实验报告中简要说明你的设计实现过程。请回答如下问题：**

  * 请说明 ucore 是否做到给每个新 fork 的线程一个唯一的 id？请说明你的分析和理由。

### 练习3：编写 proc\_run 函数（需要编码）

`proc_run` 用于将指定的进程切换到 CPU 上运行。它的大致执行步骤包括：

1.  检查要切换的进程是否与当前正在运行的进程相同，如果相同则不需要切换。
2.  禁用中断。你可以使用 `/kern/sync/sync.h` 中定义好的宏 `local_intr_save(x)` 和 `local_intr_restore(x)` 来实现关、开中断。
3.  切换当前进程为要运行的进程。
4.  切换页表，以便使用新进程的地址空间。`/libs/riscv.h` 中提供了 `lsatp(unsigned int pgdir)` 函数，可实现修改 SATP 寄存器值的功能。
5.  实现上下文切换。`/kern/process` 中已经预先编写好了 `switch.S`，其中定义了 `switch_to()` 函数。可实现两个进程的 `context` 切换。
6.  允许中断。

**请回答如下问题：**

  * 在本实验的执行过程中，创建且运行了几个内核线程？

完成代码编写后，编译并运行代码：`make qemu`

### 扩展练习 Challenge：

  * 说明语句 `local_intr_save(intr_flag);....local_intr_restore(intr_flag);` 是如何实现开关中断的？

-----

## 深入理解不同分页模式的工作原理（思考题）

`get_pte()` 函数（位于 `kern/mm/pmm.c`）用于在页表中查找或创建页表项，从而实现对指定线性地址对应的物理页的访问和映射操作。这在操作系统中的分页机制下，是实现虚拟内存与物理内存之间映射关系非常重要的内容。

1.  `get_pte()` 函数中有两段形式类似的代码， 结合 sv32，sv39，sv48 的异同，解释这两段代码为什么如此相像。
2.  目前 `get_pte()` 函数将页表项的查找和页表项的分配合并在一个函数里，你认为这种写法好吗？有没有必要把两个功能拆开？

-----

## 项目组成

```text
├── Makefile
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
│   │   ├── intr.c
│   │   ├── intr.h
│   │   ├── kbdreg.h
│   │   ├── picirq.c
│   │   └── picirq.h
│   ├── init
│   │   ├── entry.S
│   │   └── init.c
│   ├── libs
│   │   ├── readline.c
│   │   └── stdio.c
│   ├── mm
│   │   ├── default_pmm.c
│   │   ├── default_pmm.h
│   │   ├── kmalloc.c
│   │   ├── kmalloc.h
│   │   ├── memlayout.h
│   │   ├── mmu.h
│   │   ├── pmm.c
│   │   ├── pmm.h
│   │   ├── vmm.c
│   │   └── vmm.h
│   ├── process
│   │   ├── entry.S
│   │   ├── proc.c
│   │   ├── proc.h
│   │   └── switch.S
│   ├── schedule
│   │   ├── sched.c
│   │   └── sched.h
│   ├── sync
│   │   └── sync.h
│   └── trap
│       ├── trap.c
│       ├── trap.h
│       └── trapentry.S
├── libs
│   ├── atomic.h
│   ├── defs.h
│   ├── elf.h
│   ├── error.h
│   ├── hash.h
│   ├── list.h
│   ├── printfmt.c
│   ├── riscv.h
│   ├── sbi.h
│   ├── stdarg.h
│   ├── stdio.h
│   ├── stdlib.h
│   ├── string.c
│   └── string.h
└── tools
    ├── boot.ld
    ├── function.mk
    ├── gdbinit
    ├── grade.sh
    ├── kernel.ld
    ├── sign.c
    └── vector.c
```

### 相对与实验三，实验四中主要改动如下：

  * **kern/process/ （新增进程管理相关文件）**
      * `proc.[ch]`：新增：实现进程、线程相关功能，包括：创建进程/线程，初始化进程/线程，处理进程/线程退出等功能
      * `entry.S`：新增：内核线程入口函数 `kernel_thread_entry` 的实现
      * `switch.S`：新增：上下文切换，利用堆栈保存、恢复进程上下文
  * **kern/init/**
      * `init.c`：修改：完成虚拟内存管理初始化和进程系统初始化，并在内核初始化后切入 idle 进程
  * **kern/mm/ （基本上与本次实验没有太直接的联系，了解 kmalloc 和 kfree 如何使用即可）**
      * `kmalloc.[ch]`：新增：定义和实现了新的 `kmalloc/kfree` 函数。具体实现是基于 slab 分配的简化算法 （只要求会调用这两个函数即可）
      * `memlayout.h`：增加 slab 物理内存分配相关的定义与宏 （可不用理会）。
      * `pmm.[ch]`：修改：加入完整的页表管理功能（get\_pte/page\_insert/page\_remove 等），实现虚拟内存映射与地址转换；在 `pmm.c` 中添加了调用 `kmalloc_init` 函数,取消了老的 `kmalloc/kfree` 的实现；在 `pmm.h` 中取消了老的 `kmalloc/kfree` 的定义
      * `vmm.[ch]`：新增：定义并实现虚拟内存区域（VMA）管理，包括 `mm_struct`（内存管理结构）和 `vma_struct`（虚拟内存区域结构），提供 VMA 的创建、查找、插入和销毁等功能
  * **kern/trap/**
      * `trapentry.S`：增加了汇编写的函数 `forkrets`，用于 `do_fork` 调用的返回处理。
  * **kern/schedule/**
      * `sched.[ch]`：新增：实现 FIFO 策略的进程调度

-----

## 实验执行流程概述

整个实验过程以 ucore 的总控函数 `init` 为起点。

1.  **初始化阶段**：

      * 首先调用 `pmm_init` 函数完成物理内存管理初始化，建立空闲物理页管理机制，为后续页表建立和线程栈分配提供支持。
      * 接下来，执行中断和异常相关的初始化工作，此过程涉及调用 `pic_init` 和 `idt_init` 函数，用于初始化处理器中断控制器（PIC）和中断描述符表（IDT），与之前的 lab3 中断和异常初始化工作相同。
      * 随后，调用 `vmm_init` 函数进行虚拟内存管理机制的初始化。在此阶段，将建立内核的页表结构，并完成内核地址空间的虚拟地址到物理地址的静态映射。这里仅实现基础的页表映射机制，保证虚拟内存能够正常访问，并不会处理缺页异常，也不涉及将页面换入或换出。通过这些初始化工作，系统完成了内存的虚拟化，建立了基本的内存访问机制。

2.  **进程初始化与调度**：

      * 当内存的虚拟化完成后，整个控制流还是一条线串行执行，需要在此基础上进行 CPU 的虚拟化，即让 ucore 实现分时共享 CPU，使多条控制流能够并发执行。
      * 首先调用 `proc_init` 函数，完成进程管理的初始化，负责创建两个内核线程：第 0 个内核线程 `idleproc` 和第 1 个真正的内核线程 `initproc`。
      * `idleproc` 是系统启动后的占位线程，主要任务是在没有其他可运行线程时进入空闲循环。
      * `initproc` 则通过 `kernel_thread` 创建，是第一个实际执行任务的内核线程。在本实验中，它的任务是输出 “Hello World”，以验证内核线程的创建与调度机制是否正确。
      * 在完成上述初始化后，`idleproc` 会运行 `cpu_idle()`，当检测到需要调度时，系统通过 `schedule()` 选择可运行的线程并进行线程切换，从而让 `initproc` 获得 CPU 执行权并输出 “Hello World”。

下面我们将首先分析如何使用多级页表进行虚拟内存管理，具体分析主要需要注意的关键问题和涉及的关键数据结构。

-----

## 虚拟内存管理

### 基本原理概述

#### 虚拟内存

什么是虚拟内存？简单地说是指程序员或 CPU“看到”的内存。但有几点需要注意：

  * 虚拟内存单元不一定有实际的物理内存单元对应，即实际的物理内存单元可能不存在；
  * 如果虚拟内存单元对应有实际的物理内存单元，那二者的地址一般是不相等的；
  * 通过操作系统实现的某种内存映射可建立虚拟内存与物理内存的对应关系，使得程序员或 CPU 访问的虚拟内存地址会自动转换为一个物理内存地址。

那么这个“虚拟”的作用或意义在哪里体现呢？在操作系统中，虚拟内存其实包含多个虚拟层次，在不同的层次体现了不同的作用。

  * 首先，在有了分页机制后，程序员或 CPU“看到”的地址已经不是实际的物理地址了，这已经有一层虚拟化，我们可简称为**内存地址虚拟化**。有了内存地址虚拟化，我们就可以通过设置页表项来限定软件运行时的访问空间，确保软件运行不越界，完成内存访问保护的功能。
  * 通过内存地址虚拟化，可以使得软件在没有访问某虚拟内存地址时不分配具体的物理内存，而只有在实际访问某虚拟内存地址时，操作系统再动态地分配物理内存，建立虚拟内存到物理内存的页映射关系，这种技术称为**按需分页（demand paging）**。
  * 把不经常访问的数据所占的内存空间临时写到硬盘上，这样可以腾出更多的空闲内存空间给经常访问的数据；当 CPU 访问到不经常访问的数据时，再把这些数据从硬盘读入到内存中，这种技术称为**页换入换出（page swap in/out）**，但是这段我们目前的 ucore 中还没有实现。这种内存管理技术给了程序员更大的内存“空间”，从而可以让更多的程序在内存中并发运行。

### 页表项设计思路

我们定义一些对 sv39 页表项（Page Table Entry）进行操作的宏。这里的定义和我们在 Lab2 里介绍的定义一致。

有时候我们把多级页表中较高级别的页表（“页表的页表”）叫做 Page Directory。在实验二中我们知道 sv39 中采用的是三级页表，那么在这里我们把页表项里从高到低三级页表的页码分别称作 PDX1, PDX0 和 PTX (Page Table Index)。

```c
// kern/mm/mmu.h
#ifndef __KERN_MM_MMU_H__
#define __KERN_MM_MMU_H__

#ifndef __ASSEMBLER__
#include <defs.h>
#endif /* !__ASSEMBLER__ */

// A linear address 'la' has a four-part structure as follows:
//
// +--------9-------+-------9--------+-------9--------+---------12----------+
// | Page Directory | Page Directory |   Page Table   | Offset within Page  |
// |     Index 1    |    Index 2     |                |                     |
// +----------------+----------------+----------------+---------------------+
//  \-- PDX1(la) --/ \-- PDX0(la) --/ \--- PTX(la) --/ \---- PGOFF(la) ----/
//  \-------------------PPN(la)----------------------/

// The PDX1, PDX0, PTX, PGOFF, and PPN macros decompose linear addresses as shown.
// To construct a linear address la from PDX(la), PTX(la), and PGOFF(la),
// use PGADDR(PDX(la), PTX(la), PGOFF(la)).

// RISC-V uses 39-bit virtual address to access 56-bit physical address!
// Sv39 virtual address:
// +----9----+----9---+----9---+---12--+
// |  VPN[2] | VPN[1] | VPN[0] | PGOFF |
// +---------+----+---+--------+-------+
//
// Sv39 physical address:
// +----26---+----9---+----9---+---12--+
// |  PPN[2] | PPN[1] | PPN[0] | PGOFF |
// +---------+----+---+--------+-------+
//
// Sv39 page table entry:
// +----26---+----9---+----9---+---2----+-------8-------+
// |  PPN[2] | PPN[1] | PPN[0] |Reserved|D|A|G|U|X|W|R|V|
// +---------+----+---+--------+--------+---------------+

// page directory index
#define PDX1(la) ((((uintptr_t)(la)) >> PDX1SHIFT) & 0x1FF)
#define PDX0(la) ((((uintptr_t)(la)) >> PDX0SHIFT) & 0x1FF)

// page table index
#define PTX(la) ((((uintptr_t)(la)) >> PTXSHIFT) & 0x1FF)

// page number field of address
#define PPN(la) (((uintptr_t)(la)) >> PTXSHIFT)

// offset in page
#define PGOFF(la) (((uintptr_t)(la)) & 0xFFF)

// construct linear address from indexes and offset
#define PGADDR(d1, d0, t, o) ((uintptr_t)((d1) << PDX1SHIFT | (d0) << PDX0SHIFT | (t) << PTXSHIFT | (o)))

// address in page table or page directory entry
// 把页表项里存储的地址拿出来
#define PTE_ADDR(pte)   (((uintptr_t)(pte) & ~0x3FF) << (PTXSHIFT - PTE_PPN_SHIFT))
#define PDE_ADDR(pde)   PTE_ADDR(pde)

/* page directory and page table constants */
#define NPDEENTRY       512                    // page directory entries per page directory
#define NPTEENTRY       512                    // page table entries per page table
#define PGSIZE          4096                    // bytes mapped by a page
#define PGSHIFT         12                      // log2(PGSIZE)
#define PTSIZE          (PGSIZE * NPTEENTRY)    // bytes mapped by a page directory entry
#define PTSHIFT         21                      // log2(PTSIZE)
#define PTXSHIFT        12                      // offset of PTX in a linear address
#define PDX0SHIFT       21                      // offset of PDX0 in a linear address
#define PDX1SHIFT       30                      // offset of PDX0 in a linear address
#define PTE_PPN_SHIFT   10                      // offset of PPN in a physical address

// page table entry (PTE) fields
#define PTE_V     0x001 // Valid
#define PTE_R     0x002 // Read
#define PTE_W     0x004 // Write
#define PTE_X     0x008 // Execute
#define PTE_U     0x010 // User
```

上述代码定义了一些与内存管理单元（Memory Management Unit, MMU）相关的宏和常量，用于操作线性地址和物理地址，以及页表项的字段。

### 使用多级页表实现虚拟存储

要想实现虚拟存储，我们需要把页表放在内存里，并且需要有办法修改页表，比如在页表里增加一个页面的映射或者删除某个页面的映射。
要想实现页面映射，我们最主要需要修改的是两个接口：

1.  `page_insert()`，在页表里建立一个映射
2.  `page_remove()`，在页表里删除一个映射

这些内容都需要在 `kern/mm/pmm.c` 里面编写。然后我们可以在虚拟内存空间的第一个大大页(Giga Page)中建立一些映射来做测试。通过编写 `page_ref()` 函数用来检查映射关系是否实现，这个函数会返回一个物理页面被多少个虚拟页面所对应。

```c
static void check_pgdir(void) {
    // assert(npage <= KMEMSIZE / PGSIZE);
    // The memory starts at 2GB in RISC-V
    // so npage is always larger than KMEMSIZE / PGSIZE
    assert(npage <= KERNTOP / PGSIZE);
    //boot_pgdir是页表的虚拟地址
    assert(boot_pgdir != NULL && (uint32_t)PGOFF(boot_pgdir) == 0);
    assert(get_page(boot_pgdir, 0x0, NULL) == NULL);
    //get_page()尝试找到虚拟内存0x0对应的页，现在当然是没有的，返回NULL

    struct Page *p1, *p2;
    p1 = alloc_page();//拿过来一个物理页面
    assert(page_insert(boot_pgdir, p1, 0x0, 0) == 0);//把这个物理页面通过多级页表映射到0x0
    pte_t *ptep;
    assert((ptep = get_pte(boot_pgdir, 0x0, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert(page_ref(p1) == 1);

    ptep = (pte_t *)KADDR(PDE_ADDR(boot_pgdir[0]));
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;
    assert(get_pte(boot_pgdir, PGSIZE, 0) == ptep);
    //get_pte查找某个虚拟地址对应的页表项，如果不存在这个页表项，会为它分配各级的页表

    p2 = alloc_page();
    assert(page_insert(boot_pgdir, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert((ptep = get_pte(boot_pgdir, PGSIZE, 0)) != NULL);
    assert(*ptep & PTE_U);
    assert(*ptep & PTE_W);
    assert(boot_pgdir[0] & PTE_U);
    assert(page_ref(p2) == 1);

    assert(page_insert(boot_pgdir, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2);
    assert(page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert((*ptep & PTE_U) == 0);

    page_remove(boot_pgdir, 0x0);
    assert(page_ref(p1) == 1);
    assert(page_ref(p2) == 0);

    page_remove(boot_pgdir, PGSIZE);
    assert(page_ref(p1) == 0);
    assert(page_ref(p2) == 0);

    assert(page_ref(pde2page(boot_pgdir[0])) == 1);
    free_page(pde2page(boot_pgdir[0]));
    boot_pgdir[0] = 0;//清除测试的痕迹

    cprintf("check_pgdir() succeeded!\n");
}
```

在映射关系建立完成后，如何新增一个映射关系和删除一个映射关系也是非常重要的内容，我们来看 `page_insert()`, `page_remove()` 的实现。它们都涉及到调用两个对页表项进行操作的函数：`get_pte()` 和 `page_remove_pte()`。

```c
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm) {
    //pgdir是页表基址(satp)，page对应物理页面，la是虚拟地址
    pte_t *ptep = get_pte(pgdir, la, 1);
    //先找到对应页表项的位置，如果原先不存在，get_pte()会分配页表项的内存
    if (ptep == NULL) {
        return -E_NO_MEM;
    }
    page_ref_inc(page);//指向这个物理页面的虚拟地址增加了一个
    if (*ptep & PTE_V) { //原先存在映射
        struct Page *p = pte2page(*ptep);
        if (p == page) {//如果这个映射原先就有
            page_ref_dec(page);
        } else {//如果原先这个虚拟地址映射到其他物理页面，那么需要删除映射
            page_remove_pte(pgdir, la, ptep);
        }
    }
    *ptep = pte_create(page2ppn(page), PTE_V | perm);//构造页表项
    tlb_invalidate(pgdir, la);//页表改变之后要刷新TLB
    return 0;
}

void page_remove(pde_t *pgdir, uintptr_t la) {
    pte_t *ptep = get_pte(pgdir, la, 0);//找到页表项所在位置
    if (ptep != NULL) {
        page_remove_pte(pgdir, la, ptep);//删除这个页表项的映射
    }
}

//删除一个页表项以及它的映射
static inline void page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep) {
    if (*ptep & PTE_V) {  //(1) check if this page table entry is valid
        struct Page *page = pte2page(*ptep);  //(2) find corresponding page to pte
        page_ref_dec(page);   //(3) decrease page reference
        if (page_ref(page) == 0) {
            //(4) and free this page when page reference reachs 0
            free_page(page);
        }
        *ptep = 0;                  //(5) clear page table entry
        tlb_invalidate(pgdir, la);  //(6) flush tlb
    }
}
```

**寻找(有必要的时候分配)一个页表项 `get_pte`**

```c
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create) {
    /* LAB2 EXERCISE 2: YOUR CODE
     * ... (Comments omitted for brevity) ...
     */
    pde_t *pdep1 = &pgdir[PDX1(la)];//找到对应的Giga Page
    if (!(*pdep1 & PTE_V)) {//如果下一级页表不存在，那就给它分配一页，创造新页表
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL) {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        //我们现在在虚拟地址空间中，所以要转化为KADDR再memset.
        //不管页表怎么构造，我们确保物理地址和虚拟地址的偏移量始终相同，那么就可以用这种方式完成对物理内存的访问。
        *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);//注意这里R,W,X全零
    }
    pde_t *pdep0 = &((pde_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];//再下一级页表
    //这里的逻辑和前面完全一致，页表不存在就现在分配一个
    if (!(*pdep0 & PTE_V)) {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL) {
                return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    //找到输入的虚拟地址la对应的页表项的地址(可能是刚刚分配的)
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
}
```

在 `entry.S` 里，我们虽然构造了一个简单映射使得内核能够运行在虚拟空间上，但是这个映射是比较粗糙的。我们知道一个程序通常含有下面几段：

  * `.text` 段：存放代码，需要是可读、可执行的，但不可写。
  * `.rodata` 段：存放只读数据，顾名思义，需要可读，但不可写亦不可执行。
  * `.data` 段：存放经过初始化的数据，需要可读、可写。
  * `.bss` 段：存放经过零初始化的数据，需要可读、可写。

我们看到各个段需要的访问权限是不同的。但是现在使用一个大大页(Giga Page)进行映射时，它们都拥有相同的权限，那么在现在的映射下，我们甚至可以修改内核 `.text` 段的代码，因为我们通过一个标志位 `W=1` 的页表项就可以完成映射，但这显然会带来安全隐患。
因此，我们考虑对这些段分别进行重映射，使得他们的访问权限可以被正确设置。虽然还是每个段都还是映射以同样的偏移量映射到相同的地方，但实现过程需要更加精细。

这里还有一个小坑：对于我们最开始已经用特殊方式映射的一个大大页(Giga Page)，该怎么对那里面的地址重新进行映射？这个过程比较麻烦。但大家可以基本理解为放弃现有的页表，直接新建一个页表，在新页表里面完成重映射，然后把 satp 指向新的页表，这样就实现了重新映射。

-----

## 内核线程管理

在真正进入本节的讨论前，我们先来谈一谈何为进程，为什么需要进程。

### 进程与线程

在操作系统中，我们经常谈到的两个概念就是进程与线程的概念。这两个概念虽然有许多相似的地方，但也有很多的不同。
我们平时编写的源代码，经过编译器编译就变成了可执行文件，我们管这一类文件叫做程序。而当一个程序被用户或操作系统启动，分配资源，装载进内存开始执行后，它就成为了一个进程。进程与程序之间最大的不同在于进程是一个“正在运行”的实体，而程序只是一个不动的文件。进程包含程序的内容，也就是它的静态的代码部分，也包括一些在运行时在可以体现出来的信息，比如堆栈，寄存器等数据，这些组成了进程“正在运行”的特性。

如果我们只关注于那些“正在运行”的部分，我们就从进程当中剥离出来了线程。一个进程可以对应一个线程，也可以对应很多线程。这些线程之间往往具有相同的代码，共享一块内存，但是却有不同的 CPU 执行状态。相比于线程，进程更多的作为一个资源管理的实体（因为操作系统分配网络等资源时往往是基于进程的），这样线程就作为可以被调度的最小单元，给了调度器更多的调度可能。

### 我们为什么需要进程

进程的一个重要特点在于其可以调度。

1.  **资源管理与调度**：在我们操作系统启动的时候，操作系统相当是一个初始的进程。之后，操作系统会创建不同的进程负责不同的任务。用户可以通过命令行启动进程，从而使用计算机。如果不使用进程，所有的代码可能需要在操作系统编译的时候就打包在一块，安装软件将变成一件非常难的事情。
2.  **多核利用**：从 2000 年开始，CPU 越来越多的使用多核心的设计。操作系统也需要进行相应的调整，以适应这种多核的趋势。使用进程的概念有助于各个进程同时的利用 CPU 的各个核心，这是单进程系统往往做不到的。
3.  **分时复用**：在计算机的远古时代，存在许多“巨无霸”计算机。如果只让这些计算机服务于一个用户，有时候又有点浪费。分时操作系统通过时间片轮转的方法使得多个用户可以“同时”使用计算资源。这个时候，引入进程的概念，成为操作系统调度的单元就显得十分必要了。

### 如何进行进程管理

综合以上可以看出，操作系统的确离不开进程管理。从某种程度上，我们可以把操作系统的控制流看作是一个内核线程。这次将首先接触的是内核线程的管理。内核线程是一种特殊的进程，内核线程与用户进程的区别有两个：

1.  内核线程只运行在内核态而用户进程会在在用户态和内核态交替运行；
2.  所有内核线程直接使用共同的 ucore 内核内存空间，不需为每个内核线程维护单独的内存空间而用户进程需要维护各自的用户内存空间。

从内存空间占用情况这个角度上看，我们可以把线程看作是一种共享内存空间的轻量级进程。

为了管理这些线程，必须设计用于描述线程的数据结构，即**进程控制块**（在这里也可叫做线程控制块）。如果要让内核线程运行，我们首先要创建内核线程对应的进程控制块，还需把这些进程控制块通过链表连在一起，便于随时进行插入，删除和查找操作等进程管理事务。这个链表就是进程控制块链表。然后在通过调度器（scheduler）来让不同的内核线程在不同的时间段占用 CPU 执行，调度器会按照一定策略选择哪个线程获得 CPU 执行权，实现对 CPU 的分时共享。

接下来将主要介绍进程创建所需的重要数据结构 -- 进程控制块 `proc_struct`，以及 ucore 创建并执行内核线程 `idleproc` 和 `initproc` 的两种不同方式。

### 设计关键数据结构

#### 进程控制块

在实验四中，进程管理信息用 `struct proc_struct` 表示，在 `kern/process/proc.h` 中定义如下：

```c
struct proc_struct {
    enum proc_state state;                  // Process state
    int pid;                                // Process ID
    int runs;                               // the running times of Proces
    uintptr_t kstack;                       // Process kernel stack
    volatile bool need_resched;             // bool value: need to be rescheduled to release CPU?
    struct proc_struct *parent;             // the parent process
    struct mm_struct *mm;                   // Process's memory management field
    struct context context;                 // Switch here to run process
    struct trapframe *tf;                   // Trap frame for current interrupt
    uintptr_t pgdir;                        // the base addr of Page Directroy Table(PDT)
    uint32_t flags;                         // Process flag
    char name[PROC_NAME_LEN + 1];           // Process name
    list_entry_t list_link;                 // Process link list 
    list_entry_t hash_link;                 // Process hash list
};
```

下面重点解释一下几个比较重要的成员变量：

  * **`mm`**：这里面保存了内存管理的信息，包括内存映射，虚存管理等内容。
  * **`state`**：进程所处的状态。uCore 中进程状态有四种：分别是 `PROC_UNINIT`、`PROC_SLEEPING`、`PROC_RUNNABLE`、`PROC_ZOMBIE`。
  * **`parent`**：里面保存了进程的父进程的指针。在内核中，只有内核创建的 idle 进程没有父进程，其他进程都有父进程。进程的父子关系组成了一棵进程树。
  * **`context`**：context 中保存了进程执行的上下文，也就是几个关键的寄存器的值。这些寄存器的值用于在进程切换中还原之前进程的运行状态。切换过程的实现在 `kern/process/switch.S`。
  * **`tf`**：tf 里保存了进程的中断帧。当进程从用户空间跳进内核空间的时候，进程的执行状态被保存在了中断帧中（注意这里需要保存的执行状态数量不同于上下文切换）。系统调用可能会改变用户寄存器的值，我们可以通过调整中断帧来使得系统调用返回特定的值。
  * **`pgdir`**：即页目录（Page Directory）的基址。在 RISC-V 架构中，CPU 通过 `satp` 寄存器找到当前页表的根节点，从而进行地址翻译。当进行进程切换时，内核需要将下一个要运行进程的 `pgdir` 值加载到 `satp` 寄存器中。
  * **`kstack`**：每个线程都有一个内核栈，并且位于内核地址空间的不同位置。对于内核线程，该栈就是运行时的程序使用的栈；而对于普通进程，该栈是发生特权级改变的时候使保存被打断的硬件信息用的栈。uCore 在创建进程时分配了 2 个连续的物理页作为内核栈的空间。

为了管理系统中所有的进程控制块，uCore 维护了如下全局变量（位于 `kern/process/proc.c`）：

  * `static struct proc *current`：当前占用 CPU 且处于“运行”状态进程控制块指针。
  * `static struct proc *initproc`：本实验中，指向一个内核线程。本实验以后，此指针将指向第一个用户态进程。
  * `static list_entry_t hash_list[HASH_LIST_SIZE]`：所有进程控制块的哈希表，`proc_struct` 中的成员变量 `hash_link` 将基于 pid 链接入这个哈希表中。
  * `list_entry_t proc_list`：所有进程控制块的双向线性列表，`proc_struct` 中的成员变量 `list_link` 将链接入这个链表中。

#### 进程上下文

在操作系统中，进程上下文指的是进程在某一时刻的运行现场。为了让进程在被打断后能够原样继续，我们需要把当下的 CPU 状态保存下来。直观地理解，可以先把它看作把所有寄存器的值都保存到内存里，稍后再原样恢复回寄存器。等该进程再次被调度时，再把这些值恢复回去，这个过程就叫做上下文的保存与恢复。
进程上下文使用结构体 `struct context` 保存，其中包含了 `ra`，`sp`，`s0~s11` 共 14 个寄存器。

  * *为什么不需要保存所有的寄存器呢？* 因为线程切换在一个函数当中，编译器会自动帮助我们生成保存和恢复调用者保存（caller-saved）寄存器的代码，在实际的进程切换过程中我们只需要保存被调用者保存（callee-saved）寄存器即可。

### 创建并执行内核线程

建立进程控制块（`proc.c` 中的 `alloc_proc` 函数）后，现在就可以通过进程控制块来创建具体的进程/线程了。

#### 创建第 0 个内核线程 idleproc

`idleproc` 是系统启动后的占位线程，主要任务是在没有其他可运行线程时进入空闲循环。
在 `init.c::kern_init` 函数调用了 `proc.c::proc_init` 函数。`proc_init` 函数启动了创建内核线程的步骤。具体步骤如下：

1.  初始化进程链表。
2.  调用 `alloc_proc` 函数获得 `proc_struct` 结构的一块内存块，作为第 0 个进程控制块。
3.  对 `proc` 进行初步初始化。
    ```c
    proc->state = PROC_UNINIT;  // 设置进程为“初始”态
    proc->pid = -1;             // 设置进程pid的未初始化值
    proc->pgdir = boot_pgdir;   // 使用内核页目录表的基址
    ```
4.  `proc_init` 函数对 `idleproc` 内核线程进行进一步初始化：
    ```c
    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->need_resched = 1;
    set_proc_name(idleproc, "idle");
    ```

#### 创建第 1 个内核线程 initproc

第 0 个内核线程主要工作是完成内核中各个子系统的初始化，然后就通过执行 `cpu_idle` 函数开始过退休生活了。所以 uCore 接下来还需创建其他进程来完成各种工作，但 `idleproc` 内核子线程自己不想做，于是就通过调用 `kernel_thread` 函数创建了一个内核线程 `init_main`。
在 uCore 操作系统中，第一个内核线程的创建是通过 `kernel_thread` 函数来实现的。这个函数负责为新的内核线程创建一个初始化好的中断帧，并通过调用 `do_fork` 函数将其转化为一个新的进程。

**kernel\_thread 函数分析：**

```c
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags) {
    // 对trameframe，也就是我们程序的一些上下文进行一些初始化
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));

    // 设置内核线程的参数和函数指针
    tf.gpr.s0 = (uintptr_t)fn; // s0 寄存器保存函数指针
    tf.gpr.s1 = (uintptr_t)arg; // s1 寄存器保存函数参数

    // 设置 trapframe 中的 status 寄存器（SSTATUS）
    // SSTATUS_SPP：Supervisor Previous Privilege（设置为 supervisor 模式，因为这是一个内核线程）
    // SSTATUS_SPIE：Supervisor Previous Interrupt Enable（设置为启用中断，因为这是一个内核线程）
    // SSTATUS_SIE：Supervisor Interrupt Enable（设置为禁用中断，因为我们不希望该线程被中断）
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;

    // 将入口点（epc）设置为 kernel_thread_entry 函数，作用实际上是将pc指针指向它(*trapentry.S会用到)
    tf.epc = (uintptr_t)kernel_thread_entry;

    // 使用 do_fork 创建一个新进程（内核线程），这样才真正用设置的tf创建新进程。
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}
```

`do_fork` 是创建线程的主要函数（见练习 2）。我们尤其关注 `copy_thread` 函数（用于将当前进程的中断帧复制到新进程的内核栈中）：

```c
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE - sizeof(struct trapframe));
    *(proc->tf) = *tf;

    // Set a0 to 0 so a child process knows it's just forked
    proc->tf->gpr.a0 = 0;
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;

    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)(proc->tf);
}
```

在这里我们首先在上面分配的内核栈上分配出一片空间来保存 trapframe。然后，我们将 trapframe 中的 a0 寄存器（返回值）设置为 0，说明这个进程是一个子进程。之后我们将上下文中的 `ra` 设置为了 `forkret` 函数的入口，并且把 trapframe 放在上下文的栈顶。

### 调度并执行内核线程 initproc

在 uCore 执行完 `proc_init` 函数后，就创建好了两个内核线程：`idleproc` 和 `initproc`。此时，uCore 当前的执行现场就是 `idleproc`。`idleproc` 将通过执行 `cpu_idle` 函数主动让出 CPU。

```c
void cpu_idle(void) {
    while (1) {
        if (current->need_resched) {
            schedule();
            ...
```

#### 须知：uCore 中的进程状态

  * `PROC_UNINIT`（未初始化）
  * `PROC_RUNNABLE`（就绪/运行）
  * `PROC_SLEEPING`（睡眠/阻塞）
  * `PROC_ZOMBIE`（僵尸）

uCore 在这里实现的时一个最简单的 FIFO 调度器。`schedule()` 的执行逻辑可以概括为：

1.  将当前内核线程 `current->need_resched` 置为 0。
2.  在 `proc_list` 链表中查找下一个处于 `PROC_RUNNABLE` 状态的线程或进程 `next`。
3.  找到合适的进程后，调用 `proc_run()` 函数，保存当前进程 current 的执行现场，恢复新进程的执行现场，完成进程切换。

`proc_run()` 最终通过 `switch_to` 完成两个执行现场的切换。`switch_to` 函数（位于 `switch.S`）会保存前一个进程的寄存器上下文，恢复新进程的上下文：

```asm
.text
# void switch_to(struct proc_struct* from, struct proc_struct* to)
.globl switch_to
switch_to:
    # save from's registers
    STORE ra, 0*REGBYTES(a0)
    STORE sp, 1*REGBYTES(a0)
    STORE s0, 2*REGBYTES(a0)
    # ... (s1 to s11 saved) ...
    STORE s11, 13*REGBYTES(a0)

    # restore to's registers
    LOAD ra, 0*REGBYTES(a1)
    LOAD sp, 1*REGBYTES(a1)
    LOAD s0, 2*REGBYTES(a1)
    # ... (s1 to s11 loaded) ...
    LOAD s11, 13*REGBYTES(a1)

    ret
```

由于我们在初始化时把上下文的 `ra` 寄存器设定成了 `forkret` 函数的入口，所以这里会返回到 `forkret` 函数。`forkrets` 函数位于 `kern/trap/trapentry.S`：

```asm
    .globl forkrets
forkrets:
    # set stack to this new process's trapframe
    move sp, a0
    j __trapret
```

这里把传进来的参数，也就是进程的中断帧放在了 `sp`，这样在 `__trapret` 中就可以直接从中断帧里面恢复所有的寄存器。我们在初始化的时候，`epc` 寄存器指向的是 `kernel_thread_entry`，`s0` 寄存器里放的是新进程要执行的函数，`s1` 寄存器里放的是传给函数的参数。

```asm
.text
.globl kernel_thread_entry
kernel_thread_entry:        # void kernel_thread(void)
    move a0, s1
    jalr s0

    jal do_exit
```

我们把参数放在了 `a0` 寄存器，并跳转到 `s0` 执行我们指定的函数！这样，一个进程的初始化就完成了。至此，我们实现了基本的进程管理，并且成功创建并切换到了我们的第一个内核进程。