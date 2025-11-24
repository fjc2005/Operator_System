# Lab 4 实验报告：进程管理

姓名: 黄俊雄 学号: 2313896
姓名: 付嘉晨 学号: 2313903
姓名: 王文轩 学号: 2311058

## 实验概述

本实验在前面实验的基础上，完成了内核线程管理机制的实现。通过引入进程控制块、上下文切换和调度器，实现了多线程并发执行，使内核能够调度多个执行实体轮流使用 CPU 运行。

实验主要完成了以下内容：
1. 完善虚拟内存管理，实现基本的地址空间结构
2. 引入内核线程机制，实现多执行流并发运行能力
3. 实现进程创建、切换和调度的基本功能

---

## 练习 0：填写已有实验

本实验依赖实验 2 和实验 3 的代码。已将相关代码填入本实验中标有 `LAB2` 和 `LAB3` 注释的相应部分。

主要包括：
- LAB2：物理内存管理相关代码（`pmm.c` 中的 `get_pte`、`page_insert`、`page_remove` 等函数）
- LAB3：页表管理和虚拟内存映射相关代码

---

## 练习 1：分配并初始化一个进程控制块

### 设计实现过程

`alloc_proc` 函数负责分配并返回一个新的 `struct proc_struct` 结构，用于存储新建立的内核线程的管理信息。该函数的主要任务是对进程控制块的各个字段进行正确的初始化。

#### 实现代码（`kern/process/proc.c` 第 86-125 行）

```c
static struct proc_struct *
alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL)
    {
        proc->state = PROC_UNINIT;                     // 状态初始化为未初始化
        proc->pid = -1;                                // PID初始化为无效值-1
        proc->runs = 0;                                // 运行时间为0
        proc->kstack = 0;                              // 内核栈尚未分配
        proc->need_resched = 0;                        // 不需要调度
        proc->parent = NULL;                           // 父进程为空
        proc->mm = NULL;                               // 内核线程共享内核内存，mm为空
        memset(&proc->context, 0, sizeof(struct context)); // 上下文清零
        proc->tf = NULL;                               // 中断帧指针为空
        proc->pgdir = boot_pgdir_pa;                   // 内核线程页表即内核页表
        proc->flags = 0;                               // 进程标志初始化为0
        memset(proc->name, 0, sizeof(proc->name));     // 进程名称清空
        list_init(&proc->list_link);                   // 初始化进程链表链接
        list_init(&proc->hash_link);                   // 初始化进程哈希链表链接
    }
    return proc;
}
```

#### 初始化说明

各字段初始化的原因如下：

| 字段 | 初始值 | 原因 |
|------|--------|------|
| `state` | `PROC_UNINIT` | 进程刚分配，处于未初始化状态 |
| `pid` | -1 | 尚未分配有效的进程ID |
| `runs` | 0 | 进程还未运行过 |
| `kstack` | 0 | 内核栈还未分配 |
| `need_resched` | 0 | 刚创建的进程不需要立即调度 |
| `parent` | NULL | 父进程指针稍后设置 |
| `mm` | NULL | 内核线程共享内核内存空间，不需要独立的mm结构 |
| `context` | 全0 | 上下文寄存器清零，避免脏数据 |
| `tf` | NULL | 中断帧稍后在copy_thread中设置 |
| `pgdir` | `boot_pgdir_pa` | 内核线程使用内核页表 |
| `flags` | 0 | 进程标志位初始化为0 |
| `name` | 全0 | 进程名称清空 |

**关键点**：`memset(&proc->context, 0, sizeof(struct context))` 非常重要。如果 context 里有脏数据，在进程切换时恢复寄存器可能会导致 PC 指针跳到非法地址，引发严重错误。

### 问题回答

**问题**：请说明 `proc_struct` 中 `struct context context` 和 `struct trapframe *tf` 成员变量含义和在本实验中的作用是啥？

**回答**：

#### `struct context context` - 进程上下文

**含义**：
- `context` 保存了进程执行的上下文，即进程切换时需要保存和恢复的寄存器状态
- 包含 `ra`（返回地址）、`sp`（栈指针）和 `s0-s11`（被调用者保存寄存器）共 14 个寄存器

**作用**：
1. **进程切换的基础**：在 `switch_to` 函数中，通过保存当前进程的 context 和恢复目标进程的 context 来实现进程切换
2. **维护执行流的连续性**：`context.ra` 保存了进程被切换出去时的返回地址，当进程再次被调度时，会从这个地址继续执行
3. **栈切换**：`context.sp` 保存了进程的栈指针，切换时会同时切换到新进程的栈空间

