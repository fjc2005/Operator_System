# Lab 5 实验报告：用户程序

姓名: 黄俊雄 学号: 2313896
姓名: 付嘉晨 学号: 2313903
姓名: 王文轩 学号: 2311058

## 实验概述

本实验在 Lab4 内核线程的基础上，实现了用户进程管理机制。通过创建第一个用户进程，使用户程序能够在用户态执行，并通过系统调用获得 ucore 提供的服务。

实验主要完成了以下内容：
1. 实现用户进程的创建和加载，支持从内核态到用户态的切换
2. 实现系统调用框架，连接用户态和内核态
3. 实现进程管理相关的系统调用（`fork`/`exec`/`wait`/`exit`）
4. 实现父子进程内存空间的复制机制

---

## 练习 0：填写已有实验

本实验依赖实验 2、3 和 4 的代码。已将相关代码填入本实验中标有 `LAB2`、`LAB3` 和 `LAB4` 注释的相应部分，并根据 Lab5 的需求进行了必要的调整。

主要包括：
- LAB2：物理内存管理（`pmm.c` 中的页表操作函数）
- LAB3：虚拟内存管理和页面置换机制
- LAB4：进程控制块、进程创建和调度相关代码

---

## 练习 1：加载应用程序并执行

### 设计实现过程

`load_icode` 函数是加载用户程序的核心函数，它负责解析 ELF 格式的可执行文件，将程序的代码段、数据段等加载到进程的地址空间，并设置好进程的执行环境。练习 1 要求我们完成第 6 步：设置进程的中断帧（trapframe），使进程能够正确地从内核态返回到用户态并开始执行。

#### 实现代码（`kern/process/proc.c` 第 737-742 行）

```c
//(6) setup trapframe for user environment
struct trapframe *tf = current->tf;
// Keep sstatus
uintptr_t sstatus = tf->status;
memset(tf, 0, sizeof(struct trapframe));

// 设置用户栈指针为栈顶
tf->gpr.sp = USTACKTOP;
// 设置程序计数器为ELF入口点
tf->epc = elf->e_entry;
// 设置状态寄存器：SPP=0(返回用户态)，SPIE=1(开中断)
tf->status = (sstatus & ~SSTATUS_SPP) | SSTATUS_SPIE;
```

#### 实现要点

1. **设置用户栈指针（sp）**：
   - 将 `tf->gpr.sp` 设置为 `USTACKTOP`（用户栈顶地址）
   - 用户栈从高地址向低地址增长，因此栈顶是用户栈的最高地址
   - `USTACKTOP` 在 `memlayout.h` 中定义，通常为 `0x80000000`

2. **设置程序入口点（epc）**：
   - 将 `tf->epc` 设置为 `elf->e_entry`（ELF 文件头中的入口地址）
   - `epc` 寄存器（Exception Program Counter）保存了异常返回后要执行的指令地址
   - 通过 `sret` 指令返回时，CPU 会跳转到 `epc` 指向的地址

3. **设置状态寄存器（status）**：
   - 清除 `SSTATUS_SPP` 位：设置为 0 表示返回到用户态（U mode）
   - 设置 `SSTATUS_SPIE` 位：设置为 1 表示返回用户态后开启中断
   - 使用位运算 `(sstatus & ~SSTATUS_SPP) | SSTATUS_SPIE` 实现

#### RISC-V 状态寄存器详解

`sstatus` 寄存器是 RISC-V Supervisor 模式的状态寄存器，控制着中断、特权级等关键信息：

- **SPP (Supervisor Previous Privilege)**：第 8 位
  - SPP=0：异常/中断前处于用户态（U mode）
  - SPP=1：异常/中断前处于内核态（S mode）
  - `sret` 指令会根据 SPP 决定返回到哪个特权级

- **SPIE (Supervisor Previous Interrupt Enable)**：第 5 位
  - 保存进入 trap 前的中断使能状态
  - `sret` 返回时，会将 SPIE 的值恢复到 SIE 位

- **SIE (Supervisor Interrupt Enable)**：第 1 位
  - 当前的中断使能状态
  - SIE=1：允许接收中断
  - SIE=0：屏蔽中断

#### 为什么要这样设置？

1. **清除 SPP 位是关键**：
   - 如果不清除 SPP 位，`sret` 会返回到 S mode（内核态）
   - 用户程序必须运行在 U mode，否则用户程序可以执行特权指令，破坏系统安全

2. **设置 SPIE 确保中断响应**：
   - 用户程序需要响应时钟中断（用于进程调度）
   - 需要响应系统调用（用户程序请求内核服务）
   - 如果不开中断，用户进程一旦开始执行就无法被打断，系统失去控制

3. **保留原 sstatus 的其他位**：
   - 通过先读取 `sstatus`，然后只修改特定位，保留了其他控制位的状态
   - 这样可以避免影响到其他系统功能

### 用户态进程执行的完整过程

**问题**：请简要描述这个用户态进程被 ucore 选择占用 CPU 执行（RUNNING 态）到具体执行应用程序第一条指令的整个经过。

**回答**：

#### 阶段 1：进程创建和初始化

1. **系统启动**（`kern_init`）：
   - 完成各子系统初始化
   - 调用 `proc_init()` 创建 `idleproc` 和 `initproc`

2. **创建第一个内核线程**（`proc_init`）：
   ```c
   int pid = kernel_thread(user_main, NULL, 0);
   ```
   - 通过 `kernel_thread` 创建一个内核线程，执行 `user_main` 函数
   - 该线程的状态被设置为 `PROC_RUNNABLE`，可被调度

3. **执行 user_main**：
   ```c
   static int user_main(void *arg) {
       KERNEL_EXECVE(exit);  // 加载并执行用户程序 exit
   }
   ```

#### 阶段 2：加载用户程序（第一次进入用户态）

4. **调用 kernel_execve**（`kern/process/proc.c` 第 904-922 行）：
   ```c
   static int kernel_execve(const char *name, unsigned char *binary, size_t size) {
       // 通过 ebreak 触发断点中断，模拟系统调用
       asm volatile(
           "li a0, %1\n"      // 系统调用号 SYS_exec
           "li a7, 10\n"      // a7=10 表示这是一个特殊的 ebreak
           "ebreak\n"         // 触发断点异常
           : "=m"(ret)
           : "i"(SYS_exec), "m"(name), "m"(len), "m"(binary), "m"(size)
       );
   }
   ```
   - 因为当前在 S mode，不能用 `ecall`（`ecall` 用于从 U mode 调用 S mode）
   - 使用 `ebreak` 触发断点异常，通过设置 `a7=10` 标识这是一个系统调用

5. **进入异常处理**（`kern/trap/trap.c`）：
   ```c
   void exception_handler(struct trapframe *tf) {
       switch (tf->cause) {
           case CAUSE_BREAKPOINT:
               if(tf->gpr.a7 == 10) {  // 检查是否是系统调用
                   tf->epc += 4;       // 跳过 ebreak 指令
                   syscall();          // 转发到系统调用处理
               }
               break;
       }
   }
   ```

