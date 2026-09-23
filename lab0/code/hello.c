/* h0: 最小裸机程序 —— 通过 UART 串口输出 */
/* QEMU virt 机器的 UART0 MMIO 基地址 */
#define UART0_BASE 0x10000000UL
/* sifive_test 测试设备：写入 0x5555 触发 QEMU 关机退出 */
#define TEST_BASE 0x100000UL

static void poweroff(void)
{
    *(volatile unsigned int *)TEST_BASE = 0x5555;
}

static void uart_putc(char c)
{
    /* 向 UART 发送寄存器写入一个字节 */
    *(volatile unsigned char *)UART0_BASE = c;
}

static void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

int main(void)
{
    uart_puts("Hello, RISC-V!\n");
    uart_puts("cross-compile + QEMU works.\n");
    poweroff(); /* 触发 QEMU 自动退出 */
    return 0;
}
