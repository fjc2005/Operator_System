## 实验目的

* 理解操作系统的调度管理机制
* 熟悉 ucore 的系统调度器框架，实现缺省的Round-Robin 调度算法
* 基于调度器框架实现一个(Stride Scheduling)调度算法来替换缺省的调度算法

## 实验内容

在前两章中，我们已经分别实现了内核进程和用户进程，并且让他们正确运行了起来。同时我们也实现了一个简单的调度算法，FIFO调度算法，来对我们的进程进行调度,可通过阅读实验五下的 `kern/schedule/sched.c` 的 `schedule` 函数的实现来了解其FIFO调度策略。但是，单单如此就够了吗？显然，我们可以让ucore支持更加丰富的调度算法，从而满足各方面的调度需求。与实验五相比，实验六专门需要针对处理器调度框架和各种算法进行设计与实现，为此对ucore的调度部分进行了适当的修改，使得 `kern/schedule/sched.c` 只实现调度器框架，而不再涉及具体的调度算法实现。而调度算法在单独的文件（`default_sched.[ch]`）中实现。

在本次实验中，我们在 `init/init.c` 中加入了对 `sched_init` 函数的调用。这个函数主要完成调度器和特定调度算法的绑定。初始化后，我们在调度函数中就可以使用相应的接口，切换你实现的不同的调度算法了。这也是在C语言环境下对于面向对象编程模式的一种模仿。这样之后，我们只需要关注于实现调度类的接口即可，操作系统也同样不关心调度类具体的实现，方便了新调度算法的开发。本次实验，主要是熟悉ucore的系统调度器框架，以及基于此框架实现Round-Robin（RR） 调度算法。然后进一步完成Stride调度算法。

## 练习

### 对实验报告的要求：

* 基于markdown格式来完成，以文本方式为主
* 填写各个基本练习中要求完成的报告内容
* 列出你认为本实验中重要的知识点，以及与对应的OS原理中的知识点，并简要说明你对二者的含义，关系，差异等方面的理解（也可能出现实验中的知识点没有对应的原理知识点）
* 列出你认为OS原理中很重要，但在实验中没有对应上的知识点

### 练习0：填写已有实验

本实验依赖实验2/3/4/5。请把你做的实验2/3/4/5的代码填入本实验中代码中有“LAB2”/“LAB3”/“LAB4”“LAB5”的注释相应部分。并确保编译通过。 注意：为了能够正确执行lab6的测试应用程序，可能需对已完成的实验2/3/4/5的代码进行进一步改进。 由于我们在进程控制块中记录了一些和调度有关的信息，例如Stride、优先级、时间片等等，因此我们需要对进程控制块的初始化进行更新，将调度有关的信息初始化。同时，由于时间片轮转的调度算法依赖于时钟中断，你可能也要对时钟中断的处理进行一定的更新。

### 练习1: 理解调度器框架的实现（不需要编码）

请仔细阅读和分析调度器框架的相关代码，特别是以下两个关键部分的实现：

在完成练习0后，请仔细阅读并分析以下调度器框架的实现：

* **调度类结构体 `sched_class` 的分析**：请详细解释 `sched_class` 结构体中每个函数指针的作用和调用时机，分析为什么需要将这些函数定义为函数指针，而不是直接实现函数。
* **运行队列结构体 `run_queue` 的分析**：比较lab5和lab6中 `run_queue` 结构体的差异，解释为什么lab6的 `run_queue` 需要支持两种数据结构（链表和斜堆）。
* **调度器框架函数分析**：分析 `sched_init()`、`wakeup_proc()` 和 `schedule()` 函数在lab6中的实现变化，理解这些函数如何与具体的调度算法解耦。

对于调度器框架的使用流程，请在实验报告中完成以下分析：

* **调度类的初始化流程**：描述从内核启动到调度器初始化完成的完整流程，分析 `default_sched_class` 如何与调度器框架关联。
* **进程调度流程**：绘制一个完整的进程调度流程图，包括：时钟中断触发、`proc_tick` 被调用、`schedule()` 函数执行、调度类各个函数的调用顺序。并解释 `need_resched` 标志位在调度过程中的作用
* **调度算法的切换机制**：分析如果要添加一个新的调度算法（如stride），需要修改哪些代码？并解释为什么当前的设计使得切换调度算法变得容易。

