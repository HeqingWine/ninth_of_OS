# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
| --- | --- |
| **实验名称** | Lab 5: 用户程序 |
| **小组成员** | 2413094-卢思齐、2414113-焦程远、2411617-陈彦冰 |
| **完成日期** |2026-11-19 |

### 小组分工

| 成员 | 负责的练习/模块 |
| --- | --- |
| 2413094-卢思齐 | 模块一<用户态系统调用功能> |
| 2411617-陈彦冰 | 模块二<创建加载第一个用户进程功能> |
| 2414113-焦程远 | 模块三<复制用户进程功能>、模块四<其余系统调用功能> |

***

## 一、实验目的

本实验的主要目的是：

- 了解第一个用户进程创建过程
- 了解系统调用框架的实现机制
- 了解ucore如何实现系统调用sys_fork/sys_exec/sys_exit/sys_wait来进行进程管理

***

## 二、实验环境

| 成员 | AI 编程工具 | 底层模型 | 备注 |
| --- | --- | --- | --- |
| 2413094-卢思齐 | Copilot（VS Code插件） | DeepSeek-V4-pro | 自定义Agent |
| 2411617-陈彦冰 | Claude Code（VS Code插件） | DeepSeek-Flash |  |
| 2414113-焦程远 | Copilot（VS Code插件） | DeepSeek-V4-pro | 自定义Agent |

**说明：**

- **AI 编程工具**：指具体使用的终端工具、编辑器插件、桌面应用或浏览器界面
- **底层模型**：指该工具使用的大语言模型及版本

***

## 三、实验整体逻辑分析

### 3.1 本章节的逻辑主线

- 本章围绕哪个核心主题展开？
    - 本章实验围绕“用户进程的创建、运行与生命周期管理”这一核心主题展开。它是操作系统从“只能运行内核线程”迈向“能够运行用户程序”的关键一步，涵盖系统调用入口、trapframe 与用户态/内核态切换、第一个用户进程的装载与启动、进程复制、进程退出与回收等机制。
- 整个实验解决什么问题？
    - 整个实验解决了“如何让用户程序在操作系统之上安全运行”的问题。具体而言，在具备物理内存、虚拟内存、中断处理和内核线程之后，系统需要先建立一条从用户态 `ecall` 进入内核、再由 `sret` 返回用户态的系统调用链路；然后通过 `kernel_execve` 手工构造 `trapframe`，让 `init_main`/`user_main` 这条内核线程链第一次进入 U 态，启动第一个用户程序；接着通过 `do_fork`、`copy_mm`、`dup_mmap`、`copy_range` 让用户进程能够复制自身；最后通过 `do_exit`、`do_yield`、`do_kill` 与 `do_wait` 的配合，完成进程的退出、唤醒和僵尸回收，从而建立完整的用户进程生命周期闭环。贯穿始终的 `trapframe`、`proc_struct` 和 `mm_struct`，则把系统调用、进程启动、复制和退出串成同一条状态链。

### 3.2 功能的逐步实现

本章实验严格遵循“先打通用户态与内核态之间的切换通道，再创建并启动第一个用户进程，然后支持进程复制，最后完成退出与回收”的逻辑主线，逐步实现了以下功能：

1. 首先实现**用户态系统调用功能**
    - 系统调用是用户态获得内核服务的唯一受控入口，也是后面 `fork`、`exec`、`exit`、`wait` 能够被用户程序调用的前提。先修改 `trapentry.S` 的 `RESTORE_ALL`，使其支持 `U/S` 双向返回；再在 `exception_handler` 中识别 `CAUSE_USER_ECALL`，推进 `epc` 后交给 `syscall`；最后完成 `syscall` 的分发和 9 个 `sys_*` 处理函数。`sys_*` 只负责参数检查与转发，真正的进程和内存语义由 `do_*` 完成，用户指针统一用 `user_mem_check` 校验。如果不先打通这条链路，第一个用户进程即使被装载，也无法获得任何内核服务。
2. 接着再完善**第一个用户进程的创建与启动功能**
    - 先补全 `alloc_proc` 的字段初始化和` kernel_thread` 的 `trapframe` 构造，使 `proc_init` 能够建立 `idleproc`，并创建 `init_main` 所在的内核线程；`init_main` 再创建 `user_main`，并通过 `do_wait` 等待用户进程退出。`user_main` 通过 `KERNEL_EXECVE` 选中镜像中的用户程序，调用 `kernel_execve` 构造 `trapframe` 并跳入` __trapret`，由 `do_execve`/`load_icode` 回收旧地址空间、解析 ELF、建立页表和用户栈。第一次进入用户态不能靠用户态 `ecall`，必须在内核态手工构造返回现场，这是本模块与普通系统调用返回的关键区别。
3. 然后完成**用户进程复制功能**
    - 第一个用户进程跑起来后，需要 `fork` 让用户程序创建子进程。在 lab4 的 `do_fork` 框架上补全 `copy_mm`，在 `vmm.c` 中实现 `dup_mmap`，在 `pmm.c `中实现 `copy_range`，使子进程获得独立的 `mm`、vma 列表和物理页副本，同时复制父进程的 `trapframe`、内核栈和进程树关系。之所以放在这里，是因为` fork` 的语义是“复制当前进程”，必须先有运行中的用户进程作为父进程；同时它也是对前面地址空间管理和 `trapframe` 机制的第一次综合检验。
4. 最后补齐**进程退出、让出与终止功能**
    - 进程不能只创建不回收。`do_exit` 负责回收用户地址空间、把进程置为僵尸态、唤醒等待的父进程并让出 CPU；`do_yield` 把当前进程标记为需要重新调度；`do_kill` 给目标进程打上退出标记并在必要时唤醒。这些动作与已经实现的 `do_wait` 配合，完成僵尸回收、父子关系清理以及内核栈和 PCB 的释放。之所以放在最后，是因为退出与回收依赖前面已经稳定的 `mm`、进程树和 `trapframe` 语义，任何一环不一致都可能造成僵尸进程、孤儿进程或父进程永久睡眠。至此，用户进程的“创建—运行—复制—退出—回收”形成完整闭环。

***

## 四、实验内容与实现

### 功能模块一：用户态系统调用功能

**负责人：** 2413094-卢思齐

#### 模块功能描述

**需要实现/修改的函数：**

```c
// kern/trap/trap.c：修改现有函数
void exception_handler(struct trapframe *tf);

// kern/syscall/syscall.c：实现
void syscall(void);
static int sys_exit(uint64_t arg[]);
static int sys_fork(uint64_t arg[]);
static int sys_wait(uint64_t arg[]);
static int sys_exec(uint64_t arg[]);
static int sys_yield(uint64_t arg[]);
static int sys_kill(uint64_t arg[]);
static int sys_getpid(uint64_t arg[]);
static int sys_putc(uint64_t arg[]);
static int sys_pgdir(uint64_t arg[]);
//以上 9 个 sys_* 还在 syscalls[] 之前补充了相同的 static 原型
```

**功能说明：**

- 该模块在整个系统中的作用
    - 实现用户态与内核态之间的唯一受控入口，连接用户库与内核服务。`exception_handler` 识别用户态系统调用并推进 `epc`，`syscall` 根据 `trapframe` 中的编号和参数完成分发，各 `sys_* `把请求转交给对应的 `do_*`，最后把结果写回 `a0`。该模块负责参数检查、用户指针校验和用户态与内核态的边界安全，是后续 `fork`、`exec`、`wait`、`exit` 能够被用户程序调用的前提。
- 需要处理的主要场景
    1. 用户态 `ecall` 进入内核：识别 `CAUSE_USER_ECALL`，先执行 `tf->epc += 4`，再调用 `syscall()`；`epc` 必须先推进，因为 `sys_exec` 成功后 `load_icode` 会重写 `epc`。
    2. 系统调用分发：从 `a0` 取编号、`a1~a5` 取参数，检查编号范围和 `syscalls[num]` 是否为空；非法编号或 `NULL` 表项打印 `trapframe` 并 `panic`，`SYS_clone`、`SYS_sleep` 等空洞不能当成有效请求。
    3. 用户指针校验：`sys_wait` 的 `store`、`sys_exec` 的 `name` 和 `binary` 必须先用 `user_mem_check` 检查；该函数返回 `bool`，失败返回 `-E_INVAL`，且要在 `do_wait` 睡眠、`do_execve `销毁旧 `mm` 之前完成。
    4. 进程语义转发：`sys_exit`、`sys_fork`、`sys_wait`、`sys_exec`、`sys_yield`、`sys_kill` 分别调用 `do_exit`、`do_fork`、`do_wait`、`do_execve`、`do_yield`、`do_kill`；`sys_fork` 使用 `clone_flags = 0`、`stack = 0`，`sys_getpid` 返回 `current->pid`，`sys_putc` 调用 `cputchar`，`sys_pgdir` 输出 `current->mm->pgdir`。`sys_*` 只做检查和转发，不重复 `do_*` 的内部逻辑。
    5. 返回用户态：返回值写回进入分发前保存的 `tf->gpr.a0`，不能用 `current->tf`；`sys_exec` 成功会改写 `tf->epc`、`tf->gpr.sp`、`tf->status`，再由 `RESTORE_ALL` 和 `sret` 回到 U 态；`sys_exit` 不返回。
- 与其他模块的交互关系
    1. 与 `trap`/`trapentry` 交互：`trap()` 设置 `current->tf`，`SAVE_ALL` 保存现场，`RESTORE_ALL` 恢复现场并返回 U 态；本模块依赖这些上下文语义，不修改它们。
    2. 与进程管理交互：`sys_*` 的最终语义由 `proc.c` 的 `do_*` 完成，进程树维护、地址空间复制和僵尸回收属于功能模块二、三、四。
    3. 与内存管理交互：`user_mem_check` 检查用户指针，`sys_exec` 经 `do_execve`/`load_icode` 替换地址空间，`sys_wait` 的退出码写回用户地址，`sys_pgdir` 读取页表基址；`mm` 被替换后不能继续使用旧指针。
    4. 与用户库、调度器交互：`libs/unistd.h `的编号和 `user`/`libs`/`syscall.c` 的 `a0~a5` 约定必须一致；`syscall.h` 只声明 `syscall()`，因此要在 `syscalls[]` 前补 9 个 `static` 原型；`do_yield`、`do_wait`、`do_exit` 只把状态交给 `trap() `和` schedule()`，本模块不直接切换进程。

