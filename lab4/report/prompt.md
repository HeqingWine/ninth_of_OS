# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
| --- | --- |
| **实验名称** | Lab 4: 进程管理 |
| **小组成员** | 2413094-卢思齐、2414113-焦程远、2411617-陈彦冰 |
| **完成日期** | YYYY-MM-DD |

### 小组分工

| 成员 | 负责的练习/模块 |
| --- | --- |
| 2413094-卢思齐 | 模块一<虚拟内存页操作功能> |
| 2411617-陈彦冰 | 模块二<进程创建及初始化功能> |
| 2414113-焦程远 | 模块三<进程调度> |

***
## 实验内容与实现

### 功能模块一：虚拟内存页操作功能

**负责人：** 2413094-卢思齐

#### 模块功能描述

**需要实现/修改的函数：**

```c
//在 kern/mm/pmm.c 中实现
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);
```

#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

```markdown
[PROMPT]
任务：修改 kern/mm/pmm.c文件，实现虚拟内存页操作功能模块中的 page_insert 和 page_remove 函数，并修复导致 QEMU 10.2.1 运行期页错误崩溃的底层遗留 Bug。

操作要求：你必须在项目中修改实际文件，保留文件中的所有现有代码。原文件中存在的 //LAB4 function1 YOUR CODE 这一行注释你绝对不能修改、删除或覆盖这一行注释本身。你必须将 page_insert 和 page_remove 这两个函数的实现代码严格插入、追加在这一行注释的下方。
极其重要：除了实现映射函数，你必须根因修复以下两处代码：
1.修改 get_pte 函数，去除中间级非叶子 PTE 的 PTE_U 标志；
2.修改 check_pgdir 函数中的错误断言。不要试图在 page_insert 中去同步非叶子 PTE 的权限。

输出要求：直接进行文件操作，输出修改后的代码片段。使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 部分的信息作为唯一实现依据。

[RELY]
宏定义 (来自 mmu.h)
#define PGSIZE 4096
#define PTE_V 0x001 // Valid
#define PTE_R 0x002 // Read
#define PTE_W 0x004 // Write
#define PTE_X 0x008 // Execute
#define PTE_U 0x010 // User
#define PTE_PPN_SHIFT 10

数据结构 (来自 memlayout.h)
struct Page {
    int ref;                        // page frame's reference counter
    uint_t flags;                   // flags ...
    unsigned int property;          // free block size
    list_entry_t page_link;
    list_entry_t pra_page_link;
    uintptr_t pra_vaddr;
};
typedef uintptr_t pte_t;            
typedef uintptr_t pde_t;            

可用函数 (来自 pmm.h，均为 static inline 或已实现)
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create); // 获取la对应的pte虚拟地址。create=1且中间级页表不存在时会分配，失败返回 NULL。
static inline pte_t pte_create(uintptr_t ppn, int type); // 构造PTE，内部已包含 PTE_V
static inline struct Page *pte2page(pte_t pte);          // 由PTE提取对应的物理页结构体
static inline int page_ref_inc(struct Page *page);       // 物理页引用计数 +1
static inline int page_ref_dec(struct Page *page);       // 物理页引用计数 -1，返回减后的值
static inline int page_ref(struct Page *page);           // 获取物理页引用计数
static inline int set_page_ref(struct Page *page, int val); // 直接设置物理页引用计数
static inline uintptr_t page2pa(struct Page *page);      // 获取物理页的物理地址
static inline ppn_t page2ppn(struct Page *page);         // 获取物理页的页号
void free_page(struct Page *page);                       // 释放物理页
void tlb_invalidate(pde_t *pgdir, uintptr_t la);         // 刷新对应线性地址的 TLB 表项

需要修改的现有代码片段
1. get_pte 中分配中间级页表时的原代码：*pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V); 以及 *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
2. check_pgdir 中的错误断言：assert(boot_pgdir_va[0] & PTE_U);

[GUARANTEE]
必须实现或修改的接口：
需插入到 //LAB4 function1 YOUR CODE 下方的函数：
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);

需要修复的现有函数：
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create);
static void check_pgdir(void);

你可以根据需要添加：
辅助函数（建议设为 static，例如实现一个 static void page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep)，供 page_insert 和 page_remove 共用，以降低代码冗余）

[SPECIFICATION]
## 修复 get_pte 与 check_pgdir
根据 RISC-V Sv39 规范，非叶子 PTE 的 U 位应始终为 0，否则 QEMU 10.2.1 会在 S-Mode 访问时报错。
1. get_pte 修改逻辑：在分配第一级和第二级中间页表时，使用 pte_create 构造 PTE 时只传入 PTE_V（或其他非 U 标志），严禁传入 PTE_U。
2. check_pgdir 修改逻辑：将原本的 assert(boot_pgdir_va[0] & PTE_U); 替换为 assert(!(boot_pgdir_va[0] & PTE_U));。函数中的其他断言保持原样。

## page_insert 
Pre-Condition: 
pgdir 为页表基址，page 为待映射的物理页，la 为目标线性地址，perm 为期望的权限位。该函数需要处理潜在的页表页分配（get_pte 且 create 传 1）。

Post-Condition: 
指定的线性地址 la 成功稳定映射到 page，权限位正确，物理页引用计数正确，且 TLB 保持一致。
Case 1：get_pte(pgdir, la, 1) 返回 NULL。直接返回 -E_NO_MEM，不改变物理页引用计数。
Case 2：la 已映射到同一个物理页 page。不增加物理页引用计数。直接用新的 perm 覆盖原有权限位，即 *ptep = pte_create(page2ppn(page), perm)，调用 tlb_invalidate 刷新 TLB，返回 0。
Case 3：la 已映射到另一个不同的物理页。先调用 page_remove_pte（或等效逻辑）解除原有映射：将旧物理页引用计数减 1，若为 0 则 free_page，清空 PTE。然后增加新 page 的引用计数，设置新 PTE，刷新 TLB，返回 0。
Case 4：la 此前未被映射。增加 page 引用计数，设置 PTE，刷新 TLB，返回 0。

## page_remove 
Pre-Condition: 
pgdir 为页表基址，la 为目标线性地址。不能假定 la 一定存在有效映射。

Post-Condition: 
若存在有效映射，则撤销映射并清理资源；若不存在，安静返回。
Case 1：get_pte(pgdir, la, 0) 返回 NULL，或 PTE 无效。直接返回，不报错。
Case 2：la 存在有效映射。通过 pte2page 获取物理页，引用计数减 1。若减后为 0，则 free_page 释放。将 PTE 内容清 0，调用 tlb_invalidate 刷新 TLB。

Requirements: 
1. 绝对不要通过修改 page_insert 的叶子映射逻辑来试图修复非叶子 PTE 的权限问题。
2. 确保更新叶子 PTE 后，一定调用 tlb_invalidate(pgdir, la)。
3. 保证物理页引用计数增减与页表项修改严格同步。

```

#### 实现迭代过程

本模块的实现经历了 [N] 次迭代，过程如下：

