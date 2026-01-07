# Lab 8 实验报告：文件系统

姓名: 黄俊雄 学号: 2313896
姓名: 付嘉晨 学号: 2313903
姓名: 王文轩 学号: 2311058

## 实验概述

本实验在前面实验的基础上，完成了文件系统的实现。通过引入虚拟文件系统（VFS）、Simple FS（SFS）文件系统和设备抽象层，实现了文件的创建、读写、执行等功能，使操作系统能够管理持久存储设备上的数据。

实验主要完成了以下内容：
1. 实现文件系统的初始化和挂载
2. 实现文件的读写操作
3. 实现基于文件系统的程序加载和执行机制
4. 实现简单的 Shell 终端程序

---

## 练习 0：填写已有实验

本实验依赖实验 2/3/4/5/6/7 的代码。已将相关代码填入本实验中标有 `LAB2`、`LAB3`、`LAB4`、`LAB5`、`LAB6`、`LAB7` 注释的相应部分。

主要包括：
- LAB2：物理内存管理相关代码
- LAB3：页表管理和虚拟内存映射相关代码
- LAB4：进程管理相关代码
- LAB5：用户进程和系统调用相关代码
- LAB6：调度器相关代码
- LAB7：同步互斥相关代码

---

## 练习 1：完成读文件操作的实现

### 设计实现过程

`sfs_io_nolock` 函数是 SFS 文件系统中读写文件的核心函数。该函数需要处理文件的读写操作，包括处理未对齐的起始块、中间的完整块和末尾的剩余部分。

#### 实现代码（`kern/fs/sfs/sfs_inode.c`）

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

    // (1) 处理起始块（如果 offset 未对齐）
    if ((blkoff = offset % SFS_BLKSIZE) != 0) {
        size = (nblks != 0) ? (SFS_BLKSIZE - blkoff) : (endpos - offset);
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) {
            goto out;
        }
        if ((ret = sfs_buf_op(sfs, buf, size, ino, blkoff)) != 0) {
            goto out;
        }
        alen += size;
        if (nblks == 0) {
            goto out;
        }
        buf += size, blkno++, nblks--;
    }

    // (2) 处理中间的完整块
    if (nblks > 0) {
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) {
            goto out;
        }
        if ((ret = sfs_block_op(sfs, buf, ino, nblks)) != 0) {
            goto out;
        }
        alen += nblks * SFS_BLKSIZE;
        buf += nblks * SFS_BLKSIZE;
        blkno += nblks;
    }

    // (3) 处理末尾块（如果 endpos 未对齐）
    if ((size = endpos % SFS_BLKSIZE) != 0) {
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) {
            goto out;
        }
        if ((ret = sfs_buf_op(sfs, buf, size, ino, 0)) != 0) {
            goto out;
        }
        alen += size;
    }

