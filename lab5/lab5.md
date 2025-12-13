# lab5: 用户程序

实验4完成了内核线程，但到目前为止，所有的运行都在内核态执行。实验5将创建用户进程，让用户进程在用户态执行，且在需要 ucore 支持时，可通过系统调用来让 ucore 提供服务。

## 实验目的

*   了解第一个用户进程创建过程
*   了解系统调用框架的实现机制
*   了解 ucore 如何实现系统调用 `sys_fork`/`sys_exec`/`sys_exit`/`sys_wait` 来进行进程管理

## 实验内容

实验4完成了内核线程，但到目前为止，所有的运行都在内核态执行。实验5将创建用户进程，让用户进程在用户态执行，且在需要 ucore 支持时，可通过系统调用来让 ucore 提供服务。为此需要构造出第一个用户进程，并通过系统调用 `sys_fork`/`sys_exec`/`sys_exit`/`sys_wait` 来支持运行不同的应用程序，完成对用户进程的执行过程的基本管理。

## 练习

**对实验报告的要求：**

*   基于 markdown 格式来完成，以文本方式为主
*   填写各个基本练习中要求完成的报告内容
*   列出你认为本实验中重要的知识点，以及与对应的 OS 原理中的知识点，并简要说明你对二者的含义，关系，差异等方面的理解（也可能出现实验中的知识点没有对应的原理知识点）
*   列出你认为 OS 原理中很重要，但在实验中没有对应上的知识点

### 练习0：填写已有实验

本实验依赖实验2/3/4。请把你做的实验2/3/4的代码填入本实验中代码中有“LAB2”/“LAB3”/“LAB4”的注释相应部分。注意：为了能够正确执行 lab5 的测试应用程序，可能需对已完成的实验2/3/4的代码进行进一步改进。

### 练习1: 加载应用程序并执行（需要编码）

`do_execve` 函数调用 `load_icode`（位于 `kern/process/proc.c` 中）来加载并解析一个处于内存中的 ELF 执行文件格式的应用程序。你需要补充 `load_icode` 的第6步，建立相应的用户内存空间来放置应用程序的代码段、数据段等，且要设置好 `proc_struct` 结构中的成员变量 `trapframe` 中的内容，确保在执行此进程后，能够从应用程序设定的起始执行地址开始执行。需设置正确的 `trapframe` 内容。

*   请在实验报告中简要说明你的设计实现过程。
*   请简要描述这个用户态进程被 ucore 选择占用 CPU 执行（RUNNING 态）到具体执行应用程序第一条指令的整个经过。

### 练习2: 父进程复制自己的内存空间给子进程（需要编码）

创建子进程的函数 `do_fork` 在执行中将拷贝当前进程（即父进程）的用户内存地址空间中的合法内容到新进程中（子进程），完成内存资源的复制。具体是通过 `copy_range` 函数（位于 `kern/mm/pmm.c` 中）实现的，请补充 `copy_range` 的实现，确保能够正确执行。

*   请在实验报告中简要说明你的设计实现过程。
*   如何设计实现 Copy on Write 机制？给出概要设计，鼓励给出详细设计。

> **Copy-on-write（简称COW）的基本概念**：是指如果有多个使用者对一个资源A（比如内存块）进行读操作，则每个使用者只需获得一个指向同一个资源A的指针，就可以该资源了。若某使用者需要对这个资源A进行写操作，系统会对该资源进行拷贝操作，从而使得该“写操作”使用者获得一个该资源A的“私有”拷贝—资源B，可对资源B进行写操作。该“写操作”使用者对资源B的改变对于其他的使用者而言是不可见的，因为其他使用者看到的还是资源A。

### 练习3: 阅读分析源代码，理解进程执行 fork/exec/wait/exit 的实现，以及系统调用的实现（不需要编码）

请在实验报告中简要说明你对 `fork`/`exec`/`wait`/`exit` 函数的分析。并回答如下问题：

*   请分析 `fork`/`exec`/`wait`/`exit` 的执行流程。重点关注哪些操作是在用户态完成，哪些是在内核态完成？内核态与用户态程序是如何交错执行的？内核态执行结果是如何返回给用户程序的？
*   请给出 ucore 中一个用户态进程的执行状态生命周期图（包执行状态，执行状态之间的变换关系，以及产生变换的事件或函数调用）。（字符方式画即可）

**执行：** `make grade`。如果所显示的应用程序检测都输出 ok，则基本正确。（使用的是 qemu-4.1.1）

### 扩展练习 Challenge

**实现 Copy on Write （COW）机制**

给出实现源码,测试用例和设计报告（包括在 cow 情况下的各种状态转换（类似有限状态自动机）的说明）。

这个扩展练习涉及到本实验和上一个实验“虚拟内存管理”。在 ucore 操作系统中，当一个用户父进程创建自己的子进程时，父进程会把其申请的用户空间设置为只读，子进程可共享父进程占用的用户内存空间中的页面（这就是一个共享的资源）。当其中任何一个进程修改此用户内存空间中的某页面时，ucore 会通过 page fault 异常获知该操作，并完成拷贝内存页面，使得两个进程都有各自的内存页面。这样一个进程所做的修改不会被另外一个进程可见了。请在 ucore 中实现这样的 COW 机制。

由于 COW 实现比较复杂，容易引入 bug，请参考 https://dirtycow.ninja/ 看看能否在 ucore 的 COW 实现中模拟这个错误和解决方案。需要有解释。

这是一个 big challenge.

*   说明该用户程序是何时被预先加载到内存中的？与我们常用操作系统的加载有何区别，原因是什么？

## 项目组成