***

##### 第一次迭代

**遇到的问题：**

- 问题1：QEMU 运行期触发页错误崩溃

    - AI 根据第一版提示词正确生成了 page_insert 和 page_remove 的代码逻辑，但在 QEMU 中运行测试时，系统在 check_pgdir 或 check_boot_pgdir 阶段直接崩溃。

- 问题2：AI 修复方案引入底层逻辑冲突

    - 在向 AI 提出修补要求时，AI 根据提示词，尝试在 page_insert 中根据 perm 动态同步中间级 PTE 的 PTE_U 位。未发生编译错误，但经过代码审查发现，同一页表分支下可能同时存在不同 PTE_U 权限的叶子映射（内核与用户页共享页表分支），按单个映射调整父级位会造成权限踩踏。

**问题解决策略：**

- 针对问题1：在第一版提示词中，直接将解决问题的目标指向“根因修复”。明确要求 AI 不要试图在 page_insert 中同步中间级权限，而是修改 get_pte 函数，在创建非叶子 PTE 时移除 PTE_U 标志，只保留 PTE_V。同时，要求 AI 修正 check_pgdir 中的错误断言，改为检查非叶子 PTE 不包含 U 位。
- 针对问题2：在提示词中明确“问题背景”和“根源冲突”，并在 [RELY] 和 [SPECIFICATION] 部分严格限制 AI 的修改范围。强调“不要修改 page_insert 和 page_remove 的映射建立逻辑”，给出正确的代码片段作为参考，强制 AI 按照 RISC-V 规范进行架构级别的修正，从而彻底闭环解决此问题。

***

##### 第二次迭代（如有）

如果设计的prompt生成代码后出现问题：

**遇到的问题：**

- 问题1：问题描述、错误信息和原因分析
- 问题2：问题描述、错误信息和原因分析

**问题解决策略：**

- 针对问题1：[说明如何修改提示词]
- 针对问题2：[说明如何修改提示词]

如果没有问题：

**最终结果：**

经过 [N] 次迭代，该模块最终：

- ✅ 编译通过
- ✅ 所有相关测试通过
- ✅ 功能符合预期

**关键改进点总结（如有）：**

1. [改进点1：例如，在 [RELY] 中补充了完整的系统调用号定义]
2. [改进点2：例如，在 [SPECIFICATION] 中明确了异常处理的边界条件]
3. [改进点3：例如，增加了对用户指针合法性检查的要求]

***

##### 第三次迭代（如有）

[按照和第一次迭代相同的格式]

***

### 功能模块二：进程创建及初始化功能

**负责人：** 2411617-陈彦冰

#### 模块功能描述

**需要实现/修改的函数：**

```c
// 在 kern/process/proc.c 中实现
static struct proc_struct *alloc_proc(void);
void proc_init(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
```

#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

```markdown
# 模块二：进程创建及初始化功能实现提示词
### Prompt任务说明

#### 1. 修改位置与标记核对

目标文件只有一个：`lab4/code/kern/process/proc.c`。

事实：该文件只有一处 `//LAB4 function2 YOUR CODE`，位于第 221 行；第 224 行是 `//LAB4 function3 YOUR CODE`；文件到第 225 行结束。六个函数全部写在第 221 行标记之后、第 224 行标记之前。

实现前必须先核对：
- 实际标记位置、数量是否与报告一致；
- 标记是否位于待实现函数体内；
- 六个函数是否确实允许集中写在该标记后。

若核对结果与报告不一致，或标记不在函数体内，**立即停止并报告**，不得移动函数定义、不得修改标记前代码、不得自行选择其他插入点。

| 函数 | 文件 | 允许写入位置 |
|---|---|---|
| `static struct proc_struct *alloc_proc(void)` | `kern/process/proc.c` | 仅 `//LAB4 function2 YOUR CODE` 后 |
| `void proc_init(void)` | 同上 | 同上 |
| `int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)` | 同上 | 同上 |
| `int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)` | 同上 | 同上 |
| `static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)` | 同上 | 同上 |
| `static int copy_mm(uint32_t clone_flags, struct proc_struct *proc)` | 同上 | 同上 |

硬性要求：
- 不得移动、删除、改写 `//LAB4 function2 YOUR CODE` 和 `//LAB4 function3 YOUR CODE` 标记。
- 不得修改标记之前的已有代码：include、`proc_list`、`hash_list`、`idleproc`、`initproc`、`current`、`nr_process`、`set_proc_name`、`get_proc_name`、`get_pid`、`forkret`、`hash_proc`、`find_proc`、`setup_kstack`、`put_kstack`、`do_exit`、`init_main`。
- 不得修改任何其他文件，包括 `proc.h`、`pmm.h`、`pmm.c`、`memlayout.h`、`mmu.h`、`riscv.h`、`trap.h`、`sched.c`、`sched.h`、`switch.S`、`entry.S`、`trapentry.S`、`Makefile`、链接脚本、`test_lab4.c`、`inject.py`。
- 允许在标记后新增仅服务于本模块的内部 `static` 辅助函数，但不得新增全局符号、不得改变对外接口。
- 不得实现或修改模块三内容：`cpu_idle`、`schedule`、`proc_run`、`switch_to`。

#### 2. 功能目标

1. `alloc_proc`：从内核堆分配 PCB，并初始化为确定的“未初始化”状态。
2. `proc_init`：初始化进程管理全局结构；构造 0 号 `idleproc`；设置 `current`；通过 `kernel_thread` 建立 1 号 `initproc` 并命名为 `"init"`。
3. `kernel_thread`：把 `fn`、`arg` 包装成内核线程首次运行现场，并通过 `do_fork` 创建线程。
4. `do_fork`：完成 PCB、内核栈、mm、首次运行现场、`proc_list`/`hash_list` 挂接、唤醒和 PID 返回。
5. `copy_thread`：在新内核栈上建立 `trapframe` 与 `context`，衔接 `forkret → forkrets → __trapret → kernel_thread_entry`。
6. `copy_mm`：当前阶段为占位接口，保持完整函数形态并固定返回 `0`，不复制用户地址空间。

#### 3. 可修改 / 不可修改范围

可修改：
- 仅 `proc.c` 中 `//LAB4 function2 YOUR CODE` 之后、`//LAB4 function3 YOUR CODE` 之前的区域。
- 可在该区域新增本模块专用 `static` 辅助函数。

不可修改：
- 标记之前的所有代码；`//LAB4 function3 YOUR CODE` 及其后内容。
- 所有宏、`struct proc_struct`、`struct context`、`struct trapframe`、`struct pushregs`、`enum proc_state`。
- 所有全局变量及其初值、所有已有函数签名与接口、lab2/lab3 已完成代码。
- 错误码约定、进程状态宏、`SSTATUS_*`、`CLONE_*`、`kernel_thread_entry`、`forkret`、`forkrets`、`__trapret` 的定义。
- 模块三函数。

#### 4. 输出验证要求