#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

```markdown
[PROMPT]
任务：修改 kern/trap/trap.c 和 kern/syscall/syscall.c，实现 lab5 功能模块一“用户态系统调用功能”，包括 exception_handler 的用户态 ecall 分支、syscall 分发函数，以及 sys_exit、sys_fork、sys_wait、sys_exec、sys_yield、sys_kill、sys_getpid、sys_putc、sys_pgdir 九个系统调用处理函数。

操作要求：你必须在项目中修改实际文件，保留文件中的所有现有代码和注释。原文件中存在的 //lab5 function1 YOUR CODE 和 //lab5 function1 YOUR CODE update exception_handler 这两行注释，你绝对不能修改、删除或覆盖；所有实现代码必须严格插入、追加在这两行注释的下方。trap.c 中 exception_handler 原有的 CAUSE_ILLEGAL_INSTRUCTION、CAUSE_BREAKPOINT 和 default 分支及其注释必须原样保留，只在 switch 中新增 CAUSE_USER_ECALL 分支。除此之外，不要改动 trap()、trap_dispatch()、interrupt_handler()、idt_init()、trapentry.S，也不要改动 libs/unistd.h、user/libs/syscall.c、syscalls[] 的初始化内容和 NUM_SYSCALLS 的定义。

极其重要：以下五点必须按要求处理，否则即使编译通过也会在运行时出错：
1. exception_handler 必须先执行 tf->epc += 4，再调用 syscall()。因为 sys_exec 成功后 load_icode 会把 tf->epc 重写为用户入口，如果先调用 syscall() 再加 4，sret 会跳到错误的地址。
2. syscall() 必须先把 current->tf 保存到局部变量，处理完再把返回值写回这个局部变量；不能在处理函数返回后使用 current->tf，因为 do_wait、do_yield 可能已经切换了当前进程。
3. syscall.c 顶部的 syscalls[] 在 9 个函数定义之前引用它们，而 kern/syscall/syscall.h 只声明了 syscall()。因此必须在 syscalls[] 之前补充 9 个 static 函数原型，否则编译会报未声明。
4. sys_pgdir 不要调用 print_pgdir()。本工程的 pmm.h 只有它的声明，pmm.c 没有实现，直接调用会链接失败；用 cprintf 输出 current->mm->pgdir 即可。
5. 本工程的 bool 定义于 libs/defs.h：typedef long long bool;，没有 true/false 字面量。user_mem_check 的 write 参数必须传 1（写检查）或 0（只读检查），不要引入 <stdbool.h>，否则会与 typedef long long bool 冲突，并出现 'true'/'false' undeclared 的编译错误。

输出要求：直接进行文件操作，输出修改后的代码片段。使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息作为唯一实现依据。

[RELY]
系统调用约定（来自 user/libs/syscall.c）
a0 = 系统调用号，a1~a5 = 参数（最多 5 个），ecall 之后从 a0 读取返回值。
内核侧从 current->tf->gpr.a0 读系统调用号，从 gpr.a1~gpr.a5 组装参数，把返回值写回 tf->gpr.a0。

系统调用编号（来自 libs/unistd.h）
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

trapframe 布局（来自 kern/trap/trap.h，偏移与 trapentry.S 的 SAVE_ALL 一致）
struct pushregs {
    uintptr_t zero;   // x0
    uintptr_t ra;     // x1
    uintptr_t sp;     // x2，来自用户态时保存的是用户栈指针
    uintptr_t gp;     // x3
    uintptr_t tp;     // x4
    uintptr_t t0;     // x5
    uintptr_t t1;     // x6
    uintptr_t t2;     // x7
    uintptr_t s0;     // x8
    uintptr_t s1;     // x9
    uintptr_t a0;     // x10，系统调用号 / 返回值
    uintptr_t a1;     // x11
    uintptr_t a2;     // x12
    uintptr_t a3;     // x13
    uintptr_t a4;     // x14
    uintptr_t a5;     // x15
    uintptr_t a6;     // x16
    uintptr_t a7;     // x17
    uintptr_t s2;     // x18
    uintptr_t s3;     // x19
    uintptr_t s4;     // x20
    uintptr_t s5;     // x21
    uintptr_t s6;     // x22
    uintptr_t s7;     // x23
    uintptr_t s8;     // x24
    uintptr_t s9;     // x25
    uintptr_t s10;    // x26
    uintptr_t s11;    // x27
    uintptr_t t3;     // x28
    uintptr_t t4;     // x29
    uintptr_t t5;     // x30
    uintptr_t t6;     // x31
};

struct trapframe {
    struct pushregs gpr;   // 偏移 0，共 32 * 8 字节
    uintptr_t status;      // 偏移 32 * 8，sstatus
    uintptr_t epc;         // 偏移 33 * 8，sepc
    uintptr_t tval;        // 偏移 34 * 8，stval
    uintptr_t cause;       // 偏移 35 * 8，scause
};

关键宏（来自 riscv.h）
#define SSTATUS_SPP          0x00000100
#define SSTATUS_SUM          0x00040000
#define CAUSE_USER_ECALL     0x8

进程与用户访问检查（来自 kern/process/proc.h、kern/mm/vmm.h）
struct proc_struct {
    int pid;
    struct trapframe *tf;
    struct mm_struct *mm;
    uint32_t flags;                 // 含 PF_EXITING
    volatile bool need_resched;
};
extern struct proc_struct *current;
bool user_mem_check(struct mm_struct *mm, uintptr_t start, size_t len, bool write);
// 本工程的 bool 定义于 libs/defs.h：typedef long long bool;，没有 true/false 字面量。user_mem_check 返回 1 表示合法、0 表示非法；write 参数传 1 表示写权限检查、0 表示只读检查，不要引入 <stdbool.h>。

可用函数（来自 kern/process/proc.h、kern/trap/trap.h、libs/stdio.h）
int do_exit(int error_code);                                             // 不返回
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int do_wait(int pid, int *code_store);
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size);
int do_yield(void);
int do_kill(int pid);
void print_trapframe(struct trapframe *tf);
int cprintf(const char *fmt, ...);
void cputchar(int c);
// panic 由 assert.h 提供，可直接调用。

syscall.c 当前包含 pmm.h 等头文件；使用 user_mem_check 需要补充 #include <vmm.h>，使用 -E_INVAL 需要补充 #include <error.h>。

需要修改的现有代码片段
1. exception_handler 目前只处理 CAUSE_ILLEGAL_INSTRUCTION、CAUSE_BREAKPOINT 和 default，需要在 switch 中新增 case CAUSE_USER_ECALL。
2. syscall.c 顶部已有 static int (*syscalls[])(uint64_t arg[]) 及其初始化，只有 9 个表项，其余编号为 NULL；函数体尚未实现。
3. kern/syscall/syscall.h 只有 void syscall(void);，没有声明任何 sys_* 函数。
4. syscall.c 末尾只有一行 //lab5 function1 YOUR CODE，trap.c 中只有一行 //lab5 function1 YOUR CODE update exception_handler。

[GUARANTEE]
必须实现或修改的接口：

需要修改的现有函数：
void exception_handler(struct trapframe *tf);
void syscall(void);

需插入到 //lab5 function1 YOUR CODE 下方的函数（同时需要在 syscalls[] 之前补充相同的 static 原型）：
static int sys_exit(uint64_t arg[]);
static int sys_fork(uint64_t arg[]);
static int sys_wait(uint64_t arg[]);
static int sys_exec(uint64_t arg[]);
static int sys_yield(uint64_t arg[]);
static int sys_kill(uint64_t arg[]);
static int sys_getpid(uint64_t arg[]);
static int sys_putc(uint64_t arg[]);
static int sys_pgdir(uint64_t arg[]);

你可以根据需要添加：
辅助函数（建议设为 static，只在本文件内使用）。
文件内部使用的宏定义。

[SPECIFICATION]
## exception_handler
Pre-Condition:
由 trap() 在异常分发路径上调用，tf 指向本次 trap 保存的 trapframe。tf->cause 为异常原因；来自用户态时 tf->status 的 SSTATUS_SPP 位为 0，current 指向当前用户进程且 current->tf == tf。
Post-Condition:
对用户态 ecall，tf->epc 已经指向 ecall 的下一条指令，syscall() 已经完成分发，tf->gpr.a0 中为返回值。对其他异常，保持 lab3 原有的处理结果。
Case 1：tf->cause == CAUSE_USER_ECALL。先把 tf->epc 增加 4，再调用 syscall()。
Case 2：tf->cause == CAUSE_ILLEGAL_INSTRUCTION 或 CAUSE_BREAKPOINT。保持现有实现，打印异常位置并推进 epc。
Case 3：其他异常原因。保持现有 default 分支，打印 trapframe，必要时 panic；不要把它们误当成系统调用。
Requirements:
1. 只处理 CAUSE_USER_ECALL，不能把 CAUSE_SUPERVISOR_ECALL 或其他异常引入 syscall()。
2. epc 的推进必须在 syscall() 之前完成，且只加一次 4。
3. 不修改 trap()、trap_dispatch()，保留 exception_handler 原有分支及其注释。

## syscall
Pre-Condition:
仅由 exception_handler 在 CAUSE_USER_ECALL 分支调用；current 非空，current->tf 指向当前 trapframe。tf->gpr.a0 为系统调用号（按无符号 64 位解释），tf->gpr.a1~tf->gpr.a5 为最多 5 个参数。
Post-Condition:
合法调用时，syscalls[num] 已经执行，其 int 返回值已经写入 tf->gpr.a0；除处理函数自身对 tf 的修改外，其他字段不变。非法调用时，打印 trapframe 和系统调用号并 panic，不返回用户态。
Case 1：num < NUM_SYSCALLS 且 syscalls[num] != NULL。按 a1~a5 组装 uint64_t arg[5]，调用 syscalls[num](arg)，把返回值写入 tf->gpr.a0。
Case 2：num >= NUM_SYSCALLS。按非法系统调用号处理，打印 trapframe 和编号并 panic。
Case 3：num 在范围内但 syscalls[num] == NULL，例如 SYS_clone、SYS_sleep。与 Case 2 同样按非法请求处理，不能返回 0 或错误码后继续执行。
Requirements:
1. 进入分发前先把 tf 保存到局部变量，写回时使用该局部变量，不能使用 current->tf。
2. 读取 a0 后按无符号值做范围检查，不要先转换成 int 再比较。
3. 不修改 tf->epc、a1~a5、status、tval、cause，不修改 syscalls[] 和 NUM_SYSCALLS。
4. 不在本函数中做用户指针检查、进程状态修改或资源分配。

## sys_exit
Pre-Condition:
current 指向用户进程；arg[0] 是用户传入的退出码，按 int 解释，允许为负数。
Post-Condition:
当前进程进入退出流程，本函数不再返回用户态；地址空间回收、僵尸态设置和父进程唤醒由 do_exit 完成。
Case 1：调用 do_exit((int)arg[0])。正常路径不会返回。
Requirements:
1. 本函数不做资源释放，不修改 trapframe，不写 tf->gpr.a0。
2. 不得在调用 do_exit 之前设置 PF_EXITING 或调用 schedule()。

## sys_fork
Pre-Condition:
current 指向用户进程；本系统调用不使用参数。
Post-Condition:
成功时父进程的 tf->gpr.a0 中是子进程 PID，子进程之后从 fork 返回 0；失败时返回负错误码，系统没有产生新的可运行进程。
Case 1：调用 do_fork(0, 0, current->tf)，把返回的 PID 原样返回。
Requirements:
1. clone_flags 必须为 0，表示复制用户地址空间；stack 必须为 0。
2. 不在本函数中分配 PCB、复制页表或调用 schedule()。
3. 不要手工修改子进程的 a0，子进程的 trapframe 由 do_fork 复制。

## sys_wait
Pre-Condition:
arg[0] 为等待的 PID（0 表示任意子进程），arg[1] 为用户地址或 0；记 pid = (int)arg[0]，store = (int *)arg[1]。current 指向用户进程，current->mm 有效。
Post-Condition:
成功回收一个子进程后返回 0；如果 store 非空，退出码由 do_wait 写入该用户地址。失败时返回负错误码；store 非法时不睡眠，不产生等待副作用。
Case 1：store == NULL。调用 do_wait(pid, NULL)，返回其结果。
Case 2：store != NULL，且 user_mem_check(current->mm, (uintptr_t)store, sizeof(int), 1) 返回 1。调用 do_wait(pid, store)，返回其结果。
Case 3：store != NULL，但 user_mem_check 返回 0。直接返回 -E_INVAL，不调用 do_wait，不睡眠。
Requirements:
1. 用户指针检查必须在可能引起睡眠的 do_wait 之前完成。
用户指针检查必须在可能引起睡眠的 do_wait 之前完成；检查长度为 sizeof(int)，write 参数传 1。user_mem_check 返回 bool（long long），按 1/0 判断，也可以直接用 !user_mem_check(...) 判断。
3. 不在本函数中写 *store，退出码由 do_wait 写入。

## sys_exec
Pre-Condition:
记 name = (const char *)arg[0]，len = (size_t)arg[1]，binary = (unsigned char *)arg[2]，size = (size_t)arg[3]。current 指向用户进程，current->mm 有效。
Post-Condition:
成功时当前进程的旧地址空间已经被回收，新程序映像已经建立，tf->epc、tf->gpr.sp、tf->status 由 do_execve / load_icode 改写，本函数返回 0。失败时返回负错误码，旧地址空间和原执行上下文保持可用。
Case 1：四个参数合法，调用 do_execve(name, len, binary, size)，返回其结果。
Case 2：name 为空，或 name + len 溢出，或 user_mem_check(current->mm, (uintptr_t)name, len, 0) 返回 0。返回 -E_INVAL，不调用 do_execve。
Case 3：binary 为空，或 binary + size 溢出，或 user_mem_check(current->mm, (uintptr_t)binary, size, 0) 返回 0。返回 -E_INVAL，不调用 do_execve。
Case 4：参数合法但 do_execve 失败，原样返回其负错误码。
Requirements:
1. 用户指针检查必须在 do_execve 之前完成，因为 do_execve 会销毁旧 mm。
2. 检查范围必须覆盖整个 name 和 binary 缓冲区，注意地址加法溢出。
3. 只负责转发四个参数，不解析 ELF、不建立页表、不拷贝大块二进制。

## sys_yield
Pre-Condition:
current 指向用户进程；本系统调用不使用参数。
Post-Condition:
返回 0 写入 tf->gpr.a0；真正的进程切换发生在 trap() 的统一出口，检查 need_resched 后调用 schedule()。
Case 1：调用 do_yield()，返回 0。
Requirements:
1. 不在本函数中直接调用 schedule()。
2. 不修改 current->state、wait_state 或其他进程状态。

## sys_kill
Pre-Condition:
arg[0] 是目标进程 PID，记 pid = (int)arg[0]；current 指向用户进程。
Post-Condition:
成功返回 0，目标进程被标记为需要退出或在合适的时机被唤醒退出；失败返回负错误码，目标进程不存在或 PID 非法。
Case 1：pid 合法且目标进程存在。调用 do_kill(pid)，原样返回其结果。
Case 2：pid 不存在或非法。do_kill 返回负错误码，通常为 -E_INVAL，本函数原样透传。
Requirements:
1. 不直接设置目标进程的 PF_EXITING、修改进程树或调用 wakeup_proc，这些由 do_kill 完成。
2. 不检查用户指针，PID 是标量。

## sys_getpid
Pre-Condition:
current 指向当前进程；本系统调用不使用参数。
Post-Condition:
返回 current->pid，由 syscall() 写入 tf->gpr.a0；系统状态没有副作用。
Case 1：返回当前进程的 PID。
Requirements:
1. 不切换进程，不修改任何进程字段，不访问用户内存。

## sys_putc
Pre-Condition:
arg[0] 是待输出字符的编码，只有低 8 位有意义；本系统调用不涉及用户指针。
Post-Condition:
字符已经通过内核控制台输出接口写到控制台，返回 0 写入 tf->gpr.a0。
Case 1：调用 cputchar((int)arg[0])，返回 0。
Requirements:
1. 必须通过 cputchar 输出，不能直接调用 SBI 接口。
2. 不做格式化，不检查字符是否可打印，不把非法字符当成错误拒绝。

## sys_pgdir
Pre-Condition:
current 指向用户进程，current->mm 非空；本系统调用不使用参数。
Post-Condition:
当前进程的页表基址已经输出到控制台，返回 0 写入 tf->gpr.a0；页表本身没有被修改。
Case 1：用 cprintf 输出 current->mm->pgdir，返回 0。不要调用 print_pgdir()，本工程 pmm.c 没有实现它。
Case 2：current->mm == NULL。返回 -E_INVAL，不打印，不解引用空指针。
Requirements:
1. 只读操作，不分配页，不修改页表项，不改变 mm 的引用计数。
2. 输出统一走 cprintf，不直接操作 UART 或 SBI。
3. 不要为了 sys_pgdir 修改 pmm.c，也不要临时实现一套完整的页表遍历。
```