**为什么只保存这些寄存器**：
根据 RISC-V 的 Calling Convention，函数调用时：
- Caller-Saved 寄存器（如 `t0-t6`、`a0-a7`）由调用者保存，编译器会自动生成代码处理
- Callee-Saved 寄存器（如 `s0-s11`）由被调用者保存，需要我们手动保存
- 进程切换通过函数调用 `switch_to` 实现，因此只需手动保存 Callee-Saved 寄存器

#### `struct trapframe *tf` - 中断帧

**含义**：
- `tf` 保存了进程在发生中断/异常时的完整 CPU 状态
- 包含所有通用寄存器（`x0-x31`）和关键 CSR 寄存器（`status`、`epc`、`cause` 等）

**作用**：
1. **中断处理**：当进程从用户态陷入内核态或发生中断时，硬件和软件协同将 CPU 状态保存到 trapframe 中
2. **系统调用参数传递**：系统调用的参数通过 trapframe 中的寄存器传递，返回值也通过修改 trapframe 返回
3. **新进程的启动**：对于新创建的进程，通过构造一个特殊的 trapframe（包含入口地址 `epc`、函数指针 `s0`、参数 `s1` 等），然后通过 `forkrets` 和 `__trapret` 恢复这个 trapframe，实现新进程的"伪装启动"

**两者的区别**：

| 对比项 | context | trapframe |
|--------|---------|-----------|
| **保存时机** | 进程主动切换（`schedule`） | 中断/异常发生时 |
| **保存内容** | 14个寄存器（Callee-Saved） | 所有寄存器 + CSR |
| **保存位置** | 进程控制块内部 | 内核栈顶 |
| **恢复方式** | `switch_to` 函数 | `__trapret` + `sret` 指令 |
| **使用场景** | 进程间切换 | 中断返回、新进程启动 |

**在本实验中的协同作用**：
1. 新进程创建时，`copy_thread` 将 trapframe 放置在内核栈顶，并将 `context.ra` 设为 `forkret`，`context.sp` 指向 trapframe
2. 第一次调度到新进程时，`switch_to` 恢复 context，跳转到 `forkret`
3. `forkret` 调用 `forkrets`，通过 `__trapret` 恢复 trapframe，实现新进程的启动
4. 这样，新进程就从 trapframe 的 `epc` 字段指定的地址（`kernel_thread_entry`）开始执行

---

## 练习 2：为新创建的内核线程分配资源

### 设计实现过程

`do_fork` 函数是创建新进程的核心函数，负责为新进程分配资源并初始化。根据实验指导，需要完成以下步骤：

#### 实现代码（`kern/process/proc.c` 第 316-381 行）

```c
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS)
    {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    
    // 1. 调用 alloc_proc 分配进程控制块
    if ((proc = alloc_proc()) == NULL) {
        goto fork_out;
    }
    proc->parent = current; // 设置父进程为当前进程
    
    // 2. 调用 setup_kstack 为子进程分配内核栈
    if(setup_kstack(proc) != 0){
        goto bad_fork_cleanup_proc;
    }
    
    // 3. 调用 copy_mm 复制或共享内存管理信息
    // 对于内核线程，这一步实际上是空操作，因为内核线程共享内核内存
    if(copy_mm(clone_flags, proc) != 0){
        goto bad_fork_cleanup_kstack;
    }
    
    // 4. 调用 copy_thread 设置进程的中断帧和上下文
    copy_thread(proc, stack, tf);
    
    // 5. 将新进程添加到进程列表和哈希表（临界区，需要关中断）
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();              // 获取唯一PID
        hash_proc(proc);                     // 加入哈希表
        list_add(&proc_list, &(proc->list_link)); // 加入进程链表
        nr_process = nr_process + 1;         // 进程数+1
    }
    local_intr_restore(intr_flag);
    
    // 6. 调用 wakeup_proc 唤醒新进程，使其变为可运行状态
    wakeup_proc(proc);
    
    // 7. 返回新进程的 PID
    ret = proc->pid;
    
fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}
```

#### 实现要点

1. **资源分配顺序**：按照 PCB → 内核栈 → 内存信息 → 上下文 的顺序分配，确保依赖关系正确

2. **错误处理**：使用 `goto` 标签实现多级错误回滚，避免资源泄漏
   - 如果 `setup_kstack` 失败，只需释放 PCB
   - 如果 `copy_mm` 失败，需要释放内核栈和 PCB

3. **临界区保护**：在修改全局数据结构（进程链表、哈希表、进程计数）时使用 `local_intr_save` 和 `local_intr_restore` 关闭中断，保证操作的原子性

