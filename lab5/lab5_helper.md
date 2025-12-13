# Lab5 实验帮助文档（用户进程 / 系统调用 / fork-exec-wait-exit）

本文档目标：按 `lab5/lab5.md` 的练习划分，**细致到每个练习需要阅读哪些源文件、需要填写哪些代码块、实现思路与关键注意点**。你可以把它当作“做题路线图”。

> 约定：文中所有路径均相对 `lab5/` 目录。

---

## 总览：Lab5 做什么、验证什么

Lab5 的核心是让 ucore 从纯内核线程（S 态）走到 **真正的用户进程（U 态）**：

- **首次进入用户态**：在内核里构造用户进程的地址空间（页表 + VMA）和用户态上下文（trapframe），然后借助 `trapentry.S` 的 `sret` 返回路径切到 U 态执行第一条用户指令。
- **系统调用框架**：用户态用 `ecall` 触发 trap，内核态在 `exception_handler()` 中识别 `CAUSE_USER_ECALL` 并分发到 `kern/syscall/syscall.c` 的 `syscall()`，再调用 `do_*` 系列实现。
- **进程管理四件套**：`fork/exec/exit/wait` 的内核实现（`do_fork/do_execve/do_exit/do_wait`）与用户库封装（`user/libs`）。

最终用 `make grade` 跑用户程序（默认 `user/exit.c`）做验收：父进程 `fork()` 出子进程，子进程 `exit(magic)`，父进程 `waitpid()` 正确回收并拿到退出码。

---

## 练习0：把 LAB2/LAB3/LAB4 的代码填回 Lab5（必须）

### 你需要做什么

Lab5 代码里仍然保留了前置实验的占位标记。你需要把你在 Lab2/3/4 完成过的实现，按注释位置填进来（或者确认该仓库版本已填好）。

### 你应该先读哪些文件（阅读路径）

- **进程/线程与调度相关**
  - `kern/process/proc.h`：`proc_struct` 字段（Lab5 新增 `wait_state/cptr/yptr/optr`）
  - `kern/process/proc.c`：`alloc_proc/do_fork/proc_run` 这些是后续 fork/wait 的基础设施
- **Trap 与时钟中断（Lab3）**
  - `kern/trap/trap.c`：`interrupt_handler()` 的 `IRQ_S_TIMER` 分支
  - `kern/trap/trapentry.S`：保存/恢复上下文与 `sret`（理解即可，不需要改）

### 你需要填哪些代码块（按文件定位）

- `kern/process/proc.c`
  - `alloc_proc()`：`// LAB4:EXERCISE1 YOUR CODE` 与紧随其后的 `// LAB5 YOUR CODE`（需要**新增/初始化** Lab5 新字段）
  - `do_fork()`：`// LAB4:EXERCISE2 YOUR CODE` 与 `// LAB5 YOUR CODE : (update LAB4 steps)`（需要在 Lab4 逻辑上**补父子关系链**）
  - `proc_run()`：`// LAB4:EXERCISE3 YOUR CODE`（切换页表 + `switch_to`）
- `kern/trap/trap.c`
  - `interrupt_handler()` 的 `IRQ_S_TIMER`：`/* LAB3 EXERCISE1 YOUR CODE */`（时钟中断处理）

> 注意：Lab5 的评分依赖这些基础逻辑；如果练习1/2 写对了但练习0 没填，系统仍然可能无法跑到用户态或 fork/wait 不工作。

### 实现思路要点（为什么 Lab5 要你“更新 Lab4”）

- **`alloc_proc()`**：Lab5 的 `do_wait/do_exit` 会用到 `wait_state` 与子进程链（`cptr/yptr/optr`），所以必须在 `alloc_proc()` 里把这些字段清零/初始化成一致状态（常见做法：`wait_state=0; cptr=yptr=optr=NULL;`）。
- **`do_fork()` 的“关系链更新”**：Lab4 只要能创建线程并放进 runnable 队列即可，但 Lab5 要支持 `wait()`，所以必须维护父子关系：
  - 新建子进程时 `proc->parent = current`
  - 把子进程插入到父进程的孩子链表（调用 `set_links(proc)`）
  - 确保父进程初始不处于等待态（一般 `current->wait_state = 0`）