1. 不引入新的编译错误；使用工程既有 Makefile 可编译到本模块为止。
2. 若构建因与本模块无关的既有问题失败（如 `default_pmm.c` 引用的 `best_fit_pmm.h` 不存在、`default_pmm_manager` 未定义），实现 AI 必须报告该事实，不得修改其他文件绕过。
3. `tools/test_lab4.c:68` 的 `test_lab4_thread` 返回 `void`，与 `kernel_thread` 的 `int (*)(void *)` 不严格匹配；这是测试文件的既有情况，`-Wall` 只告警、无 `-Werror`，不得修改测试文件或改签名来消除告警。
4. 可观察验证点逐项成立：
   - `proc_init` 返回后：`idleproc != NULL && idleproc->pid == 0 && idleproc->state == PROC_RUNNABLE && idleproc->kstack == (uintptr_t)bootstack`。
   - `proc_init` 返回后：`current == idleproc`。
   - `idleproc->parent == NULL`、`idleproc->mm == NULL`、`idleproc->tf == NULL`、`idleproc->runs == 0`、`idleproc->flags == 0`、`idleproc->context.ra == 0`。
   - `initproc != NULL && initproc->pid == 1 && initproc->state == PROC_RUNNABLE`。
   - `initproc->parent == idleproc`、`initproc->mm == NULL`、`initproc->context.ra != 0`、`initproc->context.sp != 0`。
   - `initproc` 名称为 `"init"`，评分期望输出 `this initproc, pid = 1, name = "init"`。
   - `kernel_thread(...)` 成功返回值 `> 0`；连续创建两个线程均 `> 0`。
   - `do_fork` 成功后 `find_proc(pid)` 能找到 PCB，且该 PCB 同时位于 `proc_list` 与对应 `hash_list` 桶。
   - `copy_thread` 后 `proc->tf` 指向新内核栈内完整 `trapframe`，`proc->context.ra` 指向 `forkret`，`proc->context.sp` 指向该 `trapframe`，均非零。
5. 每条失败路径必须给出可检查的返回值、清理动作和状态不变式，不得只写“正确处理边界情况”。

---

### Rely依赖说明

只列模块二真正需要的最小依赖。模块三的 `cpu_idle`、`schedule`、`proc_run`、`switch_to` 不是本模块依赖，禁止实现、调用或写入。

#### 1. 数据结构

| 依赖 | 定义位置 | 关键内容/用途 |
|---|---|---|
| `enum proc_state` | `kern/process/proc.h:10-16` | `PROC_UNINIT=0`、`PROC_SLEEPING=1`、`PROC_RUNNABLE=2`、`PROC_ZOMBIE=3` |
| `struct context` | `kern/process/proc.h:18-34` | 字段顺序固定：`ra, sp, s0..s11`，共 14 个 `uintptr_t` |
| `struct proc_struct` | `kern/process/proc.h:42-58` | 字段：`state, pid, runs, kstack, need_resched, parent, mm, context, tf, pgdir, flags, name[16], list_link, hash_link` |
| `struct pushregs` | `kern/trap/trap.h:6-40` | 32 个 `uintptr_t`，顺序：`zero, ra, sp, gp, tp, t0, t1, t2, s0, s1, a0..a7, s2..s11, t3..t6` |
| `struct trapframe` | `kern/trap/trap.h:42-49` | `gpr` 后依次 `status`、`epc`、`badvaddr`、`cause`；RV64 下总大小 288 字节 |
| `struct mm_struct` | `kern/mm/vmm.h:30-36` | 本模块仅需知道 `mm` 字段类型，不操作内部 |
| `list_entry_t` | `libs/list.h:17-21` | 双向链表节点 |

`trapframe` 布局硬约束（来自 `kern/trap/trapentry.S:4-52,55-102,120-124`）：
- `gpr.ra` 在 `1*REGBYTES`，`gpr.sp` 在 `2*REGBYTES`，`gpr.s0` 在 `8*REGBYTES`，`gpr.s1` 在 `9*REGBYTES`；
- `status` 在 `32*REGBYTES`，`epc` 在 `33*REGBYTES`；
- `REGBYTES = 8`（RV64）。

#### 2. 全局变量与内部符号（均定义在标记前）

| 依赖 | 位置 | 类型/初值 | 用途 |
|---|---|---|---|
| `proc_list` | `proc.c:62` | `list_entry_t`，BSS 全零，未 `list_init` | 进程总链表；`get_pid` 依赖 |
| `HASH_SHIFT` / `HASH_LIST_SIZE` | `proc.c:64-65` | `10` / `1024` | 哈希桶数量 |
| `pid_hashfn(x)` | `proc.c:66` | `hash32(x, HASH_SHIFT)` | PID → 桶下标 |
| `hash_list[]` | `proc.c:69` | `static list_entry_t[1024]`，BSS 全零，各桶未 `list_init` | `hash_proc`/`find_proc` |
| `idleproc` | `proc.c:72` | `NULL` | 0 号进程 |
| `initproc` | `proc.c:74` | `NULL` | 1 号进程 |
| `current` | `proc.c:76` | `NULL` | 当前进程 |
| `nr_process` | `proc.c:78` | `static int = 0` | 进程计数，仅本文件内访问 |

#### 3. 已有函数（只允许调用，不允许修改）

| 函数 | 签名 | 位置 | 返回/错误码约定 | 本模块用途 |
|---|---|---|---|---|
| `kmalloc` | `void *kmalloc(size_t n)` | `kern/mm/kmalloc.c:260-264` | 失败返回 `NULL`；内部已处理中断 | `alloc_proc` |
| `kfree` | `void kfree(void *objp)` | `kern/mm/kmalloc.c:266-294` | 无；`NULL` 安全 | 失败清理 |
| `memset` | `void *memset(void *s, char c, size_t n)` | `libs/string.h:19` | 返回 `s` | PCB/trapframe 清零 |
| `set_proc_name` | `char *set_proc_name(struct proc_struct *, const char *)` | `proc.c:84-90` | 返回 `proc->name`；先清零再复制 `PROC_NAME_LEN` 字节 | 命名 `"idle"`、`"init"` |
| `get_pid` | `static int get_pid(void)` | `proc.c:101-142` | 总是返回 PID，无失败路径；首次调用返回 1 | `do_fork` |
| `hash_proc` | `static void hash_proc(struct proc_struct *)` | `proc.c:155-159` | 无；要求目标桶已 `list_init` | 挂入 hash 桶 |
| `find_proc` | `struct proc_struct *find_proc(int pid)` | `proc.c:162-178` | 找到返回指针，否则 `NULL`；仅接受 `0 < pid < MAX_PID` | `proc_init` 由 pid 找回 `initproc` |
| `setup_kstack` | `static int setup_kstack(struct proc_struct *)` | `proc.c:182-192` | 成功 `0`，失败 `-E_NO_MEM`；分配 `KSTACKPAGE` 页 | `do_fork` |
| `put_kstack` | `static void put_kstack(struct proc_struct *)` | `proc.c:194-199` | 无；释放 `KSTACKPAGE` 页 | 失败清理 |
| `wakeup_proc` | `void wakeup_proc(struct proc_struct *)` | `kern/schedule/sched.c:7-11` | 无；内部断言 `state != PROC_ZOMBIE && state != PROC_RUNNABLE`，随后置 `PROC_RUNNABLE` | `do_fork` 唤醒子进程 |
| `forkret` | `static void forkret(void)` | `proc.c:148-152` | 无；调用 `forkrets(current->tf)` | `copy_thread` 设置 `context.ra` |
| `init_main` | `static int init_main(void *arg)` | `proc.c:212-219` | 返回 `int` | `proc_init` 传给 `kernel_thread` |
| `local_intr_save` / `local_intr_restore` | 宏 | `kern/sync/sync.h:22-26` | 保存/恢复 `sstatus.SIE` | `do_fork` 临界区 |
| `assert` | 宏 | `kern/debug/assert.h:15-20` | 失败 `panic` | 不变量检查 |
| `panic` | 宏 | `kern/debug/assert.h:12-13` | 不返回 | 启动期不可恢复错误 |
| `cprintf` | `int cprintf(const char *fmt, ...)` | `libs/stdio.h:8` | 返回字符数 | 可选日志，保持现有风格 |