4. **进程状态转换**：通过 `wakeup_proc` 将新进程状态设为 `PROC_RUNNABLE`，使其可以被调度器选中

### 问题回答

**问题**：请说明 ucore 是否做到给每个新 fork 的线程一个唯一的 id？请说明你的分析和理由。

**回答**：

是的，ucore 做到了给每个新 fork 的线程一个唯一的 ID。具体分析如下：

#### `get_pid` 函数的实现机制（`kern/process/proc.c` 第 145-185 行）

```c
static int get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    
    if (++last_pid >= MAX_PID)
    {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid)  // 发现冲突
            {
                if (++last_pid >= next_safe)
                {
                    if (last_pid >= MAX_PID)
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;  // 重新搜索
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid)
            {
                next_safe = proc->pid;  // 更新下一个安全检查点
            }
        }
    }
    return last_pid;
}
```

#### 唯一性保证机制

1. **冲突检测**：
   - `get_pid` 使用两个静态变量 `last_pid` 和 `next_safe` 来追踪 PID 分配状态
   - 每次分配时，先递增 `last_pid`，然后检查是否与现有进程冲突
   - 如果 `last_pid >= next_safe`，需要遍历整个进程链表检查冲突

2. **冲突解决**：
   - 当发现 `proc->pid == last_pid` 时，说明有冲突，递增 `last_pid` 并重新检查
   - 通过 `goto repeat` 重新开始搜索，直到找到一个未被使用的 PID

3. **优化策略**：
   - `next_safe` 记录了下一个需要全面检查的临界点
   - 如果 `last_pid < next_safe`，说明在 `[last_pid, next_safe)` 区间内没有已分配的 PID，可以直接使用
   - 这避免了每次分配都遍历整个进程链表，提高了效率

4. **循环复用**：
   - 当 `last_pid >= MAX_PID` 时，重置为 1（PID 0 保留给 idle 进程）
   - 这样实现了 PID 的循环复用，避免 PID 耗尽

#### 原子性保证

在 `do_fork` 函数中，`get_pid` 的调用被包裹在 `local_intr_save` 和 `local_intr_restore` 之间：

```c
bool intr_flag;
local_intr_save(intr_flag);
{
    proc->pid = get_pid();
    hash_proc(proc);
    list_add(&proc_list, &(proc->list_link));
    nr_process++;
}
local_intr_restore(intr_flag);
```

这确保了：
- PID 分配和进程加入链表是原子操作
- 不会出现两个进程同时调用 `get_pid` 获得相同 PID 的情况
- 新进程加入链表后，后续的 `get_pid` 调用能立即看到这个新的 PID

#### 结论

通过**冲突检测、冲突解决、优化策略和原子性保证**四重机制，ucore 确保了每个新 fork 的线程都能获得一个唯一的 PID。即使在高并发创建进程的场景下，也不会出现 PID 冲突的情况。

---

## 练习 3：编写 proc_run 函数

### 设计实现过程

`proc_run` 函数用于将指定的进程切换到 CPU 上运行。该函数需要完成页表切换、上下文切换，并保证整个过程的原子性。

#### 实现代码（`kern/process/proc.c` 第 189-214 行）

```c
void proc_run(struct proc_struct *proc)
{
    if (proc != current)
    {
        bool intr_flag;
        struct proc_struct *prev = current; // 保存当前进程
        local_intr_save(intr_flag);         // 1. 禁用中断，保存中断状态
        {
            current = proc;                 // 2. 将当前进程切换为目标进程
            lsatp(current->pgdir);          // 3. 切换页表：加载新进程的页目录基址到 satp 寄存器
            switch_to(&(prev->context), &(current->context)); // 4. 执行上下文切换
        }
        local_intr_restore(intr_flag);      // 5. 恢复中断状态
    }
}
```

#### 实现要点

1. **相同进程检查**：
   - 如果 `proc == current`，说明目标进程就是当前进程，无需切换，直接返回

2. **关闭中断**：
   - 使用 `local_intr_save(intr_flag)` 关闭中断
   - 保证进程切换过程不被打断，避免竞态条件

3. **更新 current 指针**：
   - 将全局变量 `current` 指向新进程
   - 后续代码（包括中断处理）将看到新的 `current` 值

4. **切换页表**：
   - 调用 `lsatp(current->pgdir)` 将新进程的页目录基址加载到 `satp` 寄存器
   - 在 RISC-V 中，`satp` 寄存器控制地址翻译，修改它就切换了地址空间
   - 对于内核线程，虽然都使用内核页表，但保持这个操作是良好的习惯

