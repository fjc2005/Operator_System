# Lab8 文件系统实验指导文档

## 实验概览

Lab8 的主要目标是实现文件系统的核心功能，包括：
1. 实现文件读写操作（练习1）
2. 实现基于文件系统的程序加载机制（练习2）

本实验将使你理解：
- 虚拟文件系统（VFS）的设计与实现
- Simple FS（SFS）文件系统的工作原理
- 文件的索引节点（inode）管理机制
- 基于文件系统的程序加载和执行

---

## 实验实现框架

### 整体实现步骤

```
Step 0: 填写已有实验代码（练习0）
    ↓
Step 1: 实现文件读写核心函数 sfs_io_nolock()（练习1）
    ├─ 1.1 处理起始未对齐的块
    ├─ 1.2 处理中间对齐的块
    └─ 1.3 处理末尾未对齐的块
    ↓
Step 2: 实现基于文件系统的程序加载 load_icode()（练习2）
    ├─ 2.1 创建内存管理结构
    ├─ 2.2 创建页目录表
    ├─ 2.3 从文件加载 ELF
    ├─ 2.4 建立用户栈
    ├─ 2.5 设置中断帧
    └─ 2.6 处理命令行参数
    ↓
Step 3: 编译测试
```

---

## 练习0：填写已有实验

### 任务说明

将 Lab2-Lab7 的代码填入 Lab8 对应的位置。

### 实现步骤

```bash
# 查找所有需要填写的位置
cd /home/fjc/os/lab8
grep -r "LAB[2-7]" kern/
```

**重点关注以下文件：**
- `kern/mm/pmm.c` - Lab2 物理内存管理
- `kern/mm/vmm.c` - Lab3 虚拟内存管理
- `kern/process/proc.c` - Lab4/Lab5 进程管理
- `kern/mm/swap.c` - Lab6 页面置换
- `kern/sync/sem.c` - Lab7 同步互斥

### 操作建议

1. 从之前的实验中复制相应代码
2. 确保编译通过：`make clean && make`
3. 如果有编译错误，检查接口是否匹配

---

## 练习1：完成读文件操作的实现

### 任务概述

**文件位置：** `kern/fs/sfs/sfs_inode.c`

**函数名：** `sfs_io_nolock()`

**功能：** 实现文件的读写操作，需要处理未对齐的块和对齐的块

### 核心概念理解

#### 1. SFS 文件系统的块结构

```
文件偏移量(offset)和块的关系：
┌─────────────┬─────────────┬─────────────┬─────────────┐
│   Block 0   │   Block 1   │   Block 2   │   Block 3   │
└─────────────┴─────────────┴─────────────┴─────────────┘
 0          4096         8192        12288        16384
            ↑                                      ↑
         offset=5000                          endpos=13000
         
情况分析：
- offset=5000: 位于 Block 1 的中间（5000 % 4096 = 904）
- endpos=13000: 位于 Block 3 的中间（13000 % 4096 = 664）
- 需要读取: Block 1(部分) + Block 2(完整) + Block 3(部分)
```

#### 2. 三段式读取策略

```
Phase 1: 起始未对齐部分
┌─────────────┐
│   Block 1   │  只读取从 offset 到块末尾的部分
└─────────────┘
      ↑~~~~~~~
    offset

Phase 2: 中间对齐部分
┌─────────────┐
│   Block 2   │  完整读取整个块
└─────────────┘
~~~~~~~~~~~~~~~

Phase 3: 末尾未对齐部分
┌─────────────┐
│   Block 3   │  只读取从块开始到 endpos 的部分
└─────────────┘
~~~~~~~~~~~↑
        endpos
```

### 实现步骤详解

#### 步骤1：理解函数接口

```c
static int
sfs_io_nolock(struct sfs_fs *sfs,        // SFS文件系统结构
              struct sfs_inode *sin,      // SFS的inode
              void *buf,                  // 缓冲区
              off_t offset,               // 文件内偏移
              size_t *alenp,              // 读写长度（输入输出参数）
              bool write)                 // true=写，false=读
```