链表操作（`libs/list.h`，均无返回值）：`list_init` `:40-43`、`list_add` `:53-56`、`list_add_before` `:66-69`、`list_del` `:91-94`、`list_del_init` `:102-106`、`list_empty` `:112-115`、`list_next`/`list_prev` `:121-133`。

#### 4. 宏与常量

| 依赖 | 值 | 位置 | 用途 |
|---|---|---|---|
| `PROC_UNINIT` | 0 | `proc.h:12` | `alloc_proc` 初始状态 |
| `PROC_RUNNABLE` | 2 | `proc.h:14` | idleproc/initproc 可运行状态 |
| `PROC_NAME_LEN` | 15 | `proc.h:36` | 名称长度 |
| `MAX_PROCESS` | 4096 | `proc.h:37` | 现有上限；本模块不新增检查 |
| `MAX_PID` | 8192 | `proc.h:38` | `get_pid`/`find_proc` 内部使用 |
| `KSTACKPAGE` | 2 | `memlayout.h:44` | 内核栈页数 |
| `KSTACKSIZE` | 8192 | `memlayout.h:45` | 内核栈字节数 |
| `PGSIZE` | 4096 | `mmu.h:51` | 如需要栈顶对齐时使用 |
| `CLONE_VM` | `0x00000100` | `pmm.h:11` | `copy_mm` 判定；以报告为准，不得修改定义 |
| `CLONE_THREAD` | `0x00000200` | `pmm.h:12` | `kernel_thread`/`do_fork` 判定；以报告为准 |
| `SSTATUS_SIE` | `0x00000002` | `riscv.h:46` | 首次运行 `status` 需清除 |
| `SSTATUS_SPIE` | `0x00000020` | `riscv.h:48` | 首次运行 `status` 需设置 |
| `SSTATUS_SPP` | `0x00000100` | `riscv.h:49` | 首次运行 `status` 需设置 |
| `E_NO_MEM` | 4 | `error.h:8` | 内存不足错误码 |
| `E_INVAL` | 3 | `error.h:7` | 参数非法错误码（仅在明确需要时使用） |
| `bootstack` | `char[]` | 定义 `kern/init/entry.S:47-51`；声明 `pmm.h:180` | `idleproc->kstack` 必须等于它 |

CSR 宏（`libs/riscv.h`）：`read_csr` `:210-212`、`write_csr` `:214`、`set_csr` `:220-222`、`clear_csr` `:224-226`。

#### 5. 只读汇编符号

| 符号 | 位置 | 事实 |
|---|---|---|
| `kernel_thread_entry` | `kern/process/entry.S:2-7` | `move a0, s1; jalr s0; jal do_exit`。契约：`s0 = fn`，`s1 = arg`，参数经 `a0` 传入，返回后 `do_exit` |
| `forkret` | `proc.c:148-152` | 调用 `forkrets(current->tf)` |
| `forkrets` | `kern/trap/trapentry.S:120-124` | `move sp, a0; j __trapret` |
| `__trapret` | `kern/trap/trapentry.S:114-118` | `RESTORE_ALL` 后 `sret` |

首次运行链（仅用于约束 `copy_thread`）：
`switch_to(模块三)` → 恢复 `context.ra/sp` → `forkret` → `forkrets(current->tf)` → `__trapret` → `sret` → `tf->epc`。

#### 6. 实现前需确认的显式假设

以下按显式假设实现，并在交付说明中列明；不得臆造为既有事实。

- **A1（`proc_init` 失败处理）**：`alloc_proc` 或 `kernel_thread` 失败时使用 `panic(...)` 终止启动。若教学参考另有约定，先停止并报告，不得静默选择其他行为。
- **A2（`idleproc->need_resched`）**：置 `1` 以配合后续首轮调度；测试未断言该字段。若不置，交付说明写明。
- **A3（`idleproc` 是否入 hash）**：`idleproc` 必须挂入 `proc_list` 并递增 `nr_process`；`hash_proc(idleproc)` 可选，测试不依赖。注意 `find_proc` 只接受 `0 < pid < MAX_PID`，因此 `hash_proc(idleproc)` 不会使 `find_proc(0)` 可用；不得声称 `find_proc(0)` 可查询。
- **A4（中断保护，必须）**：`proc_init` 在 `intr_enable` 之前执行，无需额外锁；但 `do_fork` 还可能从 `init_main`/`test_lab4_schedule` 在中断开启后调用。`get_pid()`、`hash_proc()`、`list_add()`、`nr_process++` 必须放在 `local_intr_save/restore` 保护的同一个临界区内；`kmalloc`/`alloc_pages`/`free_pages` 的内部保护不覆盖这段。`wakeup_proc` 必须在挂接完成后调用。
- **A5（`kernel_thread` 的 `status`）**：首次运行 `tf.status` 必须使 `sret` 返回 S 态且返回后开中断，即设置 `SPP` 与 `SPIE`、清除 `SIE`；可基于当前 `sstatus` 或等价常量构造。
- **A6（`stack`/`esp` 语义）**：`kernel_thread` 向 `do_fork` 传入 `stack = 0`；`copy_thread` 在 `esp == 0` 时使用新内核栈内的有效栈地址，不得让 `tf->gpr.sp == 0`；`esp != 0` 时使用 `esp`。测试线程会实际使用栈，`sp` 必须有效。
- **A7（`tf == NULL` 路径）**：`copy_thread` 必须能处理 `tf == NULL`（全零 `trapframe`），但评测只覆盖 `kernel_thread` 的 `tf != NULL` 路径；交付说明注明该分支未验证。
- **A8（`alloc_proc` 的 `pid` 初值）**：显式赋值为 `-1`，表示未分配；不得保留未初始化值。
- **A9（`nr_process` 上限）**：本模块不新增 `MAX_PROCESS` 检查，保持与现有代码一致。
- **A10（PID 回滚）**：`get_pid` 没有回滚接口；`do_fork` 后续失败时不回滚 PID，允许 PID 空洞；交付说明注明。
- **A11（`proc->pgdir` 跨模块契约）**：报告未确认模块三 `proc_run` 是否读取 `proc->pgdir`。模块二按 `alloc_proc` 置 `0`、`copy_mm` 不修改；实现 AI 只能只读核对模块三的 `proc_run` 声明/注释，若发现其依赖 `proc->pgdir`，立即停止并报告，不得在模块二擅自设置 `boot_pgdir_pa`，也不得修改 `proc_run`。
- **A12（`lsatp` 的 RV64 形态）**：`libs/riscv.h:244-248` 的 `lsatp` 使用 `SATP32_MODE`，在 RV64 上模式位可疑；`kern/init/entry.S:24-29` 与 `pmm.c:162` 使用 Sv39。模块二不得修改 `lsatp`；若模块三 `proc_run` 依赖它，需在联调时单独确认，不得在本模块绕过。
- **A13（通用 fork 的 `a0` 语义）**：若 `tf` 来自用户态 fork（本实验不覆盖），子进程 `tf->gpr.a0` 应清零以表示 fork 返回 0；`kernel_thread` 路径由 `kernel_thread_entry` 用 `s1` 覆盖 `a0`，因此该字段不影响本实验评测。实现 AI 可设置 `a0 = 0`，但不得影响 `s0/s1/epc/status`。