5. **上下文切换**：
   - 调用 `switch_to(&(prev->context), &(current->context))`
   - 这是一个汇编函数，负责：
     - 保存当前进程的寄存器到 `prev->context`
     - 恢复新进程的寄存器从 `current->context`
     - 通过修改 `ra` 和 `sp` 实现执行流的切换

6. **恢复中断**：
   - 使用 `local_intr_restore(intr_flag)` 恢复中断状态
   - 注意：这行代码不一定是原来的进程执行的！
   - 当原进程再次被调度回来时，会从 `switch_to` 返回，继续执行这行代码

#### 为什么先切换页表再切换上下文？

虽然在 Lab4 中所有内核线程共享同一个页表，但在未来的用户进程中：
- 每个进程有独立的用户虚拟地址空间
- 进程的栈位于其虚拟地址空间中
- 如果不先切换页表，新的栈指针（SP）在旧页表中可能是未映射的，会立即触发缺页异常

因此，必须先 `lsatp` 切换页表，再 `switch_to` 切换栈和寄存器。

### 问题回答

**问题**：在本实验的执行过程中，创建且运行了几个内核线程？

**回答**：

在本实验中，创建并运行了 **2 个内核线程**：

#### 1. 第 0 号内核线程：`idleproc`

**创建过程**（`kern/process/proc.c` 第 414-440 行）：
```c
if ((idleproc = alloc_proc()) == NULL)
{
    panic("cannot alloc idleproc.\n");
}
// ...
idleproc->pid = 0;
idleproc->state = PROC_RUNNABLE;
idleproc->kstack = (uintptr_t)bootstack;
idleproc->need_resched = 1;
set_proc_name(idleproc, "idle");
nr_process++;

current = idleproc;
```

**特点**：
- **PID = 0**
- **创建方式特殊**：不是通过 `do_fork` 创建，而是直接将当前的执行环境"封装"为一个进程
- **内核栈**：使用启动时的 `bootstack`
- **作用**：系统空闲时的占位进程，在没有其他可运行进程时执行
- **执行内容**：运行 `cpu_idle()` 函数，在死循环中检查 `need_resched`，有需要时调用 `schedule()`

#### 2. 第 1 号内核线程：`initproc`

**创建过程**（`kern/process/proc.c` 第 442-449 行）：
```c
int pid = kernel_thread(init_main, "Hello world!!", 0);
if (pid <= 0)
{
    panic("create init_main failed.\n");
}

initproc = find_proc(pid);
set_proc_name(initproc, "init");
```

**特点**：
- **PID = 1**
- **创建方式**：通过 `kernel_thread` → `do_fork` 创建
- **内核栈**：通过 `setup_kstack` 动态分配
- **作用**：第一个通过正常流程创建的内核线程，是所有后续用户进程的祖先
- **执行内容**：运行 `init_main()` 函数，打印 "Hello world!!" 等信息

#### 执行流程

1. **系统启动**：
   - `kern_init()` 依次完成各子系统初始化
   - 调用 `proc_init()` 创建 `idleproc` 和 `initproc`
   - 此时 `current = idleproc`，`initproc` 状态为 `PROC_RUNNABLE`

2. **进入调度循环**：
   - `kern_init()` 最后调用 `cpu_idle()`
   - `idleproc` 检查到 `need_resched = 1`，调用 `schedule()`

3. **调度到 initproc**：
   - `schedule()` 遍历进程链表，发现 `initproc` 是 `PROC_RUNNABLE` 的
   - 调用 `proc_run(initproc)` 切换到 `initproc`

4. **initproc 执行**：
   - 通过 `switch_to` → `forkret` → `forkrets` → `__trapret` → `kernel_thread_entry` → `init_main`
   - 打印 "Hello world!!" 后返回

5. **返回 idle**：
   - `initproc` 执行完毕后，再次调度回 `idleproc`
   - 系统在 `idleproc` 的 `cpu_idle()` 循环中持续运行

#### 验证

可以通过查看 `make qemu` 的输出验证：
```
alloc_proc() correct!
this initproc, pid = 1, name = "init"
To U: "Hello world!!".
To U: "en.., Bye, Bye. :)"
```

说明两个内核线程都成功创建并运行。

---

## 扩展练习 Challenge

**问题**：说明语句 `local_intr_save(intr_flag);....local_intr_restore(intr_flag);` 是如何实现开关中断的？

### 实现机制分析

这两个宏定义在 `kern/sync/sync.h` 中：

#### 宏定义

```c
#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)

#define local_intr_restore(x) __intr_restore(x);
```

#### 底层实现函数

