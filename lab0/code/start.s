# h0: 入口汇编 —— 设置栈指针后跳入 C 的 main
    .section .text.start
    .globl _start
_start:
    la   sp, _stack_top
    call main
hang:
    j    hang            # main 返回后死循环

    .section .bss
    .align 4
_stack_bottom:
    .skip 4096           # 4KB 栈空间
    .globl _stack_top
_stack_top:
