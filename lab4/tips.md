## init.c的解读

### 核心变革（39, 44 行）—— Lab 4 的主角
这是本 Lab 最需要关注的两行代码：

```c
39|    proc_init(); // init process table
```
**解读**：这是“盘古开天地”的一刻。
*   **创建第 0 号进程（idleproc）**：在 Lab4 之前，内核跑在这一行时，它只是无名代码。`proc_init` 做了一件极其巧妙的事（你可以看 `proc.c`）：它把**当前正在使用的内核栈和环境**直接指派给了 `idleproc`。
    *   *潜台词*：“原来我一直在裸奔，现在我有了身份证，我是 PID=0。”
*   **创建第 1 号进程（initproc）**：利用 `do_fork` 机制，创建了第二个内核线程，准备打印 "Hello World"。

```c
44|    cpu_idle(); // run idle process
```
**解读**：这是“不归路”。
*   注意第 16 行的声明 `__attribute__((noreturn))`，这个函数有去无回。
*   在此之前，CPU 是一条直线执行下来的。
*   进入 `cpu_idle()` 后，内核变成了一个**死循环**（参考 `proc.c` 中的实现）。它只有一件事做：**当没有其他进程想跑时，就跑我（空转）；一旦有其他进程（如 initproc）想跑，调度器（schedule）就会介入，切换上下文。**

---

## proc.h的解读

如果说 `init.c` 是**开工仪式**，那么 `proc.h` 就是**设计蓝图**。

在 Lab4 中，我们面临的**主要矛盾**是：**如何在同一个 CPU 上，让多个程序“以此交替”运行，且互不干扰？**
为了解决这个问题，我们需要定义一个数据结构，来把一个程序的“灵魂”打包封存起来。这个数据结构就是 PCB（Process Control Block）。

`proc.h` 极其关键，它定义了操作系统如何看待一个“活着的”进程。我们用 Kaiming-Style 的视角，把这 78 行代码拆解为三个核心维度：

---

### 1. 核心维度一：快照机制 (`struct context`)
**位置**：Line 18-34

```c
struct context {
    uintptr_t ra;
    uintptr_t sp;
    uintptr_t s0; ... s11;
};
```

**深度解读**：
这是一个**极简主义**的设计。你可能会问：RISC-V 有 32 个通用寄存器，为什么这里只保存了 `ra`, `sp` 和 `s0-s11` 这 14 个？
*   **核心原理**：这是基于 RISC-V 的 Calling Convention（调用约定）。
    *   `s` 开头的寄存器是 **Callee-Saved（被调用者保存）**。这意味着，当进程 A 切换到进程 B 时，这是通过一个函数调用 `switch_to` 实现的。既然是函数调用，编译器会自动处理那些临时寄存器（`t` 开头）和参数寄存器（`a` 开头），我们只需要手动保存那些“在这个函数返回后还需要保持原样”的寄存器。
*   **主要矛盾**：`context` 是为了**进程间的平滑切换**（Switch）。它相当于游戏的“存档点”，记录了代码执行到哪一行（`ra`）以及当前的堆栈在哪（`sp`）。

---

### 2. 核心维度二：灵魂容器 (`struct proc_struct`)
**位置**：Line 42-58

这是 Lab4 最大的 BOSS。每一个运行的线程/进程，在内核里就是这么一个结构体。我们可以把它里面的字段分成四类来看：

#### A. 身份信息（Identity）
```c
int pid;                      // 身份证号
char name[PROC_NAME_LEN + 1]; // 名字（方便调试打印）
struct proc_struct *parent;   // 父亲是谁（进程树结构）
```

#### B. 核心资源（Resources）
```c
uintptr_t kstack;     // 【最关键】内核栈
struct mm_struct *mm; // 内存视野（Lab3 的成果）
uintptr_t pgdir;      // 页表基址（CR3/satp）
```
*   **点睛之笔**：`kstack` 是 Lab4 的重中之重。每个线程必须有自己独立的内核栈。如果共用一个栈，A 线程切走，B 线程往栈里压数据，A 回来时栈就乱了。**独立的栈是独立执行流的物理基础。**

