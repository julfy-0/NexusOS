/*
 * NexusOS — remap PIC (8259) на векторы 32-47 и диспетчеризация
 * аппаратных прерываний зарегистрированным драйверам (таймер,
 * клавиатура и т.д. через irq_install_handler).
 */
#include "irq.h"
#include "idt.h"
#include "gdt.h"
#include "io.h"
#include <lib/string.h>

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI   0x20

static irq_handler_t irq_routines[16];

static void pic_remap(void) {
    u8 mask1 = inb(PIC1_DATA);
    u8 mask2 = inb(PIC2_DATA);

    outb(PIC1_CMD, 0x11); io_wait();  /* init, cascade mode */
    outb(PIC2_CMD, 0x11); io_wait();

    outb(PIC1_DATA, 0x20); io_wait(); /* master IRQ0 -> vector 32 */
    outb(PIC2_DATA, 0x28); io_wait(); /* slave  IRQ8 -> vector 40 */

    outb(PIC1_DATA, 0x04); io_wait(); /* master: slave подключён на IRQ2 */
    outb(PIC2_DATA, 0x02); io_wait(); /* slave: свой номер каскада */

    outb(PIC1_DATA, 0x01); io_wait(); /* режим 8086/88 */
    outb(PIC2_DATA, 0x01); io_wait();

    outb(PIC1_DATA, mask1);           /* восстанавливаем маски */
    outb(PIC2_DATA, mask2);
}

void irq_install_handler(u8 irq, irq_handler_t handler) {
    irq_routines[irq] = handler;
}

void irq_uninstall_handler(u8 irq) {
    irq_routines[irq] = 0;
}

void irq_init(void) {
    memset(irq_routines, 0, sizeof(irq_routines));
    pic_remap();

    idt_set_gate(32, (u32)irq0,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(33, (u32)irq1,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(34, (u32)irq2,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(35, (u32)irq3,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(36, (u32)irq4,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(37, (u32)irq5,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(38, (u32)irq6,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(39, (u32)irq7,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(40, (u32)irq8,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(41, (u32)irq9,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(42, (u32)irq10, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(43, (u32)irq11, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(44, (u32)irq12, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(45, (u32)irq13, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(46, (u32)irq14, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(47, (u32)irq15, GDT_KERNEL_CODE_SEL, 0x8E);
}

/* Вызывается из irq_common_stub (irq.S) для каждого IRQ 0-15 */
void irq_handler(registers_t regs) {
    u8 irq_num = (u8)(regs.int_no - 32);

    if (irq_routines[irq_num] != 0) {
        irq_routines[irq_num](&regs);
    }

    /* Подтверждаем прерывание контроллеру(ам) */
    if (irq_num >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    outb(PIC1_CMD, PIC_EOI);
}
