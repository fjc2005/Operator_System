遵照您的要求，这是重构后的 Markdown 格式内容：

-----

## 实验目的：

实验3主要讲解的是中断处理机制。操作系统是计算机系统的监管者，必须能对计算机系统状态的突发变化做出反应，这些系统状态可能是程序执行出现异常，或者是突发的外设请求。当计算机系统遇到突发情况时，不得不停止当前的正常工作，应急响应一下，这是需要操作系统来接管，并跳转到对应处理函数进行处理，处理结束后再回到原来的地方继续执行指令。这个过程就是中断处理过程。

### 本章你将学到：

  * riscv 的中断相关知识
  * 中断前后如何进行上下文环境的保存与恢复
  * 处理最简单的断点中断和时钟中断

-----

## 实验内容：

实验3主要讲解的是中断处理机制。通过本章的学习，我们了解了 riscv 的中断处理机制、相关寄存器与指令。我们知道在中断前后需要恢复上下文环境，用一个名为中断帧（TrapFrame）的结构体存储了要保存的各寄存器，并用了很大篇幅解释如何通过精巧的汇编代码实现上下文环境保存与恢复机制。最终，我们通过处理断点和时钟中断验证了我们正确实现了中断机制

-----

## 练习

### 对实验报告的要求：

  * 基于markdown格式来完成，以文本方式为主
  * 填写各个基本练习中要求完成的报告内容
  * 列出你认为本实验中重要的知识点，以及与对应的OS原理中的知识点，并简要说明你对二者的含义，关系，差异等方面的理解（也可能出现实验中的知识点没有对应的原理知识点）
  * 列出你认为OS原理中很重要，但在实验中没有对应上的知识点

### 练习1：完善中断处理 （需要编程）

请编程完善trap.c中的中断处理函数trap，在对时钟中断进行处理的部分填写kern/trap/trap.c函数中处理时钟中断的部分，使操作系统每遇到100次时钟中断后，调用print\_ticks子程序，向屏幕上打印一行文字”100 ticks”，在打印完10行后调用sbi.h中的shut\_down()函数关机。

要求完成问题1提出的相关函数实现，提交改进后的源代码包（可以编译执行），并在实验报告中简要说明实现过程和定时器中断中断处理的流程。实现要求的部分代码后，运行整个系统，大约每1秒会输出一次”100 ticks”，输出10行。

### 扩展练习 Challenge1：描述与理解中断流程

回答：描述ucore中处理中断异常的流程（从异常的产生开始），其中`mov a0，sp`的目的是什么？SAVE\_ALL中寄寄存器保存在栈中的位置是什么确定的？对于任何中断，\_\_alltraps 中都需要保存所有寄存器吗？请说明理由。

### 扩增练习 Challenge2：理解上下文切换机制

回答：在trapentry.S中汇编代码 `csrw sscratch, sp`；`csrrw s0, sscratch, x0`实现了什么操作，目的是什么？save all里面保存了stval scause这些csr，而在restore all里面却不还原它们？那这样store的意义何在呢？

### 扩展练习 Challenge3：完善异常中断

编程完善在触发一条非法指令异常和断点异常，在 kern/trap/trap.c的异常处理函数中捕获，并对其进行处理，简单输出异常类型和异常指令触发地址，即“Illegal instruction caught at 0x(地址)”，“ebreak caught at 0x（地址）”与“Exception type:Illegal instruction"，“Exception type: breakpoint”。

**tips：**

  * 参考资料RV 硬件简要手册-中文 ：重点第 10 章
  * 非法指令可以加在任意位置，比如在通过内联汇编加入，也可以直接修改汇编。但是要注意，什么时候时候异常触发了才会被处理？
  * 查阅参考资料，判断自己触发的异常属于什么类型的，在相应的情况下进行代码修改。

-----

## 项目组成和执行流

### Lab3项目组成

```
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
│   │   ├── intr.c
│   │   └── intr.h
│   ├── init
│   │   ├── entry.S
│   │   └── init.c
│   ├── libs
│   │   └── stdio.c
│   ├── mm
│   │   ├── memlayout.h
│   │   ├── mmu.h
│   │   ├── pmm.c
│   │   └── pmm.h
│   └── trap
│       ├── trap.c
│       ├── trap.h
│       └── trapentry.S
├── lab3.md
├── libs
│   ├── defs.h
│   ├── error.h
│   ├── printfmt.c
│   ├── readline.c
│   ├── riscv.h
│   ├── sbi.c
│   ├── sbi.h
│   ├── stdarg.h
│   ├── stdio.h
│   ├── string.c
│   └── string.h
├── readme.md
└── tools
    ├── function.mk
    ├── gdbinit
    ├── grade.sh
    ├── kernel.ld
    ├── sign.c
    └── vector.c
```

**只介绍新增的或变动较大的文件。**

#### 硬件驱动层

  * **kern/driver/clock.c(h):** 通过OpenSBI的接口, 可以读取当前时间(rdtime), 设置时钟事件(sbi\_set\_timer)，是时钟中断必需的硬件支持。
  * **kern/driver/intr.c(h):** 中断也需要CPU的硬件支持，这里提供了设置中断使能位的接口（其实只封装了一句riscv指令）。

#### 初始化

  * **kern/init/init.c:** 需要调用中断机制的初始化函数。

#### 中断处理

  * **kern/trap/trapentry.S:** 我们把中断入口点设置为这段汇编代码。这段汇编代码把寄存器的数据挪来挪去，进行上下文切换。
  * **kern/trap/trap.c(h):** 分发不同类型的中断给不同的handler, 完成上下文切换之后对中断的具体处理，例如外设中断要处理外设发来的信息，时钟中断要触发特定的事件。中断处理初始化的函数也在这里，主要是把中断向量表(stvec)设置成所有中断都要跳到trapentry.S进行处理。

