/* h0: 链接脚本 —— 程序加载到 QEMU virt 机器的复位地址 0x80000000 */
OUTPUT_ARCH(riscv)
ENTRY(_start)

BASE_ADDRESS = 0x80000000;

SECTIONS
{
    . = BASE_ADDRESS;

    .text : {
        *(.text.start)
        *(.text)
    }

    .rodata : { *(.rodata) }

    .data : { *(.data) }1

    .bss : {
        _sbss = .;
        *(.bss)
        _ebss = .;
    }
}