```c
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {  // 检查中断是否开启
        intr_disable();                      // 关闭中断
        return 1;                            // 返回1表示之前中断是开启的
    }
    return 0;                                // 返回0表示之前中断是关闭的
}

static inline void __intr_restore(bool flag) {
    if (flag) {                              // 如果之前中断是开启的
        intr_enable();                       // 重新开启中断
    }
}
```

### 工作原理

#### 1. `local_intr_save(intr_flag)` - 保存并关闭中断

**步骤**：
1. 读取 `sstatus` CSR 寄存器（RISC-V Supervisor 模式状态寄存器）
2. 检查 `SSTATUS_SIE` 位（Supervisor Interrupt Enable）
   - 该位为 1：表示中断当前是开启的
   - 该位为 0：表示中断当前是关闭的
3. 如果中断开启，调用 `intr_disable()` 关闭中断，并返回 1
4. 如果中断已关闭，直接返回 0
5. 将返回值保存到 `intr_flag` 变量中

**关键**：不仅关闭中断，还记录了中断的原始状态

#### 2. `local_intr_restore(intr_flag)` - 恢复中断状态

**步骤**：
1. 检查 `intr_flag` 的值
2. 如果 `intr_flag == 1`（之前中断是开启的），调用 `intr_enable()` 重新开启中断
3. 如果 `intr_flag == 0`（之前中断就是关闭的），不做任何操作，保持中断关闭

**关键**：恢复到中断的原始状态，而不是无条件开启中断

#### 3. `intr_disable()` 和 `intr_enable()` 的实现

这两个函数定义在 `kern/driver/intr.c` 中：

```c
void intr_enable(void) { 
    set_csr(sstatus, SSTATUS_SIE); 
}

void intr_disable(void) { 
    clear_csr(sstatus, SSTATUS_SIE); 
}
```

- `set_csr(sstatus, SSTATUS_SIE)`：将 `sstatus` 寄存器的 `SIE` 位置 1，开启中断
- `clear_csr(sstatus, SSTATUS_SIE)`：将 `sstatus` 寄存器的 `SIE` 位清 0，关闭中断

这两个操作通过 RISC-V 的 CSR 指令（`csrs` 和 `csrc`）实现，是原子操作。

### 使用场景和意义

#### 为什么需要这种机制？

在操作系统中，某些代码段需要保证**原子性**（不可被中断打断），例如：

1. **修改全局数据结构**：
   ```c
   local_intr_save(intr_flag);
   {
       list_add(&proc_list, &(proc->list_link));
       nr_process++;
   }
   local_intr_restore(intr_flag);
   ```
   如果不关中断，可能在 `list_add` 和 `nr_process++` 之间发生中断，导致数据不一致。

2. **进程切换**：
   ```c
   local_intr_save(intr_flag);
   {
       current = proc;
       lsatp(current->pgdir);
       switch_to(&(prev->context), &(current->context));
   }
   local_intr_restore(intr_flag);
   ```
   如果在切换过程中发生中断，可能导致 `current` 指针与实际运行的进程不一致。

#### 为什么要保存原始状态？

考虑嵌套调用的情况：
```c
// 外层函数
local_intr_save(flag1);     // 假设此时中断是开启的，flag1=1，关闭中断
{
    // 一些操作
    inner_function();        // 调用内层函数
    // 一些操作
}
local_intr_restore(flag1);  // 恢复中断（开启）

// 内层函数
void inner_function() {
    local_intr_save(flag2);     // 此时中断已关闭，flag2=0
    {
        // 一些操作
    }
    local_intr_restore(flag2);  // 保持中断关闭
}
```

如果 `local_intr_restore` 无条件开启中断，那么从 `inner_function` 返回后，中断就被提前开启了，导致外层的临界区失效。

通过保存和恢复原始状态，可以正确处理嵌套的临界区。

### 总结

`local_intr_save` 和 `local_intr_restore` 实现了一个优雅的临界区保护机制：
1. 通过操作 `sstatus` 寄存器的 `SIE` 位控制中断开关
2. 保存中断的原始状态，恢复时根据原始状态决定是否开启
3. 支持嵌套调用，不会破坏外层临界区的保护
4. 代码简洁，通过宏实现了良好的封装

这是操作系统内核中实现同步和互斥的基础机制之一。

---

## 思考题

### 思考题 1：get_pte() 函数中代码的相似性

**问题**：`get_pte()` 函数中有两段形式类似的代码，结合 sv32，sv39，sv48 的异同，解释这两段代码为什么如此相像。

**回答**：

#### `get_pte()` 函数的实现