#### C. 调度状态（Scheduling）
```c
enum proc_state state;      // 现在是睡着了、跑着呢、还是死了？
int runs;                   // 跑了多少次了？
volatile bool need_resched; // 这是一个“请辞书”。如果为 true，说明该让出 CPU 了。
```

#### D. 切换现场（Context Switching）—— 最容易混淆的地方
```c
struct context context; // 软切换现场
struct trapframe *tf;   // 硬中断现场
```
这里体现了操作系统的**两层抽象**：
1.  **`context` (Switch)**：用于**进程与进程之间**的切换。是内核态代码主动调度的结果（比如 `schedule()` 函数）。
2.  **`tf` (Interrupt)**：用于**用户态与内核态**之间，或者**中断发生时**的现场保存。
在 `copy_thread` 函数中，我们会把 `tf` 放在 `kstack` 的顶端，用来伪造一个“中断返回现场”，让新进程以为自己是从一次中断中醒来的。

---

### 3. 核心维度三：全局视野 (`extern` 变量)
**位置**：Line 63

```c
extern struct proc_struct *idleproc, *initproc, *current;
```

这三个变量定义了整个 OS 的权力结构：
*   **`idleproc` (PID 0)**: **大地**。它是第 0 号进程，从来不休息（死循环），只有当所有人都没活干时，CPU 才归它。它的存在是为了保证 CPU 永远有指令可跑。
*   **`initproc` (PID 1)**: **始祖**。它是第 1 号进程，是所有后续用户进程的祖先。
*   **`current`**: **当下**。指向当前占用 CPU 的那个 PCB。内核代码中凡是操作“自己”的地方，用的都是 `current`。

---

### 总结

`proc.h` 并没有什么复杂的算法，它只是定义了一个**契约**：
只要你申请了一块内存，按照 `struct proc_struct` 填好 `kstack`、设置好 `context.ra`（入口地址）和 `context.sp`（栈顶），然后把它挂到 `proc_list` 链表上，操作系统的调度器（Lab4 的核心逻辑）就能让它“活”过来。

---

## switch.S的解读

`switch.S` 是 Lab4 中**最核心、最神奇**的一段汇编代码。如果说 `proc.h` 是设计图，`init.c` 是开工仪式，那么 `switch.S` 就是那个**“移形换影”的魔术现场**。

**主要矛盾**：
CPU 只有一套寄存器，但我们有两个进程（Process A 和 Process B）。
如何让 CPU 上一秒还在跑 A 的代码，下一秒就无缝衔接地跑 B 的代码，而且让 A 和 B 都觉得“我一直拥有 CPU，从未被打断过”？

**核心解法**：
利用函数调用的**副作用**。
我们把“切换”伪装成一次普通的函数调用。当 `switch_to` 返回时，我们偷梁换柱，让 CPU 以为自己返回到了另一个函数里。

---

### 逐段 Kaiming-Style 解读

这个函数的 C 语言原型是：
```c
// 参看 proc.c 或 proc.h
void switch_to(struct context *from, struct context *to);
```
*   `a0` 寄存器：存放第一个参数 `from` 的地址（即当前进程的 `context` 结构体地址）。
*   `a1` 寄存器：存放第二个参数 `to` 的地址（即目标进程的 `context` 结构体地址）。

#### 1. 冻结现场（Save Context）
```asm
6| switch_to:
7|    # save from's registers
8|    STORE ra, 0*REGBYTES(a0)  // 保存返回地址！这是灵魂！
9|    STORE sp, 1*REGBYTES(a0)  // 保存栈指针！这是肉体！
10|   STORE s0, 2*REGBYTES(a0)  // 保存 Callee-Saved 寄存器
...
21|   STORE s11, 13*REGBYTES(a0)
```
**解读**：
*   这一步是在对当前进程说：“别动，我给你拍张照。”
*   `STORE ra, ...`：这行最关键。`ra` (Return Address) 寄存器里存的是**“谁调用了 switch_to”**。
    *   比如是 `schedule()` 函数在第 50 行调用了 `switch_to`，那么 `ra` 就指向 `schedule()` 的第 51 行。
    *   我们将这个地址存进 `from->context.ra`。这意味着：**只要将来有人把这个地址恢复回 ra 寄存器并执行 ret，CPU 就会跳回 schedule() 的第 51 行继续执行。**

