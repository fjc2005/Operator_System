# 实验三：中断与异常处理（Lab3）实验报告

## 摘要

本实验聚焦 RISC-V 的中断/异常（Trap）处理机制，完成从入口汇编到 C 语言处理函数的完整链路：设置 `stvec` 进入统一入口 `__alltraps`，在 `trapentry.S` 通过 `SAVE_ALL/RESTORE_ALL` 完成上下文保存与恢复，并在 `trap.c` 中根据 `scause` 分发到时钟中断与异常处理逻辑。核心编程任务为补全时钟中断处理：每次时钟中断设置下一次事件，累计 `ticks`，每 100 次打印一次“100 ticks”，累计 10 行后通过 `sbi` 关机。同时完成对断点/非法指令异常的处理与三项 Challenge 问答，梳理关键寄存器 (`sstatus/sepc/scause/stval`) 与返回路径 (`sret`) 的角色与关系。

## 实验环境与代码入口

- 架构与平台：QEMU RISC-V 64，OpenSBI 固件
- 主要文件：
  - `kern/trap/trapentry.S`：统一入口、上下文保存/恢复宏与 `__alltraps`/`__trapret`
  - `kern/trap/trap.c`：`trap()`/`trap_dispatch()`、`interrupt_handler()`、`exception_handler()`
  - `kern/driver/clock.c`：`clock_init()`、`clock_set_next_event()`、`ticks`
  - `kern/init/init.c`：初始化 `stvec`、开启中断、初始化时钟
  - `libs/sbi.[ch]`：`sbi_set_timer()`、`shut_down()` 封装

## 1. 练习：完善时钟中断处理（需要编程）

### 1.1 实现要点

- 在 `interrupt_handler()` 的 `IRQ_S_TIMER` 分支：
  1) 调用 `clock_set_next_event()` 安排下一次时钟事件；
  2) `ticks++`；
  3) 当 `ticks % 100 == 0` 时调用 `print_ticks()` 打印一行 `100 ticks`，并统计已打印次数 `num`；
  4) 当打印满 10 行时，调用 `shut_down()` 关机。

- 计时基准：`clock.c` 使用 `rdtime` 与固定 `timebase`（如 1e5）在 QEMU 下近似 100Hz，满足“每约 1s 打印一次”需求。

### 1.2 处理流程（逻辑）

1) 时钟初始化：`clock_init()` 使能 `sie` 中的 `MIP_STIP` 位，设置第一次时钟事件，清零 `ticks`；
2) 发生时钟中断：硬件写入 `scause/sepc`，跳转到 `stvec=__alltraps`；
3) `__alltraps`：执行 `SAVE_ALL` 将通用寄存器与关键 CSR 入栈，`mv a0, sp` 以 `trapframe*` 作为参数调用 `trap()`；
4) `trap()`：根据 `tf->cause` 分发至 `interrupt_handler()`；
5) `IRQ_S_TIMER`：设置下一次事件、累积 `ticks`、按 100 次打印一次，10 次后关机；
6) 返回：`__trapret` 执行 `RESTORE_ALL` 恢复寄存器，`sret` 依据 `sepc/sstatus` 返回原执行流。

### 1.3 运行结果

- `make qemu` 后可观察到每约 1 秒打印一行 `100 ticks`，合计 10 行后触发关机（或等效的结束路径）。

## 2. 扩展练习 Challenge1：描述与理解中断流程

### 2.1 uCore 中 Trap/中断处理全流程（S 模式）

- 委托：OpenSBI 通过 `medeleg/mideleg` 将多数异常/中断委托给 S 模式；
- 入口：`stvec` 指向 `__alltraps`（Direct 模式）；
- 硬件自动保存：写入 `sepc/scause/stval`，并在 `sstatus` 中更新 `SPP/SPIE/SIE`；
- 入口汇编：`SAVE_ALL` 将通用寄存器与 CSR 按 `trapframe` 布局入栈，`mv a0, sp; jal trap`；
- C 侧分发：`trap_dispatch()` 判断最高位区分中断/异常，分别交由 `interrupt_handler/exception_handler`；
- 返回：必要时调整 `tf->epc`（异常需前进 4 字节以越过致因指令），在 `__trapret` 通过 `RESTORE_ALL` 恢复 `sstatus/sepc` 与 GPR，`sret` 返回原态。

### 2.2 `mv a0, sp` 的目的