---

### Guarantee接口清单

#### 1. 模块二必须提供的六个接口

| 函数 | 完整签名 | 返回类型 | 成功返回 | 失败返回 |
|---|---|---|---|---|
| `alloc_proc` | `static struct proc_struct *alloc_proc(void)` | `struct proc_struct *` | 指向已初始化 PCB | `NULL` |
| `proc_init` | `void proc_init(void)` | `void` | 正常返回时全局结构已建立 | 报告未定义错误返回；按 A1 使用 `panic` |
| `kernel_thread` | `int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)` | `int` | 新线程 `pid > 0` | 负错误码（由 `do_fork` 透传） |
| `do_fork` | `int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)` | `int` | 新进程 `pid > 0` | `-E_NO_MEM` 等负错误码 |
| `copy_thread` | `static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)` | `void` | `proc->tf`、`proc->context` 已建立 | 无失败返回；前置条件不满足时用 `assert` |
| `copy_mm` | `static int copy_mm(uint32_t clone_flags, struct proc_struct *proc)` | `int` | 固定返回 `0`，`proc->mm` 保持 `NULL` | 本阶段不得返回负值；不得引入失败分支 |

#### 2. 内部辅助接口

允许新增的 `static` 辅助函数仅限以下用途，且必须写在 `//LAB4 function2 YOUR CODE` 之后：
- 把新 PCB 挂接到 `proc_list` 并递增 `nr_process`；
- 失败路径撤销上述挂接并递减 `nr_process`；
- 其他只在本模块内部使用、不暴露全局符号的小工具。

限制：
- 不得定义与已有函数同名的新函数（不得重定义 `hash_proc`、`find_proc`、`setup_kstack`、`put_kstack`、`get_pid`、`set_proc_name`）。
- 不得新增全局变量、全局函数、宏、类型。
- 不得修改任何已有函数签名。
- 不得把辅助函数写在标记之外。

#### 3. 对外接口与符号保证

- `proc.h` 中已有的 `proc_init`、`kernel_thread`、`do_fork` 声明保持不变。
- `alloc_proc`、`copy_thread`、`copy_mm` 保持 `static`，不新增头文件声明。
- 不新增、不删除、不重命名任何全局符号。
- `cpu_idle`、`schedule`、`proc_run`、`switch_to` 的声明与未实现状态保持不变；本模块不得定义它们。

---

### Specification行为规格

#### 1. `static struct proc_struct *alloc_proc(void)`

**前置状态**
- `kmalloc` 可用；不要求 `current`、`proc_list`、`hash_list` 已初始化，本函数不得依赖它们。
- 可在 `proc_init` 和 `do_fork` 两条路径中被调用。

**后置效果**
返回非 `NULL` 时 PCB 必须处于确定状态：
- `state == PROC_UNINIT`；
- `pid == -1`（按 A8）；
- `runs == 0`、`kstack == 0`、`need_resched == 0`；
- `parent == NULL`、`mm == NULL`；
- `context` 全部寄存器为 `0`，尤其 `context.ra == 0`；
- `tf == NULL`、`pgdir == 0`（按 A11 保持 0）、`flags == 0`；
- `name` 为全零字符串；
- `list_link`、`hash_link` 处于可被 `list_add` 覆盖的确定状态（全零即可）。

**分支场景**
- 成功：`kmalloc` 返回非 `NULL`，完成初始化后返回该指针。
- 失败：`kmalloc` 返回 `NULL`，直接返回 `NULL`；不得写全局结构、不得分配内核栈、不得改变 `nr_process`。
- 本函数不分配内核栈，不设置实际 PID，不挂接任何链表。

**可验证断言/测试映射**
- `proc_init` 后的测试断言要求 `idleproc->context.ra == 0`、`idleproc->tf == NULL`、`idleproc->mm == NULL`、`idleproc->runs == 0`、`idleproc->flags == 0`。
- 额外不变量：`alloc_proc()` 返回后、任何外部字段覆盖前，`state == PROC_UNINIT`。

#### 2. `void proc_init(void)`

**前置状态**
- `proc_list`、`hash_list[0..1023]` 均为 BSS 全零，尚未初始化，不能直接用于 `list_add`/`list_next`。
- `idleproc == NULL`、`initproc == NULL`、`current == NULL`、`nr_process == 0`。
- 中断尚未使能（`kern_init` 在 `proc_init` 之后才 `intr_enable`）。
- `bootstack` 已建立。

**执行顺序要求**
必须先完成 `proc_list` 和全部 hash 桶初始化、`idleproc` 字段设置、`current = idleproc`、`idleproc` 挂入 `proc_list` 并 `nr_process++`，然后才能调用 `kernel_thread` 创建 initproc。不得先创建 initproc 再设置 `current`，否则 `initproc->parent` 不会是 `idleproc`。

**后置效果**
1. `proc_list` 已 `list_init`，成为合法空链表头。
2. `hash_list` 的 1024 个桶全部 `list_init`，保证后续 `hash_proc` 的 `list_add` 不会解引用空指针。
3. `idleproc` 非 `NULL`，且：
   - `pid == 0`、`state == PROC_RUNNABLE`；
   - `kstack == (uintptr_t)bootstack`（不得为 idle 调用 `setup_kstack`）；
   - `parent == NULL`、`mm == NULL`、`tf == NULL`、`runs == 0`、`flags == 0`、`context.ra == 0`；
   - `name == "idle"`；
   - `need_resched` 按 A2 置 `1`；
   - `pgdir` 按 A11 保持 `0`。