#### 步骤2：分析已有代码

在 `sfs_io_nolock()` 函数中，找到注释：

```c
//LAB8:EXERCISE1 YOUR CODE HINT: call sfs_bmap_load_nolock, sfs_rbuf, sfs_rblock,etc.
```

这部分需要实现三个阶段的读写操作。

#### 步骤3：实现代码

**完整代码实现：**

```c
static int
sfs_io_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, void *buf, off_t offset, size_t *alenp, bool write) {
    struct sfs_disk_inode *din = sin->din;
    assert(din->type != SFS_TYPE_DIR);
    off_t endpos = offset + *alenp, blkoff;
    *alenp = 0;
    
    // 计算读写结束位置
    if (offset < 0 || offset >= SFS_MAX_FILE_SIZE || offset > endpos) {
        return -E_INVAL;
    }
    if (offset == endpos) {
        return 0;
    }
    if (endpos > SFS_MAX_FILE_SIZE) {
        endpos = SFS_MAX_FILE_SIZE;
    }
    if (!write) {
        if (offset >= din->size) {
            return 0;
        }
        if (endpos > din->size) {
            endpos = din->size;
        }
    }

    // 设置读写函数指针
    int (*sfs_buf_op)(struct sfs_fs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset);
    int (*sfs_block_op)(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks);
    if (write) {
        sfs_buf_op = sfs_wbuf, sfs_block_op = sfs_wblock;
    }
    else {
        sfs_buf_op = sfs_rbuf, sfs_block_op = sfs_rblock;
    }

    int ret = 0;
    size_t size, alen = 0;
    uint32_t ino;
    uint32_t blkno = offset / SFS_BLKSIZE;          // 起始块号
    uint32_t nblks = endpos / SFS_BLKSIZE - blkno;  // 需要读写的块数

    // ============ LAB8 EXERCISE1: YOUR CODE ============
    
    // (1) 处理起始块的未对齐部分
    if ((blkoff = offset % SFS_BLKSIZE) != 0) {
        // offset 不是块对齐的，需要先处理第一个块的部分数据
        size = (nblks != 0) ? (SFS_BLKSIZE - blkoff) : (endpos - offset);
        // size 是本次要读写的字节数
        
        // 获取 blkno 对应的磁盘块号
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) {
            goto out;
        }
        
        // 读写部分块数据（从 blkoff 开始，长度为 size）
        if ((ret = sfs_buf_op(sfs, buf, size, ino, blkoff)) != 0) {
            goto out;
        }
        
        // 更新相关变量
        alen += size;      // 已处理的字节数
        buf += size;       // 缓冲区指针前移
        blkno++;           // 下一个块
        nblks--;           // 剩余块数减1
    }
    
    // (2) 处理中间的对齐块
    while (nblks > 0) {
        // 获取 blkno 对应的磁盘块号
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) {
            goto out;
        }
        
        // 读写整个块（4096 字节）
        if ((ret = sfs_block_op(sfs, buf, ino, 1)) != 0) {
            goto out;
        }
        
        // 更新相关变量
        alen += SFS_BLKSIZE;
        buf += SFS_BLKSIZE;
        blkno++;
        nblks--;
    }
    
    // (3) 处理末尾块的未对齐部分
    if ((size = endpos % SFS_BLKSIZE) != 0) {
        // endpos 不是块对齐的，需要处理最后一个块的部分数据
        
        // 获取 blkno 对应的磁盘块号
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) {
            goto out;
        }
        
        // 读写部分块数据（从块开始，长度为 size）
        if ((ret = sfs_buf_op(sfs, buf, size, ino, 0)) != 0) {
            goto out;
        }
        
        // 更新已处理的字节数
        alen += size;
    }
    
    // ============ END OF LAB8 EXERCISE1 ============

out:
    *alenp = alen;
    if (offset + alen > sin->din->size) {
        sin->din->size = offset + alen;
        sin->dirty = 1;
    }
    return ret;
}
```

### 关键函数说明

#### `sfs_bmap_load_nolock()`