### 执行流

1.  **内核初始化函数kern\_init()的执行流：** (从kern/init/entry.S进入) -\> 输出一些信息说明正在初始化 -\> 设置中断向量表(stvec)跳转到的地方为kern/trap/trapentry.S里的一个标记 -\>在kern/driver/clock.c设置第一个时钟事件，使能时钟中断-\>设置全局的S mode中断使能位-\> 现在开始不断地触发时钟中断
2.  **产生一次时钟中断的执行流：** set\_sbi\_timer()通过OpenSBI的时钟事件触发一个中断，跳转到kern/trap/trapentry.S的\_\_alltraps标记 -\> 保存当前执行流的上下文，并通过函数调用，切换为kern/trap/trap.c的中断处理函数trap()的上下文，进入trap()的执行流。切换前的上下文作为一个结构体，传递给trap()作为函数参数 -\> kern/trap/trap.c按照中断类型进行分发(trap\_dispatch(), interrupt\_handler())-\>执行时钟中断对应的处理语句，累加计数器，设置下一次时钟中断-\>完成处理，返回到kern/trap/trapentry.S-\>恢复原先的上下文，中断处理结束。

-----

## 中断与中断处理流程

### riscv64 中断介绍

#### 中断概念

中断（interrupt）机制，就是不管CPU现在手里在干啥活，收到“中断”的时候，都先放下来去处理其他事情，处理完其他事情可能再回来干手头的活。

例如，CPU要向磁盘发一个读取数据的请求，由于磁盘速度相对CPU较慢，在“发出请求”到“收到磁盘数据"之间会经过很多时间周期，如果CPU干等着磁盘干活就相当于CPU在磨洋工。因此我们可以让CPU发出读数据的请求后立刻开始干另一件事情。但是，等一段时间之后，磁盘的数据取到了，而CPU在干其他的事情，我们怎么办才能让CPU知道之前发出的磁盘请求已经完成了呢？我们可以让磁盘给CPU一个“中断”，让CPU放下手里的事情来接受磁盘的数据。

再比如，为了保证CPU正在执行的程序不会永远运行下去，我们需要定时检查一下它是否已经运行“超时”。想象有一个程序由于bug进入了死循环，如果CPU一直运行这个程序，那么其他的所有程序都会因为等待CPU资源而无法运行，造成严重的资源浪费。但是检查是否超时，需要CPU执行一段代码，也就是让CPU暂停当前执行的程序。我们不能假设当前执行的程序会主动地定时让出CPU，那么就需要CPU定时“打断”当前程序的执行，去进行一些处理，这通过时钟中断来实现。

从这些描述我们可以看出，中断机制需要软件硬件一起来支持。硬件进行中断和异常的发现，然后交给软件来进行处理。回忆一下组成原理课程中学到的各个控制寄存器以及他们的用途（下一小节会进行简单回顾），这些寄存器构成了重要的硬件/软件接口。由此，我们也可以得到在一般OS中进行中断处理支持的方法：

1.  编写相应的中断处理代码
2.  在启动中正确设置控制寄存器
3.  CPU捕获异常
4.  控制转交给相应中断处理代码进行处理
5.  返回正在运行的程序

#### Trap分类

在RISC-V架构中，Trap是一个总称，指代任何导致控制权从当前执行流转移到陷阱处理程序的事件。Trap分为两类：

  * **中断（Interrupt）：** 异步事件，由外部设备触发，与当前执行的指令无关。例如：定时器中断、磁盘I/O完成中断。
  * **异常（Exception）：** 同步事件，由当前执行的指令触发。例如：访问无效内存地址、执行非法指令、缺页异常，以及执行`ecall`和`ebreak`指令。

由于中断处理需要进行较高权限的操作，中断处理程序一般处于内核态，或者说，处于"比被打断的程序更高的特权级"。注意，在RISCV里，中断(interrupt)和异常(exception)统称为trap。

> **须知**
>
> 在传统操作系统中，我们通常将Trap（陷阱）狭义地定义为主动触发的异常，如系统调用（通过`ecall`指令）和断点（通过`ebreak`指令）。而在RISC-V架构中，Trap被用作所有控制转移事件（包括中断和异常）的总称。
>
> 此外，传统分类中的 **内部中断（软件中断）** 大致对应RISC-V中的异常，而 **外部中断（硬件中断）** 对应RISC-V中的中断。请注意这一术语差异。

### riscv64 权限模式

我们在lab1中简单的提及过特权级，现在我们具体介绍RISC-V 的三个特权级。在现代处理器中，系统通常会区分 **用户态（User Mode）** 和 **特权态（Supervisor/Kernel Mode）**。用户态运行应用程序，权限有限，不能直接访问硬件和关键寄存器。特权态运行操作系统内核，可以控制硬件、管理内存和调度任务。这种分层的目的，是保证系统的安全与稳定：用户程序即使出现错误，也不会直接破坏底层系统。
中断与异常机制负责完成这两种模式之间的切换。例如，当用户态程序发起系统调用，或出现异常/中断时，处理器会切换到特权态，交由内核代码进行处理，处理完成后再返回用户态继续执行。