4. `current == idleproc`。
5. `idleproc` 已挂入 `proc_list`；`nr_process` 递增一次；按 A3，`hash_proc(idleproc)` 可选，若调用需确保桶已初始化。
6. 通过 `kernel_thread(init_main, "Hello world!!", 0)` 创建 initproc：
   - 返回值 `pid > 0`；失败按 A1 `panic`；
   - `initproc = find_proc(pid)` 必须非 `NULL`；
   - `initproc->pid == 1`；
   - `initproc->state == PROC_RUNNABLE`；
   - `initproc->parent == idleproc`、`initproc->mm == NULL`；
   - `initproc->context.ra != 0 && initproc->context.sp != 0`；
   - `initproc` 已挂入 `proc_list` 与对应 hash 桶；
   - `nr_process` 再次递增；常规路径下最终为 `2`。
7. `set_proc_name(initproc, "init")` 已执行，`get_proc_name(initproc)` 返回 `"init"`。
8. `proc_init` 返回时 `current` 仍为 `idleproc`，不得切换到 initproc。
9. 内部不得调用 `schedule`、`proc_run`、`switch_to`、`cpu_idle`。

**分支场景**
- `alloc_proc()` 返回 `NULL`：按 A1 `panic`；不得留下部分初始化的全局结构继续运行。
- `kernel_thread(...) <= 0`：按 A1 `panic`。
- `find_proc(pid) == NULL`：按 A1 `panic`。
- 正常路径：上述后置效果全部成立。本函数不涉及地址映射、TLB、用户页表。

**可验证断言/测试映射**
`test_lab4_process_create`（`tools/test_lab4.c:47-63`）在 `proc_init` 尾部注入，全部断言必须通过：
- `current == idleproc`；
- `idleproc != NULL && idleproc->pid == 0`；
- `idleproc->state == PROC_RUNNABLE`；
- `idleproc->kstack == (uintptr_t)bootstack`；
- `idleproc->parent == NULL && idleproc->mm == NULL`；
- `idleproc->tf == NULL && idleproc->runs == 0`；
- `idleproc->flags == 0 && idleproc->context.ra == 0`；
- `initproc != NULL && initproc->pid == 1`；
- `initproc->state == PROC_RUNNABLE`；
- `initproc->parent == idleproc && initproc->mm == NULL`；
- `initproc->context.ra != 0 && initproc->context.sp != 0`。
评分输出另要求：`this initproc, pid = 1, name = "init"`。

#### 3. `int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)`

**前置状态**
- `current` 非 `NULL`；`do_fork` 已实现且可用；`fn` 非 `NULL`。
- 测试传入的 `test_lab4_thread` 返回 `void`，与 `int (*)(void *)` 不严格匹配；这是测试文件既有情况，不得修改测试文件。

**后置效果**
- 成功返回新线程 `pid > 0`。
- 新线程首次运行现场：
  - `tf->gpr.s0 == (uintptr_t)fn`；
  - `tf->gpr.s1 == (uintptr_t)arg`；
  - `tf->epc == (uintptr_t)kernel_thread_entry`；
  - `tf->status` 满足 A5：`SPP` 与 `SPIE` 置位、`SIE` 清零；
  - `tf->gpr.sp` 最终为非零有效栈地址（由 `copy_thread` 按 A6 落实）；
  - `context.ra == (uintptr_t)forkret`；
  - `context.sp != 0`。
- 新线程 `state == PROC_RUNNABLE`（由 `do_fork` 的 `wakeup_proc` 完成）。
- 不修改 `current`，不触发调度。
- `clone_flags` 原样传给 `do_fork`，不得自行追加或删除。标准 ucore 参考实现常传 `clone_flags | CLONE_THREAD`；本实验 `copy_mm` 占位，追加与否不影响测试。当前规则保留“不追加”；若实现 AI 认为必须追加，先停止并报告，不得自行决定。

**分支场景**
- 成功：`do_fork` 返回 `pid > 0`，原样返回。
- 失败：`do_fork` 返回负错误码，原样透传；不得吞掉错误、不得返回 0 表示成功。

**可验证断言/测试映射**
- `test_lab4_schedule`（`tools/test_lab4.c:86-99`）：两次 `kernel_thread(test_lab4_thread, ..., 0)` 返回值均 `> 0`；随后两次线程都能被调度执行，`test_thread1_ran && test_thread2_ran` 成立。
- 首次执行时 `kernel_thread_entry` 必须取到正确 `s0`/`s1`，否则线程跳错地址或收到错误参数。

#### 4. `int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)`

**前置状态**
- `current` 非 `NULL`。
- `proc_list` 与 `hash_list` 已由 `proc_init` 初始化；在 `proc_init` 内调用时，初始化顺序必须早于本调用。
- `get_pid` 可用；`tf` 可为非 `NULL`（评测路径）或 `NULL`（按 A7 仅保证不崩溃）。

**建议执行顺序**
先 `alloc_proc`；随后设置 `parent = current`；再 `setup_kstack`；再 `copy_mm`；再 `copy_thread`；然后在同一个 `local_intr_save/restore` 临界区内依次完成 PID 分配、hash 挂接、`proc_list` 挂接和 `nr_process` 递增；最后 `wakeup_proc` 并返回 PID。

**后置效果（成功路径）**
- 新 PCB 由 `alloc_proc()` 分配；失败时无副作用。
- 内核栈由 `setup_kstack(proc)` 分配，`proc->kstack` 非零。
- `copy_mm(clone_flags, proc)` 返回 `0`；`proc->mm == NULL`。
- `copy_thread(proc, stack, tf)` 已执行；`proc->tf` 非零、`proc->context.ra` 非零、`proc->context.sp` 非零。
- `proc->pid = get_pid()` 且 `pid > 0`。
- `proc->parent = current`。
- `proc->state` 通过 `wakeup_proc(proc)` 置为 `PROC_RUNNABLE`；调用前必须仍为 `PROC_UNINIT`。
- 新 PCB 已挂入 `proc_list`，并已通过 `hash_proc(proc)` 挂入对应 hash 桶。
- `nr_process` 恰好递增 1。
- 返回 `proc->pid`（`> 0`）。
- `current` 保持不变；不调用 `schedule`、`proc_run`、`switch_to`。