```c
// 功能：获取文件的第 index 个数据块对应的磁盘块号
// 参数：
//   - sfs: 文件系统
//   - sin: inode
//   - index: 块索引（第几个块）
//   - ino_store: 输出参数，存储磁盘块号
// 返回：0表示成功，否则返回错误码
int sfs_bmap_load_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, 
                         uint32_t index, uint32_t *ino_store);
```

#### `sfs_rbuf()` 和 `sfs_wbuf()`

```c
// 功能：读写块的部分数据
// 参数：
//   - sfs: 文件系统
//   - buf: 缓冲区
//   - len: 要读写的长度
//   - blkno: 磁盘块号
//   - offset: 块内偏移
// 返回：0表示成功，否则返回错误码
int sfs_rbuf(struct sfs_fs *sfs, void *buf, size_t len, 
             uint32_t blkno, off_t offset);
int sfs_wbuf(struct sfs_fs *sfs, void *buf, size_t len, 
             uint32_t blkno, off_t offset);
```

#### `sfs_rblock()` 和 `sfs_wblock()`

```c
// 功能：读写完整的块
// 参数：
//   - sfs: 文件系统
//   - buf: 缓冲区
//   - blkno: 磁盘块号
//   - nblks: 块数量
// 返回：0表示成功，否则返回错误码
int sfs_rblock(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks);
int sfs_wblock(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks);
```

### 测试验证

完成代码后，编译并测试：

```bash
make clean
make
make qemu
```

如果成功，应该能看到文件系统初始化信息，并能进入 shell。

---

## 练习2：完成基于文件系统的执行程序机制

### 任务概述

**文件位置：** `kern/process/proc.c`

**函数名：** `load_icode()`

**功能：** 从文件系统加载 ELF 格式的可执行文件到内存，并创建用户进程

### 核心概念理解

#### 1. ELF 文件格式

```
ELF (Executable and Linkable Format) 文件结构：
┌──────────────────┐
│   ELF Header     │  描述文件基本信息
├──────────────────┤
│ Program Headers  │  描述各个段的信息
├──────────────────┤
│   .text 段       │  代码段（可执行）
├──────────────────┤
│   .data 段       │  数据段（可读写）
├──────────────────┤
│   .bss 段        │  未初始化数据段
├──────────────────┤
│   ...            │  其他段
└──────────────────┘
```

#### 2. 程序加载过程

```
┌─────────────────┐
│  磁盘上的文件   │
└────────┬────────┘
         │ 1. 打开文件，读取 ELF
         ↓
┌─────────────────┐
│  解析 ELF Header │
└────────┬────────┘
         │ 2. 创建用户内存空间
         ↓
┌─────────────────┐
│ 分配物理页面    │
│ 建立页表映射    │
└────────┬────────┘
         │ 3. 加载各个段到内存
         ↓
┌─────────────────┐
│  设置用户栈     │
└────────┬────────┘
         │ 4. 设置中断帧
         ↓
┌─────────────────┐
│  返回用户态执行 │
└─────────────────┘
```

### 实现步骤详解

#### 步骤1：理解函数接口

```c
static int
load_icode(int fd,           // 文件描述符
           int argc,         // 参数个数
           char **kargv)     // 参数数组（内核态）
```

与 Lab5 的区别：
- Lab5: 从内存中的二进制数据加载
- Lab8: 从文件系统中的文件加载

#### 步骤2：实现代码

**完整代码实现：**