out:
    *alenp = alen;
    if (offset + alen > sin->din->size) {
        sin->din->size = offset + alen;
        sin->dirty = 1;
    }
    return ret;
}
```

#### 实现要点

1. **边界检查**：
   - 检查 offset 是否合法（不能为负数或超过最大文件大小）
   - 对于读操作，不能超过文件当前大小
   - 对于写操作，可以扩展文件大小

2. **分段处理**：
   - **起始块**：如果 offset 不是块对齐的，需要先处理起始块的部分数据
   - **中间块**：处理完整的块，可以使用更高效的块读写函数
   - **末尾块**：如果 endpos 不是块对齐的，需要处理末尾块的部分数据

3. **块索引查找**：
   - 使用 `sfs_bmap_load_nolock` 函数将逻辑块号转换为物理块号
   - 该函数会处理直接索引和间接索引

4. **实际读写**：
   - 对于部分块，使用 `sfs_rbuf`/`sfs_wbuf` 函数
   - 对于完整块，使用 `sfs_rblock`/`sfs_wblock` 函数

5. **更新文件大小**：
   - 如果写操作扩展了文件大小，需要更新 `sin->din->size`
   - 设置 `sin->dirty` 标志，以便后续将修改写回磁盘

### 问题回答

**问题**：给出设计实现"UNIX的PIPE机制"的概要设方案，鼓励给出详细设计方案

**回答**：见扩展练习 Challenge 1。

---

## 练习 2：完成基于文件系统的执行程序机制的实现

### 设计实现过程

`load_icode` 函数负责从文件系统加载 ELF 格式的可执行文件到当前进程的内存空间。与实验 5 不同的是，本实验需要从文件系统读取程序，而不是从内存中的某个位置读取。

#### 实现代码（`kern/process/proc.c`）

```c
static int
load_icode(int fd, int argc, char **kargv) {
    // 检查当前进程的内存管理结构
    if (current->mm != NULL) {
        panic("load_icode: current->mm must be empty.\n");
    }

    int ret = -E_NO_MEM;
    struct mm_struct *mm;
    
    // 1. 创建新的 mm 结构
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }
    
    // 2. 创建页目录表
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }

    struct Page *page;
    
    // 3. 读取 ELF 文件头
    struct elfhdr __elf, *elf = &__elf;
    if ((ret = load_icode_read(fd, elf, sizeof(struct elfhdr), 0)) != 0) {
        goto bad_elf_cleanup_pgdir;
    }
    
    // 4. 检查 ELF 魔数
    if (elf->e_magic != ELF_MAGIC) {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;
    }

    struct proghdr __ph, *ph = &__ph;
    uint32_t vm_flags, perm;
    
    // 5. 遍历每个程序段
    for (int i = 0; i < elf->e_phnum; i++) {
        off_t phoff = elf->e_phoff + sizeof(struct proghdr) * i;
        if ((ret = load_icode_read(fd, ph, sizeof(struct proghdr), phoff)) != 0) {
            goto bad_cleanup_mmap;
        }
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
        
        // 6. 建立虚拟地址空间映射
        vm_flags = 0, perm = PTE_U | PTE_V;
        if (ph->p_flags & ELF_PF_X) vm_flags |= VM_EXEC;
        if (ph->p_flags & ELF_PF_W) vm_flags |= VM_WRITE;
        if (ph->p_flags & ELF_PF_R) vm_flags |= VM_READ;
        if (vm_flags & VM_WRITE) perm |= (PTE_R | PTE_W);
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0) {
            goto bad_cleanup_mmap;
        }
        
        unsigned char *from = (unsigned char *)ph->p_va;
        size_t off, size;
        uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);
        
        ret = -E_NO_MEM;
        
        // 7. 分配物理内存并建立映射
        end = ph->p_va + ph->p_filesz;
        while (start < end) {
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            
            // 8. 从文件读取数据到内存
            if ((ret = load_icode_read(fd, page2kva(page) + off, size, ph->p_offset + (start - ph->p_va))) != 0) {
                goto bad_cleanup_mmap;
            }
            start += size;
        }
        
        // 9. 处理 BSS 段（清零）
        end = ph->p_va + ph->p_memsz;
        if (start < la) {
            if (start < end) {
                memset(page2kva(page) + (start - la + PGSIZE - PGSIZE), 0, 
                       (size = (end < la ? end : la) - start));
                start += size;
            }
        }
        while (start < end) {
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
        }
    }
    
    // 10. 关闭文件
    sysfile_close(fd);
    
    // 11. 建立用户栈
    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 2 * PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 3 * PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 4 * PGSIZE, PTE_USER) != NULL);
    
    // 12. 设置 mm 为当前进程的内存管理结构
    mm_count_inc(mm);
    current->mm = mm;
    current->cr3 = PADDR(mm->pgdir);
    lcr3(PADDR(mm->pgdir));
    
    // 13. 在用户栈上设置 argc 和 argv
    uint32_t argv_size = 0, i;
    for (i = 0; i < argc; i++) {
        argv_size += strnlen(kargv[i], EXEC_MAX_ARG_LEN + 1) + 1;
    }
    
    uintptr_t stacktop = USTACKTOP - (argv_size / sizeof(long) + 1) * sizeof(long);
    char **uargv = (char **)(stacktop - argc * sizeof(char *));
    
    argv_size = 0;
    for (i = 0; i < argc; i++) {
        uargv[i] = strcpy((char *)(stacktop + argv_size), kargv[i]);
        argv_size += strnlen(kargv[i], EXEC_MAX_ARG_LEN + 1) + 1;
    }
    
    stacktop = (uintptr_t)uargv - sizeof(int);
    *(int *)stacktop = argc;
    
    // 14. 设置中断帧，准备返回用户态
    struct trapframe *tf = current->tf;
    uintptr_t sstatus = tf->status;
    memset(tf, 0, sizeof(struct trapframe));
    tf->gpr.sp = stacktop;
    tf->epc = elf->e_entry;
    tf->status = (sstatus & ~SSTATUS_SPP) | SSTATUS_SPIE;
    ret = 0;

out:
    return ret;

bad_cleanup_mmap:
    exit_mmap(mm);
bad_elf_cleanup_pgdir:
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    goto out;
}
```

#### 实现要点

1. **创建内存管理结构**：
   - 使用 `mm_create` 创建新的 mm 结构
   - 使用 `setup_pgdir` 创建页目录表

2. **读取 ELF 文件**：
   - 使用 `load_icode_read` 函数从文件描述符读取数据
   - 该函数会调用 `sysfile_seek` 和 `sysfile_read` 完成文件读取

3. **解析 ELF 文件**：
   - 检查 ELF 魔数是否正确
   - 遍历所有程序段头（Program Header）

4. **建立内存映射**：
   - 使用 `mm_map` 为每个段建立 VMA
   - 设置正确的权限标志（读、写、执行）

5. **加载程序段**：
   - 为每个页分配物理内存
   - 从文件读取数据到分配的页中
   - 处理 BSS 段（将其清零）

6. **建立用户栈**：
   - 在 `USTACKTOP - USTACKSIZE` 到 `USTACKTOP` 之间建立用户栈
   - 预先分配几个页

7. **设置参数**：
   - 在用户栈上设置 argc 和 argv
   - 将参数字符串拷贝到用户栈

8. **设置中断帧**：
   - 设置 `tf->epc` 为程序入口地址
   - 设置 `tf->gpr.sp` 为用户栈栈顶
   - 设置 `tf->status` 为用户态标志

### 问题回答

**问题**：请在实验报告中给出设计实现基于"UNIX的软连接和硬连接机制"的概要设方案

**回答**：见扩展练习 Challenge 2。

---

## 扩展练习 Challenge 1：完成基于"UNIX的PIPE机制"的设计方案

### PIPE 机制概述

管道（Pipe）是 UNIX 系统中进程间通信（IPC）的一种重要机制。管道提供了一个单向的数据流通道，一个进程写入管道的数据可以被另一个进程读取。管道分为两种：
- **匿名管道（Anonymous Pipe）**：只能在有亲缘关系的进程间使用
- **命名管道（Named Pipe/FIFO）**：可以在任意进程间使用

本设计方案主要讨论匿名管道的实现。

### 数据结构设计

#### 1. 管道结构体

```c
// kern/fs/pipe/pipe.h