**分支场景与清理**
1. `alloc_proc()` 返回 `NULL`：返回 `-E_NO_MEM`；不分配内核栈；不修改 `proc_list`、`hash_list`、`nr_process`、`current`。
2. `setup_kstack(proc)` 返回负值：返回该负值；`kfree(proc)`；不修改全局结构。
3. `copy_mm(...)` 返回负值：返回该负值；先 `put_kstack(proc)` 再 `kfree(proc)`；不修改全局结构，不留下 hash/list 节点。注意：本模块 `copy_mm` 固定返回 `0`，该路径保留但不应触发。
4. `copy_thread(...)`：返回 `void`，不提供失败路径；前置条件不满足时用 `assert`。
5. 挂接阶段：`hash_proc`/`list_add` 本身无失败返回值；一旦执行，后续不存在“挂接失败”分支。若实现 AI 在挂接后引入新失败点，必须在返回前从 hash 桶删除、从 `proc_list` 删除、`nr_process--`、`put_kstack`、`kfree`。
6. PID 回滚：按 A10 不执行，允许 PID 空洞。
7. `wakeup_proc` 断言：若新 PCB 的 `state` 被提前置为 `PROC_RUNNABLE` 会 `panic`；必须保证调用前 `state == PROC_UNINIT`。
8. 中断保护：PID 分配、hash 挂接、`proc_list` 挂接、`nr_process++` 必须在同一个 `local_intr_save/restore` 临界区内完成；`wakeup_proc` 必须在挂接完成后调用。

**可验证断言/测试映射**
- `proc_init` 创建的 initproc：`parent == idleproc`、`pid == 1`、`state == PROC_RUNNABLE`。
- 成功返回后，用返回值调用 `find_proc(pid)` 必须返回同一个 PCB；若返回 `NULL`，说明 `hash_proc` 未执行或 `hash_list` 未初始化。
- 成功返回后，`proc_list` 中应能遍历到该 PCB。
- 失败路径检查：`do_fork` 返回负值后，`nr_process` 与调用前相同；对应 hash 桶中不存在该 PCB；`proc_list` 中不存在该 PCB；`kstack` 未被泄漏。
- 不得以“返回负值但已部分挂接”作为可接受结果。

#### 5. `static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)`

**前置状态**
- `proc->kstack` 已由 `setup_kstack` 分配，范围 `[kstack, kstack + KSTACKSIZE)`，`KSTACKSIZE == 8192`。
- `tf` 可为 `NULL` 或指向调用方栈上的 `struct trapframe`。
- `esp == 0` 表示使用新内核栈内的有效地址。
- 调用时 `proc->tf` 尚未设置（由 `alloc_proc` 清零为 `NULL`）。

**后置效果**
- `proc->tf` 指向新内核栈内的完整 `struct trapframe`（288 字节），必须位于 `[proc->kstack, proc->kstack + KSTACKSIZE)` 内且 8 字节对齐，不得越界。推荐放在 `proc->kstack + KSTACKSIZE - sizeof(struct trapframe)`，但实现可选择等价合法放置。
- 若 `tf != NULL`：`proc->tf` 内容是 `*tf` 的副本。
- 若 `tf == NULL`：`proc->tf` 指向全零 `trapframe`（按 A7）。
- `proc->tf->gpr.sp` 必须为非零有效栈地址：`esp != 0` 时使用 `esp`；`esp == 0` 时使用新内核栈内的有效地址，不得为 0。
- `proc->context.ra == (uintptr_t)forkret`。
- `proc->context.sp == (uintptr_t)proc->tf`，且非零。
- `proc->context` 其余字段为 `0`（或保持 `alloc_proc` 的清零结果）。
- 按 A13，若 `tf` 来自用户态 fork，子进程 `tf->gpr.a0` 应清零；本实验 `kernel_thread` 路径由 `kernel_thread_entry` 用 `s1` 覆盖 `a0`，不影响评测。
- 不修改 `proc->state`、`proc->pid`、`proc->parent`、`proc->mm`、`nr_process`、任何全局链表。
- 不分配/释放页，不修改页表，不刷新 TLB。

**分支场景**
- `tf != NULL`（评测覆盖）：完整复制 `trapframe`；`gpr.s0`、`gpr.s1`、`epc`、`status` 来自 `tf`；`gpr.sp` 按上述规则落实。
- `tf == NULL`（按 A7 不纳入评测）：清零 `trapframe`，至少保证 `gpr.sp`、`context.ra`、`context.sp` 有效。
- `esp == 0`：必须回退到新内核栈内的有效地址。
- `esp != 0`：使用 `esp`，但仍需保证该地址属于可用栈空间。

**可验证断言/测试映射**
- `test_lab4_process_create`：`initproc->context.ra != 0 && initproc->context.sp != 0`。
- 首次调度链验证：`switch_to` 恢复 `context.ra/sp` 后必须进入 `forkret`；`forkret` 使用 `current->tf`，因此 `proc->tf` 必须是完整 `trapframe`；`__trapret` 按偏移 `32/33/8/9/2` 取 `status`、`epc`、`s0`、`s1`、`sp`，副本布局不能错位。
- `init_main` 能真正执行并打印 `this initproc, pid = 1, name = "init"`，说明 `epc`、`status`、`sp`、`s0/s1` 均正确。
- 若 `tf->gpr.sp == 0` 或 `context.sp == 0`，首次函数调用/栈使用会崩溃，测试无法通过。

#### 6. `static int copy_mm(uint32_t clone_flags, struct proc_struct *proc)`

**前置状态**
- `proc` 非 `NULL`；`proc->mm == NULL`；本阶段 `current->mm == NULL`；不要求 `proc->pgdir` 有值。

**后置效果**
- 固定返回 `0`。
- `proc->mm` 保持 `NULL`。
- 不分配 `mm_struct`，不调用 `mm_create`，不复制页表，不分配页，不修改 `proc->pgdir`（按 A11 保持 `0`），不刷新 TLB。
- 不修改 `current`、`proc_list`、`hash_list`、`nr_process`。
- 对 `CLONE_VM` / `CLONE_THREAD`：仍按占位处理，不实现真正共享/复制语义；无论标志位如何，在现有评测路径下保持 `proc->mm == NULL` 并返回 `0`。

**分支场景**
- 正常调用（`clone_flags == 0`，评测路径）：返回 `0`，`proc->mm == NULL`。
- 传入 `CLONE_VM` / `CLONE_THREAD`：仍返回 `0`，`proc->mm == NULL`；不得引用未定义的 `check_mm_struct`（只有 extern 声明，没有定义，引用会导致链接失败）。
- 本阶段不得引入失败分支，不得返回负值。`do_fork` 的通用负值清理路径保留，但本模块 `copy_mm` 不应触发。
- 本函数不执行 `page_insert`，不存在“重复地址/已有映射”分支；若实现 AI 引入页表操作，属于超出本模块范围，必须停止并报告。

