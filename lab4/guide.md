# Lab 4 实验指导书：内核线程管理

## 实验核心图谱
*   **核心目标**：实现内核线程（Kernel Thread）。
*   **关键结构**：`proc_struct`（进程控制块 PCB）。
*   **关键动作**：`switch_to`（上下文切换）。

---

## 练习 1：分配并初始化进程控制块 (`alloc_proc`)

这个练习是“建房子”。我们需要初始化一个干净的 PCB，保证它没有脏数据。

### 1. 位置定位
*   **文件**：`kern/process/proc.c`
*   **代码段**：约第 91 行 `alloc_proc` 函数内的 `LAB4:EXERCISE1 YOUR CODE`。

### 2. 核心逻辑
拿到一块内存 (`kmalloc`) 只是第一步，最重要的是把它的状态重置。
*   **状态**：设为 `PROC_UNINIT`。
*   **PID**：设为 -1（表示未分配）。
*   **Cr3/Pgdir**：内核线程共享内核页表，所以直接指向 `boot_pgdir_pa`（物理地址）。

### 3. 待补全代码与解答
(你的代码中似乎已经包含了这部分，请核对是否优雅)

```c
    proc->state = PROC_UNINIT;                     // 状态初始化
    proc->pid = -1;                                // PID初始化为无效值
    proc->runs = 0;                                // 运行时间为0
    proc->kstack = 0;                              // 内核栈尚未分配
    proc->need_resched = 0;                        // 不需要调度
    proc->parent = NULL;                           // 父进程为空
    proc->mm = NULL;                               // 内核线程共享内核内存，mm为空
    memset(&proc->context, 0, sizeof(struct context)); // 上下文清零，这是Context Switch的基础
    proc->tf = NULL;                               // 中断帧指针为空
    proc->pgdir = boot_pgdir_pa;                   // 关键：内核线程页表即内核页表
    proc->flags = 0;                               
    memset(proc->name, 0, sizeof(proc->name));     
    list_init(&proc->list_link);                   
    list_init(&proc->hash_link); 
```

### 💡 PhD Tips
> **Principal Component Analysis (PCA)**: 这里的 `memset(&proc->context, 0, ...)` 极其重要。如果 context 里面有脏数据，下一次 `switch_to` 恢复寄存器时，PC 指针可能会飞到莫名其妙的地方，导致 Hard Fault，且极难 Debug。

---

## 练习 2：创建新线程 (`do_fork`)

这是本实验的 **State-of-the-Art (SOTA)** 部分。`do_fork` 是所有进程诞生的必经之路。你需要完成“克隆”父进程到子进程的全过程。

### 1. 位置定位
*   **文件**：`kern/process/proc.c`
*   **代码段**：约第 326 行 `do_fork` 函数内的 `LAB4:EXERCISE2 YOUR CODE`。

### 2. 核心逻辑 (Pipeline)
按照 `lab4.md` 的提示，这其实是一个流水线作业：
1.  **Alloc**: 申请 PCB。
2.  **Stack**: 申请内核栈。
3.  **MM**: 复制/共享内存（内核线程暂时为空操作）。
4.  **Context**: 设置上下文（让它知道醒来后从哪里开始跑）。
5.  **Hash/List**: 放入全局管理链表（需要原子操作）。
6.  **Wakeup**: 标记为可运行。
7.  **Return**: 返回 PID。

### 3. 待补全代码与解答
这一段在你的代码中是空的，请完整填入：