```text
├── boot  
├── kern   
│ ├── debug  
│ │ ├── kdebug.c   
│ │ └── ……  
│ ├── mm  
│ │ ├── memlayout.h   
│ │ ├── pmm.c  
│ │ ├── pmm.h  
│ │ ├── ......  
│ │ ├── vmm.c  
│ │ └── vmm.h  
│ ├── process  
│ │ ├── proc.c  
│ │ ├── proc.h  
│ │ └── ......  
│ ├── schedule  
│ │ ├── sched.c  
│ │ └── ......  
│ ├── sync  
│ │ └── sync.h   
│ ├── syscall  
│ │ ├── syscall.c  
│ │ └── syscall.h  
│ └── trap  
│ ├── trap.c  
│ ├── trapentry.S  
│ ├── trap.h  
│ └── vectors.S  
├── libs  
│ ├── elf.h  
│ ├── error.h  
│ ├── printfmt.c  
│ ├── unistd.h  
│ └── ......  
├── tools  
│ ├── user.ld  
│ └── ......  
└── user  
├── hello.c  
├── libs  
│ ├── initcode.S  
│ ├── syscall.c  
│ ├── syscall.h  
│ └── ......  
└── ......
```

相对与实验四，主要增加和修改的文件如上图所示。主要改动如下：