```c
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    // 第一段：处理一级页表（PDX1）
    pde_t *pdep1 = &pgdir[PDX1(la)];
    if (!(*pdep1 & PTE_V))
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    
    // 第二段：处理二级页表（PDX0）
    pde_t *pdep0 = &((pte_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
    if (!(*pdep0 & PTE_V))
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
}
```

#### sv32、sv39、sv48 的异同

这三种分页模式都是 RISC-V 架构定义的多级页表机制：

| 特性 | sv32 | sv39 | sv48 |
|------|------|------|------|
| **虚拟地址位数** | 32 位 | 39 位 | 48 位 |
| **物理地址位数** | 34 位 | 56 位 | 56 位 |
| **页表级数** | 2 级 | 3 级 | 4 级 |
| **页大小** | 4KB | 4KB | 4KB |
| **每级索引位数** | 10 位 | 9 位 | 9 位 |
| **页内偏移位数** | 12 位 | 12 位 | 12 位 |

**地址结构**：

- **sv32**：VPN[1] (10bit) | VPN[0] (10bit) | Offset (12bit)
- **sv39**：VPN[2] (9bit) | VPN[1] (9bit) | VPN[0] (9bit) | Offset (12bit)
- **sv48**：VPN[3] (9bit) | VPN[2] (9bit) | VPN[1] (9bit) | VPN[0] (9bit) | Offset (12bit)

#### 为什么代码如此相像？

**原因 1：多级页表的统一递归结构**

多级页表的核心思想是**将地址翻译过程分解为多个相同的步骤**：
1. 从虚拟地址中提取当前级的索引（VPN[i]）
2. 在当前级页表中查找对应的表项
3. 如果表项无效（PTE_V=0）且允许创建，分配新页作为下一级页表
4. 通过表项中的 PPN 找到下一级页表的物理地址
5. 重复步骤 1-4，直到最后一级页表

**每一级的处理逻辑完全相同**：
- 检查表项是否有效
- 如果无效且 `create=true`，分配新页并初始化
- 通过表项中的地址进入下一级

**原因 2：代码的可扩展性**

uCore 使用 sv39（3 级页表），但代码结构设计考虑了：
- 向下兼容 sv32（2 级页表）：只需去掉第一段代码
- 向上扩展到 sv48（4 级页表）：只需再加一段相同结构的代码

这种设计使得代码可以通过简单的复制-修改来支持不同的分页模式。

**原因 3：硬件设计的一致性**

RISC-V 的分页机制在硬件层面保证了各级页表项的格式完全相同：
```
| PPN[2] | PPN[1] | PPN[0] | RSW | D | A | G | U | X | W | R | V |
```

每一级页表项都包含：
- 物理页号（PPN）：指向下一级页表或最终的物理页
- 标志位：V（有效）、R（可读）、W（可写）、X（可执行）等

因此，**检查有效位、分配页表、设置表项**的操作在每一级都是相同的。

#### 代码差异

两段代码唯一的差异在于：
1. **索引提取**：第一段用 `PDX1(la)`，第二段用 `PDX0(la)`
2. **基地址来源**：第一段从 `pgdir` 开始，第二段从 `*pdep1` 指向的地址开始

这体现了多级页表的递归本质：**每一级都是相同的操作，只是输入不同**。

#### 结论

`get_pte()` 函数中两段代码如此相像，是因为：
1. 多级页表本质上是一个**递归的树形结构**，每一级的操作逻辑相同
2. RISC-V 的 sv32/sv39/sv48 分页模式在**硬件设计上保持了一致性**
3. 代码设计考虑了**可扩展性**，便于支持不同级数的页表

这种设计体现了操作系统设计中的**一致性原则**和**分层思想**，使得代码简洁、易于理解和维护。

---

### 思考题 2：get_pte() 函数的设计

**问题**：目前 `get_pte()` 函数将页表项的查找和页表项的分配合并在一个函数里，你认为这种写法好吗？有没有必要把两个功能拆开？

**回答**：

#### 当前设计的优缺点分析

**优点**：

1. **接口简洁**：
   - 调用者只需一个函数就能完成查找或创建页表项的操作
   - 通过 `create` 参数控制行为，使用灵活
   ```c
   pte_t *ptep = get_pte(pgdir, la, 1);  // 查找，不存在则创建
   pte_t *ptep = get_pte(pgdir, la, 0);  // 仅查找，不创建
   ```

2. **减少代码重复**：
   - 查找和分配的逻辑高度重合（都需要逐级遍历页表）
   - 合并可以避免两个函数中重复相同的页表遍历代码