```c
    // 1. 分配进程控制块
    if ((proc = alloc_proc()) == NULL) {
        goto fork_out;
    }
    
    proc->parent = current; // 设置父进程为当前进程

    // 2. 分配内核栈 (Kernel Stack)
    // 如果分配失败，需要回滚(Clean up)，释放 proc
    if (setup_kstack(proc) != 0) {
        goto bad_fork_cleanup_proc;
    }

    // 3. 复制内存管理信息 (Memory Management)
    // 对于内核线程，copy_mm 实际上直接 return 0，但为了架构统一必须调用
    if (copy_mm(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_kstack;
    }

    // 4. 设置中断帧和上下文 (Context Setup)
    // 这是让子进程"知道"自己是谁的关键
    copy_thread(proc, stack, tf);

    // 5. 将新进程加入全局链表 (Critical Section)
    // 这是一个临界区，必须关中断，防止多核或中断导致的竞争条件
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid(); // 获取唯一PID
        hash_proc(proc);       // 建立 PID -> PCB 的哈希映射
        list_add(&proc_list, &(proc->list_link)); // 加入全局进程链表
        nr_process++;          // 进程数 +1
    }
    local_intr_restore(intr_flag);

    // 6. 唤醒新进程
    wakeup_proc(proc);

    // 7. 返回新进程的 PID
    ret = proc->pid;
```

### 💡 PhD Tips
> **Error Handling (Ablation Study)**: 注意看代码里的 `goto` 语句。真正的系统级代码非常讲究 Resource Leak 的处理。如果第 2 步失败了，必须释放第 1 步申请的内存。这就是为什么 `goto` 在内核开发中不仅被允许，而且是 Best Practice。

---

## 练习 3：进程切换 (`proc_run`)

如果说 `do_fork` 是静态的构建，`proc_run` 就是动态的切换。这是让 CPU “精神分裂”的时刻。

### 1. 位置定位
*   **文件**：`kern/process/proc.c`
*   **代码段**：约第 192 行 `proc_run` 函数内的 `LAB4:EXERCISE3 YOUR CODE`。

### 2. 核心逻辑
我们要保证切换过程是**原子**的，不能被打断。
1.  **Disable Interrupt**: 关中断。
2.  **Switch Page Table**: 换页表（换脑子，切换视野）。
3.  **Switch Context**: 换寄存器（换手脚，切换执行流）。
4.  **Enable Interrupt**: 开中断。

### 3. 待补全代码与解答
(你的代码中似乎也已经包含了这部分，请重点理解 `lsatp`)

```c
    bool intr_flag;
    struct proc_struct *prev = current; // 保存当前进程（即将下台的）
    struct proc_struct *next = proc;    // 下一个进程（即将上台的）

    if (prev != next) {
        local_intr_save(intr_flag); // 1. 屏蔽中断
        {
            current = proc; // 更新全局 current 指针
            
            // 2. 切换页表 (CR3/SATP)
            // 加载新进程的页目录表物理地址。
            // 这里的 PPN 宏和 SATP_SV39 宏是为了适配 RISC-V 的 Sv39 分页机制。
            lsatp(SATP_SV39 | PPN(next->pgdir)); 
            
            // 3. 刷新 TLB (Translation Lookaside Buffer)
            // 切换页表后，旧的 TLB 条目失效。
            // 在 RISC-V 中，lsatp 指令本身不一定刷新 TLB，通常需要 sfence.vma 指令。
            // 不过 ucore 的 lsatp 实现中可能包含或者是紧接着会发生刷新。
            // (switch_to 会在汇编层面处理寄存器保存与恢复)
            
            switch_to(&(prev->context), &(next->context));
        }
        local_intr_restore(intr_flag); // 4. 恢复中断
    }
```

### 💡 PhD Tips
> **Context Awareness**: 为什么必须先 `lsatp` (换页表) 再 `switch_to` (换栈)?
> 因为内核线程虽然共享内核空间，但在未来的用户进程中，栈是存在于用户虚拟地址空间的。如果不先切换页表，新的栈指针（SP）指向的虚拟地址在旧页表中可能是未映射的，瞬间 Page Fault。即使是内核线程，养成这个顺序的好习惯也是必须的。

---

## 总结 (Conclusion)

完成这三个练习后，你的 uCore OS 就具备了多任务处理的雏形。
1.  `alloc_proc` 造出了躯壳。
2.  `do_fork` 注入了灵魂（复制状态）。
3.  `proc_run` 实现了灵魂互换（调度）。

去运行 `make qemu` 吧，如果看到了 "Hello World" 交替输出（或者 init 进程正常启动），恭喜你，你已经完成了 OS 课程中最具里程碑意义的一步。