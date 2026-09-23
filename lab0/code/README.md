# h0: 环境验证 —— 最小裸机 Hello World

验证 `riscv64-unknown-elf-gcc` 交叉编译 + QEMU 运行 riscv64 程序。

## 文件

| 文件 | 作用 |
| ---- | ---- |
| `hello.c` | C 代码：向 QEMU virt 的 UART0（MMIO 地址 `0x10000000`）写字节输出，末尾通过 sifive_test 设备触发 QEMU 自动退出 |
| `start.s` | 入口汇编：设置栈指针（4KB 栈）后调用 `main`，之后死循环 |
| `Makefile` | 通用构建脚本：`make` 编译、`make run` 运行、`make clean` 清理，可复制到任意新 lab 目录 |
| `linker.ld` | 链接脚本：入口 `_start`，程序链接到复位地址 `0x80000000` |

## 使用步骤

### 方式一：make（推荐，支持任意 lab 目录）

```sh
make          # 编译 → 生成 app.elf
make run      # 编译 + QEMU 运行（输出后自动退出）
make clean    # 清理 .o 和 app.elf
```

### 方式二：手动命令

1. **交叉编译**（x86 主机上生成 riscv64 机器码）：

```sh
riscv64-unknown-elf-gcc -nostdlib -nostartfiles \
    -march=rv64imac -mabi=lp64 -mcmodel=medany \
    -T linker.ld -o app.elf start.s hello.c
```

2. **查看 ELF**（确认是 RISC-V 架构、入口地址 0x80000000）：

```sh
riscv64-unknown-elf-readelf -h app.elf
```

3. **在 QEMU 中运行**（`-bios none`：不加载 OpenSBI，直接加载 ELF 裸跑）：

```sh
qemu-system-riscv64 -machine virt -nographic -bios none -kernel app.elf
```

预期输出：

```
Hello, RISC-V!
cross-compile + QEMU works.
```

退出：`hello.c` 末尾会向 sifive_test 测试设备（`0x100000`）写入 `0x5555` 触发 QEMU 自动关机，因此程序输出后 QEMU 会自行退出（退出码 0），无需手动干预。

## VS Code 一键编译/运行

已配置 `../.vscode/tasks.json` 与键盘绑定，在 VS Code 中：

- **一键编译并运行**：按 **Ctrl+Alt+R**（会先自动编译，再启动 QEMU，输出后自动退出）
- 任务会弹出输入框询问实验目录名，默认值=当前打开文件所在目录名，直接回车即可
- **仅编译**：`Ctrl+Shift+B` → `riscv64: 编译当前 lab`
- 编译错误会以 `$gcc` 格式显示在“问题”面板，可直接点击跳转到出错行

### 新实验目录（如 lab3）一键运行

只需把 `Makefile`、`start.s`、`linker.ld` 复制到新目录（如 `e:\opt\lab3`），
放入自己的 `*.c` 源码，打开其中任意文件后按 **Ctrl+Alt+R** 即可；
任务输入框默认已填好当前目录名（lab3），直接回车。

## 说明

- `-march=rv64imac`：目标指令集为 RV64I+MA+C（与 QEMU virt 默认一致）
- `-mabi=lp64`：64 位裸机 ABI（无浮点）
- `-mcmodel=medany`：中距离代码模型，适合 0x80000000 这类绝对地址链接
- QEMU virt 机器复位后从 `0x80000000` 开始执行，故链接地址与之一致