**◆ kern/debug/**

*   `kdebug.c`：修改：解析用户进程的符号信息表示（可不用理会）

**◆ kern/mm/ （与本次实验有较大关系）**

*   `memlayout.h`：修改：增加了用户虚存地址空间的图形表示和宏定义 （需仔细理解）。
*   `pmm.[ch]`：修改：添加了用于进程退出（`do_exit`）的内存资源回收的 `page_remove_pte`、`unmap_range`、`exit_range` 函数和用于创建子进程（`do_fork`）中拷贝父进程内存空间的 `copy_range` 函数，修改了 `pgdir_alloc_page` 函数
*   `vmm.[ch]`：修改：扩展了 `mm_struct` 数据结构，增加了一系列函数
    *   `mm_map`/`dup_mmap`/`exit_mmap`：设定/取消/复制/删除用户进程的合法内存空间
    *   `copy_from_user`/`copy_to_user`：用户内存空间内容与内核内存空间内容的相互拷贝的实现
    *   `user_mem_check`：搜索 vma 链表，检查是否是一个合法的用户空间范围

**◆ kern/process/ （与本次实验有较大关系）**

*   `proc.[ch]`：修改：扩展了 `proc_struct` 数据结构。增加或修改了一系列函数
    *   `setup_pgdir`/`put_pgdir`：创建并设置/释放页目录表
    *   `copy_mm`：复制用户进程的内存空间和设置相关内存管理（如页表等）信息
    *   `do_exit`：释放进程自身所占内存空间和相关内存管理（如页表等）信息所占空间，唤醒父进程，好让父进程收了自己，让调度器切换到其他进程
    *   `load_icode`：被 `do_execve` 调用，完成加载放在内存中的执行程序到进程空间，这涉及到对页表等的修改，分配用户栈
    *   `do_execve`：先回收自身所占用户空间，然后调用 `load_icode`，用新的程序覆盖内存空间，形成一个执行新程序的新进程
    *   `do_yield`：让调度器执行一次选择新进程的过程
    *   `do_wait`：父进程等待子进程，并在得到子进程的退出消息后，彻底回收子进程所占的资源（比如子进程的内核栈和进程控制块）
    *   `do_kill`：给一个进程设置 `PF_EXITING` 标志（“kill”信息，即要它死掉），这样在 trap 函数中，将根据此标志，让进程退出
    *   `KERNEL_EXECVE`/`__KERNEL_EXECVE`/`__KERNEL_EXECVE2`：被 `user_main` 调用，执行一用户进程

**◆ kern/trap/**

*   `trap.c`：修改：在 `idt_init` 函数中，对 IDT 初始化时，设置好了用于系统调用的中断门（`idt[T_SYSCALL]`）信息。这主要与 syscall 的实现相关

**◆ user/**

*   新增的用户程序和用户库

## 用户进程管理

### 实验流程概述

我们在 lab1 中已经讲解过 RISC-V 的特权级。这里简要回顾一下：

*   **M态（Machine）**：最高权限，运行固件 OpenSBI，负责早期引导并向 S 态提供服务接口。
*   **S态（Supervisor）**：内核态，操作系统内核运行在此级别，管理内存、中断等。
*   **U态（User）**：用户态，运行普通用户程序，权限受限。

之前我们已经实现了内存的管理和内核进程的建立，但是那都是在内核态，接下来我们将在用户态运行一些程序。

用户程序，也就是我们在计算机系前几年课程里一直在写的那些程序，到底怎样在操作系统上跑起来？

首先需要编译器把用户程序的源代码编译为可以在 CPU 执行的目标程序，这个目标程序里，既要有执行的代码，又要有关于内存分配的一些信息，告诉我们应该怎样为这个程序分配内存。

我们先不考虑怎样在 ucore 里运行编译器（编译器其实也是用户程序的一种感兴趣的同学可以研究一下怎么把编译原理的课设项目运行在 ucore 中），只考虑 ucore 如何把编译好的用户程序运行起来。这需要给它分配一些内存，把程序代码加载进来，建立一个进程，然后通过调度让这个用户进程开始执行。

用户程序与内核程序有着本质区别：它们运行在受限制的用户态，无法直接分配内存、访问硬件或执行特权指令。这就产生了一个核心问题：用户程序如何安全地获取操作系统服务？

**系统调用**正是连接用户态与内核态的桥梁。它为用户程序提供了一套标准化的服务接口，使得用户程序能够通过受控的方式使用内核功能。

当用户程序需要操作系统提供服务时，比如一个 C 程序调用 `printf()` 函数进行输出，标准库会将输出请求转换为 `write` 系统调用。这个过程涉及从用户态到内核态的特权级切换，具体通过 `ecall` 指令实现。`ecall` 指令会触发一个异常事件，使 CPU 从用户态提升到内核态，并跳转到预设的中断处理程序 `trap` 中，在其中层层转发到系统调用函数 `write` 进行处理，之后再通过 `sret` 指令返回到用户态，到此，中断处理程序的纸飞机终于飞到系统调用手里。

当我们将视线转回到 ucore 的时候，就会遇到一个鸡生蛋还是蛋生鸡的问题，也就是，我们应该如何第一次从 S 态进入到 U 态的用户进程呢？

我们之前的内容提到的都是从用户态主动或被动地进入内核态，然后再从内核态返回到用户态的完整流程。但是在 ucore 的初始化进程中，我们始终处于内核态，因此，我们并不能像之后的用户进程一样完成这样一次完整的特权级切换循环，而是需要在内核态触发一个异常，从而借助异常处理机制的返回流程进行上下文的切换，从而第一次进入到用户进程。

关于用户进程的理论讲解可查看附录用户进程的特征。

### 用户进程

**须知**：经历了前几个章节的洗礼，各位应该对启动流程已经比较熟悉了，那么大家也都明白，前面的几章中，我们始终处于内核态，也就是 S 态，那么实现我们前面中断处理乃至系统调用的构想的前提，就是我们要先从内核态进入到用户态，那么我们应该如何进入呢。

我们在 `proc_init()` 函数里初始化进程的时候, 认为启动时运行的 ucore 程序, 是一个内核进程("第0个"内核进程), 并将其初始化为 `idleproc` 进程。然后我们新建了一个内核进程执行 `init_main()` 函数。

我们比较 lab4 和 lab5 的 `init_main()` 有何不同。

```c
// kern/process/proc.c (lab4)
static int init_main(void *arg) {
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
    cprintf("To U: \"%s\".\n", (const char *)arg);
    cprintf("To U: \"en.., Bye, Bye. :)\"\n");
    return 0;
}

// kern/process/proc.c (lab5)
static int init_main(void *arg) {
    size_t nr_free_pages_store = nr_free_pages();
    size_t kernel_allocated_store = kallocated();

    int pid = kernel_thread(user_main, NULL, 0);
    if (pid <= 0) {
        panic("create user_main failed.\n");
    }

    while (do_wait(0, NULL) == 0) {
        schedule();
    }

    cprintf("all user-mode processes have quit.\n");
    assert(initproc->cptr == NULL && initproc->yptr == NULL && initproc->optr == NULL);
    assert(nr_process == 2);
    assert(list_next(&proc_list) == &(initproc->list_link));
    assert(list_prev(&proc_list) == &(initproc->list_link));

    cprintf("init check memory pass.\n");
    return 0;
}
```

注意到，lab5 新建了一个内核进程，执行函数 `user_main()`, 这个内核进程里我们将要开始执行用户进程。

`do_wait(0, NULL)` 等待子进程退出，也就是等待 `user_main()` 退出。

我们来看 `user_main()` 和 `do_wait()` 里做了什么

```c
// kern/process/proc.c
#define __KERNEL_EXECVE(name, binary, size) ({                          \
            cprintf("kernel_execve: pid = %d, name = \"%s\".\n",        \
                    current->pid, name);                                \
            kernel_execve(name, binary, (size_t)(size));                \
        })

#define KERNEL_EXECVE(x) ({                                             \
            extern unsigned char _binary_obj___user_##x##_out_start[],  \
                _binary_obj___user_##x##_out_size[];                    \
            __KERNEL_EXECVE(#x, _binary_obj___user_##x##_out_start,     \
                            _binary_obj___user_##x##_out_size);         \
        })

#define __KERNEL_EXECVE2(x, xstart, xsize) ({                           \
            extern unsigned char xstart[], xsize[];                     \
            __KERNEL_EXECVE(#x, xstart, (size_t)xsize);                 \
        })

#define KERNEL_EXECVE2(x, xstart, xsize)        __KERNEL_EXECVE2(x, xstart, xsize)

// user_main - kernel thread used to exec a user program
static int
user_main(void *arg) {
#ifdef TEST
    KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE);
#else
    KERNEL_EXECVE(exit);
#endif
    panic("user_main execve failed.\n");
}
```

lab5 的 Makefile 进行了改动， 把用户程序编译到我们的镜像里。

`_binary_obj___user_##x##_out_start` 和 `_binary_obj___user_##x##_out_size` 都是编译的时候自动生成的符号。注意这里的 `##x##`，按照 C 语言宏的语法，会直接把 x 的变量名代替进去。

于是，我们在 `user_main()` 所做的，就是执行了

`kern_execve("exit", _binary_obj___user_exit_out_start,_binary_obj___user_exit_out_size)`

这么一个函数。

如果你熟悉 `execve()` 函数，或许已经猜到这里我们做了什么。

实际上，就是加载了存储在这个位置的程序 `exit` 并在 `user_main` 这个进程里开始执行。这时 `user_main` 就从内核进程变成了用户进程。我们在后面的小节介绍 `kern_execve()` 的实现。

我们在 user 目录下存储了一些用户程序，在编译的时候放到生成的镜像里。

```c
// user/exit.c
#include <stdio.h>
#include <ulib.h>

int magic = -0x10384;

int main(void) {
    int pid, code;
    cprintf("I am the parent. Forking the child...\n");
    if ((pid = fork()) == 0) {
        cprintf("I am the child.\n");
        yield();
        yield();
        yield();
        yield();
        yield();
        yield();
        yield();
        exit(magic);
    }
    else {
        cprintf("I am parent, fork a child pid %d\n",pid);
    }
    assert(pid > 0);
    cprintf("I am the parent, waiting now..\n");

    assert(waitpid(pid, &code) == 0 && code == magic);
    assert(waitpid(pid, &code) != 0 && wait() != 0);
    cprintf("waitpid %d ok.\n", pid);

    cprintf("exit pass.\n");
    return 0;
}
```

这个用户程序 `exit` 里我们测试了 `fork()` `wait()` 这些函数。这些函数都是 `user/libs/ulib.h` 对系统调用的封装。

```c
// user/libs/ulib.c
#include <defs.h>
#include <syscall.h>
#include <stdio.h>
#include <ulib.h>
void exit(int error_code) {
    sys_exit(error_code);
    //执行完sys_exit后，按理说进程就结束了，后面的语句不应该再执行，
    //所以执行到这里就说明exit失败了
    cprintf("BUG: exit failed.\n"); 
    while (1);
}
int fork(void) { return sys_fork(); }
int wait(void) { return sys_wait(0, NULL); }
int waitpid(int pid, int *store) { return sys_wait(pid, store); }
void yield(void) { sys_yield();}
int kill(int pid) { return sys_kill(pid); }
int getpid(void) { return sys_getpid(); }
```

在用户程序里使用的 `cprintf()` 也是在 `user/libs/stdio.c` 重新实现的，和之前比最大的区别是，打印字符的时候需要经过系统调用 `sys_putc()`，而不能直接调用 `sbi_console_putchar()`。这是自然的，因为只有在 Supervisor Mode 才能通过 `ecall` 调用 Machine Mode 的 OpenSBI 接口，而在用户态 (U Mode) 就不能直接使用 M mode 的接口，而是要通过系统调用。

```c
// user/libs/stdio.c
#include <defs.h>
#include <stdio.h>
#include <syscall.h>

/* *
 * cputch - writes a single character @c to stdout, and it will
 * increace the value of counter pointed by @cnt.
 * */