#### RISC-V 的三个特权级

  * **M 模式（Machine Mode）：** 是 RISC-V 中最高的特权级。在 M 模式下，hart（硬件线程）对内存、I/O 和底层功能有完全的控制权。默认情况下，所有的中断和异常都会首先进入 M 模式处理。M 模式是所有标准 RISC-V 处理器必须实现的特权级，通常用于处理器启动、底层固件（如 OpenSBI）的执行。
  * **S 模式（Supervisor Mode）：** 是支持现代类 Unix 操作系统的特权级。操作系统内核运行在 S 模式，它支持基于页面的虚拟内存机制，使得多任务和内存隔离成为可能。在 Unix 系统中，大多数系统调用和异常都在 S 模式下处理。
  * **U 模式（User Mode）：** 是最低的特权级。用户程序运行在 U 模式，只能执行普通指令，不能直接访问硬件或执行特权操作。当用户程序需要操作系统服务时，必须通过系统调用（`ecall` 指令）陷入到 S 模式。

#### 中断的路由：从默认路径到委托机制

在RISC-V架构中，中断与异常是构建一个健壮、可响应系统的基石。理解其处理机制，关键在于把握一个核心事实：中断或异常可以发生在处理器执行时的任何特权级。这引出了我们首要解答的问题：一个发生在特定特权级的事件，最终会由哪个特权级的代码来处理？答案并非一成不变，而是由RISC-V灵活的中断委托机制与系统软件的设计共同决定的。

默认情况下，RISC-V遵循最保守的安全设计：所有中断与异常都会首先陷入最高特权级—— M 模式。M 模式作为硬件的直接管理者，拥有最终控制权，此设计确保了系统的底层安全。然而，对于运行现代操作系统的场景，让所有中断（例如来自用户态的系统调用或时钟中断）都经由 M 模式处理，会带来不必要的性能开销和灵活性限制。操作系统内核期望能直接管理属于自己的事务。

为解决此问题，RISC-V引入了中断委托机制。M模式可以通过设置两个关键寄存器，将特定的中断与异常处理权"下放"给S模式：

  * **mideleg** (Machine Interrupt Delegation Register)：负责委托中断。例如，将软件中断、定时器中断或外部中断委托给S模式。
  * **medeleg** (Machine Exception Delegation Register)：负责委托异常。例如，将用户态环境调用、非法指令、页错误等异常委托给S模式。

在ucore启动时，OpenSBI固件会进行初始化，将绝大部分S模式与U模式相关的中断和异常委托出去。这意味着，在委托发生后，中断的处理路由发生了根本性变化。现在，我们可以分场景审视一个中断的完整流程：

  * **U模式触发中断：** 这是最常见的情况。当用户程序执行`ecall`指令发起系统调用，或执行指令时发生页错误，该异常会被`medeleg`委托。硬件将直接陷入到S模式，而不再经过M模式。这是操作系统为用户提供服务的主要通道。
  * **S模式触发中断：** 内核自身在运行时也可能遇到两种性质不同的陷阱。第一种是被动陷入，例如设备I/O完成产生的外部中断，或内核代码触发的缺页异常（内核本身一般不引发缺页异常，但是在尝试访问用户空间传入的数据的时候，也可能触发）。如果该中断类型已被委托，则处理流程为S模式陷入到S模式——这被称为“自陷”。第二种是主动陷入，当内核需要访问硬件特权功能（如设置定时器）时，它会主动执行`ecall`指令。此时，由于`ecall`在S模式下执行，特权级将从S模式提升至M模式，由M模式的固件（如OpenSBI）提供服务，处理完毕后再通过`mret`指令返回S模式。
  * **M模式触发中断：** 某些与最底层硬件管理紧密相关的中断，例如M模式的定时器中断或某些安全性事件，通常不会被委托。它们始终在M模式内处理，由固件负责，这与操作系统的常规运行无关。

#### S模式的中断处理框架

一旦中断被路由到S模式，处理器便需要知道如何开始执行处理代码。实际上，RISCV架构有个CSR叫做`stvec`(Supervisor Trap Vector Base Address Register)，即所谓的中断向量表基址。中断向量表的作用就是把不同种类的中断映射到对应的中断处理程序。如果只有一个中断处理程序，那么可以让`stvec`直接指向那个中断处理程序的地址。

> **扩展**
>
> `stvec`会把最低位的两个二进制位用来编码一个"模式"，如果是`00`就说明更高的SXLEN-2个二进制位存储的是唯一的中断处理程序的地址(SXLEN是`stval`寄存器的位数)，如果是`01`说明更高的SXLEN-2个二进制位存储的是中断向量表基址，通过不同的异常原因来索引中断向量表。但是怎样用62个二进制位编码一个64位的地址？RISCV架构要求这个地址是四字节对齐的，总是在较高的62位后补两个0。
>
> 在旧版本的RISCV privileged ISA标准中（1.9.1及以前），RISCV不支持中断向量表，用最后两位数编码一个模式是1.10版本加入的。可以思考一下这个改动如何保证了后向兼容。
>
> 历史版本的ISA手册：
> 1.9.1版本的RISCV privileged architecture手册：
> 4.1.3 Supervisor Trap Vector Base Address Register (stvec) The `stvec` register is an `XLEN-bit` read/write register that holds the base address of the S-mode trap vector. When an exception occurs, the `pc` is set to `stvec`. The `stvec` register is always aligned to a 4-byte boundary

当我们触发中断，进入 S 模式进行处理前，硬件会自动扮演一名忠实的“现场记录员”，为处理程序准备好一份详尽的上下文报告。这份报告由三个关键寄存器构成：

  * **sepc** (Supervisor Exception Program Counter)：它自动保存了被中断指令的虚拟地址。这份记录回答了“事发时程序执行到了哪里？”的问题，是未来恢复执行的关键。
  * **scause** (Supervisor Cause Register)：它记录了一个编码，精确指出了中断或异常的具体原因。例如，是用户态的系统调用，还是指令页错误？或者是外部设备中断？处理程序通过查阅此寄存器来辨别事件性质。
  * **stval** (Supervisor Trap Value)：它提供了与异常相关的附加信息，是重要的"现场证据"。当发生缺页异常时，`stval`会存放导致失败的访存地址；当遇到非法指令时，它可能会记录该指令本身的内容。