### 练习2: 实现 Round Robin 调度算法（需要编码）

完成练习0后，建议大家比较一下（可用kdiff3等文件比较软件）个人完成的lab5和练习0完成后的刚修改的lab6之间的区别，分析了解lab6采用RR调度算法后的执行过程。理解调度器框架的工作原理后，请在此框架下实现时间片轮转（Round Robin）调度算法。

注意有“LAB6”的注释，你需要完成 `kern/schedule/default_sched.c` 文件中的 `RR_init`、`RR_enqueue`、`RR_dequeue`、`RR_pick_next` 和 `RR_proc_tick` 函数的实现，使系统能够正确地进行进程调度。代码中所有需要完成的地方都有“LAB6”和“YOUR CODE”的注释，请在提交时特别注意保持注释，将“YOUR CODE”替换为自己的学号，并且将所有标有对应注释的部分填上正确的代码。

提示，请在实现时注意以下细节：

* 链表操作：`list_add_before`、`list_add_after`等。
* 宏的使用：`le2proc(le, member)` 宏等。
* 边界条件处理：空队列的处理、进程时间片耗尽后的处理、空闲进程的处理等。

请在实验报告中完成：

* 比较一个在lab5和lab6都有, 但是实现不同的函数, 说说为什么要做这个改动, 不做这个改动会出什么问题提示: 如 `kern/schedule/sched.c` 里的函数。你也可以找个其他地方做了改动的函数。
* 描述你实现每个函数的具体思路和方法，解释为什么选择特定的链表操作方法。对每个实现函数的关键代码进行解释说明，并解释如何处理边界情况。
* 展示 `make grade` 的输出结果，并描述在 QEMU 中观察到的调度现象。
* 分析 Round Robin 调度算法的优缺点，讨论如何调整时间片大小来优化系统性能，并解释为什么需要在 `RR_proc_tick` 中设置 `need_resched` 标志。
* **拓展思考**：如果要实现优先级 RR 调度，你的代码需要如何修改？当前的实现是否支持多核调度？如果不支持，需要如何改进？

### 扩展练习 Challenge 1: 实现 Stride Scheduling 调度算法（需要编码）

首先需要换掉RR调度器的实现，在 `sched_init` 中切换调度方法。然后根据此文件和后续文档对Stride度器的相关描述，完成Stride调度算法的实现。 注意有“LAB6”的注释，主要是修改 `default_sched_stride_c` 中的内容。代码中所有需要完成的地方都有“LAB6”和“YOUR CODE”的注释，请在提交时特别注意保持注释，将“YOUR CODE”替换为自己的学号，并且将所有标有对应注释的部分填上正确的代码。

后面的实验文档部分给出了Stride调度算法的大体描述。这里给出Stride调度算法的一些相关的资料（目前网上中文的资料比较欠缺）。

* strid-shed paper location
* 也可GOOGLE “Stride Scheduling” 来查找相关资料

请在实验报告中完成：

* 简要说明如何设计实现”多级反馈队列调度算法“，给出概要设计，鼓励给出详细设计
* 简要证明/说明（不必特别严谨，但应当能够”说服你自己“），为什么Stride算法中，经过足够多的时间片之后，每个进程分配到的时间片数目和优先级成正比。
* 请在实验报告中简要说明你的设计实现过程。

### 扩展练习 Challenge 2 ：在ucore上实现尽可能多的各种基本调度算法(FIFO, SJF,...)，并设计各种测试用例，能够定量地分析出各种调度算法在各种指标上的差异，说明调度算法的适用范围。

### 扩展阅读：Linux 的 CFS 调度算法 （感兴趣的同学可以学习并实现，不计入成绩）

在ucore的调度器框架下也可以实现Linux的CFS调度算法。可阅读相关Linux内核书籍或查询网上资料，这里给出CFS调度算法的一些相关的资料。

* CFS 调度器 — Linux 内核文档
* Linux完全公平调度(CFS)深度解剖（安卓流畅度核心）
* 一文搞懂linux cfs调度器

可通过这些相关资料了解CFS的细节，然后大致实现在ucore中。（可以作为Challenge 2的实现）

## 项目组成