---

## 练习1：加载应用程序并执行（需要编码）

练习1 对应 `lab5.md`：补全 `load_icode()` 的第(6)步——**设置 trapframe，使得 trap 返回时能进入 U 态并从 ELF entry 开始执行**。

### 你需要读哪些文件（阅读路径）

1. **入口总流程**
   - `kern/process/proc.c`
     - `init_main()`：启动 `user_main` 内核线程并 `do_wait()` 等它退出
     - `user_main()`：调用 `KERNEL_EXECVE(exit)`（把用户程序链接进内核镜像）
     - `do_execve()`：回收旧 mm → `load_icode(binary,size)` 构建新用户地址空间
2. **Trap/返回到用户态机制**
   - `kern/trap/trap.c`
     - `exception_handler()`：`CAUSE_BREAKPOINT`（内核态用 `ebreak` 伪装 syscall exec）与 `CAUSE_USER_ECALL`
   - `kern/trap/trapentry.S`
     - `__trapret`：`RESTORE_ALL` 后 `sret`
     - `kernel_execve_ret`：把 trapframe 挪到当前进程内核栈顶（让后续 `sret` 使用你构造的用户态 `tf`）
3. **地址空间与栈布局**
   - `kern/mm/memlayout.h`：`USERBASE/UTEXT/USTACKTOP/USTACKSIZE/USERTOP`
   - `kern/mm/vmm.c`：`mm_map/dup_mmap/exit_mmap`

### 你需要填哪些代码（唯一的 LAB5:EXERCISE1）

文件：`kern/process/proc.c`  
函数：`load_icode()` 的第(6)步

占位代码块如下（你要填写的是注释里要求设置的三个字段）：

- `tf->gpr.sp`：用户栈顶（第一次进入用户态时的 `sp`）
- `tf->epc`：用户程序入口地址（ELF header 的 `e_entry`）
- `tf->status`：正确的 `sstatus`（关键是 **SPP=0**，让 `sret` 返回 U 态；并合理设置 **SPIE**）

### 实现思路（怎么填才对）

#### 1) `sp`：必须指向用户栈

- 栈顶定义在 `kern/mm/memlayout.h`：
  - `USTACKTOP = USERTOP`
  - `USTACKSIZE = USTACKPAGE * PGSIZE`
- `load_icode()` 第(4)步已经 `mm_map()` 并为 `USTACKTOP - 1..4*PGSIZE` 分配了页，所以这里应：
  - `tf->gpr.sp = USTACKTOP;`

#### 2) `epc`：必须从 ELF entry 开始跑

`load_icode()` 里已经解析了 ELF 头：

- `struct elfhdr *elf = (struct elfhdr *)binary;`

因此：

- `tf->epc = elf->e_entry;`

#### 3) `status`：最关键——让 `sret` 真的回到 U 态

RISC-V 下 `sret` 的特权级返回由 `sstatus.SPP` 决定：

- `SPP=1` → `sret` 回 S 态（错误，会“在内核态执行用户代码”）
- **`SPP=0` → `sret` 回 U 态（正确）**

同时 `SPIE` 决定返回后的中断使能（`sret` 会把 `SPIE` 复制到 `SIE`）。

该函数保存了旧的 `sstatus`：

- `uintptr_t sstatus = tf->status;`

推荐做法是：保留其它位（如 FS/XS 等）同时清掉 SPP 并打开 SPIE：

- **清 SPP**：`tf->status = sstatus & ~SSTATUS_SPP;`
- **设 SPIE**：`tf->status |= SSTATUS_SPIE;`

> 你不需要显式设置 `SIE`；第一次 `sret` 后硬件会按规则处理。核心是 `SPP=0`。

### 这个练习的意义（你做对了什么）

- 你让 `exec` 不再只是“把 ELF 搬进内存”，而是把 **“将来要返回到用户态执行的现场”** 也构造好了。
- 从此之后，用户态的 `ecall` 才能在 U↔S 的闭环里工作（否则永远回不到 U 态或入口不对）。

---

## 练习2：父进程复制自己的内存空间给子进程（需要编码）

