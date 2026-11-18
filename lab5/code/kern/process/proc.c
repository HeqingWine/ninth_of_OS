#include <proc.h>
#include <kmalloc.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc,
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:

  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  +
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : get the process's pid

*/

// the process set's list
list_entry_t proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);

// 前置声明：这些函数定义在文件后半段，但前面的 lab4 代码需要调用
static struct proc_struct *alloc_proc(void);
static void forkret(void);
static int init_main(void *arg);

//lab4 请补充你在lab4中的代码
// copy_thread - 在新进程的内核栈上建立 trapframe 与 context，
//               使其首次被调度时能够运行 trapframe 中指定的函数
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)
{
    assert(proc != NULL && proc->kstack != 0);
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    if (tf != NULL)
    {
        *(proc->tf) = *tf;
    }
    else
    {
        memset(proc->tf, 0, sizeof(struct trapframe));
    }
    // 新线程必须有有效栈：esp 为 0 时回退到自身内核栈内的地址
    proc->tf->gpr.sp = (esp != 0) ? esp : (uintptr_t)proc->tf;
    // 子进程从 fork 返回 0；kernel_thread 路径下 a0 会被 kernel_thread_entry 用参数覆盖
    proc->tf->gpr.a0 = 0;
    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)proc->tf;
}

// proc_init - 初始化进程管理链表，创建 0 号 idleproc 与 1 号 initproc 并设置 current
void
proc_init(void)
{
    int i, pid;

    list_init(&proc_list);
    for (i = 0; i < HASH_LIST_SIZE; i++)
    {
        list_init(hash_list + i);
    }

    if ((idleproc = alloc_proc()) == NULL)
    {
        panic("cannot alloc idleproc.\n");
    }

    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->need_resched = 1;
    set_proc_name(idleproc, "idle");
    list_add(&proc_list, &(idleproc->list_link));
    nr_process++;
    current = idleproc;

    pid = kernel_thread(init_main, "Hello world!!", 0);
    if (pid <= 0)
    {
        panic("create init_main failed.\n");
    }

    initproc = find_proc(pid);
    if (initproc == NULL)
    {
        panic("cannot find initproc.\n");
    }
    set_proc_name(initproc, "init");
}

// cpu_idle - 空闲线程的执行主体：持续检测调度需求并让出 CPU
void
cpu_idle(void)
{
    while (1)
    {
        if (current->need_resched)
        {
            schedule();
        }
    }
}

// proc_run - 在关中断的临界区内完成 current、页表和执行现场的切换
void
proc_run(struct proc_struct *proc)
{
    if (proc != current)
    {
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;
        local_intr_save(intr_flag);
        {
            current = next;
            // 切换页表：所有内核线程共享内核地址空间，pgdir 为 0
            // 表示尚未建立独立页表（模块二 alloc_proc 置 0），此时沿用
            // 内核启动页表 boot_pgdir_pa；将来用户进程 pgdir 非 0 时走各自页表。
            uintptr_t pgdir = (next->pgdir != 0) ? next->pgdir : boot_pgdir_pa;
            // satp = Sv39 模式位 | 页表根物理地址 >> 12
            write_csr(satp, ((uintptr_t)SATP_MODE_SV39 << 60) | (pgdir >> RISCV_PGSHIFT));
            flush_tlb();
            switch_to(&(prev->context), &(next->context));
        }
        local_intr_restore(intr_flag);
    }
}



//lab5 function2 [2411617]
// update alloc_proc
// alloc_proc - alloc a proc_struct and init all fields of proc_struct
static struct proc_struct *
alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL)
    {
        memset(proc, 0, sizeof(struct proc_struct));
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = 0;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;
        proc->tf = NULL;
        proc->pgdir = 0;
        proc->flags = 0;
        memset(proc->name, 0, sizeof(proc->name));
        proc->exit_code = 0;
        proc->wait_state = 0;
        proc->cptr = proc->yptr = proc->optr = NULL;
    }
    return proc;
}


