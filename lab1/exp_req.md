# lab1: 比麻雀更小的麻雀（最小可执行内核）

放了一把火, 烧得只剩个架架

相对于上百万行的现代操作系统(linux, windows), 几千行的ucore是一只"麻雀"。但这只麻雀依然是一只胖麻雀，我们一眼看不过来几千行的代码。所以，我们要再做简化，先用好刀法，片掉麻雀的血肉, 搞出一个\*\*"麻雀骨架"\*\*，看得通透，再像组装哪吒一样，把血肉安回去，变成一个活生生的麻雀。这就是我们的ucore step-by-step tutorial的思路。

lab1是后面实验的预备，我们构建一个最小的可执行内核（”麻雀骨架“），它能够进行格式化的输出，然后进入死循环。

下面我们就开始解剖麻雀。

## 实验目的：

实验1主要讲解最小可执行内核和启动流程。我们的内核主要在 Qemu 模拟器上运行，它可以模拟一台 64 位 RISC-V 计算机。为了让我们的内核能够正确对接到 Qemu 模拟器上，需要了解 Qemu 模拟器的启动流程，还需要一些程序内存布局和编译流程（特别是链接）相关知识。

### 本章你将学到：

  * 使用 **链接脚本** 描述内存布局
  * 进行 **交叉编译** 生成可执行文件，进而生成内核镜像
  * 使用 **OpenSBI** 作为 bootloader 加载内核镜像，并使用 Qemu 进行模拟
  * 使用 OpenSBI 提供的服务，在屏幕上格式化打印字符串用于以后调试

## 实验内容：

实验1主要讲解最小可执行内核和启动流程。我们的内核主要在 Qemu 模拟器上运行，它可以模拟一台 64 位 RISC-V 计算机。为了让我们的内核能够正确对接到 Qemu 模拟器上，需要了解 Qemu 模拟器的启动流程，还需要一些程序内存布局和编译流程（特别是链接）相关知识,以及通过opensbi固件来通过服务。

## 练习

### 对实验报告的要求：

  * 基于markdown格式来完成，以文本方式为主
  * 填写各个基本练习中要求完成的报告内容
  * 列出你认为本实验中重要的知识点，以及与对应的OS原理中的知识点，并简要说明你对二者的含义，关系，差异等方面的理解（也可能出现实验中的知识点没有对应的原理知识点）
  * 列出你认为OS原理中很重要，但在实验中没有对应上的知识点

<!-- end list -->

1.  **练习1：理解内核启动中的程序入口操作**
    阅读 `kern/init/entry.S`内容代码，结合操作系统内核启动流程，说明指令 `la sp, bootstacktop` 完成了什么操作，目的是什么？ `tail kern_init` 完成了什么操作，目的是什么？

2.  **练习2: 使用GDB验证启动流程**
    为了熟悉使用 QEMU 和 GDB 的调试方法，请使用 GDB 跟踪 QEMU 模拟的 RISC-V 从加电开始，直到执行内核第一条指令（跳转到 0x80200000）的整个过程。通过调试，请思考并回答：RISC-V 硬件加电后最初执行的几条指令位于什么地址？它们主要完成了哪些功能？请在报告中简要记录你的调试过程、观察结果和问题的答案。

    > #### tips:

    > 启动流程可以分为以下几个阶段：

    > 1)  CPU 从复位地址（0x1000）开始执行初始化固件（OpenSBI）的汇编代码，进行最基础的硬件初始化。
    > 2)  SBI 固件进行主初始化，其核心任务之一是将内核加载到 0x80200000。可以使用`watch *0x80200000`观察内核加载瞬间，避免单步跟踪大量代码。
    > 3)  SBI 最后跳转到 0x80200000，将控制权移交内核。使用 `b *0x80200000` 可在此中断，验证内核开始执行

> #### 拓展 现代笔记本的启动流程
>
> 在我们通过 QEMU 模拟的 RISC-V 开发板学习操作系统启动过程时，你可能会好奇：我们日常使用的笔记本电脑又是如何启动的呢？虽然底层架构不同（x86 vs RISC-V），但核心思想惊人地相似，都是一个从底层固件到高级操作系统的“接力”过程。
>
> 现代笔记本的启动流程可以概括为一个精致的引导链条：当你按下电源键，CPU 的第一条指令并非来自硬盘上的 Windows 或 Linux，而是来自主板上的一块芯片——其中存储着 UEFI 固件。这个 UEFI 就像是电脑的“本能”，它负责唤醒硬件、进行自检，并按照设定好的顺序，去硬盘上一个特殊分区里查找并运行名为引导加载程序（如 GRUB 或 Windows Boot Manager）的小程序。最终，这个引导程序才会将存储在内的完整操作系统内核加载到内存并执行，正式完成从固件到操作系统的交接，将电脑的控制权交给我们熟悉的桌面环境。
>
> 这个流程与我们实验中的 QEMU 启动（从 OpenSBI 到内核）本质是一样的，都经历了“固件 -\> 引导程序 -\> 操作系统”的三级跳。只不过在真实的 PC 中，每一步都更加复杂，涉及更多安全检查和硬件初始化，但其层层递进、分工协作的核心设计哲学是完全共通的。理解了这一点，下次开机时你就能感受到，短短几秒的背后，正是一场井然有序的系统交接仪式。

## lab1 项目组成和执行流

lab1的项目组成如下:

```
── Makefile 
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
│   │   ├── memlayout.h
│   │   ├── mmu.h
│   │   ├── pmm.c
│   │   └── pmm.h
│   └── trap
│         ├── trap.c
│         ├── trap.h
│         └── trapentry.S
├── libs
│   ├── defs.h
│   ├── elf.h
│   ├── error.h
│   ├── printfmt.c
│   ├── riscv.h
│   ├── sbi.c
│   ├── sbi.h
│   ├── stdarg.h
│   ├── stdio.h
│   ├── string.c
│   └── string.h
└── tools
    ├── function.mk
    └── kernel.ld
```

### 内核启动相关文件

  * **`kern/init/entry.S`**: OpenSBI启动之后将要跳转到的一段汇编代码。在这里进行内核栈的分配，然后转入C语言编写的内核初始化函数。
  * **`kern/init/init.c`**: C语言编写的内核入口点。主要包含`kern_init()`函数，从`kern/init/entry.S`跳转过来完成其他初始化工作。