**可验证断言/测试映射**
- `test_lab4_process_create`：`initproc->mm == NULL`。
- 若 `copy_mm` 擅自创建 mm 或复制页表，该断言立即失败。
- 代码审查点：`copy_mm` 中不得出现 `mm_create`、`page_insert`、`get_pte`、`tlb_invalidate`、`check_mm_struct` 等超出占位范围的调用。
```

#### 实现迭代过程

本模块的实现经历了 [N] 次迭代，过程如下：

***

##### 第一次迭代

如果设计的prompt生成代码后出现问题：

**遇到的问题：**

- 问题1：问题描述、错误信息和原因分析
- 问题2：问题描述、错误信息和原因分析

**问题解决策略：**

- 针对问题1：[说明如何修改提示词]
- 针对问题2：[说明如何修改提示词]

如果没有问题：

**最终结果：**

经过 [N] 次迭代，该模块最终：

- ✅ 编译通过
- ✅ 所有相关测试通过
- ✅ 功能符合预期

**关键改进点总结（如有）：**

1. [改进点1：例如，在 [RELY] 中补充了完整的系统调用号定义]
2. [改进点2：例如，在 [SPECIFICATION] 中明确了异常处理的边界条件]
3. [改进点3：例如，增加了对用户指针合法性检查的要求]

***

##### 第二次迭代（如有）

[按照和第一次迭代相同的格式]

***

##### 第三次迭代（如有）

[按照和第一次迭代相同的格式]

***

### 功能模块三：进程调度

**负责人：** 2414113-焦程远

#### 模块功能描述

**需要实现/修改的函数：**

```c
void cpu_idle(void);                                     // kern/process/proc.c
void proc_run(struct proc_struct *proc);                 // kern/process/proc.c
void schedule(void);                                     // kern/schedule/sched.c
void switch_to(struct context *from, struct context *to);// kern/process/switch.S
```

#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

```markdown
[PROMPT]
编辑proc.c、sched.c、switch.s文件，实现进程调度

[RELY]
可以参考的函数和结构体，包括：`struct proc_struct`、
`struct context`、`read_csr()`、`write_csr()`等。
可用接口还包括：`proc_list`、`current`、`idleproc`、`PROC_RUNNABLE`、
`need_resched`、`runs`、`list_next()`、`le2proc()`、`local_intr_save()`、
`local_intr_restore()`、`boot_pgdir_pa`、`SATP_MODE_SV39`、`RISCV_PGSHIFT`、
`flush_tlb()`。`switch_to` 的声明已在 proc.c 中给出：
`void switch_to(struct context *from, struct context *to);`

[GUARANTEE]
要求实现的函数：
cpu_idle()
schedule()
proc_run()
switch_to()

[SPECIFICATION]
cpu_idle()持续等待调度需求,是闲线程的执行主体，
schedule()从进程链表中选下一个可执行线程
proc_run()在安全的临界区内切换当前线程、页表和执行现场
switch_to()负责最底层的寄存器上下文保存与恢复
切换过程必须保持原子性。`schedule` 和 `proc_run` 都需要在关键区域内避免被中断打断，否则当前线程、页表和实际执行现场之间可能出现不一致。`schedule` 负责选择正确的 runnable 线程，并在找不到可运行线程时退回 `idleproc`；`proc_run` 负责更新 `current`、切换页表并调用 `switch_to`；`switch_to` 只做上下文保存和恢复，不参与调度策略判断。

补充规格（跨模块契约与调度语义）：
1. 所有内核线程共享内核地址空间。模块二的 `alloc_proc` 将 `pgdir` 置 0
   （表示尚未建立独立页表），因此 `proc_run` 中若 `next->pgdir == 0`，
   必须沿用内核启动页表 `boot_pgdir_pa`，不能把 satp 写成 0；将来用户进程
   `pgdir` 非 0 时再切换其独立页表。satp 写入形式：
   `(SATP_MODE_SV39 << 60) | (pgdir >> RISCV_PGSHIFT)`，写后必须 `flush_tlb()`。
2. `schedule` 查找下一个 `PROC_RUNNABLE` 线程时必须**跳过 idleproc**
   （idleproc 只作兜底）：一旦切入 idleproc，其 `need_resched` 已被清 0，
   `cpu_idle` 将空转等待，永远等不到新的调度请求，造成死锁。
   只有除 idleproc 外找不到其他就绪线程时，才退回 idleproc。
```

#### 实现迭代过程

本模块的实现经历了 3 次迭代，过程如下：

***

##### 第一次迭代

**遇到的问题：**

- 问题1：内核启动后停在 `++ setup timer interrupts`，无任何后续输出，也没有 panic 信息。
  原因分析：模块二的 `alloc_proc` 将 `pgdir` 初始化为 0（模块二显式假设 A11），
  而第一版 `proc_run` 无条件执行 `write_csr(satp, Sv39 | (next->pgdir >> 12))`，
  把 satp 指向物理地址 0 处的"空页表"。切换后下一条指令取指即发生页错误，
  而 stvec 指向的 trap 入口本身也是虚拟地址，翻译再次失败，陷入页错误死循环，
  因此屏幕上没有任何输出。

**问题解决策略：**

- 针对问题1：在提示词的 [SPECIFICATION] 中补充跨模块契约（见"补充规格"第 1 条）：
  明确 `pgdir == 0` 表示内核线程共享内核页表，`proc_run` 此时应写入 `boot_pgdir_pa`。
  修改 `proc_run` 后，initproc 首次运行链打通，输出
  `this initproc, pid = 1, name = "init"`、`To U: "Hello world!!".`。

##### 第二次迭代

**遇到的问题：**

- 问题2：官方评分注入测试运行到 `[Test Process Create] PASS` 后卡死，
  `[Test Schedule] PASS` 与 initproc 的输出消息都不出现，但时钟中断仍在正常打印 `100 ticks`。
  原因分析：`idleproc` 的 state 也是 `PROC_RUNNABLE`（`test_lab4_process_create` 还专门断言了这一点）。
  第一版 `schedule` 从 current 的下一个节点轮转时，initproc 的下一个恰好是 idleproc，
  于是调度器选中 idleproc 并 `proc_run` 切入；而 idleproc 的 `need_resched`
  在 schedule 开头已被清 0，`cpu_idle` 只能永远空转等待调度请求，
  整个系统死锁（中断照常响应，所以 ticks 继续增长）。

**问题解决策略：**

- 针对问题2：在提示词的 [SPECIFICATION] 中补充调度语义（见"补充规格"第 2 条）：
  查找时必须跳过 idleproc，只有找不到其他就绪线程时才退回 idleproc。
  修改 `schedule` 后，测试线程 thread1/thread2 与 initproc 正常轮转，
  `[Test Schedule] PASS` 出现。

##### 第三次迭代

**最终结果：**

经过 3 次迭代，该模块最终：

- ✅ 编译通过
- ✅ 官方评分测试全部通过：`[Test PMM] PASS`、`[Test Process Create] PASS`、`[Test Schedule] PASS`
- ✅ QEMU 实测 initproc 首次运行链正常：输出 `this initproc, pid = 1, name = "init"`、
  `To U: "Hello world!!".`，随后 `do_exit` panic（`process exit!!`，预期行为）
- ✅ 功能符合预期

**关键改进点总结：**

1. 在 [RELY] 中补充了 `boot_pgdir_pa`、`SATP_MODE_SV39`、`RISCV_PGSHIFT`、`flush_tlb()` 等模块三可用的接口；
2. 在 [SPECIFICATION] 中补充了「`pgdir == 0` 时沿用内核启动页表」的跨模块契约；
3. 在 [SPECIFICATION] 中补充了「idleproc 只作兜底、查找时跳过」的调度语义。

***