#define PIPE_BUF_SIZE 4096  // 管道缓冲区大小

struct pipe {
    struct spinlock lock;        // 保护管道的自旋锁
    char data[PIPE_BUF_SIZE];    // 管道缓冲区
    uint32_t read_pos;           // 读位置
    uint32_t write_pos;          // 写位置
    uint32_t ref_count;          // 引用计数
    bool read_open;              // 读端是否打开
    bool write_open;             // 写端是否打开
    wait_queue_t read_queue;     // 等待读取的进程队列
    wait_queue_t write_queue;    // 等待写入的进程队列
};
```

**字段说明**：
- `lock`：保护管道数据结构的并发访问
- `data`：环形缓冲区，存储管道中的数据
- `read_pos`：下一个读取位置的索引
- `write_pos`：下一个写入位置的索引
- `ref_count`：引用计数，用于管道的生命周期管理
- `read_open`/`write_open`：标记读端和写端是否打开
- `read_queue`/`write_queue`：等待队列，用于阻塞读写操作

#### 2. 管道 inode 信息

```c
// 在 struct inode 的 union in_info 中添加
struct pipe_inode_info {
    struct pipe *pipe;  // 指向实际的 pipe 结构
};
```

### 接口设计

#### 1. 创建管道

```c
/**
 * pipe - 创建管道
 * @fd: 文件描述符数组，fd[0] 为读端，fd[1] 为写端
 * 
 * 返回值：成功返回 0，失败返回负数错误码
 */
int sys_pipe(int *fd);
```

**实现步骤**：
1. 分配 `struct pipe` 结构并初始化
2. 创建两个 inode，类型为 pipe
3. 为当前进程分配两个文件描述符
4. `fd[0]` 设置为只读，`fd[1]` 设置为只写
5. 返回文件描述符

#### 2. 读取管道

```c
/**
 * pipe_read - 从管道读取数据
 * @inode: 管道的 inode
 * @iob: IO 缓冲区
 * 
 * 返回值：成功返回读取的字节数，失败返回负数错误码
 */
int pipe_read(struct inode *inode, struct iobuf *iob);
```

**实现逻辑**：
1. 获取管道的锁
2. 检查管道是否为空：
   - 如果为空且写端已关闭，返回 0（EOF）
   - 如果为空且写端仍打开，阻塞等待
3. 从 `read_pos` 位置读取数据到用户缓冲区
4. 更新 `read_pos`
5. 唤醒等待写入的进程
6. 释放锁

#### 3. 写入管道

```c
/**
 * pipe_write - 向管道写入数据
 * @inode: 管道的 inode
 * @iob: IO 缓冲区
 * 
 * 返回值：成功返回写入的字节数，失败返回负数错误码
 */
int pipe_write(struct inode *inode, struct iobuf *iob);
```

**实现逻辑**：
1. 获取管道的锁
2. 检查读端是否已关闭，如果是，发送 SIGPIPE 信号
3. 检查管道是否已满：
   - 如果已满，阻塞等待
4. 从用户缓冲区写入数据到 `write_pos` 位置
5. 更新 `write_pos`
6. 唤醒等待读取的进程
7. 释放锁

#### 4. 关闭管道

```c
/**
 * pipe_close - 关闭管道的一端
 * @inode: 管道的 inode
 * 
 * 返回值：成功返回 0
 */