6. **系统调用分发**（`kern/syscall/syscall.c` 第 82-100 行）：
   ```c
   void syscall(void) {
       struct trapframe *tf = current->tf;
       int num = tf->gpr.a0;  // 获取系统调用号（SYS_exec）
       
       if (syscalls[num] != NULL) {
           // 调用 sys_exec，进而调用 do_execve
           tf->gpr.a0 = syscalls[num](arg);
       }
   }
   ```

7. **执行 do_execve**（`kern/process/proc.c` 第 759-798 行）：
   ```c
   int do_execve(const char *name, size_t len, unsigned char *binary, size_t size) {
       struct mm_struct *mm = current->mm;
       
       if (mm != NULL) {
           // 如果当前进程有用户内存空间，先释放
           lsatp(boot_pgdir_pa);  // 切换到内核页表
           if (mm_count_dec(mm) == 0) {
               exit_mmap(mm);      // 释放内存映射
               put_pgdir(mm);      // 释放页表
               mm_destroy(mm);     // 销毁 mm 结构
           }
           current->mm = NULL;
       }
       
       // 调用 load_icode 加载新程序
       if ((ret = load_icode(binary, size)) != 0) {
           goto execve_exit;
       }
       
       set_proc_name(current, local_name);
       return 0;
   }
   ```

8. **执行 load_icode**（`kern/process/proc.c` 第 580-755 行）：
   
   **步骤 (1)**: 创建新的内存管理结构
   ```c
   if ((mm = mm_create()) == NULL) {
       goto bad_mm;
   }
   ```

   **步骤 (2)**: 创建新的页目录表
   ```c
   if (setup_pgdir(mm) != 0) {
       goto bad_pgdir_cleanup_mm;
   }
   ```

   **步骤 (3)**: 解析 ELF 文件，加载代码段和数据段
   ```c
   struct elfhdr *elf = (struct elfhdr *)binary;
   struct proghdr *ph = (struct proghdr *)(binary + elf->e_phoff);
   
   // 检查 ELF 魔数
   if (elf->e_magic != ELF_MAGIC) {
       goto bad_elf_cleanup_pgdir;
   }
   
   // 遍历程序头表，加载各个段
   for (; ph < ph_end; ph++) {
       if (ph->p_type != ELF_PT_LOAD) continue;
       
       // 为每个段分配内存并建立映射
       // 从二进制文件复制内容到内存
   }
   ```

   **步骤 (4)**: 建立用户栈
   ```c
   vm_flags = VM_READ | VM_WRITE | VM_STACK;
   mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL);
   
   // 预先分配 4 页栈空间
   pgdir_alloc_page(mm->pgdir, USTACKTOP - PGSIZE, PTE_USER);
   pgdir_alloc_page(mm->pgdir, USTACKTOP - 2 * PGSIZE, PTE_USER);
   pgdir_alloc_page(mm->pgdir, USTACKTOP - 3 * PGSIZE, PTE_USER);
   pgdir_alloc_page(mm->pgdir, USTACKTOP - 4 * PGSIZE, PTE_USER);
   ```

   **步骤 (5)**: 设置进程的内存管理信息
   ```c
   mm_count_inc(mm);
   current->mm = mm;
   current->pgdir = PADDR(mm->pgdir);
   lsatp(PADDR(mm->pgdir));  // 切换到新的页表
   ```

   **步骤 (6)**: 设置中断帧（本练习的核心）
   ```c
   struct trapframe *tf = current->tf;
   uintptr_t sstatus = tf->status;
   memset(tf, 0, sizeof(struct trapframe));
   
   tf->gpr.sp = USTACKTOP;              // 用户栈顶
   tf->epc = elf->e_entry;              // 程序入口
   tf->status = (sstatus & ~SSTATUS_SPP) | SSTATUS_SPIE;  // 返回用户态并开中断
   ```

#### 阶段 3：从内核态返回到用户态

9. **异常返回**（`kern/trap/trapentry.S`）：
   - `load_icode` 返回后，逐层返回到 `exception_handler` → `trap` → `__trapret`
   
   ```asm
   __trapret:
       RESTORE_ALL       # 恢复中断帧中的所有寄存器
       sret              # 从 S mode 返回
   ```

10. **执行 RESTORE_ALL 宏**：
    ```asm
    RESTORE_ALL:
        LOAD s1, 32*REGBYTES(sp)  # 加载 sstatus
        LOAD s2, 33*REGBYTES(sp)  # 加载 epc
        
        andi s0, s1, SSTATUS_SPP  # 检查 SPP 位
        bnez s0, _restore_context # 如果 SPP=1，跳过保存内核栈
        
    _save_kernel_sp:
        addi s0, sp, 36 * REGBYTES
        csrw sscratch, s0         # 将内核栈顶保存到 sscratch
        
    _restore_context:
        csrw sstatus, s1          # 恢复 sstatus
        csrw sepc, s2             # 恢复 epc
        
        # 恢复所有通用寄存器（x1, x3-x31）
        LOAD x1, 1*REGBYTES(sp)
        LOAD x3, 3*REGBYTES(sp)
        ...
        LOAD x31, 31*REGBYTES(sp)
        
        # 恢复 sp（用户栈指针）
        LOAD x2, 2*REGBYTES(sp)
    ```

11. **执行 sret 指令**：
    - `sret` 是 RISC-V 的"从 Supervisor 模式返回"指令
    - 硬件自动完成以下操作：
      1. 将 `sepc` 的值加载到 `pc` → CPU 跳转到用户程序入口（`elf->e_entry`）
      2. 将 `sstatus.SPIE` 复制到 `sstatus.SIE` → 恢复中断使能状态
      3. 根据 `sstatus.SPP` 切换特权级 → 因为 SPP=0，切换到 U mode
      4. 将 `sstatus.SPP` 设置为 0（为下次中断做准备）

#### 阶段 4：开始执行用户程序

12. **CPU 进入用户态**：
    - 此时 CPU 状态：
      - 特权级：U mode（用户态）
      - PC：指向用户程序的入口地址（`elf->e_entry`）
      - SP：指向用户栈顶（`USTACKTOP`）
      - 中断：已开启（SIE=1）

13. **执行用户程序的第一条指令**：
    - CPU 从 `elf->e_entry` 地址开始取指令执行
    - 用户程序开始运行（例如 `exit.c` 的 `main` 函数）

#### 关键机制总结

| 阶段 | 关键操作 | 目的 |
|------|---------|------|
| **进程创建** | `kernel_thread` → `do_fork` | 创建进程控制块，分配资源 |
| **触发系统调用** | `ebreak` + `a7=10` | 从 S mode 模拟系统调用 |
| **加载程序** | `do_execve` → `load_icode` | 解析 ELF，建立地址空间 |
| **设置中断帧** | 配置 `tf->sp/epc/status` | 准备返回用户态的环境 |
| **特权级切换** | `sret` 指令 | 硬件完成 S mode → U mode |
| **开始执行** | PC → `elf->e_entry` | 用户程序第一条指令 |