### 编译、链接相关文件

  * **`tools/kernel.ld`**: ucore的链接脚本(link script), 告诉链接器如何将目标文件的section组合为可执行文件，并指定内核加载地址为`0x80200000`。
  * **`tools/function.mk`**: 定义Makefile中使用的一些函数
  * **`Makefile`**: GNU make编译脚本

### 其他文件

项目中还包括基础库（`libs`）、设备驱动（`kern/drive`）、内存管理(`kern/mm`)、异常处理（`kern/trap`）等文件，这些文件的相关内容将在后续实验中进行完善和具体讲解。

### 执行流

#### 完整流程

最小可执行内核的完整启动流程为:
加电复位 → CPU从`0x1000`进入MROM → 跳转到`0x80000000`(OpenSBI) → OpenSBI初始化并加载内核到`0x80200000` → 跳转到`entry.S` → 调用`kern_init()` → 输出信息 → 结束

#### 详细步骤

1.  **硬件初始化和固件启动**。QEMU 模拟器启动后，会模拟加电复位过程。此时 PC 被硬件强制设置为固定的复位地址`0x1000`，从这里开始执行一小段写死的固件代码（MROM，Machine ROM）。MROM 的功能非常有限，主要是完成最基本的环境准备，并将控制权交给OpenSBI。OpenSBI 被加载到物理内存的`0x80000000`处。
2.  **OpenSBI 初始化与内核加载**。CPU 跳转到`0x80000000`处继续运行。OpenSBI 运行在 RISC-V 的最高特权级（M 模式），负责初始化处理器的运行环境。完成这些初始化工作后，OpenSBI 才会准备开始加载并启动操作系统内核。OpenSBI 将编译生成的内核镜像文件加载到物理内存的`0x80200000`地址处。
3.  **内核启动执行**。OpenSBI 完成相关工作后，跳转到`0x80200000`地址，开始执行`kern/init/entry.S`。在`0x80200000`这个地址上存放的是`kern/init/entry.S`文件编译后的机器码，这是因为链接脚本将`entry.S`中的代码段放在内核镜像的最开始位置。`entry.S`设置内核栈指针，为C语言函数调用分配栈空间，准备C语言运行环境，然后按照RISC-V的调用约定跳转到`kern_init()`函数。最后，`kern_init()`调用`cprintf()`输出一行信息，表示内核启动成功。

## 从机器启动到操作系统运行的过程

### OpenSBI, bin, elf

最小可执行内核里, 我们需要完成以下两项关键工作:

1.  内核的内存布局和入口点设置
2.  通过 OpenSBI 封装输入输出函数

首先我们回顾计算机的组成:
计算机的基本组成包括CPU, 存储设备（粗略地说，包括断电后遗失的内存，和断电后不遗失的硬盘），输入输出设备，总线。

在 QEMU 模拟的 RISC-V 系统中，QEMU会帮助我们模拟一块riscv64的CPU，一块物理内存，还会借助你的电脑的键盘和显示屏来模拟命令行的输入和输出。虽然QEMU不会真正模拟一堆线缆，但是总线的通信功能也在QEMU内部实现了。

还差什么呢？硬盘。

我们需要硬盘上的程序和数据。比如崭新的windows电脑里C盘已经被占据的二三十GB空间，除去预装的应用软件，还有一部分是windows操作系统的内核。在插上电源开机之后，就需要将操作系统内核加载到内存中，来运行操作系统的内核，然后由操作系统来管理计算机。

问题在于，操作系统作为一个程序，“把操作系统加载到内存里”这件事情，不是操作系统自己能做到的，就好像你不能拽着头发把自己拽离地面。

因此我们可以想象，在操作系统执行之前，必然有一个其他程序执行，他作为“先锋队”，完成“把操作系统加载到内存“这个工作，然后他功成身退，把CPU的控制权交给操作系统。

这个“其他程序”，我们一般称之为**bootloader**. 很好理解：他负责boot(开机)，还负责load(加载OS到内存里)，所以叫bootloader.

> #### 简单说明
>
> 对于有复杂的读写时序要求的设备，比如硬盘、U盘等，CPU是无法直接读取的，需要借助驱动程序来告知CPU如何与设备进行通信从而读到数据。
> 而这些驱动程序，就需要存储在一个不需要驱动程序，CPU就可以直接读取的地方，即一块像Memory一样，可以直接使用load/store命令可以访问，又不会担心数据会丢失的区域
> 在现代计算机中，有一些设备具备这样的特性，如早期的ROM芯片，以及后期相对普及的NOR Flash芯片
> 这些芯片与CPU连接之前，首先会进行初始化，在其中预置好对CPU的初始程序，即bootloader，然后再焊接至电路板上
> 在CPU启动时，会首先运行这些代码，用这些代码实现对硬盘、内存和其他复杂设备的读取

在QEMU模拟的riscv计算机里，我们使用QEMU自带的bootloader: **OpenSBI**固件，那么在 Qemu 开始执行任何指令之前，首先要将作为 bootloader 的 `OpenSBI.bin` 加载到物理内存以物理地址 `0x80000000` 开头的区域上，然后将操作系统内核加载到QEMU模拟的硬盘中。

> #### 须知
>
> 在计算机中，固件(firmware) 是一种特定的计算机软件，它为设备的特定硬件提供低级控制，也可以进一步加载其他软件。固件可以为设备更复杂的软件（如操作系统）提供标准化的操作环境。对于不太复杂的设备，固件可以直接充当设备的完整操作系统，执行所有控制、监视和数据操作功能。 在基于 x86 的计算机系统中, BIOS 或 UEFI 是固件；在基于 riscv 的计算机系统中，OpenSBI 是固件。OpenSBI运行在M态（M-mode），因为固件需要直接访问硬件。

RISCV有四种特权级（privilege level）。

| Level | Encoding | 全称 | 简称 |
| :--- | :--- | :--- | :--- |
| 0 | 00 | User/Application | U |
| 1 | 01 | Supervisor | S |
| 2 | 10 | Reserved | (目前未使用，保留) |
| 3 | 11 | Machine | M |