```text
lab6
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
│   │   ├── ide.c
│   │   ├── ide.h
│   │   ├── intr.c
│   │   ├── intr.h
│   │   ├── kbdreg.h
│   │   ├── picirq.c
│   │   └── picirq.h
│   ├── fs
│   │   ├── fs.h
│   │   ├── swapfs.c
│   │   └── swapfs.h
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
│   │   ├── swap.c
│   │   ├── swap.h
│   │   ├── swap_fifo.c
│   │   ├── swap_fifo.h
│   │   ├── vmm.c
│   │   └── vmm.h
│   ├── process
│   │   ├── entry.S
│   │   ├── proc.c
│   │   ├── proc.h
│   │   └── switch.S
│   ├── schedule
│   │   ├── default_sched.c
│   │   ├── default_sched.h
│   │   ├── default_sched_stride.c
│   │   ├── sched.c
│   │   └── sched.h
│   ├── sync
│   │   └── sync.h
│   ├── syscall
│   │   ├── syscall.c
│   │   └── syscall.h
│   └── trap
│       ├── trap.c
│       ├── trap.h
│       └── trapentry.S
├── libs
│   ├── atomic.h
│   ├── defs.h
│   ├── elf.h
│   ├── error.h
│   ├── hash.c
│   ├── list.h
│   ├── printfmt.c
│   ├── rand.c
│   ├── riscv.h
│   ├── sbi.h
│   ├── skew_heap.h
│   ├── stdarg.h
│   ├── stdio.h
│   ├── stdlib.h
│   ├── string.c
│   ├── string.h
│   └── unistd.h
├── tools
│   ├── boot.ld
│   ├── function.mk
│   ├── gdbinit
│   ├── grade.sh
│   ├── kernel.ld
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
    │   ├── initcode.S
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
    ├── softint.c
    ├── spin.c
    ├── testbss.c
    ├── waitkill.c
    └── yield.c

16 directories, 105 files

```

相对与实验五，实验六的主要改动简单说明如下：

* `libs/skew_heap.h`:提供了基本的优先队列数据结构，为本次实验提供了抽象数据结构方面的支持。
* `kern/process/proc.[ch]`：`proc.h`中扩展了`proc_struct`的成员变量，用于RR和stride调度算法。`proc.c`中实现了`lab6_set_priority`，用于设置进程的优先级。
* `kern/schedule/{sched.h,sched.c}`: 定义了 ucore 的调度器框架，其中包括相关的数据结构（包括调度器的接口和运行队列的结构），和具体的运行时机制。
* `kern/schedule/{default_sched.h,default_sched.c}`: 具体的 round-robin 算法，在此次实验中你需要填充其中的空白部分以实现一个完整的 RR 调度器。
* `kern/schedule/default_sched_stride_c`: Stride Scheduling调度器的基本框架，在此次实验中你需要填充其中的空白部分以实现一个完整的 Stride 调度器。
* `kern/syscall/syscall.[ch]`: 增加了`sys_gettime`系统调用，便于用户进程获取当前时钟值；增加了`sys_lab6_set_priority`系统调用，便于用户进程设置进程优先级（给`priority.c`用）
* `user/{matrix.c,priority.c,. . . }`: 相关的一些测试用户程序，测试调度算法的正确性，`user`目录下包含但不限于这些程序。在完成实验过程中，建议阅读这些测试程序，以了解这些程序的行为，便于进行调试。

## 调度框架和调度算法设计与实现

### 进程状态

在ucore中，进程有如下几个状态：

* `PROC_UNINIT`：这个状态表示进程刚刚被分配相应的进程控制块，但还没有初始化，需要进一步的初始化才能进入`PROC_RUNNABLE`的状态。
* `PROC_SLEEPING`：这个状态表示进程正在等待某个事件的发生，通常由于等待锁的释放，或者主动交出CPU资源（`do_sleep`）。这个状态下的进程是不会被调度的。
* `PROC_RUNNABLE`：这个状态表示进程已经准备好要执行了，只需要操作系统给他分配相应的CPU资源就可以运行。
* `PROC_ZOMBIE`：这个状态表示进程已经退出，相应的资源被回收（大部分），almost dead。

一个进程的生命周期一般由如下过程组成：