```c
static int
load_icode(int fd, int argc, char **kargv) {
    /* LAB8:EXERCISE2 YOUR CODE  HINT:how to load the file with handler fd  in to process's memory? how to setup argc/argv?
     * MACROs or Functions:
     *  mm_create        - create a mm
     *  setup_pgdir      - setup pgdir in mm
     *  load_icode_read  - read raw data content of program file
     *  mm_map           - build new vma
     *  pgdir_alloc_page - allocate new memory for  TEXT/DATA/BSS/stack parts
     *  lcr3             - update Page Directory Addr Register -- CR3
     */
    
    /* (1) 创建进程的内存管理结构 */
    if (current->mm != NULL) {
        panic("load_icode: current->mm must be empty.\n");
    }
    
    int ret = -E_NO_MEM;
    struct mm_struct *mm;
    
    // 创建 mm
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }
    
    // 创建页目录表
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }
    
    /* (2) 读取并解析 ELF 文件头 */
    struct Page *page;
    struct elfhdr __elf, *elf = &__elf;
    
    // 读取 ELF header（从文件开始位置读取）
    if ((ret = load_icode_read(fd, elf, sizeof(struct elfhdr), 0)) != 0) {
        goto bad_elf_cleanup_pgdir;
    }
    
    // 检查 ELF 魔数
    if (elf->e_magic != ELF_MAGIC) {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;
    }
    
    /* (3) 读取并处理每个程序段 */
    struct proghdr __ph, *ph = &__ph;
    uint32_t vm_flags, perm;
    
    for (int i = 0; i < elf->e_phnum; i++) {
        // 读取第 i 个 program header
        off_t phoff = elf->e_phoff + sizeof(struct proghdr) * i;
        if ((ret = load_icode_read(fd, ph, sizeof(struct proghdr), phoff)) != 0) {
            goto bad_cleanup_mmap;
        }
        
        // 只处理 LOAD 类型的段
        if (ph->p_type != ELF_PT_LOAD) {
            continue;
        }
        if (ph->p_filesz > ph->p_memsz) {
            ret = -E_INVAL_ELF;
            goto bad_cleanup_mmap;
        }
        if (ph->p_filesz == 0) {
            continue;
        }
        
        // 建立虚拟地址空间（vma）
        vm_flags = 0;
        perm = PTE_U | PTE_V;
        if (ph->p_flags & ELF_PF_X) vm_flags |= VM_EXEC;
        if (ph->p_flags & ELF_PF_W) vm_flags |= VM_WRITE;
        if (ph->p_flags & ELF_PF_R) vm_flags |= VM_READ;
        
        // 根据 ELF 段权限设置页表权限
        if (vm_flags & VM_WRITE) perm |= (PTE_R | PTE_W);
        
        // 创建 vma
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0) {
            goto bad_cleanup_mmap;
        }
        
        // 分配内存并加载段内容
        off_t offset = ph->p_offset;
        size_t off, size;
        uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);
        
        end = ph->p_va + ph->p_filesz;
        
        // 逐页分配并拷贝数据
        while (start < end) {
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            off = start - la;
            size = PGSIZE - off;
            la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            
            // 从文件读取数据到页面
            if ((ret = load_icode_read(fd, page2kva(page) + off, size, offset)) != 0) {
                goto bad_cleanup_mmap;
            }
            start += size;
            offset += size;
        }
        
        // 处理 BSS 段（p_filesz < p_memsz 的部分）
        end = ph->p_va + ph->p_memsz;
        if (start < la) {
            // 当前页面的剩余部分清零
            if (start == end) {
                continue;
            }
            off = start + PGSIZE - la;
            size = PGSIZE - off;
            if (end < la) {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
            assert((end < la && start == end) || (end >= la && start == la));
        }
        
        // 剩余的 BSS 段页面
        while (start < end) {
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            off = start - la;
            size = PGSIZE - off;
            la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
        }
    }
    
    // 关闭文件
    sysfile_close(fd);
    
    /* (4) 建立用户栈 */
    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - PGSIZE, PTE_U | PTE_V | PTE_R | PTE_W) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 2 * PGSIZE, PTE_U | PTE_V | PTE_R | PTE_W) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 3 * PGSIZE, PTE_U | PTE_V | PTE_R | PTE_W) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 4 * PGSIZE, PTE_U | PTE_V | PTE_R | PTE_W) != NULL);
    
    /* (5) 设置当前进程的 mm 和 CR3 */
    mm_count_inc(mm);
    current->mm = mm;
    current->cr3 = PADDR(mm->pgdir);
    lcr3(PADDR(mm->pgdir));
    
    /* (6) 设置用户栈中的 argc 和 argv */
    uint32_t argv_size = 0, i;
    for (i = 0; i < argc; i++) {
        argv_size += strlen(kargv[i]) + 1;
    }
    
    // 用户栈指针（栈顶向下增长）
    uintptr_t stacktop = USTACKTOP - (argv_size / sizeof(long) + 1) * sizeof(long);
    char **uargv = (char **)(stacktop - argc * sizeof(char *));
    
    argv_size = 0;
    for (i = 0; i < argc; i++) {
        uargv[i] = strcpy((char *)(stacktop + argv_size), kargv[i]);
        argv_size += strlen(kargv[i]) + 1;
    }
    
    stacktop = (uintptr_t)uargv - sizeof(int);
    *(int *)stacktop = argc;
    
    /* (7) 设置中断帧，准备返回用户态 */
    struct trapframe *tf = current->tf;
    
    // 初始化 trapframe
    memset(tf, 0, sizeof(struct trapframe));
    
    tf->gpr.sp = stacktop;           // 用户栈指针
    tf->epc = elf->e_entry;          // 程序入口地址
    tf->status = (read_csr(sstatus) | SSTATUS_SPIE) & ~SSTATUS_SPP;  // 用户态
    
    ret = 0;
    
out:
    return ret;
    
bad_cleanup_mmap:
    exit_mmap(mm);
bad_elf_cleanup_pgdir:
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    goto out;
}
```

