# NexusOS — 中文

NexusOS 是一个面向 x86_64 的轻量级 64 位操作系统，使用 C 语言从零编写。项目采用自定义 UEFI 引导程序和 freestanding 内核。

## 功能
- UEFI/GOP 启动与 framebuffer 控制台
- x86_64 long mode
- GDT、IDT、PIC、PIT 和异常处理
- PS/2 键盘和鼠标支持
- 图形界面基础与命令行
- AHCI/FAT32 存储支持
- PCI 与 USB xHCI 支持基础
- 启动模式选择：Graphic 或 Command Line
- Kernel Panic 诊断界面、日志和 30 秒重启倒计时

## 构建
```bash
make clean
make iso
make run
```