1. 刚刚开始初始化，进程处在`PROC_UNINIT`的状态
2. 进程已经完成初始化，时刻准备执行，进入`PROC_RUNNABLE`状态
3. 在调度的时候，调度器选中该进程进行执行，进程处在running的状态
4.(1) 正在运行的进程由于wait等系统调用被阻塞，进入`PROC_SLEEPING`，等待相应的资源或者信号。
4.(2) 另一种可能是正在运行的进程被外部中断打断，此时进程变为`PROC_RUNNABLE`状态，等待下次被调用
4. 等待的事件发生，进程又变成`PROC_RUNNABLE`状态
5. 重复3~6，直到进程执行完毕，通过exit进入`PROC_ZOMBIE`状态，由父进程对他的资源进行回收，释放进程控制块。至此，这个进程的生命周期彻底结束

下面我们来看一看如何实现内核对于进程的调度。

### 深入理解进程切换

在前面我们已经简单了解过内核启动过程中的进程切换，这一节将再次回顾这些内容，并对其中的一些关键细节进行更清晰的整理。

首先需要明确的是：进程切换究竟在什么情况下会发生？大体可以分为两类情形——主动调度与被动调度。

**主动调度指的是进程自愿放弃 CPU。典型情况包括：**

* 进程主动放弃当前的CPU资源，显式调用 `wait`、`sleep` 等接口表示自己需要等待；
* 进程尝试获取某个不可用的资源（比如尝试获得未被释放的锁，或进行磁盘I/O操作的时候），因此必须阻塞；
* 内核线程在执行过程中基于逻辑需要显式调用 `schedule`。

这类调度都是由执行流主动触发的。在 ucore 中，由于内核线程是不可抢占的，它们无法被其他内核线程强行打断，因此必须依靠这种主动让出 CPU 的方式来避免死锁，维持系统整体的可调度性。类似的主动调度出现在如下位置：

| 函数 | 原因 |
| --- | --- |
| `proc.c/do_exit` | 用户进程退出，主动放弃 CPU |
| `proc.c/do_wait` | 用户进程等待，主动放弃 CPU |
| `proc.c/init_main` | init线程等待其他用户线程结束，并调度 `kswapd` 执行 |
| `proc.c/cpu_idle` | idle线程等待就绪线程，必要时主动调用 `schedule` |
| `sync.c/lock` | 获取锁失败时主动进入等待 |

**与主动调度相对应，被动调度发生在进程未主动让出 CPU 时**。ucore 不信任用户进程会自动停下来，因此需要提供一种机制在用户态执行期间能够及时打断它，实现抢占式调度。被动调度主要通过两类途径完成：

* **系统调用陷入内核态**。
用户进程在执行系统调用时会通过 `ecall` 进入内核态。虽然这是由用户显式触发的，但从调度角度看此时进程并没有主动想放弃 CPU，因此属于被动调度的触发点。内核在系统调用返回前会检查调度标记，从而决定是否切换到其他就绪进程。
* **外部中断（比如时钟中断）将用户态打断**。
用户进程在正常执行过程中可能被外部硬件中断打断，其中最关键的是时钟中断。时钟中断使内核能够周期性获得控制权，用于更新时间片并在必要时触发抢占，以保证用户进程不会无限运行下去。

无论是哪一种情况，用户进程都会通过 trap 机制进入内核态。进入 trap 时，硬件会首先保存用户态的中断帧，CPU 切换到内核态并执行相应的中断或系统调用的处理函数。在处理完这些 trap 逻辑之后，内核会统一检查当前进程是否应当调度，比如时间片是否用尽或是否已被标记为需要调度。如果满足条件，内核便保存当前进程 A 的内核态上下文，切换到进程 B 的内核态。之后 B 将从内核态返回到用户态继续执行。如果将来 B 再次陷入 trap 并触发调度，则控制权可能重新回到 A，使得 A 能从先前的内核态继续执行并最终返回用户态。

综上，ucore 的调度机制同时依赖主动调度与被动调度：前者确保内核线程在不可抢占的环境下仍然保持系统活性，后者确保用户进程能够被及时打断，避免独占 CPU。下一节我们将继续讨论 ucore 调度器的框架和几种基本的调度算法。

### 调度算法框架

调度算法框架实现为一个结构体，其中保存了各个函数指针。通过实现这些函数指针即可实现各个调度算法。结构体的定义如下：