#### 2. 移形换影（Restore Context）
```asm
23|    # restore to's registers
24|    LOAD ra, 0*REGBYTES(a1)   // 加载目标进程的返回地址
25|    LOAD sp, 1*REGBYTES(a1)   // 加载目标进程的栈指针
26|    LOAD s0, 2*REGBYTES(a1)   // 加载目标进程的通用寄存器
...
37|    LOAD s11, 13*REGBYTES(a1)
```
**解读**：
*   这一步是在“加载别人的存档”。
*   `a1` 指向目标进程的 `context`。
*   `LOAD sp, ...`：**瞬间切换了栈空间**。上一行指令还在用进程 A 的栈，下一行指令 CPU 就开始用进程 B 的栈了。这是物理上的切换点。
*   `LOAD ra, ...`：**预埋伏笔**。我们把进程 B 上次沉睡前保存的“断点地址”装进了 `ra` 寄存器。

#### 3. 穿越时空（Return）
```asm
39|    ret
```
**深度解读**：
*   普通的 `ret` 指令只是简单的 `pc = ra`。
*   但在这里，`ra` 已经被我们换成了进程 B 的地址。
*   所以，当 CPU 执行完这条 `ret`，它**不会**回到调用 `switch_to` 的进程 A 里面去（因为 A 的 `ra` 被存在了内存里，而不是寄存器里）。
*   相反，它会**跳转**到 `to->context.ra` 指向的地方！

---

### 一个具体的例子

假设：
1.  **进程 A** 运行到 `schedule()` 函数，调用 `switch_to(A, B)`。
2.  **进程 B** 是**刚被创建的新进程**。

**过程推演**：
1.  CPU 执行 `switch_to` 的前半段：把 A 的状态存入 `A->context`。此时 A 睡着了，它的 `context.ra` 指向 `schedule()` 函数里 `switch_to` 下面的那行代码。
2.  CPU 执行 `switch_to` 的后半段：从 `B->context` 加载数据。
    *   因为 B 是新进程（参考 `proc.c` 中的 `copy_thread`），它的 `context.ra` 被强制设置成了 `forkret` 函数的入口地址。
    *   它的 `context.sp` 指向了 B 的内核栈顶（那里预存了一个 `trapframe`）。
3.  CPU 执行 `ret`。
    *   **奇迹发生**：CPU 没有返回 A 的 `schedule()`，而是跳到了 `forkret()` 函数！
    *   对于 CPU 来说，它觉得：“哦，我刚刚结束了一个函数调用，现在我要去执行 `ra` 指向的代码了。”
    *   于是，进程 B 开始运行。

### 总结

`switch.S` 的本质就是利用 **Caller/Callee 约定**和 **栈（Stack）** 的特性，手动修改了 CPU 的指令指针（PC）和栈指针（SP）。

它是一座桥：
*   **入口**是进程 A 的 `schedule()`。
*   **出口**是进程 B 的 `schedule()`（如果是老进程）或者 `forkret`（如果是新进程）。

这就是为什么我在解读 `init.c` 时说，一旦调用 `cpu_idle`，内核就进入了一个由 `switch_to` 编织的循环时空，不再是线性的了。

---

## trap.c和trapentry.S的解读

你好！我们继续。

在 `switch.S` 中，我们解决了**进程间的主动切换**。
但除了主动切换，操作系统更重要的职责是处理**被动事件**：比如时钟中断来了、用户程序出错了、或者系统调用发生了。

`trapentry.S` 和 `trap.c` 就是 OS 的**应激反应中心**。

**主要矛盾**：
当一个突发事件（中断或异常）发生时，CPU 会强行跳转到预设的地址。我们如何在这个瞬间保存好“犯罪现场”，以便处理完事件后能像什么都没发生过一样恢复回来？