// set_proc_name - set the name of proc
char *
set_proc_name(struct proc_struct *proc, const char *name)
{
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
char *
get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// set_links - set the relation links of process
static void
set_links(struct proc_struct *proc)
{
    list_add(&proc_list, &(proc->list_link));
    proc->yptr = NULL;
    if ((proc->optr = proc->parent->cptr) != NULL)
    {
        proc->optr->yptr = proc;
    }
    proc->parent->cptr = proc;
    nr_process++;
}

// remove_links - clean the relation links of process
static void
remove_links(struct proc_struct *proc)
{
    list_del(&(proc->list_link));
    if (proc->optr != NULL)
    {
        proc->optr->yptr = proc->yptr;
    }
    if (proc->yptr != NULL)
    {
        proc->yptr->optr = proc->optr;
    }
    else
    {
        proc->parent->cptr = proc->optr;
    }
    nr_process--;
}

// get_pid - alloc a unique pid for process
static int
get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++last_pid >= MAX_PID)
    {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid)
            {
                if (++last_pid >= next_safe)
                {
                    if (last_pid >= MAX_PID)
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid)
            {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}






// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
static void
forkret(void)
{
    forkrets(current->tf);
}

// hash_proc - add proc into proc hash_list
static void
hash_proc(struct proc_struct *proc)
{
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// unhash_proc - delete proc from proc hash_list
static void
unhash_proc(struct proc_struct *proc)
{
    list_del(&(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct *
find_proc(int pid)
{
    if (0 < pid && pid < MAX_PID)
    {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list)
        {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid)
            {
                return proc;
            }
        }
    }
    return NULL;
}

//lab5 function2 [2411617]
// update kernel_thread
// kernel_thread - create a kernel thread using `kernel_thread_entry`
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));
    tf.gpr.s0 = (uintptr_t)fn;
    tf.gpr.s1 = (uintptr_t)arg;
    // 新线程初始运行在 S 态，进入 kernel_thread_entry 后调用 fn(arg)
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
    tf.epc = (uintptr_t)kernel_thread_entry;
    return do_fork(clone_flags, 0, &tf);
}


// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
static int
setup_kstack(struct proc_struct *proc)
{
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL)
    {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - free the memory space of process kernel stack
static void
put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// setup_pgdir - alloc one page as PDT
static int
setup_pgdir(struct mm_struct *mm)
{
    struct Page *page;
    if ((page = alloc_page()) == NULL)
    {
        return -E_NO_MEM;
    }
    pde_t *pgdir = page2kva(page);
    memcpy(pgdir, boot_pgdir_va, PGSIZE);

    mm->pgdir = pgdir;
    return 0;
}

// put_pgdir - free the memory space of PDT
static void
put_pgdir(struct mm_struct *mm)
{
    free_page(kva2page(mm->pgdir));
}

//lab5 function3 YOUR CODE update copy_mm
// copy_mm - 为子进程建立地址空间：
//   父进程没有用户 mm（内核线程）时子进程同样为空；
//   clone_flags 含 CLONE_VM 时共享父进程 mm；
//   否则新建 mm + 页表，并在锁内 dup_mmap 逐 vma 复制
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    struct mm_struct *mm, *oldmm = current->mm;

    // current is a kernel thread
    if (oldmm == NULL)
    {
        return 0;
    }
    if (clone_flags & CLONE_VM)
    {
        mm = oldmm;
        goto good_mm;
    }

    int ret = -E_NO_MEM;
    if ((mm = mm_create()) == NULL)
    {
        goto bad_mm;
    }
    if (setup_pgdir(mm) != 0)
    {
        goto bad_pgdir_cleanup_mm;
    }

    lock_mm(oldmm);
    {
        ret = dup_mmap(mm, oldmm);
    }
    unlock_mm(oldmm);

    if (ret != 0)
    {
        goto bad_dup_cleanup_pgdir;
    }

good_mm:
    mm_count_inc(mm);
    proc->mm = mm;
    proc->pgdir = PADDR(mm->pgdir);
    return 0;
bad_dup_cleanup_pgdir:
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    return ret;
}



//lab5 function2 [2411617]
static int user_main(void *arg);

// init_main - the second kernel thread used to create user_main kernel threads
static int
init_main(void *arg)
{
    (void)arg;
    size_t nr_free_pages_store = nr_free_pages();
    size_t kernel_allocated_store = kallocated();

    int pid = kernel_thread(user_main, NULL, 0);
    if (pid <= 0)
    {
        panic("create user_main failed.\n");
    }

    while (do_wait(0, NULL) == 0)
    {
        schedule();
    }

    cprintf("all user-mode processes have quit.\n");
    assert(nr_free_pages_store == nr_free_pages());
    assert(kernel_allocated_store == kallocated());
    cprintf("init check memory pass.\n");
    return 0;
}