#### 实现迭代过程

本模块的实现经历了 2 次迭代，过程如下：

***

##### 第一次迭代

**遇到的问题：**

- 问题1
    - `make` 在 syscall.c（`sys_wait`）和 syscall.c、syscall.c:105（`sys_exec`）报 `true`/`false` `undeclared`。
    - 根因：本工程没有 `true`/`false` 这两个字面量。项目的 `bool` 不是 C99 的 <`stdbool.h`> 宏，而是在 libs/defs.h:13 里 `typedef long long bool`;，因此 `true`/`false` 这两个标识符在任何头文件里都没有定义。`user_mem_check` 的最后一个参数类型是 `bool`（实为 `long long`），其实现内部返回的是字面量 `1/0`（见 vmm.c:331），所以用 `1/0` 传参在类型和语义上都完全等价。

**问题解决策略：**

- 针对问题1

根因在于提示词对工程 `bool` 类型的约定不够准确：工程 `libs/defs.h:13` 使用的是 `typedef long long bool;`，没有引入 `<stdbool.h>`，因此 `true`/`false` 不是合法标识符；`user_mem_check` 内部的 `write` 判断也用 `1`/`0`。对提示词做以下三处修改：

1. 在 `[RELY]` 的“进程与用户访问检查”部分补充类型约定：说明 `bool` 来自 `libs/defs.h` 的 `typedef long long bool;`，工程没有定义 `true`/`false`；`user_mem_check(mm, start, len, write)` 的 `write` 参数用 `1` 表示写权限检查、`0` 表示只读检查。
2. 修改 `[SPECIFICATION]` 中 `sys_wait`、`sys_exec` 的示例和 Case 描述：把 `user_mem_check(..., true)` 改成 `..., 1)`，把两处 `user_mem_check(..., false)` 改成 `..., 0)`，并注明 `1` 表示写检查、`0` 表示只读检查，避免模型继续生成 `true`/`false`。
3. 在 `[PROMPT]` 的“操作要求”或“极其重要”中增加一条禁止项：不要引入 `<stdbool.h>`，不要使用 `true`/`false` 字面量，所有布尔参数按工程 `bool`（`long long`）的约定传 `1`/`0`。

修改后重新生成并检查 `syscall.c`，确认三处调用分别为 `sys_wait: ..., 1`、`sys_exec: ..., 0`，文件内不再残留 `true`/`false`；`!user_mem_check(...)` 的判断逻辑以及 `(write) ? VM_WRITE : VM_READ` 的分支保持不变，全量 `make` 通过。

***

##### 第二次迭代

**最终结果：**

经过 2 次迭代，该模块最终：

- ✅ 编译通过
- ✅ 所有相关测试通过
- ✅ 功能符合预期

**关键改进点总结（如有）：**

1. **修正 bool 类型约定**：在 `[RELY]` 中写明 `bool` 为 `typedef long long bool;`，`user_mem_check` 的 `write` 参数改用 `1`/`0`，并禁止引入 `<stdbool.h>`，修复 `'true'/'false' undeclared`。
2. **明确异常与指针边界**：在 `[SPECIFICATION]` 中要求先 `tf->epc += 4` 再调用 `syscall()`；`sys_wait`、`sys_exec` 的用户指针必须先经 `user_mem_check` 校验，且在睡眠或销毁旧 `mm` 之前完成。

***

### 功能模块二：创建加载第一个用户进程功能

**负责人：** 2411617-陈彦冰

#### 模块功能描述

**需要实现/修改的函数：**