int pipe_close(struct inode *inode);
```

**实现逻辑**：
1. 根据文件是读端还是写端，设置相应的标志为 false
2. 唤醒所有等待的进程
3. 减少引用计数
4. 如果引用计数为 0，释放管道结构

### 同步互斥问题处理

#### 1. 并发读写保护

**问题**：多个进程可能同时读写管道，需要保护数据一致性。

**解决方案**：
- 使用自旋锁 `struct spinlock lock` 保护管道结构
- 在所有读写操作前获取锁，操作完成后释放锁

#### 2. 缓冲区管理

**问题**：管道缓冲区是有限的，需要处理满和空的情况。

**解决方案**：
- 使用环形缓冲区，通过 `read_pos` 和 `write_pos` 管理
- 计算可用空间：`available = (write_pos - read_pos) % PIPE_BUF_SIZE`
- 计算空闲空间：`free = PIPE_BUF_SIZE - available - 1`

#### 3. 阻塞与唤醒

**问题**：当管道为空时读操作应阻塞，当管道满时写操作应阻塞。

**解决方案**：
- 使用等待队列 `wait_queue_t`
- 读操作阻塞：当管道为空且写端打开时，加入 `read_queue` 并调用 `schedule()`
- 写操作阻塞：当管道满时，加入 `write_queue` 并调用 `schedule()`
- 读写操作完成后，调用 `wakeup_queue()` 唤醒等待的进程

#### 4. 管道生命周期

**问题**：何时释放管道资源。

**解决方案**：
- 使用引用计数 `ref_count`
- 创建管道时，`ref_count = 2`（读端和写端各一个引用）
- 关闭一端时，`ref_count--`
- 当 `ref_count == 0` 时，释放管道结构

#### 5. 管道破裂处理

**问题**：当读端关闭后，写操作应该如何处理。

**解决方案**：
- 在写操作中检查 `read_open` 标志
- 如果读端已关闭，写操作返回 `-E_PIPE` 错误
- 可选：向进程发送 SIGPIPE 信号

### 详细实现示例

#### pipe_read 函数实现

```c
int pipe_read(struct inode *inode, struct iobuf *iob) {
    struct pipe *pipe = vop_info(inode, pipe_inode_info)->pipe;
    int ret = 0;
    size_t alen = 0;
    
    lock_pipe(pipe);
    
    while (iob->io_resid > 0) {
        // 检查管道是否为空
        if (pipe->read_pos == pipe->write_pos) {
            if (!pipe->write_open) {
                // 写端已关闭，返回已读取的字节数
                break;
            }
            // 管道为空，等待写入
            wait_t __wait, *wait = &__wait;
            wait_current_set(&pipe->read_queue, wait, WT_PIPE);
            unlock_pipe(pipe);
            
            schedule();
            
            lock_pipe(pipe);
            wait_current_del(&pipe->read_queue, wait);
            
            if (wait->wakeup_flags == WT_INTERRUPTED) {
                ret = -E_INTR;
                break;
            }
            continue;
        }
        
        // 读取数据
        size_t n = pipe->write_pos - pipe->read_pos;
        if (n > iob->io_resid) {
            n = iob->io_resid;
        }
        
        // 从管道缓冲区拷贝到用户缓冲区
        memcpy(iob->io_base, pipe->data + (pipe->read_pos % PIPE_BUF_SIZE), n);
        pipe->read_pos += n;
        iob->io_resid -= n;
        iob->io_base += n;
        alen += n;
        
        // 唤醒等待写入的进程
        wakeup_queue(&pipe->write_queue, WT_PIPE, 1);
    }
    
    unlock_pipe(pipe);
    
    if (alen > 0) {
        return alen;
    }
    return ret;
}
```

#### pipe_write 函数实现

```c
int pipe_write(struct inode *inode, struct iobuf *iob) {
    struct pipe *pipe = vop_info(inode, pipe_inode_info)->pipe;
    int ret = 0;
    size_t alen = 0;
    
    lock_pipe(pipe);
    
    // 检查读端是否打开
    if (!pipe->read_open) {
        unlock_pipe(pipe);
        return -E_PIPE;
    }
    
    while (iob->io_resid > 0) {
        // 检查管道是否已满
        size_t available = (pipe->write_pos - pipe->read_pos) % PIPE_BUF_SIZE;
        if (PIPE_BUF_SIZE - available <= 1) {
            // 管道已满，等待读取
            wait_t __wait, *wait = &__wait;
            wait_current_set(&pipe->write_queue, wait, WT_PIPE);
            unlock_pipe(pipe);
            
            schedule();
            
            lock_pipe(pipe);
            wait_current_del(&pipe->write_queue, wait);
            
            if (!pipe->read_open) {
                ret = -E_PIPE;
                break;
            }
            if (wait->wakeup_flags == WT_INTERRUPTED) {
                ret = -E_INTR;
                break;
            }
            continue;
        }
        
        // 写入数据
        size_t n = PIPE_BUF_SIZE - available - 1;
        if (n > iob->io_resid) {
            n = iob->io_resid;
        }
        
        // 从用户缓冲区拷贝到管道缓冲区
        memcpy(pipe->data + (pipe->write_pos % PIPE_BUF_SIZE), iob->io_base, n);
        pipe->write_pos += n;
        iob->io_resid -= n;
        iob->io_base += n;
        alen += n;
        
        // 唤醒等待读取的进程
        wakeup_queue(&pipe->read_queue, WT_PIPE, 1);
    }
    
    unlock_pipe(pipe);
    
    if (alen > 0) {
        return alen;
    }
    return ret;
}
```

### 使用示例

```c
// 用户程序示例：父子进程通过管道通信
int main() {
    int fd[2];
    char buf[128];
    
    // 创建管道
    if (pipe(fd) < 0) {
        printf("pipe creation failed\n");
        return -1;
    }
    
    int pid = fork();
    if (pid == 0) {
        // 子进程：读取管道
        close(fd[1]);  // 关闭写端
        int n = read(fd[0], buf, sizeof(buf));
        printf("Child received: %s\n", buf);
        close(fd[0]);
        exit(0);
    } else {
        // 父进程：写入管道
        close(fd[0]);  // 关闭读端
        write(fd[1], "Hello from parent!", 19);
        close(fd[1]);
        wait(NULL);
    }
    
    return 0;
}
```

### 总结

本设计方案实现了 UNIX 风格的管道机制，主要特点包括：
1. 使用环形缓冲区管理数据
2. 使用等待队列实现阻塞读写
3. 使用自旋锁保护并发访问
4. 使用引用计数管理管道生命周期
5. 正确处理管道破裂（SIGPIPE）

这个设计方案能够有效地解决管道实现中的同步互斥问题，并提供了良好的性能和可靠性。

---

## 扩展练习 Challenge 2：完成基于"UNIX的软连接和硬连接机制"的设计方案

### 链接机制概述

UNIX 系统提供了两种文件链接机制：
- **硬链接（Hard Link）**：多个文件名指向同一个 inode，共享同一份数据
- **软链接（Symbolic Link/Symlink）**：一个特殊的文件，其内容是另一个文件的路径名

### 数据结构设计

#### 1. inode 扩展

```c
// kern/fs/sfs/sfs.h