// do_wait - wait one OR any children with PROC_ZOMBIE, and return its exit code
int
do_wait(int pid, int *code_store)
{
    struct proc_struct *proc;
    bool intr_flag;
    bool haskid;
    if (code_store != NULL)
    {
        if (!user_mem_check(current->mm, (uintptr_t)code_store, sizeof(int), 1))
        {
            return -E_INVAL;
        }
    }

repeat:
    haskid = 0;
    if (pid != 0)
    {
        proc = find_proc(pid);
        if (proc != NULL && proc->parent == current)
        {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE)
            {
                goto found;
            }
        }
    }
    else
    {
        for (proc = current->cptr; proc != NULL; proc = proc->optr)
        {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE)
            {
                goto found;
            }
        }
    }
    if (!haskid)
    {
        return -E_BAD_PROC;
    }
    current->state = PROC_SLEEPING;
    current->wait_state = WT_CHILD;
    schedule();
    if (current->flags & PF_EXITING)
    {
        do_exit(-E_KILLED);
    }
    goto repeat;

found:
    if (proc == idleproc || proc == initproc)
    {
        panic("wait idleproc or initproc.\n");
    }
    if (code_store != NULL)
    {
        *code_store = proc->exit_code;
    }
    local_intr_save(intr_flag);
    {
        unhash_proc(proc);
        remove_links(proc);
    }
    local_intr_restore(intr_flag);
    put_kstack(proc);
    kfree(proc);
    return 0;
}


//lab5 function3 YOUR CODE
// do_fork - 复制当前进程：PCB、内核栈、地址空间与 trapframe，
//           并把子进程接入哈希表与进程树后唤醒
int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS)
    {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    if ((proc = alloc_proc()) == NULL)
    {
        goto fork_out;
    }

    proc->parent = current;

    if (setup_kstack(proc) != 0)
    {
        goto bad_fork_cleanup_proc;
    }
    if (copy_mm(clone_flags, proc) != 0)
    {
        goto bad_fork_cleanup_kstack;
    }
    // 用户态 fork 时 stack 为 0，必须显式继承父进程的用户栈指针，
    // 否则 copy_thread 会回退到子进程内核栈地址，U 态访栈会 page fault
    if (stack == 0 && tf != NULL)
    {
        stack = tf->gpr.sp;
    }
    copy_thread(proc, stack, tf);

    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        set_links(proc);
    }
    local_intr_restore(intr_flag);

    wakeup_proc(proc);

    ret = proc->pid;
fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}


//lab5 function4 YOUR CODE
// do_exit - 当前进程退出：回收地址空间、转僵尸态、唤醒父进程、
//           把子进程过继给 initproc，最后让出 CPU 不再返回
int
do_exit(int error_code)
{
    if (current == idleproc)
    {
        panic("idleproc exit.\n");
    }
    if (current == initproc)
    {
        panic("initproc exit.\n");
    }

    struct mm_struct *mm = current->mm;
    if (mm != NULL)
    {
        // 先切回内核启动页表，再回收用户地址空间
        write_csr(satp, ((uintptr_t)SATP_MODE_SV39 << 60) |
                            (boot_pgdir_pa >> RISCV_PGSHIFT));
        flush_tlb();
        if (mm_count_dec(mm) == 0)
        {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
        }
        current->mm = NULL;
    }
    current->exit_code = error_code;

    bool intr_flag;
    struct proc_struct *proc;
    local_intr_save(intr_flag);
    {
        proc = current->parent;
        if (proc->wait_state == WT_CHILD)
        {
            wakeup_proc(proc);
        }
        while (current->cptr != NULL)
        {
            proc = current->cptr;
            current->cptr = proc->optr;

            proc->yptr = NULL;
            if ((proc->optr = initproc->cptr) != NULL)
            {
                initproc->cptr->yptr = proc;
            }
            proc->parent = initproc;
            initproc->cptr = proc;
            if (proc->state == PROC_ZOMBIE)
            {
                if (initproc->wait_state == WT_CHILD)
                {
                    wakeup_proc(initproc);
                }
            }
        }
        current->state = PROC_ZOMBIE;
    }
    local_intr_restore(intr_flag);

    schedule();
    panic("do_exit will not return!! %d.\n", current->pid);
}

