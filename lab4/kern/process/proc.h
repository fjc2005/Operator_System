#ifndef __KERN_PROCESS_PROC_H__
#define __KERN_PROCESS_PROC_H__

#include <defs.h>
#include <list.h>
#include <trap.h>
#include <memlayout.h>

// process's state in his life cycle
enum proc_state
{
    PROC_UNINIT = 0, // uninitialized
    PROC_SLEEPING,   // sleeping
    PROC_RUNNABLE,   // runnable(maybe running)
    PROC_ZOMBIE,     // almost dead, and wait parent proc to reclaim his resource
};

struct context
{
    uintptr_t ra; // 代码执行到哪一行
    uintptr_t sp; // 当前栈帧的地址
    uintptr_t s0;
    uintptr_t s1;
    uintptr_t s2;
    uintptr_t s3;
    uintptr_t s4;
    uintptr_t s5;
    uintptr_t s6;
    uintptr_t s7;
    uintptr_t s8;
    uintptr_t s9;
    uintptr_t s10;
    uintptr_t s11;
};

#define PROC_NAME_LEN 15
#define MAX_PROCESS 4096
#define MAX_PID (MAX_PROCESS * 2)

extern list_entry_t proc_list;

struct proc_struct
{
    enum proc_state state;        // Process state
    int pid;                      // Process ID
    int runs;                     // the running times of Process
    uintptr_t kstack;             // 进程的内核栈
    volatile bool need_resched;   // 是否需要重新调度 如果为 true，说明该让出 CPU 了
    struct proc_struct *parent;   // 父进程
    struct mm_struct *mm;         // 进程的内存管理字段
    struct context context;       // 进程的上下文
    struct trapframe *tf;         // 当前中断的陷阱帧
    /*context (Switch)：用于进程与进程之间的切换。是内核态代码主动调度的结果（比如 schedule() 函数）。
    tf (Interrupt)：用于用户态与内核态之间，或者中断发生时的现场保存。*/
    uintptr_t pgdir;              // 页目录表的基址
    uint32_t flags;               // 进程标志
    char name[PROC_NAME_LEN + 1]; // 进程名称
    list_entry_t list_link;       // 进程链表
    list_entry_t hash_link;       // 进程哈希链表
};

#define le2proc(le, member) \
    to_struct((le), struct proc_struct, member)

extern struct proc_struct *idleproc, *initproc, *current;

/*idleproc (PID 0): 大地。它是第 0 号进程，从来不休息（死循环），只有当所有人都没活干时，CPU 才归它。它的存在是为了保证 CPU 永远有指令可跑。*/
/*initproc (PID 1): 始祖。它是第 1 号进程，是所有后续用户进程的祖先。*/
/*current: 当下。指向当前占用 CPU 的那个 PCB。内核代码中凡是操作“自己”的地方，用的都是 current。*/

void proc_init(void);
void proc_run(struct proc_struct *proc);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);

char *set_proc_name(struct proc_struct *proc, const char *name);
char *get_proc_name(struct proc_struct *proc);
void cpu_idle(void) __attribute__((noreturn));

struct proc_struct *find_proc(int pid);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int do_exit(int error_code);

#endif /* !__KERN_PROCESS_PROC_H__ */