练习2 对应 `lab5.md`：补全 `copy_range()`，使 `fork()` 能把父进程用户空间的内容复制到子进程。

### 你需要读哪些文件（阅读路径）

1. **调用链总览**
   - `kern/process/proc.c`：`do_fork()` → `copy_mm()`
   - `kern/process/proc.c`：`copy_mm()` 在 `clone_flags` 不含 `CLONE_VM` 时走 `dup_mmap()`
   - `kern/mm/vmm.c`：`dup_mmap()` → `copy_range(to->pgdir, from->pgdir, vma->vm_start, vma->vm_end, share)`
2. **页表与物理页操作**
   - `kern/mm/pmm.c`：`copy_range()`、`page_insert()`、`pgdir_alloc_page()`
   - `kern/mm/mmu.h`：`PTE_USER` 的含义（R/W/X/U/V）
   - `kern/mm/pmm.h`（如需查接口声明）：`page2kva/page_ref/...`

### 你需要填哪些代码（唯一的 LAB5:EXERCISE2）

文件：`kern/mm/pmm.c`  
函数：`copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share)`

占位块要求你完成四步（注释已经给出）：

1. 找到父进程页 `page` 对应的内核虚拟地址 `src_kvaddr`
2. 找到子进程新分配页 `npage` 的内核虚拟地址 `dst_kvaddr`
3. `memcpy(dst_kvaddr, src_kvaddr, PGSIZE)`
4. `page_insert(to, npage, start, perm)` 建立映射

### 实现思路（为什么这样写）

这里的循环按页遍历用户地址 `[start, end)`，对父进程每个 present 的 PTE：

- `page = pte2page(*ptep)`：父进程物理页
- `npage = alloc_page()`：给子进程分配新页（真正的“复制”）
- `perm = (*ptep & PTE_USER)`：继承父页用户权限（R/W/X/U/V）

你需要做的事就是：

- **复制内容**：用 `page2kva()` 拿到两个物理页在内核的映射地址，然后 `memcpy` 整页复制。
- **建立映射**：用 `page_insert(to, npage, start, perm)` 把子进程页表 `to` 上的虚拟地址 `start` 映射到 `npage`。

#### 必须注意的边界/坑

- **只复制有效 PTE**：外层已经 `if (*ptep & PTE_V)` 做了 present 判断；未 present 的页会被跳过。
- **权限继承**：不要手写权限，直接继承父页的 `PTE_USER` 子集（代码已经算好 `perm`）。
- **失败处理**：`page_insert()` 可能返回 `-E_NO_MEM`；严格来说失败时应把 `npage` 释放掉并返回错误（本仓库的占位块最后 `assert(ret == 0)`，通常按指导实现会让 `ret` 接住返回值即可）。

### 这个练习的意义（你做对了什么）

- `fork()` 的语义是“复制一份几乎完全相同的进程”——最核心的就是 **复制用户地址空间的页表与页面内容**。
- `copy_range()` 是 `dup_mmap()` 的关键路径：VMA 复制只是“合法区间的描述”，真正的数据复制靠它完成。

### 题目附问：如何设计 Copy-on-Write（COW）机制（概要设计）

Lab5 默认是 **真复制**（每页都 `alloc_page+memcpy`）。COW 的思路是把“复制成本”推迟到第一次写：

- **fork 阶段（不复制数据）**
  - 父子进程的 PTE 都指向同一物理页（共享）
  - 把这些共享页的 PTE 权限改为**只读**（清掉 `PTE_W`），并在 PTE 的软件保留位（例如 `PTE_SOFT`）或 `Page` 结构里标记“COW共享”
  - 增加共享页的引用计数（`page_ref_inc(page)`）
- **写时缺页（store page fault）处理**
  - 在 `kern/mm/vmm.c` 的 `do_pgfault()`（或对应 page fault 处理路径）里，识别“写缺页且该页是 COW 标记”
  - 如果该物理页 `ref>1`：分配新页 `npage`，把旧页内容拷贝到新页，更新当前进程 PTE 指向新页，并恢复 `PTE_W`
  - 如果 `ref==1`：说明已无人共享，直接把当前 PTE 恢复写权限即可
- **退出/解除映射**
  - `exit_mmap/unmap_range` 过程中减少引用计数，必要时释放物理页

