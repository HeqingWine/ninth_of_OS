# 通用 RISC-V 裸机实验 Makefile
# 用法（在任一 lab 目录内）:
#   make        -> 编译出 app.elf
#   make run    -> 编译并用 QEMU 运行（程序需自行触发 sifive_test 关机，见 hello.c 的 poweroff）
# 新 lab 目录只需: 复制本文件 + start.s + linker.ld + 自己的 *.c

CROSS := e:/opt/riscv/bin/riscv64-unknown-elf-gcc
QEMU  := e:/opt/qemu/qemu-system-riscv64

TARGET := app
CFLAGS := -nostdlib -nostartfiles -march=rv64imac -mabi=lp64 -mcmodel=medany
LDFLAGS := -T linker.ld

SRCS_C := $(wildcard *.c)
SRCS_S := $(wildcard *.s)
OBJS   := $(SRCS_C:.c=.o) $(SRCS_S:.s=.o)

.PHONY: all run clean

all: $(TARGET).elf

$(TARGET).elf: $(OBJS) linker.ld
	$(CROSS) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CROSS) $(CFLAGS) -c -o $@ $<

%.o: %.s
	$(CROSS) $(CFLAGS) -c -o $@ $<

run: $(TARGET).elf
	$(QEMU) -machine virt -nographic -bios none -kernel $(TARGET).elf

clean:
	-del /q $(subst /,\,$(OBJS)) $(TARGET).elf 2>nul