凭借这份由硬件自动生成的报告，处理程序便能清晰地了解现场情况，从而做出正确的响应。

#### 使能与屏蔽

在S模式处理中断时，一个关键的设计抉C择是中断的使能状态。有些时候，禁止CPU产生中断很有用，操作系统内核通常希望在执行中断处理程序的过程中，不会被新的中断所打断，就像你在做重要的事情，如操作系统lab的时候，并不想被打断。这保证了内核数据结构的修改等关键操作具有原子性。

所以，`sstatus`(Supervisor Status Register)寄存器里面有一个二进制位`SIE`(supervisor interrupt enable，在RISCV标准里是$2^1$ 对应的二进制位)，数值为0的时候，如果当程序在S态运行，将禁用全部中断。（对于在U态运行的程序，`SIE`这个二进制位的数值没有任何意义），`sstatus`还有一个二进制位`UIE`(user interrupt enable)可以在置零的时候禁止用户态程序产生中断。

实际上，`sstatus`寄存器中还包含多个重要的控制位，比如`SPIE`（Supervisor Previous Interrupt Enable） 会记录进入 S 模式之前的 `SIE` 值。当通过 `trap` 进入 S 模式时，硬件会将 `SPIE` 设置为 `SIE` 的旧值，然后将 `SIE` 清零。在从 S 模式返回时，硬件会将 `SIE` 恢复为 `SPIE` 的值，然后将 `SPIE` 设置为 1。再比如`SPP`（Supervisor Previous Privilege）会记录进入 S 模式之前的特权级。0 表示来自 U 模式，1 表示来自 S 模式。从 S 模式返回时会根据 `SPP` 的值决定返回到哪个特权级。

除了`sstatus`寄存器中的`SIE`控制位，还存在一个`sie`寄存器，即使 `sstatus.SIE` 为 1，也可以通过 `sie` 寄存器屏蔽特定类型的中断。例如，可以只允许时钟中断，而屏蔽软件中断。

#### 特权指令

在整个中断的生命周期中，特权指令扮演着流程控制器的角色，。RISCV支持以下和中断相关的特权指令：

  * **ecall**：这是主动发起trap的指令。它的行为高度依赖于执行它的当前特权级。
      * 在 U 模式下执行`ecall`，会触发一个 `ecall-from-u-mode-exception`，这是应用程序请求操作系统服务的标准方式，路径为 `U → S`。
      * 在S模式下执行`ecall`，则会触发 `ecall-from-s-mode-exception`，这通常用于内核向M模式固件（如OpenSBI）请求服务，路径为 `S → M`。
      * 它本质上是提升特权级的工具。
  * **sret**：这是从S模式返回的指令。它通常用于在完成U模式触发的异常处理后，从S模式返回U模式。它会同时恢复`sepc`和`sstatus.SIE`的状态，将`sepc`赋给`pc`，并将`SIE`恢复为`SPIE`的值。
  * **mret**：与`sret`类似，但用于从M模式陷阱中返回。当M模式处理完中断，或S模式通过`ecall`请求的服务完成后，使用`mret`可以从M模式返回S模式。
  * **ebreak**：这条指令用于触发一个断点异常，通常用于调试。它也会导致控制流跳转到`stvec`指定的中断处理程序，但其目的并非服务请求，而是调试目的。

#### 从 U 模式陷入 S 模式的完整流程

我们最常接触到的便是 U 模式陷入到 S 模式。当一个用户程序运行在 U 模式时，它通常在执行过程中有可能因为某些原因陷入 S 模式。运行在 U 模式的程序如果需要向操作系统请求服务，它会通过执行`ecall`指令发起系统调用，主动将控制权交给内核。同时，定时器也会在指定时间触发时钟中断，这会导致操作系统对当前进程进行时间片轮转调度，决定是否切换进程。另外如果程序发生了异常，比如访问非法内存地址、发生缺页等，系统也会转入内核进行处理。这些事件都触发了相关处理流程，将程序的运行状态从 U 模式转入 S 模式。

1.  **保存`pc`**：首先，保存中断发生时的`pc`值，即程序计数器的值，这个值会被保存在`sepc`寄存器中。对于异常来说，这通常是触发异常的指令地址，而对于中断来说，则是被打断的指令地址。
2.  **记录原因**：然后，记录中断或异常的类型，并将其写入`scause`寄存器。这里的`scause`会告诉系统是中断还是异常，且会给出具体的中断类型。
3.  **保存辅助信息**：接下来，保存相关的辅助信息。如果异常与缺页或访问错误相关，将相关的地址或数据保存到`stval`寄存器，以便中断处理程序在后续处理中使用。
4.  **修改中断使能**：紧接着，保存并修改中断使能状态。将当前的中断使能状态`sstatus.SIE`保存到`sstatus.SPIE`中，并且会将`sstatus.SIE`清零，从而禁用 S 模式下的中断。这是为了保证在处理中断时不会被其他中断打断。
5.  **保存特权级**：然后，保存当前的特权级信息。将当前特权级（即 U 模式，值为 0）保存到`sstatus.SPP`中，并将当前特权级切换到 S 模式。
6.  **跳转**：此时，系统已经进入 S 模式，准备跳转到中断处理程序。将`pc`设置为`stvec`寄存器中的值，并跳转到中断处理程序的入口。

进入中断处理程序后，系统会执行一系列处理步骤，下面我们就去看看在做些什么。

### 掉进兔子洞(中断入口点)