**核心解法**：
`trapentry.S` 是**急诊室护士**，负责把病人（CPU 现场）推上担架（栈），并整理好病历（TrapFrame）。
`trap.c` 是**急诊室医生**，看着病历（TrapFrame）诊断病情（分发处理逻辑）。

---

### 1. `trapentry.S`：护士的操作手册

这个汇编文件定义了所有中断的**统一入口**。在 `init.c` 的 `idt_init` 中，我们将 `stvec` 寄存器指向了这里的 `__alltraps`。

#### A. 现场打包 (`SAVE_ALL` 宏, 3-53 行)
当 CPU 遇到中断时，它会自动把 PC 跳到 `__alltraps`。
```asm
6|    csrw sscratch, sp       # 交换 sscratch 和 sp。
8|    addi sp, sp, -36 * REGBYTES # 在内核栈上腾出一块空间，大小正好是 struct trapframe
10-39| STORE x1...x31        # 把通用寄存器全部存进去
42-46| csrr s0...s4          # 把 CSR 寄存器（原因、状态、发生地址等）读出来
48-52| STORE s0...s4         # 也存进去
```
**Kaiming-Style 解读**：
还记得 `switch.S` 吗？那里只存了 Callee-Saved 寄存器。
但这里不一样。中断是**突发的**，CPU 可能在做任何事（甚至可能在算 `1+1` 的中间），所以我们必须保存**所有**寄存器（Caller-Saved + Callee-Saved），以及异常相关的 CSR 寄存器。
这一整套数据，在 C 语言里对应的就是 `struct trapframe`。

#### B. 呼叫医生 (`__alltraps`, 99-103 行)
```asm
102|    move a0, sp  # 把当前的栈指针 sp 作为第一个参数传给 trap()
103|    jal trap     # 调用 C 语言的 trap 函数
```
此时，`a0` 指向的就是刚才我们在栈上精心打包好的 `trapframe`。

#### C. 康复出院 (`RESTORE_ALL` 和 `__trapret`, 55-110 行)
当 `trap()` 函数执行完毕返回后：
```asm
108|    RESTORE_ALL  # 把栈里的数据全部恢复回寄存器
110|    sret         # 特权指令：从 Supervisor 模式返回（PC = sepc, Status = sstatus）
```
这就像倒带一样，一切恢复如初。

#### D. 特殊通道：`forkrets` (112-117 行)
这是 Lab4 的**彩蛋**，专门给**刚出生的进程**用的。
```asm
113| forkrets:
115|    move sp, a0   # 直接把栈指针指向新进程预造的 trapframe
116|    j __trapret   # 强行跳转到恢复现场的代码
```
还记得 `copy_thread` 吗？新进程的 `context.ra` 被设为了 `forkrets`。
当新进程第一次被 `switch_to` 选中时，它会跳到这里。
这里的逻辑是：**假装我刚处理完一个中断，现在要“返回”了。**
于是，新进程就通过 `__trapret` “返回”到了它在 `tf->epc` 中设定的位置（也就是 `kernel_thread_entry`）。

---

### 2. `trap.c`：医生的诊断书

`trap()` 函数只是一个**分诊台**，它根据 `tf->cause` 把请求分发给具体的处理函数。

#### A. 中断处理 (`interrupt_handler`)
**重点关注：时钟中断 (Lines 108-128)**
这是 Lab4 实现**抢占式调度**的核心动力。
```c
119| if (ticks % 100 == 0) { ... } // 每 100 次 tick 打印一次
```
在更复杂的系统（如 Lab6）中，这里会调用 `run_timer_list()` 来唤醒睡眠的进程，并检查当前进程的时间片是否用完（`current->need_resched = 1`）。但在 Lab4，我们暂时只做简单的计数和打印，证明时钟中断是活着的。

#### B. 异常处理 (`exception_handler`)
这里处理各种“意外”，比如除以零、访问非法内存等。
在 Lab4 的测试中，可能会故意触发一些异常来测试你的鲁棒性。

---