```c
struct sched_class {
    // 调度类的名字
    const char *name;
    // 初始化run queue
    void (*init)(struct run_queue *rq);
    // 把进程放进run queue，这个是run queue的维护函数
    void (*enqueue)(struct run_queue *rq, struct proc_struct *proc);
    // 把进程取出run queue
    void (*dequeue)(struct run_queue *rq, struct proc_struct *proc);
    // 选择下一个要执行的进程
    struct proc_struct *(*pick_next)(struct run_queue *rq);
    // 每次时钟中断调用
    void (*proc_tick)(struct run_queue *rq, struct proc_struct *proc);
};

```

所有的进程被组织成一个`run_queue`数据结构。这个数据结构虽然没有保存在调度类中，但是是由调度类来管理的。目前ucore仅支持单个CPU核心，所以只有一个全局的`run_queue`。

我们在进程控制块中也记录了一些和调度有关的信息：

```c
struct proc_struct {
    // ...
    // 表示这个进程是否需要调度
    volatile bool need_resched;
    // run queue的指针
    struct run_queue *rq;
    // 与这个进程相关的run queue表项
    list_entry_t run_link;
    // 这个进程剩下的时间片
    int time_slice;
    // 以下几个都和Stride调度算法实现有关
    // 这个进程在优先队列中对应的项
    skew_heap_entry_t lab6_run_pool;
    // 该进程的Stride值
    uint32_t lab6_stride;
    // 该进程的优先级
    uint32_t lab6_priority;
};

```

前面的几个成员变量的含义都比较直接，最后面的几个的含义可以参见Stride调度算法。这也是本次lab的实验内容。

结构体`run_queue`实现了运行队列，其内部结构如下：

```c
struct run_queue {
    // 保存着链表头指针
    list_entry_t run_list;
    // 运行队列中的线程数
    unsigned int proc_num;
    // 最大的时间片大小
    int max_time_slice;
    // Stride调度算法中的优先队列
    skew_heap_entry_t *lab6_run_pool;
};

```

有了这些基础，我们就来实现一个最简单的调度算法：Round-Robin调度算法，也叫时间片轮转调度算法。

## RR调度算法实现

时间片轮转调度(Round-Robin Scheduling)算法非常简单。它为每一个进程维护了一个最大运行时间片。当一个进程运行够了其最大运行时间片那么长的时间后，调度器会把它标记为需要调度，并且把它的进程控制块放在队尾，重置其时间片。这种调度算法保证了公平性，每个进程都有均等的机会使用CPU，但是没有区分不同进程的优先级（这个也就是在Stride算法中需要考虑的问题）。

在当前进程的运行过程中，每过一段时间就会触发一次时钟中断，在时钟中断的处理函数中会调用`proc_tick`减少其时间片。当时间片减少为0时，便把当前进程设置为可调度。当scheduler来调度当前进程时，如果当前进程状态依旧为`PROC_RUNNABLE`，便会将当前进程放到rq的最后进行排队，直到排到这个进程时才会再次获取CPU来执行。

下面我们来实现以下时间片轮转算法相对应的调度器接口吧！

首先是enqueue操作。RR算法直接把需要入队的进程放在调度队列的尾端（可以通过调用`list_add_before`实现），并且如果这个进程的剩余时间片为0（刚刚用完时间片被收回CPU），则需要把它的剩余时间片设为最大时间片。最后记得把rq中的进程数目更新。

```c
static void RR_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    // LAB6: YOUR CODE
}

```

dequeue操作非常普通，将相应的项从队列中删除即可（可以通过调用`list_del_init`实现），最后同样记得把rq中的进程数目更新：

```c
static void RR_dequeue(struct run_queue *rq, struct proc_struct *proc) {
    // LAB6: YOUR CODE
}

```

pick_next选取队列头的表项，用`le2proc`函数获得对应的进程控制块，返回：

```c
static struct proc_struct *RR_pick_next(struct run_queue *rq) {
    // LAB6: YOUR CODE
}

```

proc_tick函数在每一次时钟中断调用。在这里，我们需要对当前正在运行的进程的剩余时间片减一。如果在减一后，其剩余时间片为0，那么我们就把这个进程标记为“需要调度”，这样在中断处理完之后内核判断进程是否需要调度的时候就会把它进行调度：

```c
static void RR_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
    // LAB6: YOUR CODE
}

```

至此我们就实现完了和时间片轮转算法相关的所有重要接口。类似于RR算法，我们也可以参照这个方法实现自己的调度算法。本次实验中还需要同学们自己实现Stride调度算法。

## stride调度算法

### 基本思路