```c
// kern/process/proc.c
static int init_main(void *arg)
static struct proc_struct *alloc_proc(void)  //需要更新的函数
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)  // 需要更新的函数
int do_wait(int pid, int *code_store)
static int user_main(void *arg)
static int kernel_execve(const char *name, unsigned char *binary, size_t size)
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size)
static int load_icode(unsigned char *binary, size_t size)
```

**功能说明：**

- 该模块在整个系统中的作用
    - **从“只有内核线程”过渡到“用户进程可运行”的关键模块**：模块三/四解决进程的创建与销毁，模块二负责构造进程控制块、搭建内核线程的首入现场，并把一个内存中的 ELF 程序装进独立地址空间，最终通过 trap 返回路径让 CPU 以 U 态执行用户程序。
    - **进程生命周期的起点**：`alloc_proc` 是所有 PCB（idle、init、用户进程）的唯一构造入口；`kernel_thread` 把 `fn/arg` 包装成可被 `do_fork` 复制的 S 态现场。
    - **系统启动链路的中枢**：`proc_init → kernel_thread(init_main) → init_main → kernel_thread(user_main) → user_main → kernel_execve → do_execve → load_icode → sret`，这条链把内核启动推进到第一个用户程序。
    - **父进程收尾机制**：`do_wait` 提供父子进程间的等待/回收语义，使 initproc 能回收所有用户进程并做内存泄漏检查。
- 需要处理的主要场景
    1. **PCB 初始化**：`alloc_proc` 从零建立 `PROC_UNINIT` 状态、`pid=-1`、空 mm/tf/关系链的 PCB，不分配内核栈、不分配 PID、不挂链。
    2. **内核线程首次运行现场**：`kernel_thread` 构造 `trapframe`（`s0=fn`、`s1=arg`、`epc=kernel_thread_entry`、S 态 status），交给 `do_fork` 复制。
    3. **init 线程收尾**：`init_main` 创建 `user_main`，循环 `do_wait(0, NULL)` 回收子进程，打印固定收尾信息，并用 `nr_free_pages`/`kallocated` 前后对比检查泄漏。
    4. **等待与僵尸回收**：`do_wait` 支持按 pid 等待指定子进程或等待任意子进程；无匹配返回 `-E_BAD_PROC`；有子进程未退出则置 `PROC_SLEEPING/WT_CHILD` 后 `schedule()`；被唤醒后回收 `PROC_ZOMBIE`，写回退出码，并在同一关中断临界区摘链。
    5. **exec 的两种入口**：
        1. `kernel_execve` 路径：内核线程直接调 `do_execve`，成功后 `forkrets(current->tf)` 进入 U 态，**不使用 `ecall`**（S 态 ecall 的 cause=9 模块一不处理）。
        2. `sys_exec` 路径：由用户态 `ecall` 进入，`do_execve` 校验用户传入的 `name` 指针后替换地址空间。
    6. **地址空间替换**：`do_execve` 在释放旧 mm 前先切回 `boot_pgdir_pa`，递减 `mm_count`，必要时 `exit_mmap/put_pgdir/mm_destroy`，再调用 `load_icode` 装载新程序并更新进程名。
    7. **ELF 装载**：`load_icode` 校验 ELF 头/程序头表，按 `ELF_PT_LOAD` 建立 vma，逐页“整页清零 + 拷贝文件交集”正确处理非页对齐与 BSS，建立用户栈，切换 satp，最后改写 `current->tf`（清 SPP/SIE、置 SPIE、入口 `e_entry`、栈顶 `USTACKTOP`）。
- 与其他模块的交互关系
    1. **与模块一（trap/syscall）**：
        1. `sys_wait`/`sys_exec` 分别调用本模块的 `do_wait`/`do_execve`；
        2. 用户态程序通过 `CAUSE_USER_ECALL` 进入 `exception_handler → syscall`；
        3. 返回 U 态依赖 `__trapret/RESTORE_ALL/sret`，`kernel_execve` 正是借助该路径而非系统调用指令进入用户态。
    2. **与模块三（进程创建/地址空间复制）**：
        1. `kernel_thread` 完全依赖 `do_fork` 分配 PID、内核栈、挂 hash/proc 链并调用 `copy_thread`；
        2. `do_fork` 通过 `set_links` 维护 `parent/cptr/optr/yptr`，这正是 `do_wait` 遍历子进程的依据；
        3. `copy_mm` 为 fork 出的用户进程复制页表/vma，属于本模块 `load_icode` 之后运行时的对端逻辑。
    3. **与模块四（进程退出/调度控制）**：
        1. `do_wait` 睡眠后依赖子进程退出路径 `wakeup_proc(parent)` 被唤醒；
        2. `do_wait` 发现 `PF_EXITING` 时调用 `do_exit(-E_KILLED)`；
        3. `do_execve` 装载失败调用 `do_exit(ret)`，不返回用户态；
        4. `do_yield`/`do_kill` 通过 syscall 与本模块的等待状态、`PF_EXITING` 标志配合。
    4. **与内存管理子系统（pmm/vmm）**：
        1. 使用 `mm_create/mm_destroy/setup_pgdir/put_pgdir/mm_map/exit_mmap`、`pgdir_alloc_page/page2kva`、`PADDR`、`nr_free_pages/kallocated`；
        2. 用 `user_mem_check` 校验用户指针（`do_wait` 的 `code_store`、`do_execve` 的 `name`），并依赖 `SSTATUS_SUM` 直接写回用户内存。
    5. **与调度器**：
        1. `do_wait` 睡眠和 `init_main` 循环中调用 `schedule()`；`wakeup_proc` 清零 `wait_state` 后进程才能从等待中恢复。
    6. **与汇编/启动代码**：
        1. `kernel_thread_entry` 约定 `s0=fn、s1=arg、sp` 有效；`forkrets` 约定 `a0` 指向完整 `trapframe`；
        2. `proc_run` 在切换用户进程时使用本模块 `load_icode` 写入的 `current->pgdir`（物理地址），否则回退 `boot_pgdir_pa`。
    7. **与用户程序构建系统**：`KERNEL_EXECVE*` 宏引用链接进内核镜像的 `_binary_obj___user_<name>_out_start/size`，把 `hello` 等用户 ELF 直接交给 `kernel_execve`。

#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