粗略的分类：

  * **U-mode**是用户程序、应用程序的特权级
  * **S-mode**是操作系统内核的特权级
  * **M-mode**是固件的特权级。

那为何作为 bootloader 的 OpenSBI要被加载到`0x80000000` 开头的区域上呢？加载到其他的地址空间可以吗？关于这个地址的选择，其实有软件工的工作，也有硬件人的工作。

> #### 实现细节
>
> 需要解释一下的是，QEMU模拟的这款riscv处理器的**复位地址**是`0x1000`，而不是`0x80000000`
>
> 所谓复位地址，指的是CPU在上电的时候，或者按下复位键的时候，PC被赋的初始值
> 这个值的选择会因为厂商的不同而发生变化，例如，80386的复位地址是`0xFFF0`（因为复位时是16位模式，写成32位时也作`0xFFFFFFF0`），MIPS的复位地址是`0x00000000`
>
> RISCV的设计标准相对灵活，它允许芯片的实现者自主选择复位地址，因此不同的芯片会有一些差异。QEMU-4.1.1是选择了一种实现方式进行模拟
> 在QEMU模拟的这款riscv处理器中，将复位向量地址初始化为`0x1000`，再将PC初始化为该复位地址，因此处理器将从此处开始执行复位代码，复位代码主要是将计算机系统的各个组件（包括处理器、内存、设备等）置于初始状态，并且会启动Bootloader，在这里QEMU的复位代码指定加载Bootloader的位置为`0x80000000`，Bootloader将加载操作系统内核并启动操作系统的执行。

我们可以想象这样的过程：在Qemu启动时，OpenSBI作为bootloader会操作系统的二进制可执行文件从硬盘加载到内存中，然后OpenSBI会把CPU的"当前指令指针"(pc, program counter)跳转到内存里的一个位置，开始执行内存中那个位置的指令，也就是操作系统的指令。

那OpenSBI怎样知道要把操作系统加载到内存的什么位置呢？随便加载一个位置肯定是不可取的，因为加载之后OpenSBI还要把CPU的program counter跳转到操作系统所在的位置,开始操作系统的执行。如果加载操作系统到内存里的时候随便加载，那么OpenSBI怎么知道把program counter跳转到哪里去呢？难道操作系统的二进制可执行文件需要提供“program counter跳转到哪里"这样的信息吗？

也许你会觉得可以把操作系统的代码总是加载到固定的位置，比如总是加载到内存地址最高的地方。实际上，OpenSBI会将内核镜像 `os.bin` 加载到 Qemu 物理内存以地址 `0x80200000` 开头的区域上，并将CPU的控制权交给操作系统。为了正确地和上一阶段的 OpenSBI 对接，我们需要保证内核的第一条指令位于物理地址 `0x80200000` 处，因为这里的代码是**地址相关**的，这个地址是由处理器，即Qemu指定的。

一旦 CPU 开始执行内核的第一条指令，证明计算机的控制权已经被移交给我们的内核。

> #### 拓展 代码的地址相关性
>
> 这里我们需要解释一个小问题，为什么内核一定要被加载到`0x80200000`的位置开始呢？加载到别的位置行不行呢？
>
> 这要从高级语言被编译成为汇编和机器指令的过程说起了
>
> 以一行C语言代码为例，`int sum = 0;`，这句的意义是，在内存中分配一份可以容纳一个整数的空间，将其初始化为0。
>
> 那么，什么时候才会确定`sum` 被分配的空间的具体地址呢？在编译和链接的时候就会分配，假设这个地址是`pa_sum`
>
> 接下来，当出现类似`sum +=5;`这样的语句时，相应的机器指令是，将`pa_sum`位置的值加载入寄存器，完成加法计算，再将这个值写回内存中对应的`pa_sum`处
>
> 而此时生成的指令，会将`pa_sum`的值（假设为`0x55aa55aa`），直接写入到指令的编码中，如`load r1 0x55aa55aa`
>
> 如此一来，则这一段代码就成了**地址相关代码**，即指令中的访存信息在编译完成后即已成为绝对地址，那么在运行之前，自然需要将所需要的代码加载到指定的位置
>
> 与之相对的，自然会产生**地址无关代码**的技术需求，即在加载前才代码确定好被加载的位置，然后代码就可以在被加载处运行，这需要编译和加载时做一些处理工作，同学们可以思考一下这个技术需求的实现思路

在现有的ucore中，操作系统的加载实际上是由QEMU来完成，在 Qemu 开始执行任何指令之前，首先两个文件将被加载到 Qemu 的物理内存中：即作为 bootloader 的 `OpenSBI.bin` 被加载到物理内存以物理地址 `0x80000000` 开头的区域上，同时内核镜像 `os.bin` 被加载到以物理地址 `0x80200000` 开头的区域上。

在我们的在Linux系统中，有两种不同的可执行文件格式：**elf**(e是executable的意思， l是linkable的意思，f是format的意思)和**bin**(binary)。

  * **elf文件**([wikipedia: elf](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format))比较复杂，包含一个文件头(ELF header), 包含冗余的调试信息，指定程序每个section的内存布局，需要解析program header才能知道各段(section)的信息。如果我们已经有一个完整的操作系统来解析elf文件，那么elf文件可以直接执行。但是对于OpenSBI来说，elf格式还是太复杂了。
  * **bin文件**就比较简单了，简单地在文件头之后解释自己应该被加载到什么起始位置。

我们举一个例子解释elf和bin文件的区别：初始化为零的一个大数组，在elf文件里是bss数据段的一部分，只需要记住这个数组的起点和终点就可以了，等到加载到内存里的时候分配那一段内存。但是在bin文件里，那个数组有多大，有多少个字节的0，bin文件就要对应有多少个零。所以如果一个程序里声明了一个大全局数组（默认初始化为0），那么可能编译出来的elf文件只有几KB, 而生成bin文件之后却有几MB, 这是很正常的。实际上，可以认为bin文件会把elf文件指定的每段的内存布局都映射到一块线性的数据里，这块线性的数据（或者说程序）加载到内存里就符合elf文件之前指定的布局。

那么我们的任务就明确了：得到内存布局合适的elf文件，然后把它转化成bin文件（这一步通过`objcopy`实现），然后加载到QEMU里运行（作为bootloader的OpenSBI会干这个活）。下面我们来看如何设置elf文件的内存布局。