> 这套设计的关键是：fork 快、内存省；代价是 page fault 路径更复杂，且要小心同步与权限位维护。

---

## 练习3：阅读分析 fork/exec/wait/exit 与系统调用实现（不需要编码）

练习3 的要求是写报告分析。下面给你一套“从用户态到内核态再回用户态”的阅读路线，并明确每个函数在哪、扮演什么角色。

### 3.1 `fork/exec/wait/exit` 的执行流程（建议按这个顺序读）

#### A. 用户态：调用发生在哪里？

- `user/exit.c`：测试程序（父 `fork`，子 `exit`，父 `waitpid`）
- `user/libs/ulib.c`：
  - `fork()` → `sys_fork()`
  - `exit()` → `sys_exit()`
  - `wait()/waitpid()` → `sys_wait()`
  - `yield()` → `sys_yield()`
- `user/libs/syscall.c`：真正执行 `ecall` 的地方  
  - 约定：系统调用号放 `a0`，参数放 `a1~a5`（本实现最多 5 个参数）

你在报告里可以明确写：**用户态只负责“把参数按 ABI 放进寄存器 + ecall”，不做真正的进程管理**。

#### B. Trap 入口：用户态如何进入内核态？

- `kern/trap/trapentry.S`：`__alltraps` 保存寄存器到 trapframe，并调用 `trap(a0=sp)`
- `kern/trap/trap.c`
  - `trap()`：把当前 trapframe 绑定到 `current->tf`，再 `trap_dispatch()`
  - `exception_handler()`：识别 `CAUSE_USER_ECALL`，`tf->epc += 4`，再调用 `syscall()`

你在报告里应强调：**`tf->epc += 4` 是为避免返回后重复执行 `ecall`**（否则死循环）。

#### C. 系统调用分发：内核态如何找到具体 sys_*？

- `kern/syscall/syscall.c`
  - `syscall()`：从 `current->tf->gpr.a0` 取系统调用号，收集 `a1~a5` 做参数数组，调用 `syscalls[num](arg)`
  - `sys_exit/sys_fork/sys_wait/sys_exec/...`：把 sys_* 转发给 `do_exit/do_fork/do_wait/do_execve/...`

#### D. 进程管理核心：do_* 真正干活

- `kern/process/proc.c`
  - `do_fork()`：创建子进程结构、复制 mm、设置上下文、插入到就绪队列（并维护父子链）
  - `do_execve()`：回收旧地址空间 → `load_icode()` 构建新地址空间与 trapframe
  - `do_exit()`：释放自己的 mm 等资源，把自己标为 `PROC_ZOMBIE`，唤醒父进程
  - `do_wait()`：睡眠等待子进程变僵尸并回收子进程剩余资源（kstack + pcb）
- `kern/mm/vmm.c`
  - `dup_mmap()` + `copy_range()`：fork 的用户空间复制
  - `exit_mmap()`：退出时解除映射
- `kern/mm/pmm.c`
  - `copy_range()`：逐页复制（练习2）

#### E. 返回用户态：结果如何回到用户程序？

- `kern/syscall/syscall.c:syscall()` 最终把返回值写回 `a0`：`current->tf->gpr.a0 = ...`
- `kern/trap/trapentry.S:__trapret` 恢复寄存器并执行 `sret` 回到 U 态

### 3.2 哪些在用户态做？哪些在内核态做？如何交错执行？

- **用户态做的事**
  - 通过 `ulib` 调用系统调用包装（如 `fork()`/`exit()`/`waitpid()`）
  - 在 `syscall()` 内联汇编里执行 `ecall`
  - 在用户程序里根据返回值走不同路径（例如 `fork()` 的父子分叉）
- **内核态做的事**
  - trap 保存现场、识别异常原因、系统调用分发
  - `do_*` 完成进程/内存管理（创建 PCB、复制页表/页面、回收资源、阻塞/唤醒）
  - 在 trap 返回前把 syscall 返回值写回 trapframe（`a0`），然后 `sret`
- **交错执行的关键点（建议你在报告里画出来）**
  - U 态 `ecall` → S 态 `exception_handler/syscall/do_*` → S 态 `sret` → U 态继续执行（并读到 `a0` 返回值）