在前面我们已经了解了RISC-V中断机制的基本原理：当中断发生时，会保存`sepc`、设置`scause`、跳转到`stvec`指定的地址等。我们也知道了从 U 模式到 S 模式的完整切换流程。现在我们来解决如何实现中断入口点，以及完成完整的上下文切换。

首先，我们需要初始化`stvec`寄存器。我们采用Direct模式，也就是`stvec`直接指向唯一的中断处理程序入口点，所有类型的中断和异常都会跳转到这里。

中断的处理需要放下当前的事情但之后还能回来接着之前往下做，对于CPU来说，实际上只需要把原先的寄存器保存下来，做完其他事情把寄存器恢复回来就可以了。这些寄存器也被叫做CPU的context(上下文，情境)。我们要用汇编实现上下文切换(context switch)机制，这包含两步：

1.  保存CPU的寄存器（上下文）到内存中（栈上）
2.  从内存中（栈上）恢复CPU的寄存器

为了方便我们组织上下文的数据（几十个寄存器），我们定义一个结构体。

`sscratch`寄存器在处理用户态程序的中断时才起作用。在目前其实用处不大。

> **须知：RISCV汇编的通用寄存器别名和含义**
>
> *The RISC-V Instruction Set Manual Volume I: Unprivileged ISA Document Version 20191213*
> *Chapter 25 RISC-V Assembly Programmer’s Handbook*
>
> | Register | ABI Name | Description | Saver |
> | :--- | :--- | :--- | :--- |
> | x0 | zero | Hard-wired zero | — |
> | x1 | ra | Return address | Caller |
> | x2 | sp | Stack pointer | Callee |
> | x3 | gp | Global pointer | — |
> | x4 | tp | Thread pointer | — |
> | x5 | t0 | Temporary/alternate link register | Caller |
> | x6–7 | t1–2 | Temporaries | Caller |
> | x8 | s0/fp | Saved register/frame pointer | Callee |
> | x9 | s1 | Saved register | Callee |
> | x10–11 | a0–1 | Function arguments/return values | Caller |
> | x12–17 | a2–7 | Function arguments | Caller |
> | x18–27 | s2–11 | Saved registers | Callee |
> | x28–31 | t3–6 | Temporaries | Caller |

```c
// kern/trap/trap.h
#ifndef __KERN_TRAP_TRAP_H__
#define __KERN_TRAP_TRAP_H__

#include <defs.h>

struct pushregs {
    uintptr_t zero;  // Hard-wired zero
    uintptr_t ra;    // Return address
    uintptr_t sp;    // Stack pointer
    uintptr_t gp;    // Global pointer
    uintptr_t tp;    // Thread pointer
    uintptr_t t0;    // Temporary
    uintptr_t t1;    // Temporary
    uintptr_t t2;    // Temporary
    uintptr_t s0;    // Saved register/frame pointer
    uintptr_t s1;    // Saved register
    uintptr_t a0;    // Function argument/return value
    uintptr_t a1;    // Function argument/return value
    uintptr_t a2;    // Function argument
    uintptr_t a3;    // Function argument
    uintptr_t a4;    // Function argument
    uintptr_t a5;    // Function argument
    uintptr_t a6;    // Function argument
    uintptr_t a7;    // Function argument
    uintptr_t s2;    // Saved register
    uintptr_t s3;    // Saved register
    uintptr_t s4;    // Saved register
    uintptr_t s5;    // Saved register
    uintptr_t s6;    // Saved register
    uintptr_t s7;    // Saved register
    uintptr_t s8;    // Saved register
    uintptr_t s9;    // Saved register
    uintptr_t s10;   // Saved register
    uintptr_t s11;   // Saved register
    uintptr_t t3;    // Temporary
    uintptr_t t4;    // Temporary
    uintptr_t t5;    // Temporary
    uintptr_t t6;    // Temporary
};

struct trapframe {
    struct pushregs gpr;
    uintptr_t status; //sstatus
    uintptr_t epc; //sepc
    uintptr_t badvaddr; //sbadvaddr
    uintptr_t cause; //scause
};

void trap(struct trapframe *tf);

#endif /* !__KERN_TRAP_TRAP_H__ */
```

C语言里面的结构体，是若干个变量在内存里直线排列。也就是说，一个`trapFrame`结构体占据36个`uintptr_t`的空间（在64位RISCV架构里我们定义`uintptr_t`为64位无符号整数），里面依次排列通用寄存器x0到x31,然后依次排列4个和中断相关的CSR, 我们希望中断处理程序能够利用这几个CSR的数值。

首先我们定义一个汇编宏 `SAVE_ALL`, 用来保存所有寄存器到栈顶（实际上把一个`trapFrame`结构体放到了栈顶）。

```assembly
# kern/trap/trapentry.S
#include <riscv.h>

    .macro SAVE_ALL #定义汇编宏

    csrw sscratch, sp #保存原先的栈顶指针到sscratch

    addi sp, sp, -36 * REGBYTES #REGBYTES是riscv.h定义的常量，表示一个寄存器占据几个字节
    #让栈顶指针向低地址空间延伸 36个寄存器的空间，可以放下一个trapFrame结构体。
    #除了32个通用寄存器，我们还要保存4个和中断有关的CSR

    #依次保存32个通用寄存器。但栈顶指针需要特殊处理。
    #因为我们想在trapFrame里保存分配36个REGBYTES之前的sp
    #也就是保存之前写到sscratch里的sp的值
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
    # RISCV不能直接从CSR写到内存, 需要csrr把CSR读取到通用寄存器，再从通用寄存器STORE到内存
    csrrw s0, sscratch, x0
    csrr s1, sstatus
    csrr s2, sepc
    csrr s3, sbadaddr
    csrr s4, scause

    STORE s0, 2*REGBYTES(sp)
    STORE s1, 32*REGBYTES(sp)
    STORE s2, 33*REGBYTES(sp)
    STORE s3, 34*REGBYTES(sp)
    STORE s4, 35*REGBYTES(sp)
    .endm #汇编宏定义结束
```