### 总结

`trapentry.S` 和 `trap.c` 在 Lab4 中的地位发生了微妙的变化：
*   在 Lab1，它们只是单纯的中断处理者。
*   在 Lab4，它们成为了**进程调度的基础设施**。
    *   `forkrets` 利用 `__trapret` 实现了新进程的启动。
    *   时钟中断利用 `interrupt_handler` 提供了调度的时机（虽然 Lab4 的调度很简单，但机制已经就位）。

**一句话概括**：
`switch.S` 负责**进程与进程**之间的横向切换；
`trapentry.S` 负责**内核与硬件**（或用户态）之间的纵向穿梭。
两者结合，构成了操作系统控制流的完整经纬网。

---

## proc.c的解读

你好！我们终于来到 Lab4 的大本营：`proc.c`。

如果说 `proc.h` 是**蓝图**，`switch.S` 是**魔法**，`trap.c` 是**应激**，那么 `proc.c` 就是**总指挥部**。这里汇聚了进程管理的**核心逻辑**。

为了把这 466 行代码讲透，我依然采用 Kaiming-Style 的视角，将其解构为**三个核心生命周期**：**诞生、运行、消亡**。

---

### 第一幕：诞生 (alloc_proc, do_fork, kernel_thread)

这是操作系统的造人工程。一个进程是如何无中生有的？

#### 1. 捏泥人：`alloc_proc` (86-125 行)
**主要矛盾**：创建一个新进程时，哪些字段必须初始化？哪些可以留空？
**核心解法**：
*   **必填项**：状态设为 `UNINIT`，PID 设为 -1（还没上户口），`cr3` (pgdir) 设为内核页表（因为大家都在内核里跑）。
*   **关键点**：Line 116 `memset(&proc->context, 0, sizeof(struct context));`。这行代码看似平淡，实则为后续的 `fork` 埋下了伏笔。上下文如果不清零，可能会有脏数据导致跳转异常。

#### 2. 注入灵魂：`do_fork` (318-383 行) —— **全场最核心函数**
**主要矛盾**：如何克隆一个现有的进程（或者创建一个全新的内核线程）？
**核心步骤**：
1.  `alloc_proc()`：先领个尸体（空壳 PCB）。
2.  `setup_kstack()`：**分配内核栈**。这是每个进程独立的物理基础。没有栈，函数调用就没法做。
3.  `copy_mm()`：处理内存。在 Lab4 只是内核线程，内存是共享的，所以这里留空。
4.  `copy_thread()`：**设置上下文**。
    *   它把父进程的 `tf` 复制到了子进程栈顶。
    *   **神来之笔**：`proc->context.ra = (uintptr_t)forkret;` (Line 307)。这决定了新进程醒来后第一眼看到的是 `forkret` 函数。
    *   `proc->context.sp = (uintptr_t)(proc->tf);` (Line 308)。把栈顶指针指到了刚刚复制好的 `tf` 上。
5.  `wakeup_proc()`：把状态改为 `PROC_RUNNABLE`，告诉调度器：“我可以接客了”。

#### 3. 辅助手段：`kernel_thread` (256-265 行)
它其实是对 `do_fork` 的一层包装。它构造了一个临时的 `trapframe`，把我们要执行的函数（比如 `init_main`）放进 `s0` 寄存器，把参数放进 `s1`。
**为什么？** 因为 `kernel_thread_entry` 这段汇编代码（没展示，但逻辑很简单）会把 `s0` 当作函数指针去调用。

---

### 第二幕：运行 (proc_run, proc_init, cpu_idle)

这是操作系统的日常运转。

#### 1. 创世纪：`proc_init` (406-455 行)
这里发生了著名的**“第 0 号进程移花接木事件”**。
*   `idleproc` 本来也是个空壳。
*   但代码直接让 `current = idleproc` (Line 440)。
*   这意味着：**当前正在运行的这段初始化代码，突然就变成了 idleproc 的一部分。**
*   接着创建 `initproc` (PID 1)，用来打印 Hello World。