【提示】请先看challenge中提到的论文, 理解后在看下面的内容。

考察 round-robin 调度器，在假设所有进程都充分使用了其拥有的 CPU 时间资源的情况下，所有进程得到的 CPU 时间应该是相等的。但是有时候我们希望调度器能够更智能地为每个进程分配合理的 CPU 资源。假设我们为不同的进程分配不同的优先级，则我们有可能希望每个进程得到的时间资源与他们的优先级成正比关系。Stride调度是基于这种想法的一个较为典型和简单的算法。除了简单易于实现以外，它还有如下的特点：

* **可控性**：如我们之前所希望的，可以证明 Stride Scheduling对进程的调度次数正比于其优先级。
* **确定性**：在不考虑计时器事件的情况下，整个调度机制都是可预知和重现的。该算法的基本思想可以考虑如下：为每个runnable的进程设置一个当前状态stride，表示该进程当前的调度权。另外定义其对应的pass值，表示对应进程在调度后，stride 需要进行的累加值。
1. 每次需要调度时，从当前 runnable 态的进程中选择 stride最小的进程调度。
2. 对于获得调度的进程P，将对应的stride加上其对应的步长pass（只与进程的优先权有关系）。
3. 在一段固定的时间之后，回到 2.步骤，重新调度当前stride最小的进程。



可以证明，如果令 `P.pass = BigStride / P.priority` 其中 `P.priority` 表示进程的优先权（大于 1），而 `BigStride` 表示一个预先定义的大常数，则该调度方案为每个进程分配的时间将与其优先级成正比。证明过程我们在这里略去，有兴趣的同学可以在网上查找相关资料。将该调度器应用到ucore 的调度器框架中来，则需要将调度器接口实现如下：

* **init**:
* 初始化调度器类的信息（如果有的话）。
* 初始化当前的运行队列为一个空的容器结构。（比如和RR调度算法一样，初始化为一个有序列表）


* **enqueue**
* 初始化刚进入运行队列的进程 proc的stride属性。
* 将 proc插入放入运行队列中去（注意：这里并不要求放置在队列头部）。


* **dequeue**
* 从运行队列中删除相应的元素。


* **pick next**
* 扫描整个运行队列，返回其中stride值最小的对应进程。
* 更新对应进程的stride值，即`pass = BIG_STRIDE / P->priority; P->stride += pass`。


* **proc tick**:
* 检测当前进程是否已用完分配的时间片。如果时间片用完，应该正确设置进程结构的相关标记来引起进程切换。
* 一个 process 最多可以连续运行 `rq.max_time_slice`个时间片。



在具体实现时，有一个需要注意的地方：stride属性的溢出问题，在之前的实现里面我们并没有考虑 stride 的数值范围，而这个值在理论上是不断增加的，在stride溢出以后，基于stride的比较可能会出现错误。比如假设当前存在两个进程A和B，stride属性采用16位无符号整数进行存储。当前队列中元素如下（假设当前运行的进程已经被重新放置进运行队列中）：

此时应该选择 A 作为调度的进程，而在一轮调度后，队列将如下：

可以看到由于溢出的出现，进程间stride的理论比较和实际比较结果出现了偏差。我们首先在理论上分析这个问题：令`PASS_MAX`为当前所有进程里最大的步进值。则我们可以证明如下结论：对每次Stride调度器的调度步骤中，有其最大的步进值`STRIDE_MAX`和最小的步进值`STRIDE_MIN`之差：
`STRIDE_MAX – STRIDE_MIN <= PASS_MAX`

**提问 1**：如何证明该结论？

有了该结论，在加上之前对优先级有`Priority > 1`限制，我们有`STRIDE_MAX – STRIDE_MIN <= BIG_STRIDE`,于是我们只要将BigStride取在某个范围之内，即可保证对于任意两个 Stride 之差都会在机器整数表示的范围之内。而我们可以通过其与0的比较结构，来得到两个Stride的大小关系。在上例中，虽然在直接的数值表示上 98 < 65535，但是 98 - 65535 的结果用带符号的 16位整数表示的结果为99,与理论值之差相等。所以在这个意义下 98 > 65535。基于这种特殊考虑的比较方法，即便Stride有可能溢出，我们仍能够得到理论上的当前最小Stride，并做出正确的调度决定。

**提问 2**：在 ucore 中，目前Stride是采用无符号的32位整数表示。则BigStride应该取多少，才能保证比较的正确性？