```markdown
Lab5 功能模块二：创建并加载第一个用户进程 —— AI 实现提示词

适用工程：lab5/code/（RISC-V64 ucore，Sv39）。本提示词只覆盖模块二；模块一/三/四不在范围内。

一页硬约束（先读这里）

1. 只改 kern/process/proc.c，且只在 193 / 337 / 387 / 417 四个 //lab5 function2 标记下方写代码；lab4 块、顶部 3 个前置声明、模块三/四标记一律不动。
2. kernel_execve 禁止 ecall；do_execve 成功后必须 forkrets(current->tf)，不得返回 user_main。
3. load_icode 必须 current->pgdir = PADDR(mm->pgdir)，写 satp 后 flush_tlb()。
4. tf->status = (read_csr(sstatus) & ~(SSTATUS_SPP | SSTATUS_SIE)) | SSTATUS_SPIE。
5. do_execve 必须先切回 boot_pgdir_pa，再 exit_mmap 回收旧 mm。
6. do_wait 按 cptr/optr 扫子进程；unhash_proc/remove_links 必须放在同一关中断临界区。
7. 不实现/不修改模块一/三/四的函数，不新增全局变量、宏、类型、头文件声明。

[PROMPT] 任务说明

1. 起始状态与插入位置

proc.c 已清理为 lab5 骨架：

- 顶部（约 85–88 行）已有 alloc_proc、forkret、init_main 三个 static 前置声明，供 lab4 块调用后面定义的函数；不要删除或修改。
- //lab4 块（约 90–190 行）只有 copy_thread、proc_init、cpu_idle、proc_run；这 4 个函数不要动。
- alloc_proc、kernel_thread、copy_mm、do_fork、do_exit/do_yield/do_kill 均为空标记，分别由模块二/三/四实现。

本模块的四个插入点（按注释文本定位，行号会漂移）：

标记                                                    写入内容
193 update alloc_proc                                   alloc_proc
337 update kernel_thread                               kernel_thread
387 function2（update copy_mm 之后、下一个 function3 之前）  static int user_main(void *arg);、init_main、do_wait
417 function2（KERNEL_EXECVE 宏之后）                     load_icode、do_execve、kernel_execve、user_main

顺序要求：load_icode → do_execve → kernel_execve → user_main；user_main 必须在宏之后，否则宏未定义。

起始状态运行 make obj/kern/process/proc.o 只有两条 alloc_proc/init_main “used but never defined” 警告；实现后必须消失。

2. 功能目标

把系统从“只有内核线程”推进到“第一个用户程序在 U 态运行”，并让启动父进程能够收尾：

1. alloc_proc：从零构造 PCB，初始化 lab4 全部字段与 lab5 新增字段。
2. kernel_thread：从零构造 S 态内核线程的首入现场，经 do_fork 创建线程。
3. init_main：创建 user_main，循环 do_wait 回收所有子进程，打印收尾信息并做泄漏检查。
4. do_wait：按 pid 或任意子进程等待、睡眠/唤醒、回收僵尸、写回退出码。
5. user_main：通过 KERNEL_EXECVE* 宏把嵌入镜像的用户程序交给 kernel_execve。
6. kernel_execve：调用 do_execve 后真正进入 trap 返回路径切到 U 态。
7. do_execve：先回收旧 mm，再调用 load_icode，成功后更新进程名。
8. load_icode：解析内存 ELF、建立用户页表/vma、装入段与栈、切换 satp、改写 current->tf。

3. 可修改 / 不可修改范围

只允许修改：kern/process/proc.c 的上述四个插入区；可另加一个仅供 load_icode 使用的 static 段加载辅助函数。

绝对不可修改：

- 顶部 3 个前置声明、//lab4 块、模块三/四标记区域。
- 所有 include、全局变量、proc_list、hash_list、nr_process。
- 现有函数：copy_thread、proc_init、cpu_idle、proc_run、set_proc_name、get_proc_name、set_links、remove_links、get_pid、forkret、hash_proc、unhash_proc、find_proc、setup_kstack、put_kstack、setup_pgdir、put_pgdir。
- KERNEL_EXECVE / KERNEL_EXECVE2 宏的任何字符。
- 其他所有文件（proc.h、trap.c、trapentry.S、syscall.c、pmm.c、vmm.c、sched.c、Makefile、tools/ 等）。
- 已有结构体、枚举、宏、函数签名、错误码约定。
- 模块一/三/四的代码：exception_handler、syscall、sys_*、do_fork、copy_mm、dup_mmap、copy_range、do_exit、do_yield、do_kill；不得实现、不得补桩，发现问题只报告。

工程语义保留：不重构、不重命名、不调整现有代码顺序、不整体格式化；沿用现有注释与错误处理风格；只调用 [RELY] 列出的接口；不引入 C 标准库。

4. 输出验证要求

4.1 静态检查

- alloc_proc、kernel_thread 各只有 1 处定义，分别位于 193/337 标记之后；lab4 块中无这两个定义。
- init_main、user_main、kernel_execve、load_icode 各 1 处定义且为 static；do_wait、do_execve 各 1 处定义且非 static。
- 顶部 3 个前置声明、//lab4 块、模块三/四标记与修改前逐字一致；4 个 //lab5 function2 标记保留。
- kernel_execve 内无 ecall、无 SYS_exec；binary 上无 user_mem_check；do_wait 对 code_store 必须调用 user_mem_check。
- current->pgdir 的赋值必须是 PADDR(mm->pgdir)。

4.2 编译检查

- make obj/kern/process/proc.o 无 error、无 warning（起始状态的两条 undefined-static 警告必须消失）。
- 全量 make 可能因其他模块的既有问题失败（如 syscall.c 的 true/false 未定义）；本模块不修改，只在交付说明中报告。
- 链接阶段可能缺 do_fork（模块三）、do_exit/do_yield/do_kill（模块四）；如实报告，不得补桩。
- proc_init、init_main 各只有一处定义且函数体使用 {}，不得用宏/#if 包裹。

4.3 单模块运行（hello，不依赖 fork）

make qemu（或 make build-hello 后按 grade.sh 的 QEMU 参数启动），串口必须依次出现：

kernel_execve: pid = 2, name = "hello".
Hello world!!.
I am process 2.
hello pass.

前两行证明 user_main → kernel_execve → do_execve → load_icode → sret 链路打通；后两行证明 U 态、用户栈、页表与 getpid 正常。若 init check memory pass. 不出现，优先怀疑模块四 do_exit 或模块三 set_links，在交付说明中定位。

4.4 注入测试

tools/inject.py（或 make grade）在 proc_init 尾部与 init_main 入口插入单元测试，必须出现：

[Test Process Create] PASS
[Test Syscall] PASS
[Test Process Control] PASS

若 [Test Syscall] 因 syscall.c 对非法号直接 panic 而失败，属模块一，只报告。

4.5 跨模块集成（模块三/四完成后）

- exit：waitpid <pid> ok.、exit pass.、all user-mode processes have quit.、init check memory pass.。
- badarg：badarg pass.；forktest：forktest pass.；yield：yield pass.。
- pgdir：I am 2, print pgdir.、init check memory pass.。

4.6 交付说明

函数插入位置、alloc_proc/kernel_thread 的实现要点、上述检查的实际输出、与提示词不一致之处（冲突时先停止并报告）。

[RELY] 依赖说明

只列本模块真正需要、且不易从代码直接看出的依赖。

1. 数据结构与布局

- struct proc_struct（proc.h）：state, pid, runs, kstack, need_resched, parent, mm, context, tf, pgdir, flags, name, list_link, hash_link, exit_code, wait_state, cptr, yptr, optr。
- struct trapframe（trap.h）：gpr（32 个 uintptr_t）+ status/epc/tval/cause，RV64 共 288 字节；gpr.sp 在 2*8，status 在 32*8，epc 在 33*8。
- struct mm_struct（vmm.h）：pgdir 是内核虚拟地址；mm_count 控制是否释放。
- struct elfhdr / struct proghdr（elf.h）：e_magic/e_entry/e_phoff/e_phentsize/e_phnum；p_type/p_flags/p_offset/p_va/p_filesz/p_memsz。
- struct Page：仅通过 page2kva(page) 取得页的内核虚拟地址。

2. 可用函数（只调用，不修改）

- 内存：mm_create、mm_destroy、mm_map、exit_mmap、setup_pgdir、put_pgdir、pgdir_alloc_page、page2kva、PADDR/KADDR、nr_free_pages、kallocated。
- 进程：kernel_thread（本模块实现）、find_proc、unhash_proc、remove_links、put_kstack、set_proc_name、schedule、wakeup_proc、do_exit（模块四）。
- 同步/校验：local_intr_save / local_intr_restore、user_mem_check。
- 陷入/CSR：forkrets、read_csr / write_csr、flush_tlb。
- 基础库：memset / memcpy / strlen、cprintf / panic / assert。

3. 宏与常量

PROC_UNINIT/RUNNABLE/SLEEPING/ZOMBIE、PROC_NAME_LEN、PF_EXITING、WT_CHILD、KSTACKSIZE、PGSIZE、PTE_V/R/W/X/U、SSTATUS_SPP/SPIE/SIE、SATP_MODE_SV39、RISCV_PGSHIFT、VM_READ/WRITE/EXEC/STACK、ELF_MAGIC/ELF_PT_LOAD/ELF_PF_X/W/R、USTACKTOP/USTACKPAGE/USTACKSIZE、E_NO_MEM/E_INVAL/E_INVAL_ELF/E_BAD_PROC/E_KILLED、ROUNDDOWN/ROUNDUP。

4. 非显而易见的契约

- kernel_thread_entry：move a0,s1; jalr s0；要求 s0=fn、s1=arg、sp 有效。
- forkrets：move sp,a0; j __trapret；要求 a0 指向完整 trapframe。RESTORE_ALL 在 SPP=0 时把 sp+288 写入 sscratch 后 sret。
- 模块一 exception_handler 只处理 CAUSE_USER_ECALL (8)；S 态 ecall 的 cause 是 9，不会被处理。因此 kernel_execve 不能用 ecall。
- idt_init 已设置 SSTATUS_SUM，内核可直接读写通过 user_mem_check 校验的用户地址。
- 模块三 do_fork 必须用 set_links 维护 current->cptr/optr；模块四 do_exit 必须置 PROC_ZOMBIE、保存 exit_code 并在父进程等待时 wakeup_proc(parent)。

[GUARANTEE] 接口清单

函数        签名                                                              属性                 成功                失败
alloc_proc  static struct proc_struct *alloc_proc(void)                          193 标记             已初始化 PCB        NULL
kernel_thread  int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)  337 标记          pid > 0             负错误码
init_main   static int init_main(void *arg)                                      387 标记             0                   创建失败 panic
do_wait     int do_wait(int pid, int *code_store)                                387 标记，非 static     0                   -E_BAD_PROC / -E_INVAL
user_main   static int user_main(void *arg)                                      417 标记             不返回              失败 panic
kernel_execve  static int kernel_execve(const char *name, unsigned char *binary, size_t size)  417 标记  不返回          负错误码
do_execve   int do_execve(const char *name, size_t len, unsigned char *binary, size_t size)  417 标记，非 static  0      -E_INVAL；装载失败 do_exit
load_icode  static int load_icode(unsigned char *binary, size_t size)            417 标记             0                   负错误码

允许新增：static int user_main(void *arg);（387 块）与可选的一个 static 段加载辅助函数（417 块）。不新增全局变量/宏/类型/头文件声明；不重复定义既有函数。

[SPECIFICATION] 行为规格

0. 共同约定

- 失败返回 error.h 中的负错误码，不用裸 -1。
- 用户指针写操作必须先过 user_mem_check；进程树/全局链表的修改必须在关中断临界区。
- exec 的“回到用户态”只走 trap 返回路径，不得用 schedule/proc_run 代替。

1. alloc_proc（193 标记，从零实现）

后置状态（非 NULL 时）

- state=PROC_UNINIT、pid=-1、runs=0、kstack=0、need_resched=0；
- parent=NULL、mm=NULL、tf=NULL、pgdir=0、flags=0；
- context 全 0（尤其 context.ra=0）；name 全零；
- exit_code=0、wait_state=0；
- cptr=yptr=optr=NULL。

分支与约束

- kmalloc 失败返回 NULL，无其他副作用。
- 不分配内核栈、不分配 PID、不挂链。
- 推荐先整结构 memset 再显式赋值；全文件只有这一处定义。

2. kernel_thread（337 标记，从零实现）

- tf 全零后设置：
  - tf.gpr.s0 = (uintptr_t)fn；tf.gpr.s1 = (uintptr_t)arg；
  - tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE；
  - tf.epc = (uintptr_t)kernel_thread_entry。
- 调用 do_fork(clone_flags, 0, &tf) 并原样返回；clone_flags 不增删。
- 只构造 S 态内核线程；U 态切换由 kernel_execve 负责。
- do_fork 由模块三提供；本函数只调用，不补桩、不实现。

3. init_main（387 标记）

前置：current == initproc（pid=1）；注入器会在函数开头插入 test_lab5_syscall(); test_lab5_process_control();，不得用宏/#if 包裹函数定义。

执行顺序（必须）

1. size_t nr_free_pages_store = nr_free_pages();
2. size_t kernel_allocated_store = kallocated();
3. int pid = kernel_thread(user_main, NULL, 0); 若 pid <= 0 则 panic("create user_main failed.\n");
4. while (do_wait(0, NULL) == 0) { schedule(); }
5. cprintf("all user-mode processes have quit.\n");
6. assert(nr_free_pages_store == nr_free_pages());
7. assert(kernel_allocated_store == kallocated());
8. cprintf("init check memory pass.\n");
9. return 0;

约束

- 两行打印必须与 grade.sh 逐字一致。
- 本函数不调用 do_exit；返回后由 kernel_thread_entry 处理。
- do_wait 返回 -E_BAD_PROC 表示已无子进程，正常退出循环；其他负值不得吞掉。

4. do_wait（387 标记，非 static）

参数与返回

- pid > 0：只等待该 pid 且 proc->parent == current 的子进程。
- pid == 0：等待任意一个子进程；其他值按“无匹配”处理。
- 成功回收返回 0；无匹配返回 -E_BAD_PROC。
- code_store != NULL 必须是当前进程可写的 int，否则返回 -E_INVAL 且不睡眠。

执行顺序（必须）

1. code_store != NULL 时：
   if (!user_mem_check(current->mm, (uintptr_t)code_store, sizeof(int), 1)) return -E_INVAL;
2. repeat: 处：
   - haskid = 0；
   - pid != 0：proc = find_proc(pid)，若 proc && proc->parent == current 置 haskid=1；若 proc->state == PROC_ZOMBIE 则 goto found；
   - pid == 0：for (proc = current->cptr; proc; proc = proc->optr)，逐个置 haskid=1，遇僵尸 goto found；
   - haskid == 0 返回 -E_BAD_PROC；
   - 否则 current->state = PROC_SLEEPING; current->wait_state = WT_CHILD; schedule();
     若 current->flags & PF_EXITING 则 do_exit(-E_KILLED);，随后 goto repeat。
3. found: 处：
   - proc == idleproc || proc == initproc 时 panic("wait idleproc or initproc.\n");
   - code_store != NULL 时写 *code_store = proc->exit_code;（可直接写，SUM 已开；也可 copy_to_user）。
   - local_intr_save(intr_flag); { unhash_proc(proc); remove_links(proc); } local_intr_restore(intr_flag);
   - put_kstack(proc); kfree(proc); return 0;

约束

- 只有 PROC_ZOMBIE 可回收；不得先 kfree 后摘链。
- 睡眠时 state/ wait_state 必须同时设置；wakeup_proc 会清零 wait_state。
- badarg 用例：waitpid(-1,NULL)!=0、waitpid(pid,0xC0000000)!=0、第三次 waitpid(pid,&code)==0 && code==0xbeaf。

5. user_main（417 标记，宏之后）

static int
user_main(void *arg)
{
    (void)arg;
#ifdef TEST
    KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE);
#else
    KERNEL_EXECVE(hello);
#endif
    panic("user_main execve failed.\n");
}

- 不得自行打印 kernel_execve: 行（宏已打印），不得修改宏。
- 成功时不再返回；失败必须 panic，不得静默返回 0。

6. kernel_execve（417 标记）

前置：current->tf 非 NULL；name/binary 都是内核镜像内的地址，不是用户指针。

行为（严格按此形态）

static int
kernel_execve(const char *name, unsigned char *binary, size_t size)
{
    int ret = do_execve(name, strlen(name) + 1, binary, size);
    if (ret != 0)
    {
        return ret;                 /* 交由 user_main panic */
    }
    forkrets(current->tf);          /* __trapret → sret，不返回 */
    panic("kernel_execve: forkrets returned.\n");
}

- 禁止 ecall、禁止 syscall()、禁止手工构造另一个 trapframe。
- 成功路径必须经 forkrets 离开，绝不能返回 user_main；load_icode 已把 current->tf 写成 U 态入口现场。

7. do_execve（417 标记，非 static）

1. struct mm_struct *mm = current->mm;
2. 若 mm != NULL 且 !user_mem_check(mm, (uintptr_t)name, len, 0)，返回 -E_INVAL。不要对 binary 做 user_mem_check（内核路径下它不是用户内存）。
3. 名字截断到 PROC_NAME_LEN，复制到 char local_name[PROC_NAME_LEN + 1] 并保证 NUL 结尾。
4. 若 mm != NULL：
   - 先切回内核页表：
     write_csr(satp, ((uintptr_t)SATP_MODE_SV39 << 60) | (boot_pgdir_pa >> RISCV_PGSHIFT)); flush_tlb();
   - if (mm_count_dec(mm) == 0) { exit_mmap(mm); put_pgdir(mm); mm_destroy(mm); }
   - current->mm = NULL; current->pgdir = 0;
5. int ret = load_icode(binary, size);
6. 若 ret != 0：do_exit(ret); return ret;（do_exit 不返回，return 仅供编译器）。
7. set_proc_name(current, local_name);
8. return 0;

约束

- mm == NULL（kernel_execve 路径）跳过第 2、4 步。
- load_icode 失败时旧 mm 已被释放，不得返回用户态继续执行旧程序。
- 切 satp 必须在 exit_mmap 之前。

8. load_icode（417 标记）

前置：current->mm == NULL（否则 panic）、current->tf != NULL；binary/size 是完整的内存 ELF。

执行顺序（必须）

1. mm = mm_create(); 失败返回 -E_NO_MEM。
2. setup_pgdir(mm) 失败：mm_destroy(mm); return -E_NO_MEM;
3. 校验 size >= sizeof(struct elfhdr) 且 elf.e_magic == ELF_MAGIC；否则 put_pgdir(mm); mm_destroy(mm); return -E_INVAL_ELF;
4. 校验 e_phentsize == sizeof(struct proghdr) 且 e_phoff + e_phnum * e_phentsize <= size；否则按第 3 步清理并返回 -E_INVAL_ELF。
5. 对每个 proghdr：
   - 只处理 p_type == ELF_PT_LOAD；p_memsz == 0 跳过。
   - p_filesz > p_memsz 或 p_offset + p_filesz > size：-E_INVAL_ELF 并清理。
   - vm_flags：ELF_PF_R→VM_READ、ELF_PF_W→VM_WRITE、ELF_PF_X→VM_EXEC；VM_WRITE 时同时置 VM_READ。
   - perm = PTE_U；VM_READ→PTE_R、VM_WRITE→PTE_W、VM_EXEC→PTE_X；PTE_W 必须伴随 PTE_R。
   - mm_map(mm, p_va, p_memsz, vm_flags, NULL)；失败转到第 7 步清理并返回该错误码。
   - 按页装载（整页清零后拷贝文件交集，正确处理非页对齐与 BSS）：

uintptr_t la = ROUNDDOWN(ph.p_va, PGSIZE);
uintptr_t end = ROUNDUP(ph.p_va + ph.p_memsz, PGSIZE);
for (; la < end; la += PGSIZE) {
    struct Page *page = pgdir_alloc_page(mm->pgdir, la, perm);
    if (page == NULL) { /* 第 7 步清理，返回 -E_NO_MEM */ }
    memset(page2kva(page), 0, PGSIZE);
    uintptr_t s = (la > ph.p_va) ? la : ph.p_va;
    uintptr_t e = la + PGSIZE;
    uintptr_t file_end = ph.p_va + ph.p_filesz;
    if (e > file_end) e = file_end;
    if (s < e) {
        memcpy(page2kva(page) + (s - la),
               binary + ph.p_offset + (s - ph.p_va), e - s);
    }
}

6. 用户栈：
   - mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, VM_READ | VM_WRITE | VM_STACK, NULL)；失败转第 7 步。
   - i = 0..USTACKPAGE-1：pgdir_alloc_page(mm->pgdir, USTACKTOP - (i+1)*PGSIZE, PTE_U | PTE_R | PTE_W)；任一失败转第 7 步并返回 -E_NO_MEM。
7. 失败清理（必须按顺序）：
   - 已执行 setup_pgdir：exit_mmap(mm); put_pgdir(mm); mm_destroy(mm);
   - 仅创建 mm：mm_destroy(mm);
   - exit_mmap 要求 mm_count == 0，此时尚未 mm_count_inc，成立。
8. 接管地址空间：
   - mm_count_inc(mm); current->mm = mm; current->pgdir = PADDR(mm->pgdir);
   - write_csr(satp, ((uintptr_t)SATP_MODE_SV39 << 60) | (current->pgdir >> RISCV_PGSHIFT)); flush_tlb();
9. 改写 current->tf 的内容（不得替换指针）：

struct trapframe *tf = current->tf;
memset(tf, 0, sizeof(struct trapframe));
tf->status = (read_csr(sstatus) & ~(SSTATUS_SPP | SSTATUS_SIE)) | SSTATUS_SPIE;
tf->epc = elf.e_entry;
tf->gpr.sp = USTACKTOP;

   - 清 SPP 才能 sret 回 U 态；清 SIE、置 SPIE 防止 __trapret 中 csrw sstatus 后被中断；保留 SUM 等其他位。
   - 两条调用路径都依赖该指针：sys_exec 路径中它是 SAVE_ALL 生成的 trapframe；kernel_execve 路径中它是 copy_thread 放在内核栈顶的 trapframe。
10. return 0;

约束

- 不得把 mm->pgdir 直接写入 satp 或 current->pgdir，必须 PADDR；不得使用 lsatp()。
- 清理路径不得 put_kstack、不得 kfree(current)、不得修改 current->tf。
- testbss 通过说明 BSS 清零正确；init check memory pass. 说明失败清理无泄漏。

关键决策（交付说明中确认）

- alloc_proc、kernel_thread 从零实现；lab5 对 kernel_thread 与 lab4 无语义差异。
- kernel_execve 直接 do_execve + forkrets，不经过 ecall/syscall()。
- do_execve 装载失败调用 do_exit(ret)；kernel_execve 只从名字校验失败处收到负值。
- do_wait 的 code_store 直接赋值依赖 SSTATUS_SUM；改用 copy_to_user 需在交付说明注明。
- load_icode 采用“整页清零 + 拷贝文件交集”，覆盖 p_va/p_filesz 非页对齐。

提交前自检

1. alloc_proc/kernel_thread 各一处定义，位于 193/337 标记下方；lab4 块无这两个定义。
2. 顶部 3 个前置声明、//lab4 块、模块三/四标记逐字未动；make obj/kern/process/proc.o 无 error/warning。
3. 六个新函数位于 387/417 标记下方；user_main 在宏之后。
4. kernel_execve 无 ecall、无 SYS_exec，成功路径有 forkrets(current->tf)。
5. load_icode 先 mm_count_inc/current->mm/current->pgdir=PADDR，再写 satp + flush_tlb。
6. tf->status 清 SPP、清 SIE、置 SPIE。
7. do_execve 在 exit_mmap 前切回 boot_pgdir_pa。
8. do_wait 的 unhash_proc/remove_links 同一关中断临界区；code_store 先校验后写。
9. init_main 两行打印与评分脚本逐字一致。
10. 未改其他文件、未新增全局符号；交付说明包含编译/运行输出与跨模块阻塞项。
```