static void
cputch(int c, int *cnt) {
    sys_putc(c);//系统调用
    (*cnt) ++;
}

/* *
 * vcprintf - format a string and writes it to stdout
 *
 * The return value is the number of characters which would be
 * written to stdout.
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want cprintf() instead.
 * */
int
vcprintf(const char *fmt, va_list ap) {
    int cnt = 0;
    vprintfmt((void*)cputch, &cnt, fmt, ap);
    //注意这里复用了vprintfmt, 但是传入了cputch函数指针
    return cnt;
}
```

下面我们来看这些系统调用的实现。

### 系统调用实现

系统调用，是用户态(U mode)的程序获取内核态（S mode）服务的方法，所以需要在用户态和内核态都加入对应的支持和处理。我们也可以认为用户态只是提供一个调用的接口，真正的处理都在内核态进行。

**须知**

在用户进程管理中，有几个关键的系统调用尤为重要：

*   `sys_fork()` 用于创建当前进程的副本，生成子进程。父子进程都会从 `sys_fork()` 返回，但返回值不同：子进程得到0，父进程得到子进程的 PID，这使得两个进程可以执行不同的代码路径。
*   `sys_exec()` 在当前进程内启动一个新程序，保持 PID 不变但替换整个内存空间和执行代码。`fork()` 和 `exec()` 的组合是 Unix-like 系统中创建新进程的经典方式。
*   `sys_exit()` 用于终止当前进程，释放其占用的资源。
*   `sys_wait()` 使当前进程挂起，等待特定条件（如子进程退出）满足后再继续执行。

首先我们在头文件里定义一些系统调用的编号。

```c
// libs/unistd.h
/* syscall number */
#define SYS_exit            1
#define SYS_fork            2
#define SYS_wait            3
#define SYS_exec            4
#define SYS_clone           5
#define SYS_yield           10
#define SYS_sleep           11
#define SYS_kill            12
#define SYS_gettime         17
#define SYS_getpid          18
#define SYS_brk             19
#define SYS_mmap            20
#define SYS_munmap          21
#define SYS_shmem           22
#define SYS_putc            30
#define SYS_pgdir           31
```

我们注意在用户态进行系统调用的核心操作是，通过内联汇编进行 `ecall` 环境调用。这将产生一个 trap, 进入 S mode 进行异常处理。

```c
// user/libs/syscall.c
#include <defs.h>
#include <unistd.h>
#include <stdarg.h>
#include <syscall.h>
#define MAX_ARGS            5
static inline int syscall(int num, ...) {
    //va_list, va_start, va_arg都是C语言处理参数个数不定的函数的宏
    //在stdarg.h里定义
    va_list ap; //ap: 参数列表(此时未初始化)
    va_start(ap, num); //初始化参数列表, 从num开始
    //First, va_start initializes the list of variable arguments as a va_list.
    uint64_t a[MAX_ARGS];
    int i, ret;
    for (i = 0; i < MAX_ARGS; i ++) { //把参数依次取出
           /*Subsequent executions of va_arg yield the values of the additional arguments 
           in the same order as passed to the function.*/
        a[i] = va_arg(ap, uint64_t);
    }
    va_end(ap); //Finally, va_end shall be executed before the function returns.
    asm volatile (
        "ld a0, %1\n"
        "ld a1, %2\n"
        "ld a2, %3\n"
        "ld a3, %4\n"
        "ld a4, %5\n"
        "ld a5, %6\n"
        "ecall\n"
        "sd a0, %0"
        : "=m" (ret)
        : "m"(num), "m"(a[0]), "m"(a[1]), "m"(a[2]), "m"(a[3]), "m"(a[4])
        :"memory");
    //num存到a0寄存器， a[0]存到a1寄存器
    //ecall的返回值存到ret
    return ret;
}
int sys_exit(int error_code) { return syscall(SYS_exit, error_code); }
int sys_fork(void) { return syscall(SYS_fork); }
int sys_wait(int pid, int *store) { return syscall(SYS_wait, pid, store); }
int sys_yield(void) { return syscall(SYS_yield);}
int sys_kill(int pid) { return syscall(SYS_kill, pid); }
int sys_getpid(void) { return syscall(SYS_getpid); }
int sys_putc(int c) { return syscall(SYS_putc, c); }
```

我们下面看看 `trap.c` 是如何转发这个系统调用的。

```c
// kern/trap/trap.c
void exception_handler(struct trapframe *tf) {
    int ret;
    switch (tf->cause) { //通过中断帧里 scause寄存器的数值，判断出当前是来自USER_ECALL的异常
        case CAUSE_USER_ECALL:
            //cprintf("Environment call from U-mode\n");
            tf->epc += 4; 
            //sepc寄存器是产生异常的指令的位置，在异常处理结束后，会回到sepc的位置继续执行
            //对于ecall, 我们希望sepc寄存器要指向产生异常的指令(ecall)的下一条指令
            //否则就会回到ecall执行再执行一次ecall, 无限循环
            syscall();// 进行系统调用处理
            break;
        /*other cases .... */
    }
}
// kern/syscall/syscall.c
#include <unistd.h>
#include <proc.h>
#include <syscall.h>
#include <trap.h>
#include <stdio.h>
#include <pmm.h>
#include <assert.h>
//这里把系统调用进一步转发给proc.c的do_exit(), do_fork()等函数
static int sys_exit(uint64_t arg[]) {
    int error_code = (int)arg[0];
    return do_exit(error_code);
}
static int sys_fork(uint64_t arg[]) {
    struct trapframe *tf = current->tf;
    uintptr_t stack = tf->gpr.sp;
    return do_fork(0, stack, tf);
}
static int sys_wait(uint64_t arg[]) {
    int pid = (int)arg[0];
    int *store = (int *)arg[1];
    return do_wait(pid, store);
}
static int sys_exec(uint64_t arg[]) {
    const char *name = (const char *)arg[0];
    size_t len = (size_t)arg[1];
    unsigned char *binary = (unsigned char *)arg[2];
    size_t size = (size_t)arg[3];
    //用户态调用的exec(), 归根结底是do_execve()
    return do_execve(name, len, binary, size);
}
static int sys_yield(uint64_t arg[]) {
    return do_yield();
}
static int sys_kill(uint64_t arg[]) {
    int pid = (int)arg[0];
    return do_kill(pid);
}
static int sys_getpid(uint64_t arg[]) {
    return current->pid;
}
static int sys_putc(uint64_t arg[]) {
    int c = (int)arg[0];
    cputchar(c);
    return 0;
}
//这里定义了函数指针的数组syscalls, 把每个系统调用编号的下标上初始化为对应的函数指针
static int (*syscalls[])(uint64_t arg[]) = {
    [SYS_exit]              sys_exit,
    [SYS_fork]              sys_fork,
    [SYS_wait]              sys_wait,
    [SYS_exec]              sys_exec,
    [SYS_yield]             sys_yield,
    [SYS_kill]              sys_kill,
    [SYS_getpid]            sys_getpid,
    [SYS_putc]              sys_putc,
};

