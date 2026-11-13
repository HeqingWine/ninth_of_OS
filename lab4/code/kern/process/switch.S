//LAB4 function3 [2414113]
.text
.globl switch_to
switch_to:
    # a0 = &from->context, a1 = &to->context
    # 保存 from 的 14 个 callee-saved 寄存器（ra, sp, s0~s11）
    sd ra, 0*8(a0)
    sd sp, 1*8(a0)
    sd s0, 2*8(a0)
    sd s1, 3*8(a0)
    sd s2, 4*8(a0)
    sd s3, 5*8(a0)
    sd s4, 6*8(a0)
    sd s5, 7*8(a0)
    sd s6, 8*8(a0)
    sd s7, 9*8(a0)
    sd s8, 10*8(a0)
    sd s9, 11*8(a0)
    sd s10, 12*8(a0)
    sd s11, 13*8(a0)
    # 恢复 to 的 14 个 callee-saved 寄存器
    ld ra, 0*8(a1)
    ld sp, 1*8(a1)
    ld s0, 2*8(a1)
    ld s1, 3*8(a1)
    ld s2, 4*8(a1)
    ld s3, 5*8(a1)
    ld s4, 6*8(a1)
    ld s5, 7*8(a1)
    ld s6, 8*8(a1)
    ld s7, 9*8(a1)
    ld s8, 10*8(a1)
    ld s9, 11*8(a1)
    ld s10, 12*8(a1)
    ld s11, 13*8(a1)
    # 返回地址已经是 to 线程的 ra
    ret