#### 与 Lab4 的对比

在 Lab4 中，所有进程都是内核线程，运行在 S mode：
- 进程切换通过 `switch_to` 完成，不涉及特权级切换
- 新进程通过 `forkret` → `forkrets` → `__trapret` 启动，但返回后仍在 S mode

在 Lab5 中，引入了用户进程，需要在 U mode 和 S mode 之间切换：
- 用户进程在 U mode 运行，受限于用户态权限
- 通过 `sret` 指令从 S mode 返回 U mode
- 通过 `ecall` 指令从 U mode 进入 S mode（系统调用）
- `trapframe` 中的 `status` 寄存器控制返回到哪个特权级

---

## 练习 2：父进程复制自己的内存空间给子进程

### 设计实现过程

`copy_range` 函数是 `fork` 系统调用实现的关键部分，负责将父进程的内存内容复制到子进程。当用户程序调用 `fork()` 创建子进程时，子进程需要获得父进程的用户内存空间的完整副本，这样父子进程才能独立运行而互不影响。

#### 实现代码（`kern/mm/pmm.c` 第 425-432 行）

```c
/* LAB5:EXERCISE2 YOUR CODE
 * replicate content of page to npage, build the map of phy addr of
 * nage with the linear addr start
 */

// (1) 找到源页面的内核虚拟地址
void *src_kvaddr = page2kva(page);

// (2) 找到目标页面的内核虚拟地址
void *dst_kvaddr = page2kva(npage);

// (3) 从源地址复制到目标地址，大小为一页
memcpy(dst_kvaddr, src_kvaddr, PGSIZE);

// (4) 建立目标页面物理地址与线性地址start的映射
ret = page_insert(to, npage, start, perm);
```

#### 实现要点

1. **获取源页面的内核虚拟地址**：
   - `page` 是父进程的物理页面结构
   - `page2kva(page)` 将物理页面转换为内核虚拟地址
   - 内核虚拟地址是可以直接访问的地址

2. **获取目标页面的内核虚拟地址**：
   - `npage` 是新分配给子进程的物理页面
   - 同样用 `page2kva(npage)` 获取其内核虚拟地址

3. **执行内存拷贝**：
   - 使用 `memcpy(dst, src, PGSIZE)` 复制整页内容（4KB）
   - 这样子进程就获得了父进程该页的完整副本

4. **建立页表映射**：
   - `page_insert(to, npage, start, perm)` 在子进程的页表中建立映射
   - `to`：子进程的页目录基址
   - `npage`：子进程的物理页面
   - `start`：虚拟地址（在子进程地址空间中的位置）
   - `perm`：权限位，从父进程的页表项中复制而来

#### copy_range 函数的完整流程

让我们看看 `copy_range` 函数的整体逻辑（第 376-439 行）：

```c
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share)
{
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));
    
    // 按页为单位复制内容
    do {
        // 1. 在父进程页表中查找虚拟地址 start 对应的页表项
        pte_t *ptep = get_pte(from, start, 0), *nptep;
        if (ptep == NULL) {
            // 如果该页表项不存在，跳到下一个页表
            start = ROUNDDOWN(start + PTSIZE, PTSIZE);
            continue;
        }
        
        // 2. 检查页表项是否有效（PTE_V 位）
        if (*ptep & PTE_V) {
            // 3. 在子进程页表中查找或创建页表项
            if ((nptep = get_pte(to, start, 1)) == NULL) {
                return -E_NO_MEM;
            }
            
            // 4. 提取权限位
            uint32_t perm = (*ptep & PTE_USER);
            
            // 5. 获取父进程的物理页面
            struct Page *page = pte2page(*ptep);
            
            // 6. 为子进程分配新的物理页面
            struct Page *npage = alloc_page();
            assert(page != NULL);
            assert(npage != NULL);
            
            // 7. 执行内存复制和映射建立（练习 2 的内容）
            void *src_kvaddr = page2kva(page);
            void *dst_kvaddr = page2kva(npage);
            memcpy(dst_kvaddr, src_kvaddr, PGSIZE);
            ret = page_insert(to, npage, start, perm);
            
            assert(ret == 0);
        }
        
        // 8. 移动到下一页
        start += PGSIZE;
    } while (start != 0 && start < end);
    
    return 0;
}
```

#### 调用链：fork 到 copy_range

用户程序调用 `fork()` 的完整流程：

```
用户程序: fork()
    ↓
用户库: sys_fork() [user/libs/syscall.c]
    ↓
系统调用: ecall 指令
    ↓
异常处理: exception_handler() [kern/trap/trap.c]
    ↓
系统调用分发: syscall() [kern/syscall/syscall.c]
    ↓
系统调用实现: sys_fork() [kern/syscall/syscall.c]
    ↓
进程创建: do_fork() [kern/process/proc.c]
    ↓
内存复制: copy_mm() [kern/process/proc.c]
    ↓
映射复制: dup_mmap() [kern/mm/vmm.c]
    ↓
范围复制: copy_range() [kern/mm/pmm.c]  ← 练习 2
```

#### do_fork 中的 copy_mm 调用

在 `do_fork` 函数中（第 429-486 行），会调用 `copy_mm` 来处理内存空间的复制：

```c
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    // ...
    
    // 3. 调用 copy_mm 复制或共享内存管理信息
    if (copy_mm(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_kstack;
    }
    
    // ...
}
```

`copy_mm` 函数（第 360-406 行）：

```c
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc) {
    struct mm_struct *mm, *oldmm = current->mm;
    
    // 如果当前是内核线程（mm == NULL），直接返回
    if (oldmm == NULL) {
        return 0;
    }
    
    // 如果设置了 CLONE_VM 标志，共享内存空间
    if (clone_flags & CLONE_VM) {
        mm = oldmm;
        goto good_mm;
    }
    
    // 否则，创建新的内存空间
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }
    
    // 为子进程创建新的页目录表
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }
    
    // 复制父进程的内存映射
    lock_mm(oldmm);
    {
        ret = dup_mmap(mm, oldmm);  // 这里会调用 copy_range
    }
    unlock_mm(oldmm);
    
    if (ret != 0) {
        goto bad_dup_cleanup_mmap;
    }
    
good_mm:
    mm_count_inc(mm);
    proc->mm = mm;
    proc->pgdir = PADDR(mm->pgdir);
    return 0;
    
    // 错误处理...
}
```

#### 关键数据结构

1. **Page 结构**（`kern/mm/memlayout.h`）：
   ```c
   struct Page {
       int ref;                    // 引用计数
       uint32_t flags;             // 页面标志
       unsigned int property;      // 空闲块大小
       list_entry_t page_link;     // 链表指针
   };
   ```

2. **页表项格式**（RISC-V sv39）：
   ```
   | PPN[2] | PPN[1] | PPN[0] | RSW | D | A | G | U | X | W | R | V |
     63-54    53-28    27-19   18-17 16  15  14  13  12  11  10  9
   ```
   - V (Valid)：有效位
   - R (Read)：可读
   - W (Write)：可写
   - X (Execute)：可执行
   - U (User)：用户态可访问
   - PPN：物理页号

3. **地址转换辅助宏**：
   ```c
   // 物理页面 → 内核虚拟地址
   #define page2kva(page) KADDR(page2pa(page))
   
   // 物理页面 → 物理地址
   #define page2pa(page) ((page - pages) << PGSHIFT)
   
   // 物理地址 → 内核虚拟地址
   #define KADDR(pa) ((void *)((pa) + va_pa_offset))
   ```

### Copy on Write (COW) 机制设计

**问题**：如何设计实现 Copy on Write 机制？给出概要设计。

**回答**：

Copy-on-Write (COW) 是一种延迟复制优化技术。在 `fork()` 时不立即复制内存，而是让父子进程共享物理页面，只有当其中一个进程试图修改某页时，才真正复制该页。这样可以：
- 减少 `fork()` 的开销（不需要立即复制所有内存）
- 节省内存（如果页面没有被修改，就不需要额外的物理页面）
- 提高性能（很多情况下子进程 `fork()` 后立即 `exec()`，不需要访问父进程的内存）

#### COW 概要设计

**1. 数据结构扩展**

在 `struct Page` 中添加引用计数和 COW 标记：
```c
struct Page {
    int ref;                    // 物理页引用计数（已有）
    uint32_t flags;             // 添加 COW 标志位
    // ...
};

// 页面标志
#define PG_reserved     0       // 保留页
#define PG_property     1       // 空闲页
#define PG_cow          2       // COW 页面（新增）
```

**2. fork 时的处理**

修改 `copy_range` 函数，不再复制页面内容，而是共享物理页：

```c
int copy_range_cow(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end) {
    do {
        pte_t *ptep = get_pte(from, start, 0);
        if (ptep == NULL || !(*ptep & PTE_V)) {
            start += PGSIZE;
            continue;
        }
        
        // 获取父进程的物理页面
        struct Page *page = pte2page(*ptep);
        uint32_t perm = (*ptep & PTE_USER);
        
        // 1. 修改父进程的页表项：去掉写权限，标记为只读
        if (perm & PTE_W) {
            perm &= ~PTE_W;  // 清除写权限
            *ptep = pte_create(page2ppn(page), perm);
            tlb_invalidate(from, start);  // 刷新 TLB
        }
        
        // 2. 子进程也映射到同一物理页，且为只读
        pte_t *nptep = get_pte(to, start, 1);
        *nptep = pte_create(page2ppn(page), perm);
        
        // 3. 增加物理页的引用计数
        page_ref_inc(page);
        
        // 4. 标记这个页面为 COW 页面
        SetPageCOW(page);
        
        start += PGSIZE;
    } while (start != 0 && start < end);
    
    return 0;
}
```

**3. 缺页异常处理**

当进程尝试写入 COW 页面时，会触发 Page Fault，需要在 `do_pgfault` 中处理：

```c
int do_pgfault(struct mm_struct *mm, uint_t error_code, uintptr_t addr) {
    // ... 现有的缺页处理 ...
    
    pte_t *ptep = get_pte(mm->pgdir, addr, 0);
    if (ptep != NULL && (*ptep & PTE_V)) {
        // 页面存在但不可写，可能是 COW 页面
        if (!(error_code & PTE_W) && (*ptep & PTE_V) && !(*ptep & PTE_W)) {
            // 这是一个写保护错误，可能是 COW
            struct Page *page = pte2page(*ptep);
            
            if (PageCOW(page)) {
                return handle_cow_fault(mm, error_code, addr, page, ptep);
            }
        }
    }
    
    // ... 其他缺页处理 ...
}

int handle_cow_fault(struct mm_struct *mm, uint_t error_code, 
                     uintptr_t addr, struct Page *page, pte_t *ptep) {
    // 1. 检查引用计数
    if (page_ref(page) == 1) {
        // 只有一个进程引用这个页面，直接恢复写权限
        uint32_t perm = (*ptep & PTE_USER) | PTE_W;
        *ptep = pte_create(page2ppn(page), perm);
        ClearPageCOW(page);
        tlb_invalidate(mm->pgdir, addr);
        return 0;
    }
    
    // 2. 多个进程引用，需要复制页面
    struct Page *npage = alloc_page();
    if (npage == NULL) {
        return -E_NO_MEM;
    }
    
    // 3. 复制页面内容
    void *src_kva = page2kva(page);
    void *dst_kva = page2kva(npage);
    memcpy(dst_kva, src_kva, PGSIZE);
    
    // 4. 更新页表映射到新页面，并恢复写权限
    uintptr_t la = ROUNDDOWN(addr, PGSIZE);
    uint32_t perm = (*ptep & PTE_USER) | PTE_W;
    page_insert(mm->pgdir, npage, la, perm);
    
    // 5. 减少原页面的引用计数
    page_ref_dec(page);
    if (page_ref(page) == 1) {
        // 如果只剩一个引用，清除 COW 标记
        ClearPageCOW(page);
    }
    
    return 0;
}
```

**4. 状态转换图**

```
[初始状态]
父进程拥有页面 P，可读写

          ↓ fork()

[COW 共享状态]
父进程：页面 P，只读，ref=2，COW 标记
子进程：页面 P，只读，ref=2，COW 标记
          ↙          ↘
    父进程写入      子进程写入
          ↓              ↓
[分离状态]         [分离状态]
父进程：页面 P'，可读写，ref=1
子进程：页面 P，只读，ref=1
或反之（取决于谁先写入）

          ↓ 两者都写入

[完全分离状态]
父进程：页面 P'，可读写，ref=1
子进程：页面 P''，可读写，ref=1
```

**5. 实现要点**

1. **TLB 刷新**：
   - 修改页表项后必须刷新 TLB
   - 使用 `sfence.vma` 指令或 `tlb_invalidate` 函数

2. **引用计数**：
   - 正确维护物理页的引用计数
   - `fork` 时 +1，复制分离时 -1
   - 引用计数为 0 时才能释放物理页

3. **权限管理**：
   - COW 页面设置为只读（清除 `PTE_W`）
   - 真正复制后恢复写权限
   - 需要保存原始权限，以便恢复

4. **错误码判断**：
   - Page Fault 的 `error_code` 可以区分是读错误还是写错误
   - RISC-V 中通过 `scause` 寄存器判断异常原因

5. **竞态条件**：
   - 多个进程同时写入同一 COW 页面
   - 需要使用锁保护 `handle_cow_fault`
   - 或者使用原子操作处理引用计数

**6. 优化**

1. **快速路径**：
   - 如果 `ref==1`，直接恢复写权限，不需要复制
   - 这在父进程 `fork` 后子进程立即 `exit` 的场景很有用

2. **延迟刷新 TLB**：
   - 批量修改页表后一次性刷新 TLB
   - 减少昂贵的 TLB 刷新操作

3. **零页优化**：
   - 对于全零的页面，可以映射到同一个零页
   - 只有在写入时才分配真实的物理页

**7. 潜在问题**

参考 Dirty COW 漏洞 (CVE-2016-5195)：
- **问题**：在处理 COW 时，检查和复制操作之间存在竞态窗口
- **攻击**：恶意线程在检查后、复制前修改页面，绕过写保护
- **解决**：
  1. 在整个 COW 处理过程中持有锁
  2. 使用原子操作确保检查和修改的原子性
  3. 在复制后再次验证页面状态

```c
int handle_cow_fault_safe(struct mm_struct *mm, uint_t error_code,
                          uintptr_t addr, struct Page *page, pte_t *ptep) {
    // 加锁，防止竞态
    lock_mm(mm);
    
    // 再次检查页表项状态（防止 TOCTTOU）
    if (!(*ptep & PTE_V) || (*ptep & PTE_W)) {
        unlock_mm(mm);
        return 0;  // 其他线程已经处理了
    }
    
    // ... COW 处理逻辑 ...
    
    unlock_mm(mm);
    return 0;
}
```

#### 总结

COW 机制是一个复杂但非常有价值的优化：
- **优点**：减少内存复制，节省内存，提高 `fork` 性能
- **难点**：需要正确处理引用计数、TLB 刷新、竞态条件
- **应用**：几乎所有现代操作系统（Linux、BSD、Windows）都实现了 COW

在 ucore 中实现 COW 需要修改：
- `copy_range`：共享页面而不是复制
- `do_pgfault`：处理写保护异常
- `Page` 结构：添加 COW 标志
- `page_insert`/`page_remove`：正确维护引用计数

---

## 练习 3：阅读分析源代码，理解进程执行 fork/exec/wait/exit 的实现

### fork 执行流程分析

`fork()` 系统调用用于创建当前进程的副本，生成一个新的子进程。

#### 用户态部分

```c
// user/libs/ulib.c
int fork(void) { 
    return sys_fork(); 
}

// user/libs/syscall.c
int sys_fork(void) { 
    return syscall(SYS_fork); 
}

static inline int syscall(int num, ...) {
    va_list ap;
    va_start(ap, num);
    uint64_t a[MAX_ARGS];
    for (i = 0; i < MAX_ARGS; i++) {
        a[i] = va_arg(ap, uint64_t);
    }
    va_end(ap);
    
    asm volatile (
        "ld a0, %1\n"    // 系统调用号 SYS_fork
        "ecall\n"        // 触发系统调用
        "sd a0, %0"      // 返回值保存到 ret
        : "=m" (ret)
        : "m"(num), "m"(a[0]), ...
    );
    return ret;
}
```

**关键点**：
- 用户态通过 `ecall` 指令陷入内核态
- 系统调用号通过 `a0` 寄存器传递

#### 内核态部分

**1. 异常处理**（`kern/trap/trap.c`）：

```c
void exception_handler(struct trapframe *tf) {
    switch (tf->cause) {
        case CAUSE_USER_ECALL:
            tf->epc += 4;  // 跳过 ecall 指令
            syscall();     // 调用系统调用处理函数
            break;
    }
}
```

**2. 系统调用分发**（`kern/syscall/syscall.c` 第 82-100 行）：

```c
void syscall(void) {
    struct trapframe *tf = current->tf;
    uint64_t arg[5];
    int num = tf->gpr.a0;  // 获取系统调用号
    
    if (num >= 0 && num < NUM_SYSCALLS) {
        if (syscalls[num] != NULL) {
            arg[0] = tf->gpr.a1;
            arg[1] = tf->gpr.a2;
            // ...
            tf->gpr.a0 = syscalls[num](arg);  // 调用 sys_fork
            return;
        }
    }
    
    panic("undefined syscall %d\n", num);
}
```

**3. sys_fork 实现**（`kern/syscall/syscall.c` 第 16-20 行）：

```c
static int sys_fork(uint64_t arg[]) {
    struct trapframe *tf = current->tf;
    uintptr_t stack = tf->gpr.sp;  // 获取用户栈指针
    return do_fork(0, stack, tf);
}
```

**4. do_fork 核心实现**（`kern/process/proc.c` 第 429-486 行）：

```c
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    
    // 检查进程数是否达到上限
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    
    ret = -E_NO_MEM;
    
    // (1) 分配进程控制块
    if ((proc = alloc_proc()) == NULL) {
        goto fork_out;
    }
    proc->parent = current;
    
    // (2) 分配内核栈
    if (setup_kstack(proc) != 0) {
        goto bad_fork_cleanup_proc;
    }
    
    // (3) 复制或共享内存空间
    if (copy_mm(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_kstack;
    }
    
    // (4) 复制中断帧和上下文
    copy_thread(proc, stack, tf);
    
    // (5) 将新进程加入进程列表（临界区）
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        list_add(&proc_list, &(proc->list_link));
        nr_process++;
    }
    local_intr_restore(intr_flag);
    
    // (6) 唤醒新进程
    wakeup_proc(proc);
    
    // (7) 返回子进程的 PID
    ret = proc->pid;
    
fork_out:
    return ret;
    
// 错误处理
bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}
```

**5. copy_thread 实现**（`kern/process/proc.c` 第 411-422 行）：

```c
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
    // 在内核栈顶分配中断帧
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    
    // 复制父进程的中断帧
    *(proc->tf) = *tf;
    
    // 设置子进程的返回值为 0（这样子进程知道自己是被 fork 出来的）
    proc->tf->gpr.a0 = 0;
    
    // 设置栈指针
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;
    
    // 设置上下文：返回地址为 forkret，栈指针指向中断帧
    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)(proc->tf);
}
```

**关键点**：
- 子进程的 `a0` 寄存器（返回值）被设置为 0
- 父进程的 `a0` 寄存器（返回值）是子进程的 PID
- 这样父子进程从 `fork()` 返回时可以通过返回值区分自己的身份

#### fork 的返回过程

**父进程**：
```
do_fork 返回子进程 PID
    ↓
sys_fork 返回 PID
    ↓
syscall() 将 PID 写入 tf->gpr.a0
    ↓
__trapret 恢复寄存器（包括 a0）
    ↓
sret 返回用户态
    ↓
用户态 fork() 得到子进程 PID
```

**子进程**（首次被调度）：
```
schedule() 调度到子进程
    ↓
proc_run() 切换到子进程
    ↓
switch_to() 恢复 context
    ↓
返回到 context.ra（即 forkret）
    ↓
forkret 调用 forkrets
    ↓
forkrets 跳转到 __trapret
    ↓
__trapret 恢复中断帧（a0=0）
    ↓
sret 返回用户态
    ↓
用户态 fork() 得到 0
```

### exec 执行流程分析

`exec()` 系统调用用于在当前进程中加载并执行一个新的程序，替换当前进程的地址空间。

#### 用户态部分

```c
// user/libs/ulib.c
int exec(char *name, size_t len, unsigned char *binary, size_t size) {
    return sys_exec(name, len, binary, size);
}
```

#### 内核态部分

**1. sys_exec 实现**（`kern/syscall/syscall.c` 第 30-36 行）：