3. **原子性**：
   - 查找到页表项不存在时立即分配，避免了二次查找
   - 减少了 TOCTTOU（Time-of-Check to Time-of-Use）问题

4. **符合常见使用场景**：
   - 大多数情况下，查找页表项的目的是为了使用它
   - 如果不存在，通常需要创建（如 `page_insert`）
   - 合并功能符合大多数调用者的需求

**缺点**：

1. **职责不单一**：
   - 违反了单一职责原则（Single Responsibility Principle）
   - 一个函数承担了两个不同的职责：查找和分配

2. **语义不够清晰**：
   - 函数名 `get_pte` 没有明确表达"可能会分配"的含义
   - 调用者可能不清楚这个函数在某些情况下会修改页表

3. **错误处理复杂**：
   - 返回 NULL 可能有两种含义：
     - `create=0` 时：页表项不存在（正常情况）
     - `create=1` 时：内存分配失败（错误情况）
   - 调用者需要根据 `create` 参数判断 NULL 的含义

4. **性能考虑**：
   - 即使 `create=0`，函数内部仍然包含分配相关的判断逻辑
   - 对于纯查询场景，这些判断是不必要的开销（虽然很小）

#### 拆分设计方案

如果拆分成两个函数，可以这样设计：

```c
// 方案 1：完全拆分
pte_t *lookup_pte(pde_t *pgdir, uintptr_t la);
pte_t *create_pte(pde_t *pgdir, uintptr_t la);

// 方案 2：保留原函数，添加专用函数
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create);  // 保留
pte_t *ensure_pte(pde_t *pgdir, uintptr_t la);           // 新增：确保存在
```

**方案 1 的优点**：
- 职责清晰，每个函数只做一件事
- 函数名明确表达了意图
- 易于测试和维护

**方案 1 的缺点**：
- `create_pte` 需要重新遍历页表，效率较低
- 调用者需要先 `lookup_pte`，再根据结果决定是否 `create_pte`，增加了使用复杂度
- 可能引入 TOCTTOU 问题（查询和创建之间状态可能改变）

**方案 2 的优点**：
- 保留了原有功能和兼容性
- 为常见的"确保存在"场景提供了语义更清晰的接口
- 灵活性更好

#### 个人观点

**我认为当前的设计是合理的，不建议拆分**，理由如下：

1. **实用主义**：
   - 操作系统开发追求实用性和效率
   - 页表操作是性能关键路径，避免重复遍历很重要
   - 当前设计在性能和易用性之间取得了良好平衡

2. **广泛实践**：
   - Linux、FreeBSD 等主流操作系统都采用类似设计
   - 这是经过长期实践验证的设计模式
   - 例如 Linux 的 `get_user_pages()` 也包含查找和分配两种行为

3. **改进建议**：
   - 如果要改进，建议：
     - 在函数注释中明确说明 `create` 参数的含义和副作用
     - 为常见场景提供包装函数（如 `ensure_pte`）
     - 保留 `get_pte` 作为底层实现

4. **场景适配**：
   - 对于 uCore 这样的教学操作系统，当前设计：
     - 代码简洁，便于学生理解多级页表的遍历过程
     - 接口统一，减少学生需要掌握的函数数量
     - 符合大多数使用场景（如 `page_insert`、`page_remove`）

#### 结论

**当前的合并设计是合理的**，特别是在操作系统内核这种对性能敏感的场景中。通过 `create` 参数控制行为是一个经典且实用的设计模式。

如果真的需要更清晰的语义，可以在保留 `get_pte` 的基础上，添加语义更明确的包装函数，而不是完全拆分。这样既保持了性能，又提供了更好的可读性。

---

## 重要知识点总结

### 本实验中的重要知识点

1. **进程控制块（PCB）**
   - 进程的核心数据结构，包含进程状态、PID、上下文、中断帧等信息
   - 是操作系统管理进程的基础

2. **进程状态转换**
   - `PROC_UNINIT` → `PROC_RUNNABLE` → `PROC_SLEEPING` → `PROC_ZOMBIE`
   - 状态机模型，体现了进程的生命周期

3. **上下文切换（Context Switch）**
   - 通过保存/恢复 Callee-Saved 寄存器实现进程切换
   - `switch_to` 函数是操作系统多任务的核心机制

4. **中断帧（Trapframe）**
   - 保存中断/异常发生时的完整 CPU 状态
   - 用于中断返回和新进程启动

5. **进程创建（do_fork）**
   - 资源分配：PCB、内核栈、页表
   - 状态复制：中断帧、上下文
   - 错误处理：多级回滚机制