然后是恢复上下文的汇编宏，恢复的顺序和当时保存的顺序反过来，先加载两个CSR, 再加载通用寄存器。

```assembly
# kern/trap/trapentry.S
.macro RESTORE_ALL

LOAD s1, 32*REGBYTES(sp)
LOAD s2, 33*REGBYTES(sp)

# 注意之前保存的几个CSR并不都需要恢复
csrw sstatus, s1
csrw sepc, s2

# 恢复sp之外的通用寄存器，这时候还需要根据sp来确定其他寄存器数值保存的位置
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
# 最后恢复sp
LOAD x2, 2*REGBYTES(sp)
.endm
```

现在我们编写真正的中断入口点

```assembly
    .globl __alltraps

.align(2) #中断入口点 __alltraps必须四字节对齐
__alltraps:
    SAVE_ALL #保存上下文

    move  a0, sp #传递参数。
    #按照RISCV calling convention, a0寄存器传递参数给接下来调用的函数trap。
    #trap是trap.c里面的一个C语言函数，也就是我们的中断处理程序
    jal trap 
    #trap函数指向完之后，会回到这里向下继续执行__trapret里面的内容，RESTORE_ALL,sret

    .globl __trapret
__trapret:
    RESTORE_ALL
    # return from supervisor call
    sret
```

我们可以看到，`trapentry.S`作用是一个"包装器"：它负责保存和恢复上下文，并把上下文包装成结构体，传递给真正的中断处理函数`trap`那里去。

在上面的代码中，我们看到最后一条指令是`sret`。这是一条特权指令，用于从S模式返回，完成从内核态到用户态的切换。

在执行`sret`之前，需要完成一些准备工作。

1.  首先，从`trapframe`中恢复用户程序的寄存器值（这由`RESTORE_ALL`宏完成），使得用户程序能够继续运行。
2.  接着，根据中断或者异常的类型重新设置`sepc`，确保程序能够从正确的地址继续执行。对于系统调用，这通常是 `ecall`指令的下一条指令地址（即`sepc + 4`）；对于中断，这是被中断打断的指令地址（即`sepc`）；对于进程切换，这是新进程的起始地址。
3.  然后，将`sstatus.SPP`设置为 0，表示要返回到 U 模式。

当准备工作完成后，会执行`sret`指令，

1.  根据`sstatus.SPP`的值（此时为 0）切换回 U 模式。
2.  随后，恢复中断使能状态，将`sstatus.SIE`恢复为`sstatus.SPIE`的值。由于在 U 模式下总是使能中断，因此中断会重新开启。
3.  接着，更新`sstatus`，将`sstatus.SPIE`设置为 1,`sstatus.SPP`设置为 0，为下一次中断做准备。
4.  最后，将`sepc`的值赋给`pc`，并跳转回用户程序（`sepc`指向的地址）继续执行。此时，系统已经安全地从 S 模式返回到 U 模式，用户程序继续执行。

接下来，我们将详细介绍`trap`函数的实现，看看如何处理各种中断和异常。

### 中断处理程序

中断处理需要初始化，所以我们在`init.c`里调用一些初始化的函数

```c
// kern/init/init.c
#include <trap.h>

int kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);
    cons_init();  // init the console
    const char *message = "(THU.CST) os is loading ...\0";
    //cprintf("%s\n\n", message);
    cputs(message);
    print_kerninfo();
    // grade_backtrace();
    idt_init();  // init interrupt descriptor table
    pmm_init();  // init physical memory management
    idt_init();  // init interrupt descriptor table
    clock_init();   // init clock interrupt
    intr_enable();  // enable irq interrupt
    // LAB3: CAHLLENGE 1 If you try to do it, uncomment lab3_switch_test()
    // user/kernel mode switch test
    // lab3_switch_test();

    /* do nothing */
    while (1)
        ;
}
```

```c
// kern/trap/trap.c
void idt_init(void) {
    extern void __alltraps(void);
    //约定：若中断前处于S态，sscratch为0
    //若中断前处于U态，sscratch存储内核栈地址
    //那么之后就可以通过sscratch的数值判断是内核态产生的中断还是用户态产生的中断
    //我们现在是内核态所以给sscratch置零
    write_csr(sscratch, 0);
    //我们保证__alltraps的地址是四字节对齐的，将__alltraps这个符号的地址直接写到stvec寄存器
    write_csr(stvec, &__alltraps);
}
```

```c
//kern/driver/intr.c
#include <intr.h>
#include <riscv.h>

/* intr_enable - enable irq interrupt, 设置sstatus的Supervisor中断使能位 */
void intr_enable(void) { set_csr(sstatus, SSTATUS_SIE); }
/* intr_disable - disable irq interrupt */
void intr_disable(void) { clear_csr(sstatus, SSTATUS_SIE); }
```

`trap.c`的中断处理函数`trap`, 实际上把中断处理,异常处理的工作分发给了`interrupt_handler()`，`exception_handler()`, 这些函数再根据中断或异常的不同类型来处理。

```c
// kern/trap/trap.c
/* trap_dispatch - dispatch based on what type of trap occurred */
static inline void trap_dispatch(struct trapframe *tf) {
    //scause的最高位是1，说明trap是由中断引起的
    if ((intptr_t)tf->cause < 0) {
        // interrupts
        interrupt_handler(tf);
    } else {
        // exceptions
        exception_handler(tf);
    }
}

/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap()
 * returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void trap(struct trapframe *tf) { trap_dispatch(tf); }
```