```c
static int sys_exec(uint64_t arg[]) {
    const char *name = (const char *)arg[0];
    size_t len = (size_t)arg[1];
    unsigned char *binary = (unsigned char *)arg[2];
    size_t size = (size_t)arg[3];
    return do_execve(name, len, binary, size);
}
```

**2. do_execve 实现**（`kern/process/proc.c` 第 759-798 行）：

```c
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size) {
    struct mm_struct *mm = current->mm;
    
    // (1) 检查用户传入的地址是否合法
    if (!user_mem_check(mm, (uintptr_t)name, len, 0)) {
        return -E_INVAL;
    }
    
    // (2) 保存程序名
    if (len > PROC_NAME_LEN) {
        len = PROC_NAME_LEN;
    }
    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));
    memcpy(local_name, name, len);
    
    // (3) 如果当前进程有用户内存空间，先释放
    if (mm != NULL) {
        cputs("mm != NULL");
        lsatp(boot_pgdir_pa);  // 切换到内核页表
        
        // 减少引用计数，如果为 0 则释放
        if (mm_count_dec(mm) == 0) {
            exit_mmap(mm);      // 取消内存映射
            put_pgdir(mm);      // 释放页表
            mm_destroy(mm);     // 销毁 mm 结构
        }
        current->mm = NULL;
    }
    
    // (4) 加载新程序
    int ret;
    if ((ret = load_icode(binary, size)) != 0) {
        goto execve_exit;
    }
    
    // (5) 设置进程名
    set_proc_name(current, local_name);
    return 0;
    
execve_exit:
    do_exit(ret);  // 加载失败，退出进程
    panic("already exit: %e.\n", ret);
}
```

**3. load_icode 实现**（已在练习 1 中详细分析）：

主要步骤：
1. 创建新的 mm 结构
2. 创建新的页目录表
3. 解析 ELF 文件，加载代码段、数据段
4. 建立用户栈
5. 设置 mm 和页表
6. **设置中断帧**（练习 1 的内容）

**关键点**：
- `exec` 不创建新进程，只是替换当前进程的内容
- PID 保持不变
- 原有的地址空间被完全替换
- 常见用法：`fork()` 后立即 `exec()`，实现"创建新进程执行新程序"

### wait 执行流程分析

`wait()` 系统调用使父进程等待子进程退出，并回收子进程的资源。

#### 用户态部分

```c
// user/libs/ulib.c
int wait(void) { 
    return sys_wait(0, NULL); 
}

int waitpid(int pid, int *store) { 
    return sys_wait(pid, store); 
}
```

#### 内核态部分

**1. sys_wait 实现**（`kern/syscall/syscall.c` 第 23-27 行）：

```c
static int sys_wait(uint64_t arg[]) {
    int pid = (int)arg[0];
    int *store = (int *)arg[1];
    return do_wait(pid, store);
}
```

**2. do_wait 实现**（`kern/process/proc.c` 第 810-880 行）：

```c
int do_wait(int pid, int *code_store) {
    struct mm_struct *mm = current->mm;
    
    // (1) 检查 code_store 指针是否合法
    if (code_store != NULL) {
        if (!user_mem_check(mm, (uintptr_t)code_store, sizeof(int), 1)) {
            return -E_INVAL;
        }
    }
    
    struct proc_struct *proc;
    bool intr_flag, haskid;
    
repeat:
    haskid = 0;
    
    // (2) 查找目标子进程
    if (pid != 0) {
        // 等待指定 PID 的子进程
        proc = find_proc(pid);
        if (proc != NULL && proc->parent == current) {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE) {
                goto found;  // 找到已退出的子进程
            }
        }
    } else {
        // 等待任意子进程（pid == 0）
        proc = current->cptr;  // cptr 指向第一个子进程
        for (; proc != NULL; proc = proc->optr) {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE) {
                goto found;  // 找到已退出的子进程
            }
        }
    }
    
    // (3) 如果有子进程但都还在运行，父进程进入睡眠
    if (haskid) {
        current->state = PROC_SLEEPING;
        current->wait_state = WT_CHILD;
        schedule();  // 让出 CPU，等待被唤醒
        
        // 被唤醒后，检查是否被 kill
        if (current->flags & PF_EXITING) {
            do_exit(-E_KILLED);
        }
        goto repeat;  // 重新查找
    }
    
    // (4) 没有子进程
    return -E_BAD_PROC;
    
found:
    // (5) 找到已退出的子进程，回收资源
    if (proc == idleproc || proc == initproc) {
        panic("wait idleproc or initproc.\n");
    }
    
    // (6) 返回子进程的退出码
    if (code_store != NULL) {
        *code_store = proc->exit_code;
    }
    
    // (7) 从进程列表中移除（临界区）
    local_intr_save(intr_flag);
    {
        unhash_proc(proc);   // 从哈希表移除
        remove_links(proc);  // 从进程树移除
    }
    local_intr_restore(intr_flag);
    
    // (8) 释放子进程的资源
    put_kstack(proc);  // 释放内核栈
    kfree(proc);       // 释放 PCB
    
    return 0;
}
```

**关键点**：
- 如果子进程还在运行，父进程进入 `PROC_SLEEPING` 状态
- 父进程设置 `wait_state = WT_CHILD`，表示在等待子进程
- 子进程退出时会唤醒父进程（在 `do_exit` 中）
- 父进程被唤醒后重新查找，直到找到已退出的子进程
- 只有父进程调用 `wait` 后，子进程的资源才会被完全回收

### exit 执行流程分析

`exit()` 系统调用用于终止当前进程，释放其占用的资源。

#### 用户态部分

```c
// user/libs/ulib.c
void exit(int error_code) {
    sys_exit(error_code);
    cprintf("BUG: exit failed.\n");  // 不应该执行到这里
    while (1);
}
```

#### 内核态部分

**1. sys_exit 实现**（`kern/syscall/syscall.c` 第 10-13 行）：

```c
static int sys_exit(uint64_t arg[]) {
    int error_code = (int)arg[0];
    return do_exit(error_code);
}
```

**2. do_exit 实现**（`kern/process/proc.c` 第 517-574 行）：