#define NUM_SYSCALLS        ((sizeof(syscalls)) / (sizeof(syscalls[0])))

void syscall(void) {
    struct trapframe *tf = current->tf;
    uint64_t arg[5];
    int num = tf->gpr.a0;//a0寄存器保存了系统调用编号
    if (num >= 0 && num < NUM_SYSCALLS) {//防止syscalls[num]下标越界
        if (syscalls[num] != NULL) {
            arg[0] = tf->gpr.a1;
            arg[1] = tf->gpr.a2;
            arg[2] = tf->gpr.a3;
            arg[3] = tf->gpr.a4;
            arg[4] = tf->gpr.a5;
            tf->gpr.a0 = syscalls[num](arg); 
            //把寄存器里的参数取出来，转发给系统调用编号对应的函数进行处理
            return ;
        }
    }
    //如果执行到这里，说明传入的系统调用编号还没有被实现，就崩掉了。
    print_trapframe(tf);
    panic("undefined syscall %d, pid = %d, name = %s.\n",
            num, current->pid, current->name);
}
```

这样我们就完成了系统调用的转发。接下来就是在 `do_exit()`, `do_execve()` 等函数中进行具体处理了。

### 第一次进入用户态

前面我们提到过，我们要通过 `kernel_execve` 来启动第一个用户进程，进入用户态，那么应该怎么实现 `kernel_execve` 函数呢，我们先来看看 `do_execve()` 函数

```c
// kern/process/proc.c
// do_execve - call exit_mmap(mm)&put_pgdir(mm) to reclaim memory space of current process
//           - call load_icode to setup new memory space accroding binary prog.
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size) {
    struct mm_struct *mm = current->mm;
    if (!user_mem_check(mm, (uintptr_t)name, len, 0)) { //检查name的内存空间能否被访问
        return -E_INVAL;
    }
    if (len > PROC_NAME_LEN) { //进程名字的长度有上限 PROC_NAME_LEN，在proc.h定义
        len = PROC_NAME_LEN;
    }
    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));
    memcpy(local_name, name, len);

    if (mm != NULL) {
        cputs("mm != NULL");
        lcr3(boot_cr3);
        if (mm_count_dec(mm) == 0) {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);//把进程当前占用的内存释放，之后重新分配内存
        }
        current->mm = NULL;
    }
    //把新的程序加载到当前进程里的工作都在load_icode()函数里完成
    int ret;
    if ((ret = load_icode(binary, size)) != 0) {
        goto execve_exit;//返回不为0，则加载失败
    }
    set_proc_name(current, local_name);
    //如果set_proc_name的实现不变, 为什么不能直接set_proc_name(current, name)?
    return 0;

execve_exit:
    do_exit(ret);
    panic("already exit: %e.\n", ret);
}
```

那么我们如何实现 `kernel_execve()` 函数？能否直接调用 `do_execve()`?

```c
static int kernel_execve(const char *name, unsigned char *binary, size_t size) {
    int64_t ret=0, len = strlen(name);
    ret = do_execve(name, len, binary, size);
    cprintf("ret = %d\n", ret);
    return ret;
}
```

很不幸。这么做行不通。`do_execve()` `load_icode()` 里面只是构建了用户程序运行的上下文，但是并没有完成切换。上下文切换实际上要借助中断处理的返回来完成。直接调用 `do_execve()` 是无法完成上下文切换的。如果是在用户态调用 `exec()`, 系统调用的 `ecall` 产生的中断返回时， 就可以完成上下文切换。

但是，目前我们在 S mode 下，所以不能通过 `ecall` 来产生中断。我们这里采取一个取巧的办法，用 `ebreak` 产生断点中断进行处理，通过设置 a7 寄存器的值为10说明这不是一个普通的断点中断，而是要转发到 `syscall()`, 这样用一个不是特别优雅的方式，实现了在内核态复用系统调用的接口。

```c
// kern/process/proc.c
// kernel_execve - do SYS_exec syscall to exec a user program called by user_main kernel_thread
static int kernel_execve(const char *name, unsigned char *binary, size_t size) {
    int64_t ret=0, len = strlen(name);
    asm volatile(
        "li a0, %1\n"
        "lw a1, %2\n"
        "lw a2, %3\n"
        "lw a3, %4\n"
        "lw a4, %5\n"
        "li a7, 10\n"
        "ebreak\n"
        "sw a0, %0\n"
        : "=m"(ret)
        : "i"(SYS_exec), "m"(name), "m"(len), "m"(binary), "m"(size)
        : "memory"); //这里内联汇编的格式，和用户态调用ecall的格式类似，只是ecall换成了ebreak
    cprintf("ret = %d\n", ret);
    return ret;
}
// kern/trap/trap.c
void exception_handler(struct trapframe *tf) {
    int ret;
    switch (tf->cause) {
        case CAUSE_BREAKPOINT:
            cprintf("Breakpoint\n");
            if(tf->gpr.a7 == 10){
                tf->epc += 4; //注意返回时要执行ebreak的下一条指令
                syscall();
            }
            break;
          /* other cases ... */
    }
}
```

注意我们需要让 CPU 进入 U mode 执行 `do_execve()` 加载的用户程序。进行系统调用 `sys_exec` 之后，我们在 trap 返回的时候调用了 `sret` 指令，这时只要 sstatus 寄存器的 SPP 二进制位为0，就会切换到 U mode，但 SPP 存储的是“进入 trap 之前来自什么特权级”，也就是说我们这里 `ebreak` 之后 SPP 的数值为1，`sret` 之后会回到 S mode 在内核态执行用户程序。所以 `load_icode()` 函数在构造新进程的时候，会把 `SSTATUS_SPP` 设置为0，使得 `sret` 的时候能回到 U mode。

### 中断处理

由于用户进程比起内核进程多了一个"用户栈"，也就是每个用户进程会有两个栈，一个内核栈一个用户栈，所以中断处理的代码 `trapentry.S` 要有一些小变化。关注用户态产生中断时，内核栈和用户栈两个栈顶指针的移动。

`# kern/trap/trapentry.S`