### 关键函数说明

#### `load_icode_read()`

```c
// 功能：从文件中读取数据
// 参数：
//   - fd: 文件描述符
//   - buf: 缓冲区
//   - len: 要读取的长度
//   - offset: 文件偏移量
// 返回：0表示成功，否则返回错误码
static int
load_icode_read(int fd, void *buf, size_t len, off_t offset) {
    int ret;
    if ((ret = sysfile_seek(fd, offset, LSEEK_SET)) != 0) {
        return ret;
    }
    if ((ret = sysfile_read(fd, buf, len)) != len) {
        return (ret < 0) ? ret : -1;
    }
    return 0;
}
```

#### `mm_map()`

```c
// 功能：在进程的虚拟地址空间中创建一个 vma
// 参数：
//   - mm: 内存管理结构
//   - addr: 起始虚拟地址
//   - len: 长度
//   - vm_flags: 权限标志
//   - vma_store: 输出参数
// 返回：0表示成功，否则返回错误码
int mm_map(struct mm_struct *mm, uintptr_t addr, size_t len, 
           uint32_t vm_flags, struct vma_struct **vma_store);
```

#### `pgdir_alloc_page()`

```c
// 功能：分配一个物理页并建立映射
// 参数：
//   - pgdir: 页目录
//   - la: 线性地址（虚拟地址）
//   - perm: 权限
// 返回：分配的页面指针
struct Page *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm);
```

### 用户栈布局

```
高地址 USTACKTOP (0xB0000000)
        ↓
    ┌─────────┐
    │ 参数字符串 │  "arg1\0arg2\0..."
    ├─────────┤
    │ argv[n-1]│  → 指向最后一个参数
    │   ...    │
    │ argv[1] │  → 指向第二个参数
    │ argv[0] │  → 指向第一个参数
    ├─────────┤
    │  argc   │  参数个数
    └─────────┘  ← stacktop (sp 初始值)
        ↓
    低地址
```

### 测试验证

```bash
make clean
make
make qemu
```

成功后应该能看到：
```
user sh is running!!!
$ 
```

可以尝试运行测试程序：
```
$ hello
$ exit
```

---

## 调试技巧

### 1. 使用 GDB 调试

```bash
# 终端1
make qemu-gdb

# 终端2
riscv64-unknown-elf-gdb bin/kernel
(gdb) target remote :1234
(gdb) b sfs_io_nolock
(gdb) b load_icode
(gdb) c
```

### 2. 添加调试输出

```c
// 在关键位置添加 cprintf
cprintf("sfs_io_nolock: offset=%d, len=%d, write=%d\n", offset, *alenp, write);
```

### 3. 常见错误排查

#### 错误1：Page Fault