/* inode (on disk) */
struct sfs_disk_inode {
    uint32_t size;                      // 文件大小
    uint16_t type;                      // 文件类型
    uint16_t nlinks;                    // 硬链接数（已有）
    uint32_t blocks;                    // 数据块数
    uint32_t direct[SFS_NDIRECT];       // 直接索引
    uint32_t indirect;                  // 间接索引
};

// 文件类型定义
#define SFS_TYPE_FILE       1   // 普通文件
#define SFS_TYPE_DIR        2   // 目录
#define SFS_TYPE_LINK       3   // 符号链接（新增）
```

**修改说明**：
- `nlinks` 字段已经存在，用于记录硬链接数
- 新增文件类型 `SFS_TYPE_LINK` 表示符号链接
- 符号链接的 `size` 字段表示目标路径的长度
- 符号链接的数据块存储目标路径字符串

#### 2. 目录项结构（已有，无需修改）

```c
/* file entry (on disk) */
struct sfs_disk_entry {
    uint32_t ino;                       // inode 编号
    char name[SFS_MAX_FNAME_LEN + 1];   // 文件名
};
```

### 硬链接实现

#### 1. 创建硬链接接口

```c
/**
 * sys_link - 创建硬链接
 * @oldpath: 已存在的文件路径
 * @newpath: 新的链接路径
 * 
 * 返回值：成功返回 0，失败返回负数错误码
 */
int sys_link(const char *oldpath, const char *newpath);
```

#### 2. 硬链接实现逻辑

```c
// kern/fs/vfs/vfsfile.c

int vfs_link(char *oldpath, char *newpath) {
    int ret;
    struct inode *old_node, *new_dir;
    char *new_name;
    
    // 1. 查找旧文件的 inode
    if ((ret = vfs_lookup(oldpath, &old_node)) != 0) {
        return ret;
    }
    
    // 2. 检查是否为目录（不允许对目录创建硬链接）
    uint32_t type;
    if ((ret = vop_gettype(old_node, &type)) != 0) {
        vop_ref_dec(old_node);
        return ret;
    }
    if (type == SFS_TYPE_DIR) {
        vop_ref_dec(old_node);
        return -E_ISDIR;  // 不能对目录创建硬链接
    }
    
    // 3. 检查是否为符号链接（可选：是否支持对符号链接创建硬链接）
    if (type == SFS_TYPE_LINK) {
        vop_ref_dec(old_node);
        return -E_INVAL;  // 不能对符号链接创建硬链接
    }
    
    // 4. 查找新路径的父目录
    if ((ret = vfs_lookup_parent(newpath, &new_dir, &new_name)) != 0) {
        vop_ref_dec(old_node);
        return ret;
    }
    
    // 5. 在父目录中创建新的目录项
    // 注意：这里不是创建新文件，而是添加一个指向已有 inode 的目录项
    if ((ret = sfs_link(new_dir, new_name, old_node)) != 0) {
        vop_ref_dec(new_dir);
        vop_ref_dec(old_node);
        return ret;
    }
    
    // 6. 增加硬链接计数
    struct sfs_inode *sin = vop_info(old_node, sfs_inode);
    lock_sin(sin);
    sin->din->nlinks++;
    sin->dirty = 1;
    unlock_sin(sin);
    
    vop_ref_dec(new_dir);
    vop_ref_dec(old_node);
    return 0;
}
```

#### 3. SFS 层硬链接实现

```c
// kern/fs/sfs/sfs_inode.c

/**
 * sfs_link - 在目录中添加指向已有 inode 的目录项
 * @dir_node: 父目录的 inode
 * @name: 新文件名
 * @target_node: 目标 inode
 */
int sfs_link(struct inode *dir_node, const char *name, struct inode *target_node) {
    struct sfs_fs *sfs = fsop_info(vop_fs(dir_node), sfs);
    struct sfs_inode *dir_sin = vop_info(dir_node, sfs_inode);
    struct sfs_inode *target_sin = vop_info(target_node, sfs_inode);
    
    int ret;
    uint32_t ino;
    int slot;
    
    lock_sin(dir_sin);
    
    // 1. 检查文件名是否已存在
    ret = sfs_dirent_search_nolock(sfs, dir_sin, name, &ino, &slot, NULL);
    if (ret == 0) {
        // 文件已存在
        unlock_sin(dir_sin);
        return -E_EXISTS;
    }
    
    // 2. 在目录中添加新的目录项，指向已有的 inode
    ret = sfs_dirent_create_nolock(sfs, dir_sin, name, target_sin->ino, &slot);
    
    unlock_sin(dir_sin);
    return ret;
}
```

#### 4. 删除硬链接（unlink）

```c
// 修改现有的 vfs_unlink 函数