#### 2. 永动机：`cpu_idle` (458-467 行)
*   这是 idle 进程的**本命工作**。
*   只要 `current->need_resched == 1`，它就交出 CPU。
*   注意：idle 进程永远是 `RUNNABLE` 的，所以调度器如果没有别的进程可选，最后兜底的总是它。

#### 3. 切换现场：`proc_run` (189-214 行)
**主要矛盾**：切换进程时，不仅要换 CPU 寄存器，还要换页表。
**核心解法**：
*   `lsatp(current->pgdir)` (Line 208)：**切换页表**。虽然 Lab4 大家用的都是内核页表，但在未来的 Lab 中，这一步意味着进入了另一个进程的内存世界。
*   `switch_to(...)` (Line 210)：**切换执行流**。这一行代码执行完，CPU 就跑到别人的栈上去跑代码了。

---

### 第三幕：消亡 (do_exit)

**位置**：Line 387
在 Lab4 中，这个函数只有一个 `panic`。这意味着：**只管生，不管埋。**
我们的进程（`init_main`）跑完后，应该调用 `do_exit` 来回收资源。但在 Lab4，我们只需要验证调度机制，所以还没有实现完整的回收逻辑（那是 Lab5 的事）。

---

### 总结：Lab4 的核心剧情

`proc.c` 导演了一出好戏：

1.  **初始化**：内核启动，自己把自己封为 `idleproc`。
2.  **造人**：`idleproc` 觉得太孤单，用 `do_fork` 创造了 `initproc`。
3.  **交权**：`idleproc` 进入 `cpu_idle` 死循环，大喊一声 `schedule()`。
4.  **上位**：调度器发现 `initproc` 也是 `RUNNABLE` 的，于是 `switch_to` 切换过去。
5.  **执行**：`initproc` 醒来（在 `forkret` 里），打印 "Hello World"。
6.  **落幕**：`initproc` 打印完毕，因为没有实现 `exit`，系统可能会在这里 panic 或者停住，但这正是 Lab4 预期的终点。

这就是 Lab4 的全部奥义。它搭建了一个**最小完备**的多线程内核骨架。

---

## sched.c的解读

你好！这是 Lab4 上下文解读的最后一站：`sched.c`。

如果说 `switch.S` 是切换的**机制**（How），那么 `sched.c` 就是切换的**策略**（When & Who）。
它回答了两个终极问题：
1.  **When**：什么时候把一个睡着的进程叫醒？（`wakeup_proc`）
2.  **Who**：下一个让谁来用 CPU？（`schedule`）

虽然这个文件很短（仅 42 行），但它实现了一个最朴素的调度算法：**FIFO（先进先出）轮转调度**。

---

### 1. 唤醒机制：`wakeup_proc` (7-11 行)

```c
8| wakeup_proc(struct proc_struct *proc) {
9|    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
10|   proc->state = PROC_RUNNABLE;
11|}
```

**Kaiming-Style 解读**：
这个函数极其简单，甚至有点“反常识”。
*   它**不**涉及上下文切换。
*   它**不**涉及把进程加入运行队列（因为 Lab4 的进程只要被创建了，就一直挂在全局 `proc_list` 链表里，不像 Linux 那样有专门的 `runqueue`）。
*   它做的唯一一件事就是：**改状态**。把进程从“睡着”改成“醒着”（`PROC_RUNNABLE`）。
*   **潜台词**：调度器（`schedule`）在遍历链表时，只认 `PROC_RUNNABLE` 的进程。所以只要状态一改，下次调度器路过时就会选中它。

---

### 2. 核心调度器：`schedule` (13-40 行)

这是操作系统的**心跳**。每当一个进程时间片用完，或者主动放弃 CPU（比如调用 `yield` 或 `wait`），都会走到这里。

#### A. 关中断保护 (18, 39 行)
```c
18|    local_intr_save(intr_flag);
...
39|    local_intr_restore(intr_flag);
```
**核心矛盾**：调度过程涉及对全局链表 `proc_list` 的读取。如果读到一半，时钟中断来了，又触发了一次调度，那就乱套了（重入问题）。所以必须关中断，保证原子性。