我们可以看到，`interrupt_handler()`和`exception_handler()`的实现还比较简单，只是简单地根据`scause`的数值更仔细地分了下类，做了一些输出就直接返回了。switch里的各种case, 如`IRQ_U_SOFT`,`CAUSE_USER_ECALL`,是riscv ISA 标准里规定的。我们在`riscv.h`里定义了这些常量。我们接下来主要关注时钟中断的处理。

在这里我们对时钟中断进行了一个简单的处理，即每次触发时钟中断的时候，我们会给一个计数器加一，并且设定好下一次时钟中断。当计数器加到100的时候，我们会输出一个`100ticks`表示我们触发了100次时钟中断。通过在模拟器中观察输出我们即刻看到是否正确触发了时钟中断，从而验证我们实现的异常处理机制。

```c
void interrupt_handler(struct trapframe *tf) {
    intptr_t cause = (tf->cause << 1) >> 1; //抹掉scause最高位代表“这是中断不是异常”的1
    switch (cause) {
        case IRQ_U_SOFT:
            cprintf("User software interrupt\n");
            break;
        case IRQ_S_SOFT:
            cprintf("Supervisor software interrupt\n");
            break;
        case IRQ_H_SOFT:
            cprintf("Hypervisor software interrupt\n");
            break;
        case IRQ_M_SOFT:
            cprintf("Machine software interrupt\n");
            break;
        case IRQ_U_TIMER:
            cprintf("User software interrupt\n");
            break;
        case IRQ_S_TIMER:
            //时钟中断
            /* LAB3 EXERCISE2   YOUR CODE :  */
            /*(1)设置下次时钟中断
             *(2)计数器（ticks）加一
             *(3)当计数器加到100的时候，我们会输出一个`100ticks`表示我们触发了100次时钟中断，同时打印次数（num）加一
            * (4)判断打印次数，当打印次数为10时，调用<sbi.h>中的关机函数关机
            */
            break;
        case IRQ_H_TIMER:
            cprintf("Hypervisor software interrupt\n");
            break;
        case IRQ_M_TIMER:
            cprintf("Machine software interrupt\n");
            break;
        case IRQ_U_EXT:
            cprintf("User software interrupt\n");
            break;
        case IRQ_S_EXT:
            cprintf("Supervisor external interrupt\n");
            break;
        case IRQ_H_EXT:
            cprintf("Hypervisor software interrupt\n");
            break;
        case IRQ_M_EXT:
            cprintf("Machine software interrupt\n");
            break;
        default:
            print_trapframe(tf);
            break;
    }
}
```

```c
void exception_handler(struct trapframe *tf) {
    switch (tf->cause) {
        case CAUSE_MISALIGNED_FETCH:
            break;
        case CAUSE_FAULT_FETCH:
            break;
        case CAUSE_ILLEGAL_INSTRUCTION:
            //非法指令异常处理
            /* LAB3 CHALLENGE3   YOUR CODE :  */
            /*(1)输出指令异常类型（ Illegal instruction）
             *(2)输出异常指令地址
             *(3)更新 tf->epc寄存器
             */
            break;
        case CAUSE_BREAKPOINT:
            //非法指令异常处理
            /* LAB3 CHALLLENGE3   YOUR CODE :  */
            /*(1)输出指令异常类型（ breakpoint）
             *(2)输出异常指令地址
             *(3)更新 tf->epc寄存器
             */
            break;
        case CAUSE_MISALIGNED_LOAD:
            break;
        case CAUSE_FAULT_LOAD:
            break;
        case CAUSE_MISALIGNED_STORE:
            break;
        case CAUSE_FAULT_STORE:
            break;
        case CAUSE_USER_ECALL:
            break;
        case CAUSE_SUPERVISOR_ECALL:
            break;
        case CAUSE_HYPERVISOR_ECALL:
            break;
        case CAUSE_MACHINE_ECALL:
            break;
        default:
            print_trapframe(tf);
            break;
    }
}
```

下面是RISCV标准里`scause`的部分，可以看到有个`scause`的数值与中断/异常原因的对应表格。

（*此处原文档应有图片或表格，但未提供*）

下一节我们将仔细讨论如何设置好时钟模块。

### 滴答滴答（时钟中断）

时钟中断需要CPU硬件的支持。CPU以"时钟周期"为工作的基本时间单位，对逻辑门的时序电路进行同步。

我们的“时钟中断”实际上就是”每隔若干个时钟周期执行一次的程序“。

”若干个时钟周期“是多少个？太短了肯定不行。如果时钟中断处理程序需要100个时钟周期执行，而你每50个时钟周期就触发一个时钟中断，那么间隔时间连一个完整的时钟中断程序都跑不完。如果你200个时钟周期就触发一个时钟中断，那么CPU的时间将有一半消耗在时钟中断，开销太大。一般而言，可以设置时钟中断间隔设置为CPU频率的1%，也就是每秒钟触发100次时钟中断，避免开销过大。

我们用到的RISCV对时钟中断的硬件支持包括：

  * OpenSBI提供的`sbi_set_timer()`接口，可以传入一个时刻，让它在那个时刻触发一次时钟中断
  * `rdtime`伪指令，读取一个叫做`time`的CSR的数值，表示CPU启动之后经过的真实时间。在不同硬件平台，时钟频率可能不同。在QEMU上，这个时钟的频率是10MHz, 每过1s, `rdtime`返回的结果增大10000000

> **趣闻**
>
> 在RISCV32和RISCV64架构中，`time`寄存器都是64位的。
> `rdcycle`伪指令可以读取经过的时钟周期数目，对应一个寄存器`cycle`