6. **进程调度（Scheduler）**
   - FIFO 调度策略
   - Round Robin 遍历进程链表
   - `idleproc` 作为兜底进程

7. **临界区保护**
   - 通过关中断实现原子操作
   - `local_intr_save` / `local_intr_restore`
   - 保存和恢复中断状态，支持嵌套

8. **内核线程**
   - 运行在内核态，共享内核地址空间
   - 通过独立的内核栈实现并发

9. **虚拟内存管理**
   - 多级页表的遍历和分配
   - 页表切换与地址空间切换

10. **同步与互斥**
    - 关中断是最基础的同步机制
    - 为实现更高级的同步原语（信号量、锁）奠定基础

### 与 OS 原理的对应关系

| 实验内容 | OS 原理知识点 | 关系说明 |
|---------|--------------|---------|
| **进程控制块 PCB** | 进程的概念和组成 | PCB 是进程在内核中的表示，包含进程的所有管理信息 |
| **进程状态** | 进程状态模型（五状态模型） | 实验实现了四状态模型，是五状态模型的简化版 |
| **上下文切换** | 进程调度与切换 | 实验展示了上下文切换的底层实现（寄存器保存/恢复） |
| **进程创建** | `fork()` 系统调用 | `do_fork` 是 Unix `fork()` 的内核实现原型 |
| **调度算法** | 进程调度算法（FCFS） | 实验实现了最简单的 FIFO/FCFS 调度 |
| **临界区** | 同步与互斥 | 通过关中断实现临界区保护，是最原始的同步机制 |
| **虚拟内存** | 分页机制 | 多级页表的实现，虚拟地址到物理地址的翻译 |
| **内核线程** | 线程的概念 | 内核线程是线程概念在内核空间的实现 |

### 实验与原理的差异

1. **进程创建**：
   - 原理：通常讲解用户进程的创建，涉及地址空间复制
   - 实验：只实现了内核线程，共享内核地址空间

2. **调度算法**：
   - 原理：讲解多种调度算法（优先级、时间片轮转、多级反馈队列等）
   - 实验：只实现了最简单的 FIFO 调度

3. **同步机制**：
   - 原理：信号量、管程、条件变量等高级同步原语
   - 实验：只使用了关中断这一最底层的机制

4. **进程通信**：
   - 原理：管道、消息队列、共享内存等 IPC 机制
   - 实验：未涉及

---

## OS 原理中重要但实验未涉及的知识点

1. **用户进程**
   - 用户态和内核态的切换
   - 独立的用户地址空间
   - 系统调用接口

2. **进程间通信（IPC）**
   - 管道（Pipe）
   - 消息队列（Message Queue）
   - 共享内存（Shared Memory）
   - 信号（Signal）

3. **高级调度算法**
   - 优先级调度
   - 时间片轮转（RR）
   - 多级反馈队列（MLFQ）
   - 完全公平调度（CFS）

4. **高级同步原语**
   - 信号量（Semaphore）
   - 互斥锁（Mutex）
   - 条件变量（Condition Variable）
   - 读写锁（RW Lock）

5. **死锁**
   - 死锁的四个必要条件
   - 死锁预防、避免、检测和恢复
   - 银行家算法

6. **进程间关系**
   - 进程树
   - 父子进程关系
   - 孤儿进程和僵尸进程的处理

7. **线程实现模型**
   - 用户级线程
   - 内核级线程
   - 混合模型

8. **实时调度**
   - 硬实时和软实时
   - 速率单调调度（RMS）
   - 最早截止时间优先（EDF）

9. **多处理器调度**
   - SMP 和 NUMA 架构
   - 负载均衡
   - 处理器亲和性（CPU Affinity）

10. **虚拟化**
    - 轻量级进程容器
    - 虚拟机进程管理

---

## 实验总结

通过本次实验，我深入理解了操作系统内核线程管理的核心机制：

1. **进程控制块**是操作系统管理进程的基础数据结构，需要精心设计和初始化
2. **上下文切换**是实现多任务并发的关键技术，通过保存/恢复寄存器状态实现执行流的切换
3. **进程调度**决定了 CPU 资源的分配策略，即使是最简单的 FIFO 调度也能实现多任务
4. **临界区保护**对于保证内核数据结构的一致性至关重要
5. **虚拟内存管理**为进程提供了独立的地址空间（虽然内核线程共享内核空间）

本实验为理解现代操作系统的进程管理机制奠定了坚实基础，也为后续实现用户进程、系统调用、IPC 等功能做好了准备。

---

*本报告完整回答了 Lab4 实验指导书中的所有问题，并对实验中的关键知识点进行了深入分析和总结。*