```c
int do_exit(int error_code) {
    // (1) 检查是否是特殊进程
    if (current == idleproc) {
        panic("idleproc exit.\n");
    }
    if (current == initproc) {
        panic("initproc exit.\n");
    }
    
    struct mm_struct *mm = current->mm;
    
    // (2) 如果是用户进程，释放用户内存空间
    if (mm != NULL) {
        lsatp(boot_pgdir_pa);  // 切换到内核页表
        
        // 减少引用计数，如果为 0 则释放
        if (mm_count_dec(mm) == 0) {
            exit_mmap(mm);      // 取消所有内存映射
            put_pgdir(mm);      // 释放页目录表
            mm_destroy(mm);     // 销毁 mm 结构
        }
        current->mm = NULL;
    }
    
    // (3) 设置进程状态为僵尸态
    current->state = PROC_ZOMBIE;
    current->exit_code = error_code;
    
    bool intr_flag;
    struct proc_struct *proc;
    
    // (4) 唤醒父进程（临界区）
    local_intr_save(intr_flag);
    {
        proc = current->parent;
        
        // 如果父进程在等待子进程，唤醒它
        if (proc->wait_state == WT_CHILD) {
            wakeup_proc(proc);
        }
        
        // (5) 将所有子进程托付给 initproc
        while (current->cptr != NULL) {
            proc = current->cptr;
            current->cptr = proc->optr;  // 断开与子进程的链接
            
            // 将子进程插入到 initproc 的子进程链表
            proc->yptr = NULL;
            if ((proc->optr = initproc->cptr) != NULL) {
                initproc->cptr->yptr = proc;
            }
            proc->parent = initproc;
            initproc->cptr = proc;
            
            // 如果子进程也是僵尸态，唤醒 initproc
            if (proc->state == PROC_ZOMBIE) {
                if (initproc->wait_state == WT_CHILD) {
                    wakeup_proc(initproc);
                }
            }
        }
    }
    local_intr_restore(intr_flag);
    
    // (6) 调度到其他进程
    schedule();
    
    // 不应该执行到这里
    panic("do_exit will not return!! %d.\n", current->pid);
}
```

**关键点**：
- 进程自己只能释放用户内存空间，不能释放内核栈和 PCB
- 进程进入僵尸态（`PROC_ZOMBIE`），保留 PCB 和退出码
- 如果父进程在 `wait`，会被唤醒来回收子进程
- 子进程被托付给 `initproc`（PID=1），避免成为孤儿进程
- 最后调用 `schedule()`，永不返回

### 进程状态生命周期图

```
                    [创建]
                      ↓
               alloc_proc()
                      ↓
                 PROC_UNINIT
                      ↓
                wakeup_proc()
                      ↓
              +→ PROC_RUNNABLE ←+
              |       ↓          |
              |  schedule()      |
              |       ↓          |
              |   proc_run()     |
              |       ↓          |
              |   RUNNING        |
              |       ↓          | do_yield()
              |   (执行中)       | 时间片用完
              |       ↓          |
              |  +---+---+-------+
              |  |       |       
              |  |   系统调用    
              |  |       |       
              |  |   +---+-------+-------+
              |  |   |           |       |
              |  wait()       exit()  其他
              |  |               |       
              |  ↓               ↓       
         PROC_SLEEPING     PROC_ZOMBIE
              |               |
              | wakeup_proc() | parent wait()
              +---------------+
                              ↓
                          资源回收
                              ↓
                          [销毁]
```

**状态说明**：

1. **PROC_UNINIT**（未初始化）
   - 进程刚被创建，PCB 已分配但未完全初始化
   - 转换：`wakeup_proc()` → `PROC_RUNNABLE`

2. **PROC_RUNNABLE**（可运行）
   - 进程准备就绪，等待被调度到 CPU
   - 转换：`schedule()` → `RUNNING`

3. **RUNNING**（运行中）
   - 进程正在 CPU 上执行
   - 转换：
     - `do_yield()` / 时间片用完 → `PROC_RUNNABLE`（主动或被动让出 CPU）
     - `do_wait()` → `PROC_SLEEPING`（等待子进程）
     - `do_exit()` → `PROC_ZOMBIE`（退出）

4. **PROC_SLEEPING**（睡眠）
   - 进程等待某个事件发生（如 I/O、子进程退出）
   - 转换：`wakeup_proc()` → `PROC_RUNNABLE`

5. **PROC_ZOMBIE**（僵尸）
   - 进程已退出，但资源尚未被完全回收
   - 保留 PCB 和退出码，等待父进程 `wait()`
   - 转换：父进程调用 `wait()` → 资源回收 → 销毁

### 用户态与内核态的交错执行

#### 系统调用的完整流程

```
[用户态 U Mode]
    用户程序调用 fork()
        ↓
    库函数 sys_fork()
        ↓
    设置寄存器（a0=SYS_fork）
        ↓
    执行 ecall 指令
        ↓ (硬件自动完成)
    +-----------------------+
    | 1. 保存 PC 到 sepc    |
    | 2. 保存特权级到 SPP   |
    | 3. 切换到 S Mode      |
    | 4. 跳转到 stvec       |
    +-----------------------+
        ↓
[内核态 S Mode]
    __alltraps (trapentry.S)
        ↓
    保存中断帧到内核栈
        ↓
    trap(tf)
        ↓
    exception_handler(tf)
        ↓
    syscall()
        ↓
    syscalls[SYS_fork](arg)
        ↓
    sys_fork()
        ↓
    do_fork()
        ↓
    [执行 fork 逻辑]
        ↓
    返回子进程 PID
        ↓
    将 PID 写入 tf->gpr.a0
        ↓
    __trapret (trapentry.S)
        ↓
    恢复中断帧
        ↓
    执行 sret 指令
        ↓ (硬件自动完成)
    +-----------------------+
    | 1. 恢复 PC = sepc     |
    | 2. 根据 SPP 切换特权级 |
    | 3. 返回到 U Mode      |
    +-----------------------+
        ↓
[用户态 U Mode]
    从 ecall 的下一条指令继续执行
        ↓
    fork() 返回（a0 中是返回值）
```

#### 关键寄存器的作用

| 寄存器 | 作用 | 何时使用 |
|--------|------|---------|
| `sstatus` | 保存状态信息（特权级、中断使能） | `ecall`/`sret` 时由硬件读写 |
| `sepc` | 保存异常返回地址 | `ecall` 时保存 PC，`sret` 时恢复 |
| `scause` | 保存异常原因 | 内核判断异常类型 |
| `stval` | 保存异常相关的值（如出错地址） | 处理缺页异常 |
| `stvec` | 保存中断向量表基址 | `ecall` 时跳转到这里 |
| `sscratch` | 临时寄存器 | 保存内核栈指针 |
| `a0-a7` | 参数和返回值寄存器 | 传递系统调用参数和返回值 |

#### 用户态 vs 内核态的操作划分

**用户态完成的操作**：
1. 准备系统调用参数（放入寄存器）
2. 执行 `ecall` 触发系统调用
3. 接收返回值（从 `a0` 寄存器）
4. 处理返回结果

**内核态完成的操作**：
1. 保存用户态的 CPU 状态（中断帧）
2. 根据系统调用号分发到具体函数
3. 执行系统调用的实际逻辑
4. 将返回值写入中断帧的 `a0`
5. 恢复用户态的 CPU 状态
6. 通过 `sret` 返回用户态

**内核态执行结果如何返回**：
- 系统调用的返回值通过 `tf->gpr.a0` 传递
- 在 `__trapret` 中恢复寄存器时，`a0` 被恢复为系统调用的返回值
- 用户态程序从 `ecall` 返回后，直接从 `a0` 读取返回值

#### fork 的特殊性

`fork()` 是一个特殊的系统调用，因为它会"返回两次"：

**父进程的返回路径**（正常返回）：
```
do_fork() 返回子进程 PID
    ↓
sys_fork() 返回 PID
    ↓
tf->gpr.a0 = PID
    ↓
__trapret 恢复寄存器
    ↓
sret 返回用户态
    ↓
用户态 fork() 返回 PID
```