```asm
#include <riscv.h>

# 若在中断之前处于 U mode(用户态)
# 则 sscratch 保存的是内核栈地址
# 否则中断之前处于 S mode(内核态)，sscratch 保存的是 0

    .altmacro
    .align 2
    .macro SAVE_ALL
    LOCAL _restore_kernel_sp
    LOCAL _save_context

    # If coming from userspace, preserve the user stack pointer and load
    # the kernel stack pointer. If we came from the kernel, sscratch
    # will contain 0, and we should continue on the current stack.
    csrrw sp, sscratch, sp #这里交换了sp和sccratch寄存器
    #sp为0，说明之前是内核态，我们刚才把内核栈指针换到了sscratch, 需要再拿回来
    #sp不为0 时，说明之前是用户态，sp里现在存的就是内核栈指针，sscratch里现在是用户栈指针
    #sp不为0，就跳到_save_context, 跳过_restore_kernel_sp的代码
    bnez sp, _save_context     

_restore_kernel_sp:
    csrr sp, sscratch #刚才把内核栈指针换到了sscratch, 需要再拿回来
_save_context:
    #分配栈帧
    addi sp, sp, -36 * REGBYTES 
    # save x registers
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

    # get sr, epc, tval, cause
    # Set sscratch register to 0, so that if a recursive exception
    # occurs, the exception vector knows it came from the kernel
    #如果之前是用户态产生的中断，用户栈指针从sscratch里挪到了s0寄存器， sscratch清零
    csrrw s0, sscratch, x0
    csrr s1, sstatus
    csrr s2, sepc
    csrr s3, 0x143 #stval
    csrr s4, scause

    STORE s0, 2*REGBYTES(sp) #如果之前是用户态发生中断，此时用户栈指针存到了内存里
    STORE s1, 32*REGBYTES(sp)
    STORE s2, 33*REGBYTES(sp)
    STORE s3, 34*REGBYTES(sp)
    STORE s4, 35*REGBYTES(sp)
    .endm

    .macro RESTORE_ALL
    LOCAL _save_kernel_sp
    LOCAL _restore_context

    LOAD s1, 32*REGBYTES(sp)
    LOAD s2, 33*REGBYTES(sp)

    andi s0, s1, SSTATUS_SPP #可以通过SSTATUS_SPP的值判断之前是用户态还是内核态
    bnez s0, _restore_context

_save_kernel_sp:
    # Save unwound kernel stack pointer in sscratch
    addi s0, sp, 36 * REGBYTES
    csrw sscratch, s0
_restore_context:
    csrw sstatus, s1
    csrw sepc, s2

    # restore x registers
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
    # restore sp last
    LOAD x2, 2*REGBYTES(sp) #如果是用户态产生的中断，此时sp恢复为用户栈指针
    .endm

    .globl __alltraps
__alltraps:
    SAVE_ALL

    move  a0, sp
    jal trap
    # sp should be the same as before "jal trap"

    .globl __trapret
__trapret:
    RESTORE_ALL
    # return from supervisor call
    sret

    .globl forkrets
forkrets:
    # set stack to this new process's trapframe
    move sp, a0
    j __trapret
```

### 进程退出

当进程执行完它的工作后，就需要执行退出操作，释放进程占用的资源。ucore 分了两步来完成这个工作，首先由进程本身完成大部分资源的占用内存回收工作，然后由此进程的父进程完成剩余资源占用内存的回收工作。为何不让进程本身完成所有的资源回收工作呢？这是因为进程要执行回收操作，就表明此进程还存在，还在执行指令，这就需要内核栈的空间不能释放，且表示进程存在的进程控制块不能释放。所以需要父进程来帮忙释放子进程无法完成的这两个资源回收工作。

为此在用户态的函数库中提供了 `exit` 函数，此函数最终访问 `sys_exit` 系统调用接口让操作系统来帮助当前进程执行退出过程中的部分资源回收。我们来看看 ucore 是如何做进程退出工作的。

```c
// /user/libs/ulib.c

void
exit(int error_code) {
    sys_exit(error_code);
    cprintf("BUG: exit failed.\n");
    while (1);
}

// /kern/syscall/syscall.c
static int
sys_exit(uint64_t arg[]) {
    int error_code = (int)arg[0];
    return do_exit(error_code);
}
```

首先，`exit` 函数会把一个退出码 `error_code` 传递给 ucore，ucore 通过执行位于 `/kern/process/proc.c` 中的内核函数 `do_exit` 来完成对当前进程的退出处理，主要工作简单地说就是回收当前进程所占的大部分内存资源，并通知父进程完成最后的回收工作，具体流程如下：