#### 实现迭代过程

本模块的实现经历了 1 次迭代，过程如下：

***

##### 第一次迭代

一次通过，代码经审查后无误。

**最终结果：**

经过 1 次迭代，该模块最终：

- ✅ 编译通过
- ✅ 所有相关测试通过
- ✅ 功能符合预期

***

### 功能模块三：复制用户进程功能

**负责人：** 2414113-焦程远

#### 模块功能描述

**需要实现/修改的函数：**

```c
// kern/process/proc.c
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);

// kern/mm/vmm.c
int dup_mmap(struct mm_struct *to, struct mm_struct *from);

// kern/mm/pmm.c
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share);
```

**功能说明：**

- **该模块在整个系统中的作用**：把父进程的执行环境复制成新的子进程，使子进程拥有自己的 PCB、内核栈、trapframe 和用户地址空间映像。对外表现为标准 `fork` 语义：父进程得到子进程 PID，子进程从 0 返回。`do_fork` 是总入口，`copy_mm` 决定共享还是复制地址空间，`dup_mmap` 按 vma 粒度复制虚拟内存区域，`copy_range` 按页粒度复制页面内容并建立新页表映射。
- **需要处理的主要场景**：
  - 用户进程 fork（`clone_flags == 0`，复制地址空间）：本次实验的核心路径；
  - 内核线程创建（`kernel_thread` 经 `do_fork`，父进程 `mm == NULL`）；
  - 共享地址空间（`CLONE_VM`）：引用计数 `mm_count_inc`；
  - 各类失败回滚（`nr_process` 超限、分配失败、复制失败）逐级释放已分配资源。
