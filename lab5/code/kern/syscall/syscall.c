#include <unistd.h>
#include <proc.h>
#include <syscall.h>
#include <trap.h>
#include <stdio.h>
#include <pmm.h>
#include <vmm.h>
#include <error.h>
#include <assert.h>


// 9 个系统调用处理函数原型：syscalls[] 在函数定义之前引用它们，
// 而 kern/syscall/syscall.h 只声明了 syscall()，故必须在此提前声明
static int sys_exit(uint64_t arg[]);
static int sys_fork(uint64_t arg[]);
static int sys_wait(uint64_t arg[]);
static int sys_exec(uint64_t arg[]);
static int sys_yield(uint64_t arg[]);
static int sys_kill(uint64_t arg[]);
static int sys_getpid(uint64_t arg[]);
static int sys_putc(uint64_t arg[]);
static int sys_pgdir(uint64_t arg[]);

static int (*syscalls[])(uint64_t arg[]) = {
    [SYS_exit]              sys_exit,
    [SYS_fork]              sys_fork,
    [SYS_wait]              sys_wait,
    [SYS_exec]              sys_exec,
    [SYS_yield]             sys_yield,
    [SYS_kill]              sys_kill,
    [SYS_getpid]            sys_getpid,
    [SYS_putc]              sys_putc,
    [SYS_pgdir]             sys_pgdir,
};

#define NUM_SYSCALLS        ((sizeof(syscalls)) / (sizeof(syscalls[0])))

//lab5 function1 [2413094]

// syscall - 分发用户态系统调用：
//   先把 tf 保存到局部变量，处理完把返回值写回该局部变量；
//   不能在处理函数返回后使用 current->tf，因为 do_wait/do_yield 可能已切换进程。
void
syscall(void)
{
    struct trapframe *tf = current->tf;
    uint64_t num = tf->gpr.a0;
    uint64_t arg[5];

    if (num >= NUM_SYSCALLS || syscalls[num] == NULL)
    {
        print_trapframe(tf);
        panic("undefined syscall %lu, pid = %d, name = %s.\n",
              (unsigned long)num, current->pid, current->name);
    }

    arg[0] = tf->gpr.a1;
    arg[1] = tf->gpr.a2;
    arg[2] = tf->gpr.a3;
    arg[3] = tf->gpr.a4;
    arg[4] = tf->gpr.a5;

    tf->gpr.a0 = syscalls[num](arg);
}

static int
sys_exit(uint64_t arg[])
{
    return do_exit((int)arg[0]);
}

static int
sys_fork(uint64_t arg[])
{
    return do_fork(0, 0, current->tf);
}

static int
sys_wait(uint64_t arg[])
{
    int pid = (int)arg[0];
    int *store = (int *)arg[1];

    if (store != NULL && !user_mem_check(current->mm, (uintptr_t)store, sizeof(int), 1))
    {
        return -E_INVAL;
    }
    return do_wait(pid, store);
}

static int
sys_exec(uint64_t arg[])
{
    const char *name = (const char *)arg[0];
    size_t len = (size_t)arg[1];
    unsigned char *binary = (unsigned char *)arg[2];
    size_t size = (size_t)arg[3];

    if (name == NULL || (uintptr_t)name + len < (uintptr_t)name ||
        !user_mem_check(current->mm, (uintptr_t)name, len, 0))
    {
        return -E_INVAL;
    }
    if (binary == NULL || (uintptr_t)binary + size < (uintptr_t)binary ||
        !user_mem_check(current->mm, (uintptr_t)binary, size, 0))
    {
        return -E_INVAL;
    }
    return do_execve(name, len, binary, size);
}

static int
sys_yield(uint64_t arg[])
{
    do_yield();
    return 0;
}

static int
sys_kill(uint64_t arg[])
{
    return do_kill((int)arg[0]);
}

static int
sys_getpid(uint64_t arg[])
{
    return current->pid;
}

static int
sys_putc(uint64_t arg[])
{
    cputchar((int)arg[0]);
    return 0;
}

static int
sys_pgdir(uint64_t arg[])
{
    if (current->mm == NULL)
    {
        return -E_INVAL;
    }
    cprintf("pgdir = %p\n", current->mm->pgdir);
    return 0;
}