int vfs_unlink(char *path) {
    int ret;
    struct inode *node, *dir;
    char *name;
    
    // 1. 查找父目录和文件名
    if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0) {
        return ret;
    }
    
    // 2. 查找文件的 inode
    if ((ret = vop_lookup(dir, name, &node)) != 0) {
        vop_ref_dec(dir);
        return ret;
    }
    
    // 3. 从目录中删除目录项
    if ((ret = vop_unlink(dir, name)) != 0) {
        vop_ref_dec(node);
        vop_ref_dec(dir);
        return ret;
    }
    
    // 4. 减少硬链接计数
    struct sfs_inode *sin = vop_info(node, sfs_inode);
    lock_sin(sin);
    sin->din->nlinks--;
    sin->dirty = 1;
    
    // 5. 如果硬链接计数为 0，释放 inode 和数据块
    if (sin->din->nlinks == 0) {
        // 释放所有数据块
        sfs_truncate_nolock(sin);
        // 释放 inode
        sfs_free_inode(sin);
    }
    unlock_sin(sin);
    
    vop_ref_dec(node);
    vop_ref_dec(dir);
    return 0;
}
```

### 软链接实现

#### 1. 创建软链接接口

```c
/**
 * sys_symlink - 创建符号链接
 * @target: 目标文件路径（可以不存在）
 * @linkpath: 符号链接路径
 * 
 * 返回值：成功返回 0，失败返回负数错误码
 */
int sys_symlink(const char *target, const char *linkpath);
```

#### 2. 软链接实现逻辑

```c
// kern/fs/vfs/vfsfile.c

int vfs_symlink(char *target, char *linkpath) {
    int ret;
    struct inode *link_node, *dir;
    char *link_name;
    
    // 1. 检查目标路径长度
    size_t target_len = strlen(target);
    if (target_len == 0 || target_len > SFS_MAX_LINK_LEN) {
        return -E_INVAL;
    }
    
    // 2. 查找符号链接的父目录
    if ((ret = vfs_lookup_parent(linkpath, &dir, &link_name)) != 0) {
        return ret;
    }
    
    // 3. 创建符号链接文件
    if ((ret = vop_create(dir, link_name, 0, &link_node)) != 0) {
        vop_ref_dec(dir);
        return ret;
    }
    
    // 4. 设置 inode 类型为符号链接
    struct sfs_inode *sin = vop_info(link_node, sfs_inode);
    lock_sin(sin);
    sin->din->type = SFS_TYPE_LINK;
    sin->din->size = target_len;
    sin->dirty = 1;
    unlock_sin(sin);
    
    // 5. 将目标路径写入符号链接文件的数据块
    struct iobuf iob;
    iobuf_init(&iob, (void *)target, target_len, 0);
    if ((ret = vop_write(link_node, &iob)) != target_len) {
        vop_unlink(dir, link_name);
        vop_ref_dec(link_node);
        vop_ref_dec(dir);
        return (ret < 0) ? ret : -E_IO;
    }
    
    vop_ref_dec(link_node);
    vop_ref_dec(dir);
    return 0;
}
```

#### 3. 读取软链接

```c
/**
 * sys_readlink - 读取符号链接的目标路径
 * @path: 符号链接路径
 * @buf: 存储目标路径的缓冲区
 * @bufsize: 缓冲区大小
 * 
 * 返回值：成功返回读取的字节数，失败返回负数错误码
 */
int sys_readlink(const char *path, char *buf, size_t bufsize);

int vfs_readlink(char *path, char *buf, size_t bufsize) {
    int ret;
    struct inode *node;
    
    // 1. 查找符号链接的 inode（不跟随链接）
    if ((ret = vfs_lookup_nofollow(path, &node)) != 0) {
        return ret;
    }
    
    // 2. 检查是否为符号链接
    uint32_t type;
    if ((ret = vop_gettype(node, &type)) != 0) {
        vop_ref_dec(node);
        return ret;
    }
    if (type != SFS_TYPE_LINK) {
        vop_ref_dec(node);
        return -E_INVAL;  // 不是符号链接
    }
    
    // 3. 读取目标路径
    struct sfs_inode *sin = vop_info(node, sfs_inode);
    size_t len = sin->din->size;
    if (len > bufsize) {
        len = bufsize;
    }
    
    struct iobuf iob;
    iobuf_init(&iob, buf, len, 0);
    if ((ret = vop_read(node, &iob)) != len) {
        vop_ref_dec(node);
        return (ret < 0) ? ret : -E_IO;
    }
    
    vop_ref_dec(node);
    return len;
}
```

#### 4. 跟随软链接（修改 vfs_lookup）

```c
// kern/fs/vfs/vfslookup.c

/**
 * vfs_lookup_follow - 查找文件，跟随符号链接
 * @path: 文件路径
 * @node_store: 返回的 inode
 * @max_depth: 最大跟随深度（防止循环链接）
 */
int vfs_lookup_follow(char *path, struct inode **node_store, int max_depth) {
    int ret;
    struct inode *node;
    
    if (max_depth <= 0) {
        return -E_LOOP;  // 符号链接循环
    }
    
    // 1. 查找 inode
    if ((ret = vfs_lookup_nofollow(path, &node)) != 0) {
        return ret;
    }
    
    // 2. 检查是否为符号链接
    uint32_t type;
    if ((ret = vop_gettype(node, &type)) != 0) {
        vop_ref_dec(node);
        return ret;
    }
    
    if (type != SFS_TYPE_LINK) {
        // 不是符号链接，直接返回
        *node_store = node;
        return 0;
    }
    
    // 3. 读取符号链接的目标路径
    struct sfs_inode *sin = vop_info(node, sfs_inode);
    size_t len = sin->din->size;
    char *target = kmalloc(len + 1);
    if (target == NULL) {
        vop_ref_dec(node);
        return -E_NO_MEM;
    }
    
    struct iobuf iob;
    iobuf_init(&iob, target, len, 0);
    if ((ret = vop_read(node, &iob)) != len) {
        kfree(target);
        vop_ref_dec(node);
        return (ret < 0) ? ret : -E_IO;
    }
    target[len] = '\0';
    
    vop_ref_dec(node);
    
    // 4. 递归查找目标路径
    ret = vfs_lookup_follow(target, node_store, max_depth - 1);
    kfree(target);
    return ret;
}