// do_yield - 主动让出 CPU：置 need_resched，由 trap 返回路径统一调度
int
do_yield(void)
{
    current->need_resched = 1;
    return 0;
}

// do_kill - 终止指定进程：置 PF_EXITING，等待态可中断时将其唤醒
int
do_kill(int pid)
{
    struct proc_struct *proc;
    if ((proc = find_proc(pid)) != NULL)
    {
        bool intr_flag;
        local_intr_save(intr_flag);
        {
            if (!(proc->flags & PF_EXITING))
            {
                proc->flags |= PF_EXITING;
                if (proc->wait_state & WT_INTERRUPTED)
                {
                    wakeup_proc(proc);
                }
            }
        }
        local_intr_restore(intr_flag);
        return 0;
    }
    return -E_INVAL;
}


#define __KERNEL_EXECVE(name, binary, size) ({           \
    cprintf("kernel_execve: pid = %d, name = \"%s\".\n", \
            current->pid, name);                         \
    kernel_execve(name, binary, (size_t)(size));         \
})

#define KERNEL_EXECVE(x) ({                                    \
    extern unsigned char _binary_obj___user_##x##_out_start[], \
        _binary_obj___user_##x##_out_size[];                   \
    __KERNEL_EXECVE(#x, _binary_obj___user_##x##_out_start,    \
                    _binary_obj___user_##x##_out_size);        \
})

#define __KERNEL_EXECVE2(x, xstart, xsize) ({   \
    extern unsigned char xstart[], xsize[];     \
    __KERNEL_EXECVE(#x, xstart, (size_t)xsize); \
})

#define KERNEL_EXECVE2(x, xstart, xsize) __KERNEL_EXECVE2(x, xstart, xsize)


