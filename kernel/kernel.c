/*
 * NexusOS — kernel.c
 * Точка входа ядра на C. Сюда прыгает boot/boot.S сразу после того,
 * как настроен стек. Отсюда по порядку инициализируются все
 * подсистемы: GDT -> IDT/ISR/IRQ -> драйверы -> память -> "shell".
 */
#include "kernel.h"
#include "panic.h"

#include <arch/i386/gdt.h>
#include <arch/i386/idt.h>
#include <arch/i386/isr.h>
#include <arch/i386/irq.h>

#include <drivers/vga/vga.h>
#include <drivers/serial/serial.h>
#include <drivers/keyboard/keyboard.h>
#include <drivers/timer/timer.h>

#include <mm/pmm.h>
#include <lib/stdio.h>

#include <nexus/multiboot.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* Определены в arch/i386/linker.ld — адреса конца ядра в памяти */
extern u32 kernel_end;

static void print_banner(void) {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("=========================================\n");
    kprintf(" %s v%s -- freestanding x86 kernel\n", NEXUSOS_NAME, NEXUSOS_VERSION);
    kprintf("=========================================\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

void kernel_main(u32 magic, u32 mbi_addr) {
    vga_init();
    serial_init();

    print_banner();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        panic("Invalid Multiboot magic number — загрузчик не GRUB/Multiboot");
    }

    kprintf("[boot] Multiboot magic OK (0x%x)\n", magic);

    kprintf("[init] GDT...\n");
    gdt_init();

    kprintf("[init] IDT...\n");
    idt_init();

    kprintf("[init] ISR (обработчики исключений CPU)...\n");
    isr_init();

    kprintf("[init] IRQ (PIC remap)...\n");
    irq_init();

    kprintf("[init] Timer (PIT, 100 Hz)...\n");
    timer_init(100);

    kprintf("[init] Keyboard...\n");
    keyboard_init();

    /* Прерывания включаем только после того, как все обработчики
     * зарегистрированы — иначе можно получить IRQ на несуществующий стаб. */
    __asm__ volatile ("sti");

    kprintf("[init] Physical Memory Manager...\n");
    pmm_init((multiboot_info_t *)mbi_addr, (u32)&kernel_end);

    kprintf("\nNexusOS готов. Нажимай клавиши — они появятся ниже:\n\n");

    for (;;) {
        char c = keyboard_getchar();
        if (c) {
            kprintf("%c", c);
        }
        __asm__ volatile ("hlt"); /* спим до следующего прерывания, не жжём CPU */
    }
}