// 默认的 vfs_lookup 函数跟随符号链接
int vfs_lookup(char *path, struct inode **node_store) {
    return vfs_lookup_follow(path, node_store, MAX_SYMLINK_DEPTH);
}
```

### 同步互斥问题处理

#### 1. 硬链接计数的原子性

**问题**：多个进程可能同时创建或删除硬链接，需要保证 `nlinks` 计数的原子性。

**解决方案**：
- 使用 inode 的信号量 `sin->sem` 保护
- 在修改 `nlinks` 前调用 `lock_sin(sin)`
- 修改完成后调用 `unlock_sin(sin)`

#### 2. 符号链接的循环检测

**问题**：符号链接可能形成循环，如 A -> B, B -> A。

**解决方案**：
- 设置最大跟随深度 `MAX_SYMLINK_DEPTH`（如 8）
- 在递归查找时传递深度参数
- 如果超过最大深度，返回 `-E_LOOP` 错误

#### 3. 删除文件时的引用计数

**问题**：多个进程可能同时打开同一个文件，删除时需要正确处理。

**解决方案**：
- 使用 inode 的引用计数 `ref_count`
- 只有当 `nlinks == 0` 且 `ref_count == 0` 时才真正释放
- 删除时只是将 `nlinks` 减为 0，实际释放在最后一个引用关闭时进行

#### 4. 目录操作的原子性

**问题**：在目录中添加或删除目录项时，需要保证操作的原子性。

**解决方案**：
- 使用目录 inode 的信号量保护
- 在修改目录前调用 `lock_sin(dir_sin)`
- 修改完成后调用 `unlock_sin(dir_sin)`

### 使用示例

#### 硬链接示例

```c
// 用户程序：创建硬链接
int main() {
    int fd;
    char buf[128];
    
    // 创建原始文件
    fd = open("file1.txt", O_CREAT | O_RDWR);
    write(fd, "Hello, World!", 13);
    close(fd);
    
    // 创建硬链接
    if (link("file1.txt", "file2.txt") < 0) {
        printf("link failed\n");
        return -1;
    }
    
    // 通过硬链接读取文件
    fd = open("file2.txt", O_RDONLY);
    read(fd, buf, 13);
    printf("Read from hard link: %s\n", buf);
    close(fd);
    
    // 删除原始文件，硬链接仍然有效
    unlink("file1.txt");
    fd = open("file2.txt", O_RDONLY);
    read(fd, buf, 13);
    printf("After unlink file1: %s\n", buf);
    close(fd);
    
    return 0;
}
```

#### 软链接示例

```c
// 用户程序：创建符号链接
int main() {
    int fd;
    char buf[128];
    char linkbuf[128];
    
    // 创建原始文件
    fd = open("original.txt", O_CREAT | O_RDWR);
    write(fd, "Symbolic link test", 18);
    close(fd);
    
    // 创建符号链接
    if (symlink("original.txt", "link.txt") < 0) {
        printf("symlink failed\n");
        return -1;
    }
    
    // 通过符号链接读取文件
    fd = open("link.txt", O_RDONLY);
    read(fd, buf, 18);
    printf("Read from symlink: %s\n", buf);
    close(fd);
    
    // 读取符号链接本身的内容
    if (readlink("link.txt", linkbuf, sizeof(linkbuf)) > 0) {
        printf("Symlink points to: %s\n", linkbuf);
    }
    
    // 删除原始文件后，符号链接变成悬空链接
    unlink("original.txt");
    fd = open("link.txt", O_RDONLY);
    if (fd < 0) {
        printf("Symlink is now broken\n");
    }
    
    return 0;
}
```

### 总结

本设计方案实现了 UNIX 风格的硬链接和软链接机制，主要特点包括：

**硬链接**：
1. 多个文件名指向同一个 inode
2. 使用引用计数 `nlinks` 管理
3. 只有当所有硬链接都删除时才释放文件
4. 不能对目录创建硬链接（防止循环）

**软链接**：
1. 存储目标文件的路径字符串
2. 支持跨文件系统链接
3. 可以链接到不存在的文件
4. 需要循环检测机制

**同步互斥**：
1. 使用信号量保护 inode 操作
2. 使用引用计数管理生命周期
3. 检测符号链接循环
4. 保证目录操作的原子性

这个设计方案能够有效地实现链接机制，并正确处理各种并发和异常情况。

---

## 重要知识点总结

### 本实验中的重要知识点

1. **虚拟文件系统（VFS）**
   - 提供统一的文件系统接口
   - 屏蔽不同文件系统的实现细节
   - 支持多种文件系统共存

2. **文件系统层次结构**
   - 通用文件访问接口层
   - 文件系统抽象层（VFS）
   - 具体文件系统层（SFS）
   - 外设接口层

3. **文件系统数据结构**
   - 超级块（Superblock）：全局信息
   - 索引节点（inode）：文件元数据
   - 目录项（dentry）：目录结构
   - 文件对象（file）：打开文件描述

4. **SFS 文件系统**
   - 基于索引节点组织
   - 直接索引 + 间接索引
   - 位图管理空闲块
   - 每个 inode 占用一个完整块

5. **设备抽象**
   - "一切皆文件"思想
   - 设备文件与普通文件统一接口
   - stdin、stdout、disk0 设备

6. **文件操作**
   - open：打开文件
   - read/write：读写文件
   - seek：定位文件位置
   - close：关闭文件

7. **程序加载**
   - ELF 文件格式解析
   - 从文件系统加载程序
   - 建立用户地址空间
   - 设置中断帧

8. **Shell 终端**
   - 命令解析
   - fork + exec 执行程序
   - 标准输入输出重定向

9. **文件系统初始化**
   - VFS 初始化
   - 设备初始化
   - SFS 挂载

10. **同步互斥**
    - 信号量保护文件系统数据结构
    - 互斥访问文件
    - 等待队列处理阻塞操作

### 与 OS 原理的对应关系

| 实验内容 | OS 原理知识点 | 关系说明 |
|---------|--------------|---------|
| **虚拟文件系统 VFS** | 文件系统抽象 | VFS 是文件系统抽象的具体实现，提供统一接口 |
| **inode** | 文件控制块（FCB） | inode 是文件控制块在 UNIX 系统中的实现 |
| **目录项** | 目录结构 | 目录项实现了文件名到 inode 的映射 |
| **文件描述符** | 打开文件表 | 文件描述符是进程打开文件表的索引 |
| **索引节点** | 文件组织方式 | 实验实现了索引式文件组织（直接索引+间接索引） |
| **位图** | 空闲空间管理 | 位图是空闲块管理的一种实现方式 |
| **设备文件** | I/O 设备管理 | 将设备抽象为文件，统一接口 |
| **ELF 加载** | 程序执行 | 实现了从文件系统加载和执行程序 |
| **Shell** | 命令解释器 | Shell 是操作系统的用户接口 |

### 实验与原理的差异

1. **文件系统实现**：
   - 原理：讲解多种文件系统（FAT、ext2/3/4、NTFS 等）
   - 实验：只实现了简化的 SFS 文件系统

2. **索引结构**：
   - 原理：多级间接索引、extent 等复杂结构
   - 实验：只实现了直接索引 + 一级间接索引

3. **缓存机制**：
   - 原理：页缓存（Page Cache）、缓冲区缓存（Buffer Cache）
   - 实验：未实现缓存机制，每次都访问磁盘

4. **文件权限**：
   - 原理：用户、组、其他的读写执行权限
   - 实验：简化的权限处理

---

## OS 原理中重要但实验未涉及的知识点

1. **高级文件系统特性**
   - 日志文件系统（Journaling）
   - 写时复制（Copy-on-Write）
   - 快照（Snapshot）
   - 压缩和加密

2. **文件系统性能优化**
   - 预读（Read-ahead）
   - 延迟写（Delayed Write）
   - 页缓存（Page Cache）
   - I/O 调度器

3. **高级文件组织方式**
   - B+ 树（用于索引）
   - Extent（连续块分配）
   - 多级间接索引
   - 内联数据（Inline Data）

4. **文件系统一致性**
   - fsck（文件系统检查）
   - 日志恢复
   - 检查点（Checkpoint）
   - 崩溃一致性

5. **磁盘调度算法**
   - FCFS
   - SSTF（最短寻道时间优先）
   - SCAN/C-SCAN（电梯算法）
   - LOOK/C-LOOK

6. **文件系统类型**
   - FAT32
   - NTFS
   - ext2/ext3/ext4
   - Btrfs、ZFS
   - 网络文件系统（NFS、CIFS）

7. **高级权限管理**
   - ACL（访问控制列表）
   - SELinux
   - 文件属性（immutable、append-only）

8. **磁盘管理**
   - 分区管理
   - LVM（逻辑卷管理）
   - RAID
   - 磁盘配额

9. **文件锁**
   - 劝告锁（Advisory Lock）
   - 强制锁（Mandatory Lock）
   - 记录锁（Record Lock）
   - 文件租约（Lease）

10. **特殊文件类型**
    - 套接字文件（Socket）
    - 命名管道（FIFO）
    - 设备文件（块设备、字符设备）

---

## 实验总结

通过本次实验，我深入理解了文件系统的实现原理和机制：

1. **文件系统架构**：通过 VFS 实现了文件系统的分层设计，使得操作系统可以支持多种文件系统

2. **文件组织方式**：SFS 使用索引式文件组织，通过直接索引和间接索引实现了灵活的文件存储

3. **设备抽象**：将设备抽象为文件，实现了"一切皆文件"的 UNIX 哲学

4. **程序执行**：实现了从文件系统加载和执行 ELF 格式程序的完整流程

5. **Shell 实现**：实现了简单的命令解释器，完成了操作系统的用户交互界面

本实验是整个操作系统实验的最后一个部分，至此我们完成了一个基本的操作系统，包括内存管理、进程管理、同步互斥和文件系统等核心功能。这个完整的实验过程让我对操作系统的工作原理有了深入的理解，也为未来进一步学习和开发操作系统打下了坚实的基础。

---

*本报告完整回答了 Lab8 实验指导书中的所有问题，并对两个 Challenge 进行了详细的设计和分析。*