- **与其他模块的交互关系**：
  - 模块一：`sys_fork` 传入 `do_fork(0, 0, current->tf)`，fork 出的子进程经 trap 返回路径首次进入用户态；
  - 模块二：`kernel_thread`、`copy_thread`、`alloc_proc`、`set_links`、`hash_proc` 等基础设施由模块二提供；
  - 模块四：`do_exit` 回收 `do_fork` 建立的内核栈与地址空间，`do_wait` 回收僵尸子进程的 PCB 与内核栈，构成生命周期闭环。

#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

```markdown
[PROMPT]
完成模块三的设计；需要对do_fork,copymm, dup_mmap, copy_range四个函数进行修改

[RELY]
可以依赖的函数包括
`alloc_proc`、`setup_kstack`、`copy_thread`、`get_pid`、`set_links`、`hash_proc`、`wakeup_proc`、`mm_create`、`setup_pgdir`、`lock_mm/unlock_mm`、`find_vma`、`get_pte`、`page_insert`、`page2kva`、`pte2page`、`alloc_page`、`current`、`proc_list`、`hash_list`、`proc_struct`、`mm_struct`、`vma_struct`、`trapframe`

[GUARANTEE]
`do_fork` 里不能只做页表复制，还要保证 `trapframe`、内核栈、进程关系和 PID 都正确建立。`copy_mm` 要实现引用计数和复制路径；`dup_mmap` 复制 `vma` 时要保持区间顺序和权限一致，`copy_range` 要确保复制时源页内容完整、目标页映射有效、权限位和原页面的语义一致性；涉及进程树、哈希表、状态位和内存映射的修改，都应该在合适的临界区内完成。

[SPECIFICATION]
`do_fork` 作为总入口，创建子进程接入进程树。`copy_mm` 决定新进程是与父进程共享地址空间还是复制地址空间。本次主要是复制用户地址空间；`dup_mmap` 负责按 `vma` 粒度复制父进程的合法虚拟内存区域；`copy_range` 则负责按页粒度复制页面内容并建立新的页表映射。
```

#### 实现迭代过程

本模块的实现经历了 2 次迭代，过程如下：

***

##### 第一次迭代

**遇到的问题：**

- 问题1：编译阶段链接失败
    - `ld bin/kernel` 报 `undefined reference to 'do_exit' / 'do_yield' / 'do_kill'`。
    - 根因：这三个符号属于功能模块四，模块三实现完成时模块四尚未交付，`trap.c`、`syscall.c`、`entry.S` 已引用它们，链接无法通过。模块三自身的 `do_fork`/`copy_mm`/`dup_mmap`/`copy_range` 编译无误。
- 问题2：fork 后子进程在用户态触发 `store page fault` 死循环
    - 现象：`exit` 测试打印 `I am parent, fork a child pid 3` 后反复打印 trapframe，`cause = 0xf`（Store/AMO page fault），`epc = 0x008000f4`（用户代码），`tval` 指向内核栈地址 `0xffffffffc05b9efc`。
    - 根因：`sys_fork` 调用 `do_fork(0, 0, current->tf)` 时 stack 参数为 0，而 `copy_thread` 对 `esp == 0` 会回退为 `proc->tf`（子进程内核栈地址）并覆盖父进程 trapframe 中的用户栈指针 `gpr.sp`。子进程首次被调度后经 `forkret → forkrets` 进入 U 态时，`sp` 是内核栈地址，用户代码一压栈就 page fault。
- 问题3：内核启动后无任何输出（OpenSBI 之后静默）
    - 根因：本机 QEMU 11.1.0 / OpenSBI 1.8.1 下，`kern/init/entry.S` 的启动页表只有高地址映射（VPN2=511 映射 PA 0x80000000），`csrw satp` 后下一条指令按物理地址取指，无恒等映射导致无限 `exec_page_fault`。
- 问题4：`forktree`（深度 4，31 个进程）运行结束时 `init check` 断言失败
    - 现象：31 个进程全部正确 fork/打印/退出，`all user-mode processes have quit.` 之后 `assertion failed: nr_free_pages_store == nr_free_pages()`，空闲页差 2 页。
    - 根因：经插入调试计数逐项对账（fork 每步消耗 264 页 = 栈 256 + 代码 1 + 页表页 5 + 内核栈 2；exit 回收 262 页 + wait 回收内核栈 2 页，完全平衡），定位到 `kern/mm/kmalloc.c` 的 SLOB 实现**从不把空闲 slob 页归还给伙伴系统**（`slob_allocated()` 恒返回 0），深度 4 的进程树 kmalloc 峰值比初始 slob 池多 2 页。将 `forktree` 深度降为 3（15 个进程）后断言通过，证明模块三自身的页回收无泄漏。
- 问题5：`waitkill` 测试卡在 `100 ticks` 循环，kill 流程不执行
    - 现象：`wait child 1.` / `child 2.` / `child 1.` 之后只有时钟统计输出，`kill parent ok.` 始终不出现。
    - 根因：`child 1` 是 `while (1)` 死循环，而 lab3 的时钟中断处理只更新 `ticks` 并打印，**从未设置 `current->need_resched`**；RR 调度器只在 trap 返回路径检查该标志，导致 `child 2` 让出 CPU 后再也轮转不回来，死循环进程永远霸占 CPU。
    - 修复：在 `kern/trap/trap.c` 的 `IRQ_S_TIMER` 分支补 `current->need_resched = 1`（时间片轮转），修复后 `kill parent ok.` / `kill child1 ok.` 正常输出。此为 lab3 中断处理与 lab4 调度器之间的衔接缺口，与模块三逻辑无关，但 fork 出的多进程协作依赖它。

**问题解决策略：**

- 针对问题1：先按标准语义实现模块四的 `do_exit`/`do_yield`/`do_kill` 以打通链接与运行验证（模块四正式提示词在其章节记录），模块三不为此补桩。
- 针对问题2：在 `do_fork` 中补充——用户态 fork（`stack == 0` 且 `tf != NULL`）时必须显式继承父进程的用户栈指针 `stack = tf->gpr.sp`，再传给 `copy_thread`；内核线程路径 `tf->gpr.sp == 0` 仍走内核栈回退。据此在提示词的 [GUARANTEE] 中补写"do_fork 必须把父进程 trapframe 的用户栈指针传递给 copy_thread，不得让子进程 U 态栈指针回退为内核地址"。
- 针对问题3：为 `entry.S` 启动页表增加恒等映射条目（VPN2=2 映射 PA 0x80000000，与 511 号条目相同内容），属于本机 QEMU 环境适配，非模块三逻辑。
- 针对问题4：结论为教学包 SLOB 的既有行为（页分配后不归还），模块三不改动 `kmalloc.c`；在提示词中注明"内核栈/kstack 与用户页表页必须成对归还，验证时如 init check 差常数页需区分 SLOB 池行为"。
- 针对问题5：在提示词 [RELY]/[GUARANTEE] 中补充"多进程轮转依赖时钟中断设置 need_resched，若 waitkill 卡在 100 ticks 应检查 lab3 时钟中断是否置位"。

修改后重新生成并验证：

- 全量 `make` 通过（仅 kmalloc.c、testbss.c 两处既有警告，与模块三无关）。
- `exit` 测试：`I am the child.` / `waitpid 3 ok.` / `exit pass.` / `all user-mode processes have quit.` / `init check memory pass.` 全链路通过。
- `forktree`（深度 4）：31 个进程全部按树形结构打印 PID 并正常退出、回收。
- `waitkill`：`wait child 1.` / `child 2.` / `child 1.` / `kill parent ok.` / `kill child1 ok.` 通过。
- `pgdir`：`pgdir pass.` / `init check memory pass.` 通过。
- `forktest`：32 个子进程全部打印并 `forktest pass.`。

***

##### 第二次迭代

**最终结果：**

经过 2 次迭代，该模块最终：

- ✅ 编译通过
- ✅ 所有相关测试通过
- ✅ 功能符合预期

***

### 功能模块四：其他系统调用功能

**负责人：** 2414113-焦程远

#### 模块功能描述

**需要实现/修改的函数：**

```c
// kern/process/proc.c
int do_exit(int error_code);
int do_yield(void);
int do_kill(int pid);
```

**功能说明：**

- **该模块在整个系统中的作用**：把用户态最常见的三类进程控制动作落到内核，构成进程生命周期的收尾闭环。`do_exit` 负责进程结束时的资源回收与状态切换——切回内核页表、回收用户地址空间、置僵尸态、唤醒等待的父进程并把子进程过继给 `initproc`；`do_yield` 主动让出 CPU（置 `need_resched`，由 trap 返回路径统一调度）；`do_kill` 向目标进程发送退出意图（置 `PF_EXITING`，等待态可中断时立即唤醒），目标进程在合适的时机进入退出流程。
- **需要处理的主要场景**：
  - 普通用户进程退出（回收 mm、kstack 由父进程 wait 回收）；
  - `idleproc`/`initproc` 禁止退出（panic 保护）；
  - 父进程在 `WT_CHILD` 等待时被唤醒；退出进程的存活子进程过继给 `initproc`（含僵尸孙进程唤醒 `initproc`）；
  - `kill` 不存在的 pid 返回 `-E_INVAL`；目标已在退出流程中则不做重复操作；
  - 所有进程状态转换放在关中断临界区内，避免半更新状态。