```c
// /kern/process/proc.c

int
do_exit(int error_code) {
    // 检查当前进程是否为idleproc或initproc，如果是，发出panic
    if (current == idleproc) {
        panic("idleproc exit.\n");
    }
    if (current == initproc) {
        panic("initproc exit.\n");
    }

    // 获取当前进程的内存管理结构mm
    struct mm_struct *mm = current->mm;

    // 如果mm不为空，说明是用户进程
    if (mm != NULL) {
        // 切换到内核页表，确保接下来的操作在内核空间执行
        lcr3(boot_cr3);

        // 如果mm引用计数减到0，说明没有其他进程共享此mm
        if (mm_count_dec(mm) == 0) {
            // 释放用户虚拟内存空间相关的资源
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
        }
        // 将当前进程的mm设置为NULL，表示资源已经释放
        current->mm = NULL;
    }

    // 设置进程状态为PROC_ZOMBIE，表示进程已退出
    current->state = PROC_ZOMBIE;
    current->exit_code = error_code;

    bool intr_flag;
    struct proc_struct *proc;

    // 关中断
    local_intr_save(intr_flag);
    {
        // 获取当前进程的父进程
        proc = current->parent;

        // 如果父进程处于等待子进程状态，则唤醒父进程
        if (proc->wait_state == WT_CHILD) {
            wakeup_proc(proc);
        }

        // 遍历当前进程的所有子进程
        while (current->cptr != NULL) {
            proc = current->cptr;
            current->cptr = proc->optr;

            // 设置子进程的父进程为initproc，并加入initproc的子进程链表
            proc->yptr = NULL;
            if ((proc->optr = initproc->cptr) != NULL) {
                initproc->cptr->yptr = proc;
            }
            proc->parent = initproc;
            initproc->cptr = proc;

            // 如果子进程也处于退出状态，唤醒initproc
            if (proc->state == PROC_ZOMBIE) {
                if (initproc->wait_state == WT_CHILD) {
                    wakeup_proc(initproc);
                }
            }
        }
    }
    // 开中断
    local_intr_restore(intr_flag);

    // 调用调度器，选择新的进程执行
    schedule();

    // 如果执行到这里，表示代码执行出现错误，发出panic
    panic("do_exit will not return!! %d.\n", current->pid);
}
```

## 分支任务：gdb 调试系统调用以及返回

之前，我们使用双重 gdb 的方案观测了 qemu 将一个虚拟地址翻译成物理地址的整个过程。那么这一次，我们温故而知新，使用同一套方案来观察操作系统中一个至关重要的机制——系统调用的完整流程。

系统调用是用户程序与操作系统内核交互的核心桥梁。当运行在受限的用户态(U mode)的程序需要获得内核服务时（如读写文件、分配内存），必须通过系统调用请求内核在更高特权的内核态(S mode)代为执行。这种特权级的隔离保障了系统的安全性和稳定性。

通过本次调试，我们将能够亲眼观察从用户态触发系统调用到返回用户态的完整过程，包括特权级切换、参数传递、内核处理等关键环节。

本次我们使用的双重 gdb 方案和 lab2 中调试地址翻译流程的操作流程都是基本相同的。我们先分析一下调试思路。

### 思考

回想一下我们之前调试地址翻译流程的调试思路，我们是找到了一个地址，并且知道这个地址一定会被访问，并且第一次访问的时候一定需要查找页表，我们直接在必经之路上打上条件断点来判断当前传入的地址是否是我们要观测的地址，然后直接启动内核执行就可以了，当 ucore 访问到这个地址的时候，qemu 就会自动停下，我们就可以跟踪代码的执行。

那么系统调用的观测是不是也可以这么做呢？

应该是可以的，用大模型查看一下用户程序，可以发现，所有的系统调用接口最后都是对 `syscall` 这个函数的封装，在这个函数中，将参数放在指定的位置之后，使用内联汇编调用 `ecall`，从而触发中断，进入到我们之前设置的中断入口点来进行中断处理流程。

那么很显然，这个地方的 `ecall` 就是一个相当合适的观测点，我们可以让 ucore 运行到这个 `ecall` 指令之前停住，然后为 qemu 设置合适的断点（同理，可以询问大模型：qemu 的源码中是如何处理 `ecall` 指令的，给我找一下关键的代码和流程），随后单步执行这条指令，qemu 就会及时打住执行，我们就可以继续跟踪 qemu 的代码执行逻辑，来观测它是如何处理 `ecall` 的。

等到 `ecall` 处理完之后，移除掉原本的断点，防止之后的执行被莫名打断，然后根据中断处理的流程找一下，系统调用执行结束之后，控制流是如何返回到用户态的，我们需要让 ucore 停在返回用户态之前的 `sret` 指令处，再次重复之前的流程，也就是找到 qemu 处理 `sret` 指令的关键代码，并设置断点跟踪执行。

### 实际操作

而当我们真正开始进行调试的时候，可能遇到的第一个问题就是，在前四章里面，我们都是处于内核态的，而这一次，我们调试的重点在于从用户态进入内核态再返回（也就是我们调试的实质上是一个运行在你的 linux 环境中的 qemu 上运行的内核上运行的用户程序），那么很自然的，我们按照先前的想法在用户态的 `syscall` 函数处打下一个断点，然后就会喜提一个奇怪的输出：

```gdb
(gdb) b kern_entry
Breakpoint 1 at 0xffffffffc0200000: file kern/init/entry.S, line 10.
(gdb) b user/libs/syscall.c:26
No source file named user/libs/syscall.c.
<future shared library load? (y or [n]) y
Breakpoint 2 (user/libs/syscall.c:26) pending.
(gdb) b syscall
Breakpoint 3 at 0xffffffffc0205180: file kern/syscall/syscall.c, line 83.
```

这个输出的结果是什么意思呢（要不你问问大模型吧），其实就是目前 gdb 找到的"调试信息"中找不到用户程序的 libs 等内容，也就是说，我们目前的 makefile 中编写的用于调试的 `make debug` 和 `make gdb` 这两个目标只加载了内核本身的调试信息，而用户程序的调试信息并没有加载进去。

**模型使用示例** 向一个你使用顺手的大模型提问，我目前的内核是如何将用户程序加载进去的，如果我想要调试内核上运行的用户程序，是不是需要将用户程序的符号表也都加载进去，我应该怎么操作，为什么使用 `makedebug` 和 `makegdb` 调试的时候就会自动将内核符号表加载进去。