### 3.3 用户态进程生命周期图（报告可用字符画）

你可以参考 `kern/process/proc.c` 文件头部注释里的状态转移图，并补上 Lab5 的关键事件：

- `PROC_RUNNABLE` → `PROC_SLEEPING`：`do_wait()` 设置 `wait_state=WT_CHILD` 并 `schedule()`
- `PROC_SLEEPING` → `PROC_RUNNABLE`：子进程 `do_exit()` 唤醒父进程（`wakeup_proc(parent)`）
- `PROC_RUNNABLE` → `PROC_ZOMBIE`：`do_exit()` 把自己设为僵尸等待父回收
- `PROC_ZOMBIE` →（被释放）→：`do_wait()` 最终 `put_kstack+kfree(proc)`

---

## 补充：用户程序何时/如何“预先加载”到内存？与常见 OS 有何不同？

你在 `kern/process/proc.c` 会看到：

- `KERNEL_EXECVE(exit)` 依赖链接器生成的符号：
  - `_binary_obj___user_exit_out_start`
  - `_binary_obj___user_exit_out_size`

这意味着 Lab5 的用户程序不是从“磁盘文件系统”按需读取，而是在**构建镜像时就把用户 ELF 作为二进制段链接进了内核镜像**（常见于教学 OS，便于绕开文件系统与驱动栈）。

建议你阅读：

- `Makefile`：用户程序是如何被编译成 `obj/__user_*.out` 并被链接进最终镜像的
- `tools/user.ld`（以及相关链接脚本/规则）：用户程序的链接地址、段布局、符号导出方式

在报告里可以写出对比：

- **现实 OS（如 Linux）**：`execve()` 通过 VFS 从磁盘读取 ELF，按需映射（mmap）并可能采用 demand paging。
- **本实验**：ELF 已经在内存中（镜像里），`load_icode(binary,size)` 直接解析内存中的 ELF 并建立页表映射。

---

## 实验验收：如何运行、如何定位错误

### 你应该怎么跑

在 `lab5/` 下：

- `make grade`

若需要单独跑 qemu 或调试目标，请查看 `Makefile` 中的 `qemu / debug / gdb` 目标。

### 常见错误与现象（高频排查清单）

- **卡死/反复打印 trapframe**
  - 多半是 `tf->epc` 没有 `+4`（会重复执行 `ecall`），或练习1 的 `tf->status.SPP` 没清导致回到 S 态跑“用户代码”
- **`fork()` 后子进程/父进程行为异常**
  - 多半是练习2 的 `copy_range()` 没复制内容或映射权限错（例如 `page_insert` 没做）
- **`waitpid` 永远等不到**
  - 多半是练习0 里 `do_fork()` 没维护父子关系链（`parent/set_links`），或 `do_exit()` 没唤醒父进程

---

## 分支任务（Challenge/扩展）：双重 gdb 观测 ecall/sret 与 QEMU TCG（选做）

`lab5.md` 末尾给了一个很明确的调试任务：用“双 gdb”（一个调 ucore，一个 attach qemu）观测：

- 用户态执行 `ecall` 时，qemu 如何处理并切到 S 态
- 内核态执行 `sret` 返回用户态时，qemu 如何处理
- 了解 QEMU 的 TCG translation（指令翻译）与之前地址翻译调试的关系

### 推荐的实验操作路线（按 `lab5.md` 的建议整理）

- 先正常 `make debug` / `make gdb` 启动
- 在 ucore 的 gdb 中手动加载用户程序符号（因为默认只加载了内核符号）：
  - `add-symbol-file obj/__user_exit.out`
- 在 `user/libs/syscall.c` 的 `ecall` 前下断点并单步到 `ecall`
- 切到 attach qemu 的 gdb，`Ctrl+C` 打断“Continuing”，在 qemu 里对 `ecall` 的实现路径下断点
- 单步执行 `ecall` 与 `sret`，跟踪 qemu 源码里对应指令的翻译与执行逻辑

> 这一部分是报告型任务：重点是你能讲清楚“模拟器是怎么用软件模拟硬件指令语义”的，以及你如何定位到 qemu 源码的关键函数/调用链。