注意，我们需要“每隔若干时间就发生一次时钟中断”，但是OpenSBI提供的接口一次只能设置一个时钟中断事件。我们采用的方式是：一开始只设置一个时钟中断，之后每次发生时钟中断的时候，设置下一次的时钟中断。

在`clock.c`里面初始化时钟并封装一些接口

```c
//libs/sbi.c
//当time寄存器(rdtime的返回值)为stime_value的时候触发一个时钟中断
void sbi_set_timer(unsigned long long stime_value) {
    sbi_call(SBI_SET_TIMER, stime_value, 0, 0);
}
```

```c
// kern/driver/clock.c
#include <clock.h>
#include <defs.h>
#include <sbi.h>
#include <stdio.h>
#include <riscv.h>

//volatile告诉编译器这个变量可能在其他地方被瞎改一通，所以编译器不要对这个变量瞎优化
volatile size_t ticks;

//对64位和32位架构，读取time的方法是不同的
//32位架构下，需要把64位的time寄存器读到两个32位整数里，然后拼起来形成一个64位整数
//64位架构简单的一句rdtime就可以了
//__riscv_xlen是gcc定义的一个宏，可以用来区分是32位还是64位。
static inline uint64_t get_time(void) { //返回当前时间
#if __riscv_xlen == 64
    uint64_t n;
    __asm__ __volatile__("rdtime %0" : "=r"(n));
    return n;
#else
    uint32_t lo, hi, tmp;
    __asm__ __volatile__(
        "1:\n"
        "rdtimeh %0\n"
        "rdtime %1\n"
        "rdtimeh %2\n"
        "bne %0, %2, 1b"
        : "=&r"(hi), "=&r"(lo), "=&r"(tmp));
    return ((uint64_t)hi << 32) | lo;
#endif
}

// Hardcode timebase
static uint64_t timebase = 100000;

void clock_init(void) {
    // sie这个CSR可以单独使能/禁用某个来源的中断。默认时钟中断是关闭的
    // 所以我们要在初始化的时候，使能时钟中断
    set_csr(sie, MIP_STIP); // enable timer interrupt in sie
    //设置第一个时钟中断事件
    clock_set_next_event();
    // 初始化一个计数器
    ticks = 0;

    cprintf("++ setup timer interrupts\n");
}

//设置时钟中断：timer的数值变为当前时间 + timebase 后，触发一次时钟中断
//对于QEMU, timer增加1，过去了10^-7 s， 也就是100ns
void clock_set_next_event(void) { sbi_set_timer(get_time() + timebase); }
```

回来看`trap.c`里面时钟中断处理的代码, 还是很简单的：每秒100次时钟中断，触发每次时钟中断后设置10ms后触发下一次时钟中断，每触发100次时钟中断（1秒钟）输出一行信息到控制台。

```c
// kern/trap/trap.c
#include <clock.h>

#define TICK_NUM 100

static void print_ticks() {
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

void interrupt_handler(struct trapframe *tf) {
    intptr_t cause = (tf->cause << 1) >> 1;
    switch (cause) {
           /* blabla 其他case*/
        case IRQ_S_TIMER:
            clock_set_next_event(); //发生这次时钟中断的时候，我们要设置下一次时钟中断
            if (++ticks % TICK_NUM == 0) {
                print_ticks();
            }
            break;
        /* blabla 其他case*/
    }
}
```

现在执行`make qemu`, 应该能看到打印一行行的`100 ticks`

### 断亦有断(中断的关闭和开启)

中断机制固然重要，然而在操作系统运行过程中，依然有一些操作是不能接受被中断打断的，例如修改内存管理相关的数据结构等。为了确保这些操作不受干扰，操作系统提供了原子操作的机制。

以我们刚刚完成的物理内存管理为例，我们添加了以下内容来保证操作的原子性：

  * **kern/sync/sync.h：** 为确保内存管理修改相关数据时不被中断打断，提供两个功能，一个是保存 `sstatus`寄存器中的中断使能位(SIE)信息并屏蔽中断的功能，另一个是根据保存的中断使能位信息来使能中断的功能
  * **libs/atomic.h：** 定义了对一个二进制位进行读写的原子操作，确保相关操作不被中断打断。包括`set_bit()`设置某个二进制位的值为1, `change_bit()`给某个二进制位取反，`test_bit()`返回某个二进制位的值。

<!-- end list -->

```c
// kern/sync/sync.h
#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include <defs.h>
#include <intr.h>
#include <riscv.h>

static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();
        return 1;
    }
    return 0;
}

static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}

//思考：这里宏定义的 do{}while(0)起什么作用?
#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)

#define local_intr_restore(x) __intr_restore(x);

#endif /* !__KERN_SYNC_SYNC_H__ */
```

在分配和释放内存的过程中，我们会先关闭中断，然后进行对应操作，最后重新开启中断

```c
// kern/mm/pmm.c
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}
```

-----

## 实验报告要求

从oslab网站上取得实验代码后，进入目录labcodes/lab3，完成实验要求的各个练习。在实验报告中回答所有练习中提出的问题。在目录labcodes/lab3下存放实验报告，推荐用markdown格式。每个小组建一个gitee或者github仓库，对于lab3中编程任务，完成编写之后，再通过git push命令把代码和报告上传到仓库。最后请一定提前或按时提交到git网站。

注意有“LAB3”的注释，代码中所有需要完成的地方（challenge除外）都有“LAB3”和“YOUR CODE”的注释，请在提交时特别注意保持注释，并将“YOUR CODE”替换为自己的学号，并且将所有标有对应注释的部分填上正确的代码。