//lab5 function2 [2411617]
// load_icode - load the elf binary into current process's address space
static int
load_icode(unsigned char *binary, size_t size)
{
    if (current->mm != NULL)
    {
        panic("load_icode: current->mm must be empty.\n");
    }

    int ret = -E_NO_MEM;
    struct mm_struct *mm = mm_create();
    if (mm == NULL)
    {
        return -E_NO_MEM;
    }

    if ((ret = setup_pgdir(mm)) != 0)
    {
        mm_destroy(mm);
        return ret;
    }

    // 校验 ELF 头与程序头表
    struct elfhdr elf;
    if (size < sizeof(struct elfhdr))
    {
        ret = -E_INVAL_ELF;
        goto bad_elf;
    }
    memcpy(&elf, binary, sizeof(struct elfhdr));
    if (elf.e_magic != ELF_MAGIC)
    {
        ret = -E_INVAL_ELF;
        goto bad_elf;
    }
    if (elf.e_phentsize != sizeof(struct proghdr) ||
        elf.e_phoff + (uint64_t)elf.e_phnum * elf.e_phentsize > (uint64_t)size)
    {
        ret = -E_INVAL_ELF;
        goto bad_elf;
    }

    uint32_t i, perm;
    uint32_t vm_flags;
    for (i = 0; i < elf.e_phnum; i++)
    {
        struct proghdr ph;
        memcpy(&ph, binary + elf.e_phoff + i * elf.e_phentsize,
               sizeof(struct proghdr));
        if (ph.p_type != ELF_PT_LOAD || ph.p_memsz == 0)
        {
            continue;
        }
        if (ph.p_filesz > ph.p_memsz || ph.p_offset + ph.p_filesz > size)
        {
            ret = -E_INVAL_ELF;
            goto failed_cleanup;
        }

        vm_flags = 0;
        perm = PTE_U;
        if (ph.p_flags & ELF_PF_R)
        {
            vm_flags |= VM_READ;
            perm |= PTE_R;
        }
        if (ph.p_flags & ELF_PF_W)
        {
            vm_flags |= VM_WRITE;
            perm |= PTE_W;
        }
        if (ph.p_flags & ELF_PF_X)
        {
            vm_flags |= VM_EXEC;
            perm |= PTE_X;
        }
        if (vm_flags & VM_WRITE)
        {
            vm_flags |= VM_READ;
        }
        if (perm & PTE_W)
        {
            perm |= PTE_R;
        }

        if ((ret = mm_map(mm, ph.p_va, ph.p_memsz, vm_flags, NULL)) != 0)
        {
            goto failed_cleanup;
        }

        // 按页装载：整页清零后拷贝文件内容，剩余部分作为 BSS
        uintptr_t la = ROUNDDOWN(ph.p_va, PGSIZE);
        uintptr_t end = ROUNDUP(ph.p_va + ph.p_memsz, PGSIZE);
        for (; la < end; la += PGSIZE)
        {
            struct Page *page = pgdir_alloc_page(mm->pgdir, la, perm);
            if (page == NULL)
            {
                ret = -E_NO_MEM;
                goto failed_cleanup;
            }
            memset(page2kva(page), 0, PGSIZE);
            uintptr_t s = (la > ph.p_va) ? la : ph.p_va;
            uintptr_t e = la + PGSIZE;
            uintptr_t file_end = ph.p_va + ph.p_filesz;
            if (e > file_end)
            {
                e = file_end;
            }
            if (s < e)
            {
                memcpy(page2kva(page) + (s - la),
                       binary + ph.p_offset + (s - ph.p_va), e - s);
            }
        }
    }

    // 建立用户栈及其 vma
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE,
                      VM_READ | VM_WRITE | VM_STACK, NULL)) != 0)
    {
        goto failed_cleanup;
    }
    for (i = 0; i < USTACKPAGE; i++)
    {
        if (pgdir_alloc_page(mm->pgdir, USTACKTOP - (i + 1) * PGSIZE,
                             PTE_U | PTE_R | PTE_W) == NULL)
        {
            ret = -E_NO_MEM;
            goto failed_cleanup;
        }
    }

    // 接管新地址空间
    mm_count_inc(mm);
    current->mm = mm;
    current->pgdir = PADDR(mm->pgdir);
    write_csr(satp, ((uintptr_t)SATP_MODE_SV39 << 60) |
                        (current->pgdir >> RISCV_PGSHIFT));
    flush_tlb();

    // 改写当前进程的 trapframe，使其 sret 后进入用户程序
    struct trapframe *tf = current->tf;
    memset(tf, 0, sizeof(struct trapframe));
    tf->status = (read_csr(sstatus) & ~(SSTATUS_SPP | SSTATUS_SIE)) | SSTATUS_SPIE;
    tf->epc = elf.e_entry;
    tf->gpr.sp = USTACKTOP;
    return 0;

bad_elf:
    put_pgdir(mm);
    mm_destroy(mm);
    return ret;

failed_cleanup:
    exit_mmap(mm);
    put_pgdir(mm);
    mm_destroy(mm);
    return ret;
}

// do_execve - free current mm, then load the elf binary and run it
int
do_execve(const char *name, size_t len, unsigned char *binary, size_t size)
{
    struct mm_struct *mm = current->mm;

    if (mm != NULL && !user_mem_check(mm, (uintptr_t)name, len, 0))
    {
        return -E_INVAL;
    }

    // 名字截断到 PROC_NAME_LEN，并保证 NUL 结尾
    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));
    memcpy(local_name, name, (len < PROC_NAME_LEN) ? len : PROC_NAME_LEN);

    if (mm != NULL)
    {
        // 回收旧用户地址空间前先切回内核页表
        write_csr(satp, ((uintptr_t)SATP_MODE_SV39 << 60) |
                            (boot_pgdir_pa >> RISCV_PGSHIFT));
        flush_tlb();
        if (mm_count_dec(mm) == 0)
        {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
        }
        current->mm = NULL;
        current->pgdir = 0;
    }

    int ret = load_icode(binary, size);
    if (ret != 0)
    {
        do_exit(ret);
        return ret;
    }
    set_proc_name(current, local_name);
    return 0;
}

// kernel_execve - do the sys_exec work in kernel, then return to user mode
static int
kernel_execve(const char *name, unsigned char *binary, size_t size)
{
    int ret = do_execve(name, strlen(name) + 1, binary, size);
    if (ret != 0)
    {
        return ret;
    }
    // load_icode 已把 current->tf 改成用户态入口现场，经 trap 返回路径进入 U 态
    forkrets(current->tf);
    panic("kernel_execve: forkrets returned.\n");
}

// user_main - kernel thread used to exec a user program
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