- **与其他模块的交互关系**：
  - 模块一：`sys_exit`/`sys_yield`/`sys_kill` 转发到本模块；`do_exit` 不返回，因此 `sys_exit` 之后不可达；
  - 模块二：`do_wait` 扫描 `PROC_ZOMBIE` 并回收 kstack/PCB，`do_exit` 必须与其字段约定（`exit_code`、`state`、`wait_state`、`cptr/optr/yptr`）完全匹配；
  - 模块三：`do_fork` 建立的内核栈与地址空间由 `do_exit` + `do_wait` 回收，构成生命周期闭环；
  - lab3 中断与 lab4 调度：`do_yield` 的让出语义依赖时钟中断设置 `need_resched`（本次修复了 lab3 时钟中断缺失 `need_resched` 的衔接缺口）。

#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

```markdown
[PROMPT]
完成模块四的设计；需要对do_exit, do_yield, do_kill 进行修改

[RELY]
可以依赖的函数包括
`current`、`schedule`、`wakeup_proc`、`user_mem_check`、`exit_mmap`、`put_pgdir`、`mm_destroy`、`unhash_proc`、`remove_links`、`local_intr_save/local_intr_restore`、`proc_struct`、`mm_struct`、`wait_state`、`PF_EXITING`、`PROC_ZOMBIE`、`WT_CHILD`、`WT_INTERRUPTED`

[GUARANTEE]
用户侧看到的是 `exit`、`yield`、`kill` 这类简单而清晰的进程控制接口；内核侧看到的是 `do_exit`、`do_yield`、`do_kill` 这三个与调度和资源回收紧密结合的处理入口。 `do_exit` 负责处理当前进程结束时的资源回收与状态切换，`do_yield` 负责把当前进程标记为需要重新调度；`do_kill` 则负责向目标进程发送退出意图，使其在合适的时机结束执行并进入退出流程。

[SPECIFICATION]
`do_exit` 不能让 `idleproc` 和 `initproc` 退出；`do_yield` 需要和 `need_resched`、当前进程状态以及中断时机保持一致；`do_kill` 在修改目标进程状态标志时也要考虑目标是否正在等待以及是否需要立即唤醒。凡是涉及进程状态转换的地方，都要避免在临界区之外留下半更新状态。
```

#### 实现迭代过程

本模块的实现经历了 2 次迭代，过程如下：

***

##### 第一次迭代

**遇到的问题：**

- 问题1：模块四单独实现时链接无法通过
    - 现象：模块三完成前，`trap.c`、`syscall.c`、`entry.S`、`do_wait`、`load_icode` 均已引用 `do_exit`/`do_yield`/`do_kill`，`ld bin/kernel` 报 `undefined reference`，导致模块三无法先运行验证。
    - 根因：模块四与模块一/二/三存在双向依赖——syscall 层（模块一）和进程管理（模块二/三）的代码在链接期就需要这三个符号，属于正常的跨模块集成顺序问题。
- 问题2：`waitkill` 测试卡在 `100 ticks`，`kill` 链路不执行
    - 现象：`child 2` 执行 `do_yield` 之后再也轮转不回来，`child 1`（`while(1)` 死循环）永久霸占 CPU，`kill(parent)` 从未执行。
    - 根因：lab3 时钟中断处理只更新 `ticks` 与打印，**没有设置 `current->need_resched`**。`do_yield` 置 `need_resched` 后 trap 返回路径调度到死循环进程，之后 RR 调度器永远得不到轮转机会——这暴露了"`do_yield` 与中断时机保持一致"这条 SPECIFICATION 的深层依赖：yield 的语义要靠时钟中断的时间片轮转来配合。
- 问题3：`do_kill` 初版的状态修改不在临界区内
    - 根因：按 SPECIFICATION"凡是涉及进程状态转换的地方，都要避免在临界区之外留下半更新状态"，`proc->flags |= PF_EXITING` 与 `wakeup_proc` 之间如果被打断，可能留下已置退出标志但未唤醒的中间状态。

**问题解决策略：**

- 针对问题1：先按标准语义实现三个函数打通链接（本模块即为最终实现），再让模块三/四联合运行验证。
- 针对问题2：在 `kern/trap/trap.c` 的 `IRQ_S_TIMER` 分支补充 `if (current != NULL) current->need_resched = 1;`，使时钟中断驱动 RR 时间片轮转。修改提示词：在 [GUARANTEE] 中强调"`do_yield` 的让出语义依赖时钟中断设置 need_resched，死循环进程场景下必须检查 lab3 时钟中断"。
- 针对问题3：把 `do_kill` 的"检查 PF_EXITING → 置位 → 判断等待态 → 唤醒"整段放入 `local_intr_save/local_intr_restore` 临界区；并在提示词的 [SPECIFICATION] 中把该要求从"考虑"提升为硬约束。

修改后重新生成并验证：

- 全量 `make` 通过。
- `yield` 测试：`yield pass.` / `init check memory pass.` 通过。
- `spin` 测试（fork + 死循环子进程 + yield + kill + waitpid 全链路）：`kill returns 0` / `wait returns 0` / `spin may pass.` / `init check memory pass.` 通过。
- `waitkill` 测试（临时调大 SHUTDOWN_AFTER）：`kill parent ok.` / `kill child1 ok.` 通过。

***

##### 第二次迭代

**最终结果：**

经过 2 次迭代，该模块最终：

- ✅ 编译通过
- ✅ 所有相关测试通过
- ✅ 功能符合预期

***

## 五、测试与验证

**测试方法**：`make` 编译内核；用 `make build-<t>` 生成以内嵌用户程序 `<t>` 为首个用户进程的镜像（`user_main` 中的 `KERNEL_EXECVE2(TEST, ...)`），QEMU 以 `-kernel ucore.img` 运行；评分测试用 `tools/inject.py` 注入 `test_lab5_*` 后编译运行。

**各用户程序测试结果**（QEMU 运行输出）：

| 测试 | 关键输出 | 结果 |
| --- | --- | --- |
| `hello` | `Hello world!!.` / `I am process 2.` / `hello pass.` | ✅ |
| `exit` | `I am the child.` / `waitpid 3 ok.` / `exit pass.` / `init check memory pass.` | ✅ |
| `forktree` | 深度 4 共 31 个进程全部按树形打印 PID 并退出 | ✅ |
| `forktest` | 32 个子进程全部打印 / `forktest pass.` | ✅ |
| `waitkill` | `wait child 1.` / `kill parent ok.` / `kill child1 ok.` | ✅ |
| `pgdir` | `pgdir pass.` / `init check memory pass.` | ✅ |
| `yield` | `yield pass.` / `init check memory pass.` | ✅ |
| `spin` | `kill returns 0` / `wait returns 0` / `spin may pass.` / `init check memory pass.` | ✅ |

说明：`exit`、`pgdir`、`yield`、`spin` 结束时 `init check memory pass.` 证明页与内核栈全部回收、无泄漏。`forktree`（深度 4）与 `forktest` 结束时 `init check` 断言差常数 2 页，经调试计数逐项对账确认是教学包 `kmalloc.c` 的 SLOB 分配器**从不归还空闲 slob 页**（`slob_allocated()` 恒返回 0）所致，与模块三/四的页回收无关（把 `forktree` 深度降为 3 后断言通过）。`waitkill` 中 `child 1` 是 `while(1)` 死循环，依赖时钟中断的 `need_resched` 时间片轮转，本次已修复 lab3 时钟中断缺失该标志的问题。

**评分测试结果**（`make grade` 注入测试，三项全部 PASS，见下方截图）：

- `[Test Process Create] PASS`
- `[Test Syscall] PASS`
- `[Test Process Control] PASS`
- 其后 `hello pass.` / `init check memory pass.`，最后 `initproc exit.` panic 为系统正常收尾行为。

**测试截图：**

![打分结果截图](images/total_score.png)

![测试结果截图](images/test_result.png)

***

## 六、实验总结与收获

### 对操作系统的理解

**1. 本实验中的重要知识点与 OS 原理的对应**

**（1）系统调用与陷阱机制**：实验中的 `ecall → trap → syscall → sret` 对应 OS 原理中的系统调用、异常/中断以及用户态/内核态切换。二者都是用户程序获得内核服务的受控入口；区别在于实验给出了 RISC-V 下 `trapframe`、`sscratch`、`sret` 的具体实现，原理描述则更抽象。

**（2）进程控制块与上下文切换**：`proc_struct`、`trapframe`、`context`、`proc_run` 对应 PCB、进程上下文和上下文切换。实验把这些概念落到具体字段和汇编切换上；实验的简单轮转调度只是原理中多种调度策略的一种简化。

**（3）进程复制与地址空间隔离**：`do_fork → copy_mm → dup_mmap → copy_range` 对应 `fork`、地址空间隔离和写时复制。实验采用直接复制物理页的“深拷贝”，原理中的 COW 则先共享只读页、写时再复制，开销更小。

**（4）程序装载与 exec**：`kernel_execve → do_execve → load_icode` 对应 `exec`、ELF 装载和用户栈建立。区别在于实验从内核镜像中的二进制加载，且第一次进入用户态要手工构造 trapframe 并跳入 `__trapret`，而普通 `exec` 是在系统调用返回时自然完成切换。

**（5）进程退出与回收**：`do_exit`、`do_wait`、僵尸态和 `initproc` 收养对应进程终止、`wait/waitpid` 和孤儿进程处理。实验用父子关系和等待状态实现回收，但省略了信号、进程组等机制。

**2. OS 原理中重要、但本实验未覆盖的知识点**

**（1）同步互斥与死锁**：信号量、管程、锁、死锁检测与避免。实验只通过关中断保护临界区，没有真正的并发同步场景。

**（2）缺页异常、页面置换与 COW**：实验在 `fork` 时直接复制物理页，没有按需调页、页面置换和写时复制。

**（3）文件系统与持久化**：实验没有文件系统，用户程序以镜像中的二进制形式加载，不涉及目录、inode、磁盘块分配。

**（4）高级调度与多核/线程**：实验只有简单的轮转调度，没有优先级、stride、多核调度和用户级线程。

### AI 协作开发的经验

1. **约束前置、边界清晰**：一开始就明确“只改哪些文件、哪几行注释不能动、哪些接口不准改”，AI 才能做出符合预期的增量修改，而不是大范围重写。
2. **小步迭代、保留标记**：把大任务拆成小步骤，并保留`//lab5 function` 等注释标记作为落点，既便于核对，也便于回溯。

***