## 内存布局，链接脚本，入口点

一般来说，一个程序按照功能不同会分为下面这些段：

  * `.text` 段，即代码段，存放汇编代码；
  * `.rodata` 段，即只读数据段，顾名思义里面存放只读数据，通常是程序中的常量；
  * `.data` 段，存放被初始化的可读写数据，通常保存程序中的全局变量；
  * `.bss` 段，存放被初始化为 00 的可读写数据，与 `.data` 段的不同之处在于我们知道它要被初始化为 00，因此在可执行文件中只需记录这个段的大小以及所在位置即可，而不用记录里面的数据。
  * `stack`，即栈，用来存储程序运行过程中的局部变量，以及负责函数调用时的各种机制。它从高地址向低地址增长；
  * `heap`，即堆，用来支持程序运行过程中内存的动态分配，比如说你要读进来一个字符串，在你写程序的时候你也不知道它的长度究竟为多少，于是你只能在运行过程中，知道了字符串的长度之后，再在堆中给这个字符串分配内存。

内存布局，也就是指这些段各自所放的位置。一种典型的内存布局如下：

```
+---------------+ <--- 高地址
|     Stack     |
|       |       |
|       v       |
|               |
|       ^       |
|       |       |
|     Heap      |
+---------------+
|      BSS      |
+---------------+
|      Data     |
+---------------+
|     ROData    |
+---------------+
|      Text     |
+---------------+ <--- 低地址 (e.g., 0x80200000)
```

gnu工具链中，包含一个链接器 `ld`。