**现象：** 访问无效地址导致页错误

**排查：**
- 检查 `mm_map()` 是否正确建立 vma
- 检查 `pgdir_alloc_page()` 是否成功分配页面
- 检查地址对齐

#### 错误2：文件读取失败

**现象：** `load_icode_read()` 返回错误

**排查：**
- 检查文件是否成功打开（fd 是否有效）
- 检查文件偏移量是否正确
- 检查文件大小是否足够

#### 错误3：ELF 格式错误

**现象：** ELF 魔数检查失败

**排查：**
- 确认文件是 ELF 格式
- 检查是否为 RISC-V 架构的 ELF 文件
- 检查文件是否损坏

---

## 实验验证与测试

### 基本测试

```bash
make qemu
```

**预期输出：**
```
(...initialization messages...)
kernel_execve: pid = 2, name = "sh".
user sh is running!!!
$ 
```

### 功能测试

在 shell 中测试各种程序：

```bash
$ ls          # 列出文件（如果实现了）
$ hello       # Hello World 程序
$ exit        # 退出程序
$ forktest    # 进程测试
```

### 文件读写测试

如果有文件读写测试程序：

```bash
$ filetest    # 测试文件创建、读写
```

---

## 扩展练习（可选）

### Challenge 1: PIPE 机制

**需求：** 实现 UNIX 的管道机制

**数据结构设计：**

```c
#define PIPE_BUF_SIZE 4096

struct pipe {
    char buffer[PIPE_BUF_SIZE];  // 环形缓冲区
    int read_pos;                // 读位置
    int write_pos;               // 写位置
    int ref_count;               // 引用计数
    semaphore_t read_sem;        // 读信号量
    semaphore_t write_sem;       // 写信号量
    wait_queue_t read_wait;      // 读等待队列
    wait_queue_t write_wait;     // 写等待队列
};
```

**接口设计：**

```c
// 创建管道
int pipe(int *fd);

// 从管道读取
int pipe_read(struct pipe *p, void *buf, size_t len);

// 向管道写入
int pipe_write(struct pipe *p, const void *buf, size_t len);

// 关闭管道
int pipe_close(struct pipe *p, bool is_read_end);
```

### Challenge 2: 软连接和硬连接

**硬链接数据结构：**

```c
// inode 中添加链接计数
struct sfs_disk_inode {
    uint32_t size;
    uint16_t type;
    uint16_t nlinks;  // 硬链接计数
    // ...
};
```

**软链接数据结构：**

```c
#define SFS_TYPE_LINK 0x4  // 软链接类型

struct sfs_disk_inode {
    // ... 
    // 对于软链接，size 存储目标路径长度
    // direct[0] 指向存储目标路径的数据块
};
```

**接口设计：**

```c
// 创建硬链接
int sys_link(const char *oldpath, const char *newpath);

// 创建软链接
int sys_symlink(const char *target, const char *linkpath);

// 读取软链接
int sys_readlink(const char *path, char *buf, size_t bufsiz);
```

---

## 实验总结

完成本实验后，你应该掌握：

1. ✅ 文件系统的层次结构（VFS、SFS、设备层）
2. ✅ 文件的索引节点（inode）组织方式
3. ✅ 文件读写的实现原理
4. ✅ ELF 文件的加载过程
5. ✅ 进程虚拟地址空间的建立
6. ✅ 用户栈的初始化
7. ✅ 文件系统与进程管理的协同工作

**关键知识点：**
- 块设备的对齐读写
- 虚拟文件系统的抽象层
- ELF 文件格式解析
- 用户态内存空间管理
- 系统调用的实现机制

---

## 参考资料

1. [uCore 实验指导书](https://objectkuan.gitbooks.io/ucore-docs/)
2. [ELF 文件格式规范](https://refspecs.linuxfoundation.org/elf/elf.pdf)
3. [RISC-V 特权架构规范](https://riscv.org/specifications/privileged-isa/)
4. [Unix 文件系统设计](https://en.wikipedia.org/wiki/Unix_File_System)

---

**祝实验顺利！ 🎉**