将当前栈顶处的 `trapframe` 指针作为第 1 个参数传递给 C 函数 `trap(struct trapframe *tf)`，便于 C 代码读取并修改 `status/epc/cause/stval` 等上下文信息。

### 2.3 `SAVE_ALL` 中寄存器在栈中的位置如何确定

由 `struct trapframe`/`struct pushregs` 的字段顺序与大小决定。汇编宏严格按该结构体字段顺序执行 `STORE`，从而保证 C 侧按相同布局访问。

### 2.4 是否必须保存所有寄存器

理论上可仅保存必要集（如被调用者保存寄存器、`sepc/sstatus` 等）。但为简化实现与避免被中断点任意破坏寄存器造成不一致，教学内核采用“保存所有 GPR + 关键 CSR”的方案，成本小、正确性强，便于 C 代码无约束地使用寄存器。

## 3. 扩展练习 Challenge2：理解上下文切换机制

### 3.1 `csrw sscratch, sp` 与 `csrrw s0, sscratch, x0` 的含义与目的

- `csrw sscratch, sp`：将原始栈指针暂存到 `sscratch`；
- 在保存完其它寄存器后用 `csrrw s0, sscratch, x0` 读回该值到 `s0` 并清零 `sscratch`；
- 目的：在分配 `trapframe` 空间后仍能记录“进入中断前”的栈指针并将其写入 `tf->gpr.sp`，保证返回时恢复到中断发生前的正确栈。

### 3.2 为何保存 `stval/scause` 但恢复时不写回

`scause/stval` 记录异常原因与附加信息，仅供处理阶段读取决策；返回现场时硬件依据 `sstatus/sepc` 即可恢复控制流，将 `scause/stval` 写回没有意义。必须恢复的是 `sepc`（返回地址）与 `sstatus`（中断使能与上一次特权级）。

## 4. 扩展练习 Challenge3：完善异常处理（非法指令与断点）

实现要点：
- 在 `exception_handler()` 的 `CAUSE_ILLEGAL_INSTRUCTION` 与 `CAUSE_BREAKPOINT` 分支：
  - 输出异常类型与触发地址：如 `Illegal instruction caught at 0x%p`、`ebreak caught at 0x%p`，以及 `Exception type: ...`；
  - 更新 `tf->epc += 4` 跳过致因指令，避免重复陷入；
  - 继续返回正常执行流。

## 5. 实验知识点与操作系统原理对应

| 实验知识点 | OS 原理要点 | 关系与理解 |
| :--- | :--- | :--- |
| `stvec` 入口与 Trap 路由 | 中断向量/统一入口 | Direct 模式下统一入口由软件分发，Vector 模式可按原因跳表；教学内核采用统一入口便于演示 SAVE/RESTORE。 |
| `sstatus` 的 `SIE/SPIE/SPP` | 中断使能与返回语义 | 进入时保存“之前的中断使能与特权级”，返回时 `sret` 恢复，保障原子性与正确返回。 |
| `sepc/scause/stval` | 现场记录与诊断 | `sepc` 定位返回点，`scause/stval` 指明原因/附加信息，供处理决策与日志。 |
| SAVE/RESTORE 与 `trapframe` | 上下文保存/恢复 | 以结构体布局统一管理寄存器，确保 C 侧可安全读写上下文。 |
| 时钟中断与 `sbi_set_timer` | 定时与时钟源 | 通过 OpenSBI 触发下一次中断，形成周期性 Tick，支撑调度/时间片。 |
| `intr_enable/disable` | 临界区与原子性 | 在关键内核路径临时关闭中断，避免被抢占破坏不变式。 |

## 6. 原理中重要但本实验未完整覆盖的内容

- 嵌套中断与优先级、屏蔽/不可屏蔽中断策略；
- 向量化 `stvec` 模式与分级中断控制器；
- 多核（SMP）下中断路由与 IPI；
- 与调度器联动：时钟中断驱动调度、抢占点设计；
- 调试/安全强化：断点扩展、栈保护、异常统计与可观测性。

## 7. 结论

本实验完成了 RISC-V S 模式下的 Trap 处理链路：从 `stvec` 统一入口到上下文保存、C 侧分发与返回路径，按要求实现了周期性时钟中断打印与关机，并补全了断点/非法指令异常的处理。通过对关键寄存器与控制流的梳理，建立了“硬件自动现场记录 + 汇编包装 + C 语言分发”的清晰心智模型，为后续引入调度、系统调用与虚拟内存管理等模块奠定基础。