> 如果你很好奇，可以看 [linker script的详细语法](https://sourceware.org/binutils/docs/ld/Scripts.html)。

链接器的作用是把输入文件（往往是 `.o` 文件）链接成输出文件（往往是 elf 文件）。一般来说，输入文件和输出文件都有很多 section，\*\*链接脚本（linker script）\*\*的作用，就是描述怎样把输入文件的 section 映射到输出文件的 section，同时规定这些 section 的内存布局。

如果你不提供链接脚本，`ld` 会使用默认的一个链接脚本，这个默认的链接脚本适合链接出一个能在现有操作系统下运行的应用程序，但是并不适合链接一个操作系统内核。你可以通过 `ld --verbose` 来查看默认的链接脚本。

下面给出我们使用的链接脚本：

```ld
/* tools/kernel.ld */

OUTPUT_ARCH(riscv) /* 指定输出文件的指令集架构, 在riscv平台上运行 */
ENTRY(kern_entry)  /* 指定程序的入口点, 是一个叫做kern_entry的符号。我们之后会在汇编代码里定义它*/

BASE_ADDRESS = 0x80200000;/*定义了一个变量BASE_ADDRESS并初始化 */

/*链接脚本剩余的部分是一整条SECTIONS指令，用来指定输出文件的所有SECTION
  "." 是SECTIONS指令内的一个特殊变量/计数器，对应内存里的一个地址。*/
SECTIONS
{
    /* Load the kernel at this address: "." means the current address */
    . = BASE_ADDRESS;/*对 "."进行赋值*/

    /* 下面一句的意思是：从.的当前值（当前地址）开始放置一个叫做text的section. 
     * 花括号内部的*(.text.kern_entry .text .stub .text.* .gnu.linkonce.t.*)是正则表达式
     * 如果输入文件中有一个section的名称符合花括号内部的格式
     * 那么这个section就被加到输出文件的text这个section里
     * 输入文件中section的名称,有些是编译器自动生成的,有些是我们自己定义的*/
    .text : {
        *(.text.kern_entry) /*把输入中kern_entry这一段放到输出中text的开头*/
        *(.text .stub .text.* .gnu.linkonce.t.*)
    }

    PROVIDE(etext = .); /* Define the 'etext' symbol to this value */

    /* read only data, 只读数据，如程序里的常量 */
    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    }

    /* 进行地址对齐，将 "."增加到 2的0x1000次方的整数倍，也就是下一个内存页的起始处 */
    . = ALIGN(0x1000);

    .data : {
        *(.data)
        *(.data.*)
    }

    /* small data section, 存储字节数小于某个标准的变量，一般是char, short等类型的 */
    .sdata : {
        *(.sdata)
        *(.sdata.*)
    }

    PROVIDE(edata = .);

    /* 初始化为零的数据 */
    .bss : {
        *(.bss)
        *(.bss.*)
        *(.sbss*)
    }

    PROVIDE(end = .);

    /* /DISCARD/表示忽略，输入文件里 *(.eh_frame .note.GNU-stack)这些section都被忽略，不会加入到输出文件中 */
    /DISCARD/ : {
        *(.eh_frame .note.GNU-stack)
    }
}
```

> #### 趣闻
>
> **为什么把初始化为0（或者说，无需初始化）的数据段称作 .bss?**
>
> CSAPP 7.4 Relocatable Object files
>
> > Aside Why is uninitialized data called .bss? The use of the term .bss to denote uninitialized data is universal. It was originally an acronym for the “block started by symbol” directive from the IBM 704 assembly language (circa 1957) and the acronym has stuck. A simple way to remember the difference between the .data and .bss sections is to think of “bss” as an abbreviation for “Better Save Space\!”

我们在链接脚本里把程序的入口点定义为 `kern_entry`，那么我们的程序里需要有一个名称为 `kern_entry` 的符号。我们在 `kern/init/entry.S` 编写一段汇编代码，作为整个内核的入口点。

```asm
# kern/init/entry.S
#include <mmu.h>
#include <memlayout.h>

# The ,"ax",@progbits tells the assembler that the section is allocatable ("a"), executable ("x") and contains data ("@progbits").
# 从这里开始.text 这个section, "ax" 和 %progbits描述这个section的特征
# https://www.nongnu.org/avr-libc/user-manual/mem_sections.html
.section .text.kern_entry,"ax",@progbits 
    .globl kern_entry # 使得ld能够看到kern_entry这个符号所在的位置, globl和global同义
    # https://sourceware.org/binutils/docs/as/Global.html#Global
kern_entry: 
    la sp, bootstacktop
    tail kern_init 

#开始data section
.section .data
    .align 12 #按照2^12进行地址对齐, 也就是对齐到下一页 
    .global bootstack #内核栈
bootstack:
    .space 4096 * 4 #留出KSTACKSIZE这么多个字节的内存
    .global bootstacktop #之后内核栈将要从高地址向低地址增长, 初始时的内核栈为空
bootstacktop:
```

现在这个入口点 `kern_entry` 的作用非常明确：分配好内核栈，然后跳转到 `kern_init`。至此，汇编代码的使命完成，CPU 的执行流程交给了我们用 C 语言编写的 `kern_init` 函数，它才是我们“真正的”内核入口点。

```c
// kern/init/init.c
#include <stdio.h>
#include <string.h>
// 这里include的头文件并不是C语言的标准库，而是我们自己编写的！

// noreturn 告诉编译器这个函数不会返回
int kern_init(void) __attribute__((noreturn));

int kern_init(void) {
    extern char edata[], end[]; 
    // 这里声明的两个符号由链接器定义，分别指向.data段结束和.bss段结束
    memset(edata, 0, end - edata); 
    // 清除.bss段：由于内核没有标准库，memset需要我们自己实现

    const char *message = "(THU.CST) os is loading ...\n";
    cprintf("%s\n\n", message); // cprintf是我们在ucore中自己实现的格式化输出函数

    while (1)
        ; // 进入无限循环
}
```

在 `kern/init/init.c` 中，`kern_init` 函数肩负着内核初启的核心任务：

1.  **初始化内核环境**：例如，清理 `.bss` 段，为未初始化的全局变量赋零值。
2.  **向用户提供可视化反馈**：这是操作系统与开发者/用户的第一次交互，通过输出信息告诉我们：“os is loading ...”。

从代码中我们可以看到，内核期望调用一个类似标准库 `printf` 的格式化输出函数 `cprintf`。然而，在操作系统内核自身尚未成型之时，我们无法依赖任何现成的运行时库。那么，这个 `cprintf` 函数从何而来？它又是如何在不依赖外部库的情况下实现输出的呢？

这正是我们接下来需要解决的核心问题。我们将从最底层的硬件接口出发，自底向上地构建出一套完整的输出功能。

## 从SBI到stdio

承接上一节的问题，我们需要在“一无所有”的环境中，创造出一个能用的 `cprintf` 函数。

> #### 须知
>
> 如果我们在 Linux 下运行一个 C 程序，需要格式化输出，那么大一的同学都知道我们应该 `#include<stdio.h>`。且慢！ 在 Linux 下，当我们调用 C 语言标准库的函数时，实际上依赖于 glibc 提供的运行时环境，也就是一定程度上依赖于操作系统提供的支持。那么这样的操作在逻辑上就是不通顺的，构成了一个鸡生蛋蛋生鸡的过程，你不能在开发一个操作系统的时候还要依赖另一个操作系统提供的代码环境支持（注意，这里的支持不是指虚拟机、模拟器等，后面会学到，标准库的printf本质上就是调用了操作系统内核提供的接口）。

怎么办呢？只能自己动手，丰衣足食。解决问题的起点，是RISC-V架构下的机器态固件——**OpenSBI**。QEMU 内置的 OpenSBI 固件为我们提供了一个最原始的“输出一个字符”的接口。我们的任务就是抓住这个原始的接口，像搭积木一样，层层封装，最终构建出我们需要的、功能强大的 `cprintf` 函数

### 什么是 OpenSBI？

您可以将其理解为一套预先安装在机器（M模式）上的标准函数库。这套库提供了一些基础服务，比如设置定时器、发送处理器间中断（IPI），以及我们最需要的控制台输入输出。

然而，调用这些函数不能像普通函数那样使用 `call` 指令。因为我们的内核运行在 S 模式，而 SBI 服务运行在更高的 M 模式。跨越这种特权级别的调用，需要使用特殊的指令——`ecall`（Environment Call）。

> #### 须知 ecall
>
> `ecall` 指令是 RISC-V 中用于实现受控的权限提升的关键指令。
>
>   * 当在 **U 模式**（用户态）执行 `ecall`，会触发异常，从而陷入到 **S 模式**（内核态）。这是**系统调用**的底层机制。
>   * 当在 **S 模式**（内核态）执行 `ecall`，会触发异常，从而陷入到 **M 模式**（机器态）。这正是我们调用 **OpenSBI 服务**的方式。

### 如何调用 SBI 服务？

通过 `ecall` 调用 SBI 服务，需要遵循一个明确的调用约定。这个过程类似于在一个预定义的表格中查找一个功能号，然后按照固定的规则传递参数（后面会学习到这就是系统调用的调用格式），最后执行一个特殊指令来触发它。

1.  **指定服务编号**：将想要调用的 SBI 功能编号（例如，`SBI_CONSOLE_PUTCHAR = 1`）放入指定的寄存器（通常是 `a7` 或 `x17`）。
2.  **传递参数**：根据 RISC-V 的函数调用约定（Calling Convention），将参数放入寄存器 `a0`, `a1`, `a2`（即 `x10`, `x11`, `x12`）。
3.  **执行调用**：执行 `ecall` 指令。CPU 会 trap 到 M 模式，由 OpenSBI 固件处理请求。
4.  **获取返回值**：处理完成后，OpenSBI 会将返回值放入 `a0`（`x10`）寄存器，然后返回。

### 为什么需要内联汇编？

在 C 语言中，我们无法直接执行 `ecall` 这样的特定指令，也无法精确控制哪个变量放入哪个寄存器。因此，我们必须借助**内联汇编（Inline Assembly）** 来“手动”完成上述步骤，将底层指令的调用封装成一个对 C 语言友好的函数。

下面的代码实现了最核心的 SBI 调用封装：

```c
// libs/sbi.c
#include <sbi.h>
#include <defs.h>

// SBI 功能编号清单
uint64_t SBI_SET_TIMER = 0;
uint64_t SBI_CONSOLE_PUTCHAR = 1;
uint64_t SBI_CONSOLE_GETCHAR = 2;
// ... 其他功能编号

// sbi_call - 通用的 SBI 调用函数
// @sbi_type: SBI 功能编号
// @arg0, arg1, arg2: 传递给 SBI 服务的参数
// 返回值：SBI 服务返回的结果
uint64_t sbi_call(uint64_t sbi_type, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
    uint64_t ret_val;

    __asm__ volatile (
        // 1. 将功能编号和参数放入指定的寄存器
        "mv x17, %[sbi_type]\n" // 功能编号 -> x17 (a7)
        "mv x10, %[arg0]\n"     // arg0 -> x10 (a0)
        "mv x11, %[arg1]\n"     // arg1 -> x11 (a1)
        "mv x12, %[arg2]\n"     // arg2 -> x12 (a2)

        // 2. 执行 ecall 指令，发起调用
        "ecall\n"

        // 3. 将返回值（在 x10/a0 中）移动到 C 变量 ret_val 中
        "mv %[ret_val], x10"

        // 输出操作数：将汇编的结果输出到C变量ret_val
        : [ret_val] "=r" (ret_val)
        // 输入操作数：将C变量sbi_type, arg0, arg1, arg2的值作为输入传给汇编
        : [sbi_type] "r" (sbi_type), [arg0] "r" (arg0), [arg1] "r" (arg1), [arg2] "r" (arg2)
        // 告知编译器：内联汇编可能会读取或写入内存，防止编译器优化时出错
        : "memory", "x10", "x11", "x12", "x17"
    );
    return ret_val;
}

// 基于通用的 sbi_call，封装出专用的字符输出函数
void sbi_console_putchar(unsigned char ch) {
    sbi_call(SBI_CONSOLE_PUTCHAR, ch, 0, 0);
}
```

这样我们就可以通过`sbi_console_putchar()`来输出一个字符。接下来我们要做的事情就像月饼包装，把它封了一层又一层。

`console.c`只是简单地封装一下

```c
// kern/driver/console.c
#include <sbi.h>
#include <console.h>
void cons_putc(int c) { sbi_console_putchar((unsigned char)c); }
```

`stdio.c`里面实现了一些函数，注意我们已经实现了ucore版本的puts函数: `cputs()`

```c
// kern/libs/stdio.c
#include <console.h>
#include <defs.h>
#include <stdio.h>

/* HIGH level console I/O */
/* *
 * cputch - writes a single character @c to stdout, and it will
 * increace the value of counter pointed by @cnt.
 * */
static void cputch(int c, int *cnt) {
    cons_putc(c);
    (*cnt)++;
}

/* cputchar - writes a single character to stdout */
void cputchar(int c) { cons_putc(c); }

int cputs(const char *str) {
    int cnt = 0;
    char c;
    while ((c = *str++) != '\0') {
        cputch(c, &cnt);
    }
    cputch('\n', &cnt);
    return cnt;
}
```

我们还在`libs/printfmt.c`实现了一些复杂的格式化输入输出函数。最后得到的`cprintf()`函数仍在`kern/libs/stdio.c`定义，功能和C标准库的`printf()`基本相同。

可能你注意到我们用到一个头文件`defs.h`, 我们在里面定义了一些有用的宏和类型

```c
// libs/defs.h
#ifndef __LIBS_DEFS_H__
#define __LIBS_DEFS_H__
...
/* Represents true-or-false values */
typedef int bool;
/* Explicitly-sized versions of integer types */
typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
...
/* *
 * Rounding operations (efficient when n is a power of 2)
 * Round down to the nearest multiple of n
 * */
#define ROUNDDOWN(a, n) ({                                          \
        size_t __a = (size_t)(a);                                   \
        (typeof(a))(__a - __a % (n));                               \
        })
...
#endif
```

`printfmt.c`还依赖一个头文件`riscv.h`,这个头文件主要定义了若干和riscv架构相关的宏，尤其是将一些内联汇编的代码封装成宏，使得我们更方便地使用内联汇编来读写寄存器。当然这里我们还没有用到它的强大功能。

```c
// libs/riscv.h
...
#define read_csr(reg) ({ unsigned long __tmp; \
  asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
  __tmp; })
//通过内联汇编包装了 csrr 指令为 read_csr() 宏

#define write_csr(reg, val) ({ \
  if (__builtin_constant_p(val) && (unsigned long)(val) < 32) \
    asm volatile ("csrw " #reg ", %0" :: "i"(val)); \
  else \
    asm volatile ("csrw " #reg ", %0" :: "r"(val)); })
...
```

到现在，我们已经看过了一个最小化的内核的各个部分，虽然一些部分没有逐行细读，但我们也知道它在做什么。但一直到现在我们还没进行过编译。下面就把它编译一下跑起来。

## Just make it

我们需要：编译所有的源代码，把目标文件链接起来，生成elf文件，生成bin硬盘镜像，用qemu跑起来

这一系列复杂的命令，我们不想每次用到的时候都敲一遍，所以我们使用魔改的祖传Makefile。

### make 和 Makefile

GNU `make`(简称make)是一种代码维护工具，在大中型项目中，它将根据程序各个模块的更新情况，自动的维护和生成目标代码。

`make`命令执行时，需要一个 `makefile` （或`Makefile`）文件，以告诉`make`命令需要怎么样的去编译和链接程序

我们的`Makefile`还依赖`tools/function.mk`。只要我们的makefile写得够好，所有的这一切，我们只用一个`make`命令就可以完成，`make`命令会自动智能地根据当前的文件修改的情况来确定哪些文件需要重编译，从而自己编译所需要的文件和链接目标程序。

### makefile的基本规则简介

在使用这个makefile之前，还是让我们先来粗略地看一看makefile的规则。

```makefile
target ... : prerequisites ...
    command
    ...
    ...
```

  * `target`也就是一个目标文件，可以是object file，也可以是执行文件。还可以是一个标签（label）。
  * `prerequisites`就是，要生成那个`target`所需要的文件或是目标。
  * `command`也就是`make`需要执行的命令（任意的shell命令）。

这是一个文件的依赖关系，也就是说，`target`这一个或多个的目标文件依赖于`prerequisites`中的文件，其生成规则定义在 `command`中。如果`prerequisites`中有一个以上的文件比`target`文件要新，那么`command`所定义的命令就会被执行。这就是makefile的规则。也就是makefile中最核心的内容

### Runing ucore

在源代码的根目录下`make qemu`, 在makefile中运行的代码为

```makefile
.PHONY: qemu 
qemu: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
#   $(V)$(QEMU) -kernel $(UCOREIMG) -nographic
    $(V)$(QEMU) \
        -machine virt \
        -nographic \
        -bios default \
        -device loader,file=$(UCOREIMG),addr=0x80200000
```

这段代码就是我们启动qemu的命令，这段代码首先通过宏定义`$(UCOREIMG) $(SWAPIMG) $(SFSIMG)`的函数进行目标文件的构建，然后使用`qemu`语句进行qemu启动加载内核。,我们就把ucore跑起来了，运行结果如下。

```
+ cc kern/init/init.c
+ cc libs/sbi.c
+ ld bin/kernel
riscv64-unknown-elf-objcopy bin/kernel --strip-all -O binary bin/ucore.img

OpenSBI v0.6
   ____                    _____ ____ _____
  / __ \                  / ____|  _ \_   _|
 | |  | |_ __   ___ _ __ | (___ | |_) || |
 | |  | | '_ \ / _ \ '_ \ \___ \|  _ < | |
 | |__| | |_) |  __/ | | |____) | |_) || |_
  \____/| .__/ \___|_| |_|_____/|____/_____|
        | |
        |_|

Platform Name         : QEMU Virt Machine
Platform HART Features : RV64ACDFIMSU
Platform Max HARTs    : 8
Current Hart          : 0
Firmware Base         : 0x80000000
Firmware Size         : 120 KB
Runtime SBI Version   : 0.2

MIDELEG : 0x0000000000000222
MEDELEG : 0x000000000000b109
PMP0    : 0x0000000080000000-0x000000008001ffff (A)
PMP1    : 0x0000000000000000-0xffffffffffffffff (A,R,W,X)
(THU.CST) os is loading ...

```

它输出一行`(THU.CST) os is loading`, 然后进入死循环。

> #### tips:
>
> 关于Makefile的语法, 如果不熟悉, 可以查看GNU手册, 或者这份中文教程: [https://seisman.github.io/how-to-write-makefile/overview.html](https://seisman.github.io/how-to-write-makefile/overview.html)
>
> 对实验中Makefile感兴趣的同学可以阅读附录中的Makefile

## GDB调试工具使用体验

好，代码编译通过了，镜像也生成了，QEMU也能跑起来了。但是，如果代码运行的结果不符合预期，或者干脆就崩溃了，我们该怎么办？在应用程序开发中，我们可能会用printf打印日志，或者用IDE提供的图形化调试器。但在操作系统的底层世界里，我们最得力的伙伴是命令行调试工具——**GDB**。

### GDB工具的安装

首先，我们得确保手头有合适的工具。我们需要一个能理解RISC-V架构指令集的GDB。幸运的是，在我们之前下载的预编译工具链里，它已经准备好了。如果你不确定它在哪里，可以敲入以下命令来寻找：

```bash
sudo find / -name "riscv64-unknown-elf-gdb" 2>/dev/null
```

（你会发现它安静地待在工具链目录的`bin`文件夹里）

### 前置准备

#### 如何窥探一个正在运行的内核？

想象一下，你要调试一个普通的程序，比如一个C++小程序。你可能是直接启动调试器来运行它。但我们现在要调试的是一个操作系统内核，它本身就是运行在硬件之上的“环境”。我们怎么调试一个“环境”呢？

这里的一个巧妙思路是“远程调试”。我们不直接调试硬件上的内核，而是让QEMU这个模拟器来帮忙。QEMU可以扮演一个“被调试的目标”，它按照我们的要求启动内核，然后在某个端口上等待；同时，我们启动GDB这个“调试器”，去连接QEMU等待的那个端口。这样一来，GDB就能向我们报告QEMU内部那个虚拟CPU的一举一动，让我们像调试普通程序一样调试内核。

这就意味着，我们需要两个终端窗口：一个用来运行QEMU（作为被调试目标），另一个用来运行GDB（作为调试器）

#### 同时与两个程序打交道

怎么方便地同时看两个终端窗口呢？如果你使用的是VS Code和unbuntu虚拟机，可以直接开启两个终端，你也可以选择一些工具在同一个窗口里划分出两个窗格进行使用，这里推荐一个终端利器：**tmux**（如果没有使用`apt install tmux`下载）。

在命令行中输入`tmux`，你就进入了一个终端复用会话。这个界面本身看起来和普通终端没什么不同，但它的强大之处在于可以分割屏幕。按下`Ctrl+B`，然后再按`%`，你就会发现屏幕被垂直切成了两半。现在我们就拥有了两个命令行窗格，按`Ctrl+B`再按方向键，你就可以在两个窗格之间切换焦点。

tmux的大部分操作都依赖于一个前缀键（默认是`ctrl+B`），按下前缀键之后，我们可以按下其他的功能键来完成操作，以下列举一些常用的操作：

| 操作类别 | 快捷键/命令 | 说明 |
| :--- | :--- | :--- |
| **分割窗格** | `Ctrl+B %` | 垂直分割为左右两个窗格 |
| | `Ctrl+B "` | 水平分割为上下两个窗格 |
| **切换焦点** | `Ctrl+B` + 方向键（↑↓←→） | 在窗格间切换 |
| | `Ctrl+B ;` | 切换到上一个活动窗格 |
| **调整布局** | `Ctrl+B {` / `Ctrl+B }` | 与前/后一个窗格交换位置 |
| | `Ctrl+B Ctrl+方向键` | 调整窗格大小（持续按住） |
| **关闭/管理窗格**| `Ctrl+D` 或输入 `exit` | 关闭当前窗格 |
| | `Ctrl+B x` | 强制关闭当前窗格（需确认） |
| | `Ctrl+B z` | 最大化当前窗格，再次按下恢复 |
| **会话管理** | `Ctrl+B d` | 脱离当前会话，任务后台运行 |
| | `tmux attach -t 0` | 重新连接到编号为 0 的会话 |
| | `tmux new -s session_name`| 创建新会话（指定名称） |
| | `tmux ls` | 列出所有后台会话 |
| | `tmux attach -t session_name`| 连接到指定名称的会话 |
| **历史输出查看**| `Ctrl+B [` | 进入复制模式，滚动历史输出 |
| | 方向键 / `PageUp` / `PageDown` | 滚动查看历史输出 |
| | `q` | 退出复制模式 |

tmux 的强大之处在于其灵活的会话与窗格管理能力，建议多尝试上述快捷键，提升终端操作效率。

### 开调！

  * **左边窗格**，我们输入`make debug`。这个命令会启动QEMU，但特别注意，这里的参数`-S`会让虚拟CPU一启动就立刻暂停，乖乖地等我们发号施令；而参数`-s`则告诉QEMU：“打开1234端口，准备接受GDB的连接”。
  * **右边窗格**，我们输入`make gdb`。这个命令其实是一系列操作的集合：
      * `file bin/kernel`：让GDB加载我们编译好的内核文件，这个文件里包含宝贵的调试符号（函数名、变量名等）。
      * `set arch riscv:rv64`：告诉GDB，我们要调试的是RISC-V 64位的程序。
      * `target remote localhost:1234`：让GDB去连接本机（localhost）的1234端口，也就是QEMU正在等待我们的地方。

| QEMU (被调试目标) | GDB (调试器) |
| :--- | :--- |
| ` bash @DESKTOP-35HSFEH:lab0$ make debug  ` | ` bash @DESKTOP-35HSFEH:lab0$ make gdb riscv64-unknown-elf-gdb \ -ex 'file bin/kernel' \ -ex 'set arch riscv:rv64' \ -ex 'target remote localhost:1234' GNU gdb (SiFive GDB-Metal 10.1.0-2020.12.7) 10.1 Copyright (C) 2020 Free Software Foundation, Inc. ... Reading symbols from bin/kernel... The target architecture is set to "riscv:rv64". Remote debugging using localhost:1234 0x0000000000001000 in ?? () (gdb)  ` |

当GDB成功连上QEMU后，它会告诉我们：“现在程序停在了地址`0x1000`这个地方”。这里其实是QEMU内置的固件（BIOS）代码，还没执行到我们的内核。

那么我们的内核代码从哪里开始执行呢？还记得链接脚本吗？它指定了内核的入口地址。我们可以直接在这个地址上打断点，但更简单的方法是使用函数名。因为编译器帮我们把函数名和地址对应了起来（调试符号），所以我们可以直接对`kern_entry`函数下断点（`b`是`break`的缩写）：

```gdb
(gdb) b kern_entry
```

随后执行`continue`(缩写为`c`)开始执行程序，内核会在运行到我们设置好的断点处停止

```gdb
(gdb) c
Continuing.

Breakpoint 1, kern_entry () at kern/init/entry.S:10
10      la sp, bootstacktop
```

此时内核暂停在入口函数的第一条汇编指令处，我们可以检查寄存器状态或反汇编附近的代码。寄存器是 CPU 内部的高速存储单元，存放当前的计算状态和数据流向，对于理解执行上下文极为重要。特别需要注意的是：

  * **PC**（Program Counter）：指向当前正在执行的指令；
  * **SP**（Stack Pointer）：表示当前栈顶位置，关乎函数调用和局部变量的存储；
  * **a0–a7**：用于传递函数参数的寄存器，可反映初始化阶段的参数传递情况。

我们可以使用 `info registers`（可简写为 `i r`）可查看所有寄存器的值，也可以在`i r`后面加上想要查看的寄存器，例如`i r ra sp`。

```gdb
(gdb) i r # info registers
ra             0x80000a02          0x80000a02
sp             0x8001bd80          0x8001bd80
gp             0x0                 0x0
tp             0x8001be00          0x8001be00
t0             0x80200000          2149580800
t1             0x1                 1
t2             0x1                 1
fp             0x8001bd90          0x8001bd90
s1             0x8001be00          2147597824
a0             0x0                 0
a1             0x82200000          2183135232
a2             0x80200000          2149580800
a3             0x1                 1
a4             0x800               2048
a5             0x1                 1
a6             0x82200000          2183135232
a7             0x80200000          2149580800
s2             0x800095c0          2147521984
...
pc             0x80200000          0x80200000 <kern_entry>
...
```

除了上述基本命令之外，GDB 还提供了丰富的调试功能，例如：

  * `si`（Step Instruction）：以单条汇编指令为步长单步执行，适用于深入理解底层行为；
  * `bt`（BackTrace）：打印函数调用栈，帮助理解执行路径；
  * `frame <n>`：切换到调用栈的第 n 层，查看该层的局部变量和上下文；
  * `x/<format> <address>`：查看内存内容，如 `x/10x $sp` 表示以十六进制显示栈指针后的10个字。

这些工具在后续复杂的内核调试（如中断处理、虚拟内存、多任务切换等）中尤为重要。

### 实操建议

在 `kern_entry` 处停下后，不妨输入几次 `si`，亲眼看看汇编指令是如何一条条执行的。然后输入 `x/10x $sp`，看看栈初始化前的内存里到底是什么（很可能是一些垃圾数据）。这种亲手触摸底层的感觉，是学习操作系统最棒的体验之一。

### 结语

GDB是内核调试的利器，本章仅介绍了基本使用流程。实际上，GDB生态系统相当丰富：除了架构专用版本，还有`gdb-multiarch`这样的多架构支持工具；除了命令行界面，也有基于GDB的图形化前端（如VS Code的GDB扩展）。在更复杂的调试场景中，例如需要同时调试内核与用户进程，开发者还需运用GDB的复杂会话管理功能。

内核调试犹如侦探工作，需要细心观察和逻辑推理。GDB提供了窥探系统内部状态的能力，帮助开发者理解代码执行流程、分析问题根源。虽然命令行界面初期可能有些挑战，但一旦掌握，这种精准的控制和深入的洞察能力将变得无可替代。

预祝各位在后续开发中能够有效运用调试工具，既能享受解决复杂问题的成就感，也能快速定位并修复那些令人困扰的bug。记住，优秀的开发者不是不写bug，而是能够快速发现并修复bug。

## 实验报告要求

从oslab网站上取得实验代码后，进入目录`labcodes/lab1`，完成实验要求的各个练习。在实验报告中回答所有练习中提出的问题。在目录`labcodes/lab1`下存放实验报告，推荐用markdown格式。每个小组建一个gitee或者github仓库，完成实验之后，通过git push命令把代码和报告上传到仓库。最后请一定提前或按时提交到git网站。