### 使用优先队列实现 Stride Scheduling

在上述的实现描述中，对于每一次pick_next函数，我们都需要完整地扫描来获得当前最小的stride及其进程。这在进程非常多的时候是非常耗时和低效的，有兴趣的同学可以在实现了基于列表扫描的Stride调度器之后比较一下priority程序在Round-Robin及Stride调度器下各自的运行时间。考虑到其调度选择于优先队列的抽象逻辑一致，我们考虑使用优化的优先队列数据结构实现该调度。

优先队列是这样一种数据结构：使用者可以快速的插入和删除队列中的元素，并且在预先指定的顺序下快速取得当前在队列中的最小（或者最大）值及其对应元素。可以看到，这样的数据结构非常符合 Stride 调度器的实现。

本次实验提供了`libs/skew_heap.h`作为优先队列的一个实现，该实现定义相关的结构和接口，其中主要包括：

```c
   // 优先队列节点的结构
   typedef struct skew_heap_entry  skew_heap_entry_t;
   // 初始化一个队列节点
   void skew_heap_init(skew_heap_entry_t *a);
   // 将节点 b 插入至以节点 a 为队列头的队列中去，返回插入后的队列
   skew_heap_entry_t  *skew_heap_insert(skew_heap_entry_t  *a,
                                        skew_heap_entry_t  *b,
                                        compare_f comp);
   // 将节点 b 插入从以节点 a 为队列头的队列中去，返回删除后的队列
      skew_heap_entry_t  *skew_heap_remove(skew_heap_entry_t  *a,
                                           skew_heap_entry_t  *b,
                                           compare_f comp);

```

其中优先队列的顺序是由比较函数`comp`决定的，`sched_stride.c`中提供了`proc_stride_comp_f`比较器用来比较两个stride的大小，你可以直接使用它。当使用优先队列作为Stride调度器的实现方式之后，运行队列结构也需要作相关改变，其中包括：

* `struct run_queue`中的`lab6_run_pool`指针，在使用优先队列的实现中表示当前优先队列的头元素，如果优先队列为空，则其指向空指针（NULL）。
* `struct proc_struct`中的`lab6_run_pool`结构，表示当前进程对应的优先队列节点。本次实验已经修改了系统相关部分的代码，使得其能够很好地适应LAB6新加入的数据结构和接口。而在实验中我们需要做的是用优先队列实现一个正确和高效的Stride调度器，如果用较简略的伪代码描述，则有：

```text
init(rq):
Initialize rq->run_list
Set rq->lab6_run_pool to NULL
Set rq->proc_num to 0

enqueue(rq, proc)
Initialize proc->time_slice
Insert proc->lab6_run_pool into rq->lab6_run_pool
rq->proc_num ++

dequeue(rq, proc)
Remove proc->lab6_run_pool from rq->lab6_run_pool
rq->proc_num --

pick_next(rq)
If rq->lab6_run_pool == NULL, return NULL
Find the proc corresponding to the pointer rq->lab6_run_pool
proc->lab6_stride += BIG_STRIDE / proc->lab6_priority
Return proc

proc_tick(rq, proc):
If proc->time_slice > 0, proc->time_slice --
If proc->time_slice == 0, set the flag proc->need_resched

```

大家可以根据上述伪代码，完成本次challenge。

### stride调度算法执行make qemu的大致输出

```text
$ make qemu
......
check_swap() succeeded!
++ setup timer interrupts
kernel_execve: pid = 2, name = "priority".
Breakpoint
set priority to 6
main: fork ok,now need to wait pids.
set priority to 5
set priority to 4
set priority to 3
set priority to 2
set priority to 1
child pid 7, acc 944000, time 2010 
child pid 6, acc 788000, time 2010 
child pid 5, acc 620000, time 2010 
child pid 4, acc 460000, time 2020 
child pid 3, acc 316000, time 2020 
main: pid 3, acc 316000, time 2020 
main: pid 4, acc 460000, time 2020 
main: pid 5, acc 620000, time 2030 
main: pid 6，acc 788000， time 2030 
main: pid 0, acc 944000, time 2030 
main: wait pids over
stride sched correctresult:1 1 2 2 3 
all user-mode processes have quit. 
init check memory pass.
kernel panic at kern/process proc.c:468:
    initproc exit.

```