**子进程的返回路径**（首次调度）：
```
schedule() 选中子进程
    ↓
proc_run() 切换到子进程
    ↓
switch_to() 恢复 context
    ↓
返回到 forkret
    ↓
forkret → forkrets → __trapret
    ↓
恢复中断帧（a0 已在 copy_thread 中设为 0）
    ↓
sret 返回用户态
    ↓
用户态 fork() 返回 0
```

**关键点**：
- 子进程的中断帧是父进程的副本，但 `a0` 被修改为 0
- 子进程第一次执行时，通过恢复这个中断帧"伪装"成从 `fork()` 返回
- 这就是为什么子进程看起来也是从 `fork()` 返回的

---

## 重要知识点总结

### 本实验中的重要知识点

1. **用户态与内核态**
   - RISC-V 的特权级架构（U Mode / S Mode）
   - 通过 `ecall` 和 `sret` 指令实现特权级切换
   - `sstatus` 寄存器控制特权级和中断状态

2. **系统调用机制**
   - 用户态通过 `ecall` 触发异常陷入内核
   - 内核通过系统调用号分发到具体函数
   - 返回值通过中断帧传递回用户态

3. **用户进程的创建**
   - 解析 ELF 文件格式
   - 建立用户地址空间（代码段、数据段、栈）
   - 设置中断帧以返回用户态

4. **进程内存空间管理**
   - 每个用户进程有独立的页表和地址空间
   - 用户态和内核态使用不同的页表
   - `mm_struct` 管理进程的虚拟内存

5. **fork/exec/wait/exit 系统调用**
   - `fork`：复制进程，创建子进程
   - `exec`：替换进程的地址空间
   - `wait`：父进程等待子进程退出
   - `exit`：进程退出，释放资源

6. **进程状态管理**
   - 僵尸态（`PROC_ZOMBIE`）用于保留退出码
   - 睡眠态（`PROC_SLEEPING`）用于等待事件
   - 父子进程的关系维护

7. **内存复制机制**
   - `copy_range` 实现父子进程的内存隔离
   - 按页复制内存内容
   - 可扩展为 Copy-on-Write 优化

8. **中断帧（Trapframe）**
   - 保存完整的 CPU 状态
   - 用于系统调用参数传递和返回
   - 控制进程返回到用户态的状态

9. **第一次进入用户态**
   - 使用 `ebreak` 模拟系统调用（从 S Mode）
   - 通过设置 `tf->status` 清除 SPP 位
   - `sret` 指令根据 SPP 切换到用户态

10. **进程间关系**
    - 父子进程链接（parent、cptr、yptr、optr）
    - 孤儿进程托付给 `initproc`
    - 僵尸进程的回收机制

### 与 OS 原理的对应关系

| 实验内容 | OS 原理知识点 | 关系说明 |
|---------|--------------|---------|
| **用户态与内核态** | 处理器特权级、保护模式 | 实现了用户程序的隔离和保护 |
| **系统调用** | 系统调用接口、陷入机制 | 用户程序请求内核服务的唯一途径 |
| **fork 系统调用** | 进程创建、地址空间复制 | Unix 风格的进程创建模型 |
| **exec 系统调用** | 程序加载、ELF 格式 | 在进程中执行新程序 |
| **wait 系统调用** | 进程同步、僵尸进程 | 父进程等待子进程结束 |
| **exit 系统调用** | 进程终止、资源回收 | 进程的正常退出流程 |
| **用户地址空间** | 虚拟内存管理 | 每个进程独立的虚拟地址空间 |
| **ELF 加载** | 可执行文件格式 | 理解程序如何被加载到内存 |
| **Copy-on-Write** | 内存管理优化技术 | 延迟复制提高 fork 性能 |
| **进程状态** | 进程生命周期管理 | 包括僵尸态等特殊状态 |

### 实验与原理的差异

1. **用户程序的加载**：
   - 原理：通常从文件系统加载可执行文件
   - 实验：用户程序被编译链接到内核镜像中（Link-in-Kernel）

2. **系统调用数量**：
   - 原理：现代操作系统有数百个系统调用
   - 实验：只实现了最基本的几个（fork、exec、wait、exit、yield 等）

3. **内存管理**：
   - 原理：复杂的内存分配策略、页面置换算法
   - 实验：简化的内存管理，没有 swap

4. **进程调度**：
   - 原理：复杂的调度算法（CFS、多级反馈队列等）
   - 实验：简单的 FIFO 调度

5. **错误处理**：
   - 原理：详细的错误码、信号机制
   - 实验：简化的错误处理

---

## OS 原理中重要但实验未涉及的知识点

1. **文件系统**
   - 文件的创建、读写、删除
   - 目录结构管理
   - 文件权限和访问控制

2. **进程间通信（IPC）**
   - 管道（Pipe）
   - 消息队列
   - 共享内存
   - 信号（Signal）

3. **高级同步原语**
   - 信号量（Semaphore）
   - 互斥锁（Mutex）
   - 条件变量
   - 读写锁

4. **设备驱动**
   - 字符设备和块设备
   - 中断驱动的 I/O
   - DMA 技术

5. **网络协议栈**
   - TCP/IP 实现
   - Socket 接口
   - 网络设备驱动

6. **动态链接和加载**
   - 共享库（.so）
   - 动态链接器
   - 位置无关代码（PIC）

7. **多线程支持**
   - 用户级线程库
   - 内核级线程支持
   - 线程本地存储（TLS）

8. **安全机制**
   - 用户权限和组管理
   - 访问控制列表（ACL）
   - 安全策略（SELinux、AppArmor）

9. **虚拟化技术**
   - 虚拟机监视器（Hypervisor）
   - 容器技术
   - 资源隔离

10. **实时系统特性**
    - 实时调度算法
    - 中断延迟控制
    - 优先级继承

---

## 实验总结

通过本次实验，我深入理解了用户进程管理的核心机制：

1. **特权级切换**是操作系统安全性的基础，用户程序运行在受限的用户态，只能通过系统调用访问内核服务

2. **系统调用机制**连接了用户态和内核态，是用户程序与操作系统交互的桥梁

3. **进程的创建和加载**涉及 ELF 文件解析、内存分配、页表建立等多个步骤，需要精心设计

4. **fork/exec/wait/exit** 四个系统调用构成了 Unix 风格的进程管理模型，理解它们的实现是理解操作系统的关键

5. **内存隔离**通过独立的地址空间和页表实现，保证了进程间的相互独立

6. **父子进程关系**的维护和资源回收机制保证了系统的稳定性，避免了资源泄漏

本实验在 Lab4 内核线程的基础上，引入了用户态和系统调用，使 ucore 真正具备了运行用户程序的能力。这为后续实现文件系统、进程间通信等更高级的功能奠定了坚实基础。

---

*本报告完整回答了 Lab5 实验指导书中的所有问题，并对实验中的关键知识点进行了深入分析和总结。*