之后大模型就会给出一些解释和方案，根据项目中的文件夹和代码进行分析，大概信息如下。

> 在 Lab5 中，用户程序并不是像现代 Linux 那样存储在磁盘文件系统（如 ext4）中的。为了简化实验，ucore 采用了一种 "Link-in-Kernel" (链接进内核) 的方式。
> ...
> **如何加载用户程序符号表**
> 如果你想调试 `exit.c`，你需要手动告诉 GDB 加载这个文件的符号表。
> **操作步骤：**
> 启动调试：运行 `make debug` 或 `make qemu` + `make gdb`。
> 加载符号：在 GDB 提示符下输入：
> `(gdb) add-symbol-file obj/__user_exit.out`
> GDB 会提示确认，输入 y。
> 不需要指定地址，因为 `user.ld` 已经定义了它是静态链接到 `0x800020` 的，GDB 会自动从 ELF 头中读取这个地址。
> **打断点：**
> 现在 GDB 认识用户程序的函数了。
> `(gdb) b user/exit.c:9`  # 推荐：直接对源文件打断点

那么我们只需要在 `make gdb` 命令之后，在 gdb 命令行中执行对应的命令，就可以正确的加载用户程序的调试信息了，之后我们就可以在用户库函数的 `syscall` 处打上断点了：

```gdb
(gdb) add-symbol-file obj/__user_exit.out
add symbol table from file "obj/__user_exit.out"
(y or n) y
Reading symbols from obj/__user_exit.out...
(gdb) break user/libs/syscall.c:18
Breakpoint 1 at 0x8000f8: file user/libs/syscall.c, line 19.
```

那么我们只需要让 ucore 继续执行，就会在第一次执行到用户态的 `syscall` 函数的时候停住，此时，可能会遇到第二个问题，因为 `syscall` 是一个 c 函数，而我们真正关注的是这个 c 函数的内联汇编中的 `ecall` 指令，看起来很奇怪，不过不要被吓住，毕竟本质上 cpu 上执行的都是汇编指令，我们可以直接使用 `si` 来单步执行汇编指令，gdb 中也有一些命令来帮助我们显示当前指令之后的几条汇编指令，我们就可以找到那个我们真正关心的指令——`ecall`，我们控制 ucore 执行到 `ecall` 指令之前：

```gdb
(gdb) si
0x0000000000800104      19          asm volatile (
1: x/7i $pc
=> 0x800104 <syscall+44>:       ecall  
   0x800108 <syscall+48>:
    sd  a0,28(sp)
   0x80010c <syscall+52>:
    lw  a0,28(sp)
   0x80010e <syscall+54>:
    addi        sp,sp,144
   0x800110 <syscall+56>:       ret    
   0x800112 <sys_exit>: mv      a1,a0  
   0x800114 <sys_exit+2>:
    li  a0,1
(gdb) i r $pc
pc             0x800104 0x800104 <syscall+44>
```

此时，我们就需要为 qemu 打上断点了，目前，我们执行 `make debug` 的终端中由于调试 ucore 的 gdb 打断了 ucore 的执行而卡住，而调试 ucore 的 gdb 在等待我们的下一步指令，而附加到 qemu 的 gdb 中显示为 Continuing，我们应该如何在 qemu 中添加一个断点呢？只需要在这个显示 Continuing 的终端中按下 `ctrl + C` 即可。

这时我们就可以像之前一样为 qemu 打下断点，然后继续执行 qemu，接着让 ucore 执行 `ecall` 指令，我们就会发现 attach 在 qemu 上的 gdb 卡在了我们打下的断点处，我们就可以从这里开始跟踪执行，当 `ecall` 的处理完成之后，我们可以让 ucore 继续执行，停在 `sret` 指令前一句，并执行类似的操作，跟踪 `sret` 的处理流程。

到此，我们就完整的观测了一个从 U 态触发系统调用进入到 S 态，并在 S 态进行系统调用的处理，处理结束之后返回 U 态的过程，尤其是，我们细致的观测了 qemu 是如何模拟硬件进行 `ecall` 和 `sret` 两个指令的处理的。

**最后的小建议**

调试过程中可能会遇到各种奇怪的问题，比如断点不触发、程序跑飞等等。别慌！ 这正是学习的机会。把错误信息、你的操作步骤、以及你的困惑一起扔给大模型，它会给你提供排查思路。记住，大模型不只是帮你写代码的工具，更是你学习的智能助手。用好它，你就能在复杂系统的探索路上走得更远。

### 调试要求

*   在大模型的帮助下，完成整个调试的流程，观察一下 `ecall` 指令和 `sret` 指令是如何被 qemu 处理的，并简单阅读一下调试中涉及到的 qemu 源码，解释其中的关键流程。
*   在执行 `ecall` 和 `sret` 这类汇编指令的时候，qemu 进行了很关键的一步——指令翻译（TCG Translation），了解一下这个功能，思考一下另一个双重 gdb 调试的实验是否也涉及到了一些相关的内容。
*   记录下你调试过程中比较抓马有趣的细节，以及在观察模拟器通过软件模拟硬件执行的时候了解到的知识。
*   记录实验过程中，有哪些通过大模型解决的问题，记录下当时的情景，你的思路，以及你和大模型交互的过程。

## 实验报告要求

从 oslab 网站上取得实验代码后，进入目录 `labcodes/lab5`，完成实验要求的各个练习。在实验报告中回答所有练习中提出的问题。在目录 `labcodes/lab5` 下存放实验报告，推荐用 markdown 格式。每个小组建一个 gitee 或者 github 仓库，对于 lab5 中编程任务，完成编写之后，再通过 `git push` 命令把代码和报告上传到仓库。最后请一定提前或按时提交到 git 网站。

注意有“LAB5”的注释，代码中所有需要完成的地方（challenge 除外）都有“LAB5”和“YOUR CODE”的注释，请在提交时特别注意保持注释，并将“YOUR CODE”替换为自己的学号，并且将所有标有对应注释的部分填上正确的代码。