#### B. 寻找下一个“天选之子” (21-30 行)
这是一个典型的**循环查找（Round Robin）**逻辑：
1.  **起点**：从当前进程 `current` 的下一个位置开始找（Line 21-22）。这样保证了公平性，不会老是选中同一个进程。
2.  **遍历**：沿着 `proc_list` 链表转圈圈。
3.  **筛选**：
    *   跳过链表头节点（`&proc_list`）。
    *   **关键判据**：`if (next->state == PROC_RUNNABLE)`。只有醒着的进程才有资格被调度。
4.  **终点**：只要找到第一个符合条件的，立刻 `break`。

#### C. 兜底逻辑 (31-33 行)
```c
31|        if (next == NULL || next->state != PROC_RUNNABLE) {
32|            next = idleproc;
33|        }
```
**深度解读**：
如果把链表转了一圈，发现大家都在睡觉（或者没有别的进程），怎么办？
**不能让 CPU 停下来**。于是，把那个永远不睡觉的备胎 `idleproc` 拿出来用。
这就是为什么 `idleproc` 必须存在且必须是 `RUNNABLE` 的。

#### D. 真正切换 (35-37 行)
```c
35|        if (next != current) {
36|            proc_run(next);
37|        }
```
*   如果找到的还是自己（比如系统中只有一个进程），那就不用切了，继续跑。
*   如果找到了别人，就调用 `proc_run`。
    *   **注意**：`proc_run` 里面会调用 `switch_to`。一旦调用，当前进程就在这里**停住**了（Freeze）。
    *   **复活点**：等到下一次别的进程再调用 `schedule` 并选中“我”时，CPU 会从 `proc_run` 返回，继续执行第 39 行的 `local_intr_restore`。

---

### 总结：Lab4 的调度全景图

至此，我们已经看完了 Lab4 的所有关键拼图。让我们把它们串起来，形成一个完整的**闭环**：

1.  **`init.c`**：系统启动，创建 `idleproc` 和 `initproc`。
2.  **`proc.c` (`do_fork`)**：`initproc` 被创建，状态设为 `RUNNABLE`，`context.ra` 设为 `forkret`。
3.  **`proc.c` (`cpu_idle`)**：`idleproc` 运行，发现没事干，调用 `schedule`。
4.  **`sched.c` (`schedule`)**：遍历链表，发现 `initproc` 是醒着的，决定切换给它。
5.  **`switch.S` (`switch_to`)**：保存 `idleproc` 的现场，加载 `initproc` 的现场。
6.  **`trapentry.S` (`forkrets`)**：`initproc` 第一次醒来，假装从中断返回。
7.  **`trap.c` (`trap`)**：时钟中断不断发生，驱动着这个循环不停转动。

这就是一个现代操作系统最原始、最核心的模样。它不再是一条直线的程序，而是一个**永不停止的循环舞台**，进程们在其中你方唱罢我登场。

---

## 各个组件是怎么串联起来的

你好！我们已经完成了对 Lab4 所有关键组件（`init.c`, `proc.h`, `proc.c`, `switch.S`, `trap.c`, `sched.c`）的“Kaiming-Style”深度解剖。

现在，让我们跳出单文件的视角，站在上帝视角，用一根红线把它们串起来，看看这个**“最小内核线程调度系统”**是如何运转的。

**核心主线**：从**线性执行**到**循环调度**的质变。

---

### 第一阶段：创世纪（Genesis）
**关键词：线性初始化**

1.  **CPU 上电**：此时内核是单线程的，像传统的 C 程序一样一行行跑。
2.  **`kern_init` (`init.c`)**：
    *   这是初始化的主干道。
    *   它依次初始化了内存 (`pmm_init`, `vmm_init`) 和中断 (`pic_init`, `idt_init`)。
    *   **关键点**：此时没有“进程”的概念，CPU 处于裸奔状态。

### 第二阶段：造人（Incarnation）
**关键词：赋予身份**

1.  **`proc_init` (`proc.c`)**：
    *   **第 0 号进程 (`idleproc`)**：内核突然“自我觉醒”。它把当前的执行环境（栈、页表）指派给了一个空的 PCB `idleproc`。从此，裸奔的代码有了身份证 (PID=0)。
    *   **第 1 号进程 (`initproc`)**：`idleproc` 觉得孤独，于是调用 `kernel_thread` -> `do_fork`。
2.  **`do_fork` (`proc.c`)**：
    *   它不仅克隆了 PCB，更重要的是**预设了未来**。
    *   它为 `initproc` 分配了独立的内核栈 (`kstack`)。
    *   它伪造了一个上下文 (`context`)，把 `ra` 设为 `forkret`，把 `sp` 设为栈顶的 `trapframe`。
    *   此时，`initproc` 虽然还没跑，但已经在就绪队列（`proc_list`）里排队了，状态为 `RUNNABLE`。

### 第三阶段：交权（Handover）
**关键词：进入死循环**

1.  **`cpu_idle` (`proc.c`)**：
    *   `kern_init` 的最后一行调用了这个函数。
    *   这标志着线性初始化的结束。`idleproc` 进入了一个 `while(1)` 死循环。
    *   它不停地问：“有人要插队吗？” (`current->need_resched`)。
    *   因为 `initproc` 刚被创建且是 `RUNNABLE` 的，所以调度器立刻介入。

### 第四阶段：切换（Switch）
**关键词：移形换影**

1.  **`schedule` (`sched.c`)**：
    *   它遍历链表，一眼相中了 `initproc`。
    *   它调用 `proc_run(initproc)`。
2.  **`switch_to` (`switch.S`)**：
    *   这是物理层面的切换。
    *   它保存了 `idleproc` 的寄存器（停在 `schedule` 函数里）。
    *   它加载了 `initproc` 的寄存器。
    *   **神奇一刻**：当 `ret` 指令执行时，CPU 并没有回到 `idleproc`，而是跳到了 `initproc` 预设的入口 —— `forkret`。

### 第五阶段：重生（Resurrection）
**关键词：伪装返回**

1.  **`forkret` (`proc.c`)**：
    *   这是新进程的“第一声啼哭”。
    *   它调用 `forkrets` (`trapentry.S`)。
2.  **`forkrets` (`trapentry.S`)**：
    *   它把栈指针 `sp` 强行指向 `initproc` 内核栈顶的 `trapframe`。
    *   然后跳转到 `__trapret`。
3.  **`__trapret`**：
    *   它执行 `RESTORE_ALL` 和 `sret`。
    *   CPU 以为自己刚刚处理完一个中断，于是心满意足地“返回”到了 `trapframe->epc` 指向的地方。
    *   而这个地方，正是我们预设的 `kernel_thread_entry` -> `init_main`。
    *   于是，屏幕上打印出了 "Hello World"。

### 第六阶段：轮回（Samsara）
**关键词：时钟驱动**

1.  **时钟中断**：
    *   每隔一段时间，硬件产生时钟中断。
    *   CPU 跳转到 `trapentry.S` -> `trap.c`。
2.  **`trap` (`trap.c`)**：
    *   `interrupt_handler` 捕获时钟中断，计数 `ticks`。
    *   （在更完善的 Lab 中）这里会设置 `need_resched = 1`，强迫当前进程让出 CPU。
3.  **调度闭环**：
    *   如果当前进程被标记为 `need_resched`，中断返回前会调用 `schedule`。
    *   于是，控制权又回到了**第四阶段**，周而复始。

---

### 总结

整个 Lab4 其实就是构建了一个**“永不停止的舞台”**：

*   **`proc_struct`** 是演员的简历。
*   **`kstack`** 是演员的更衣室（独立空间）。
*   **`schedule`** 是导演，决定谁上台。
*   **`switch.S`** 是旋转舞台，负责快速换景。
*   **`trap`** 是舞台监督，负责打断表演（处理中断）或响应演员请求（系统调用）。
*   **`forkret`** 是新演员的登场特效。

理解了这套机制，你就理解了现代操作系统多任务并发的基石。