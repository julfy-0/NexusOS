/*
 * NexusOS — регистрация ISR-обработчиков в IDT и общая точка входа
 * isr_handler(), в которую прыгает isr_common_stub из isr.S.
 */
#include "isr.h"
#include "idt.h"
#include "gdt.h"
#include <kernel/panic.h>
#include <lib/stdio.h>

static const char *exception_messages[32] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Into Detected Overflow", "Out of Bounds", "Invalid Opcode",
    "No Coprocessor", "Double Fault", "Coprocessor Segment Overrun",
    "Bad TSS", "Segment Not Present", "Stack Fault",
    "General Protection Fault", "Page Fault", "Unknown Interrupt",
    "Coprocessor Fault", "Alignment Check", "Machine Check",
    "SIMD Floating-Point", "Virtualization", "Control Protection",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Hypervisor Injection", "VMM Communication",
    "Security", "Reserved"
};

void isr_init(void) {
    /* Flags 0x8E = present, ring 0, 32-bit interrupt gate */
    idt_set_gate(0,  (u32)isr0,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(1,  (u32)isr1,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(2,  (u32)isr2,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(3,  (u32)isr3,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(4,  (u32)isr4,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(5,  (u32)isr5,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(6,  (u32)isr6,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(7,  (u32)isr7,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(8,  (u32)isr8,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(9,  (u32)isr9,  GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(10, (u32)isr10, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(11, (u32)isr11, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(12, (u32)isr12, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(13, (u32)isr13, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(14, (u32)isr14, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(15, (u32)isr15, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(16, (u32)isr16, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(17, (u32)isr17, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(18, (u32)isr18, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(19, (u32)isr19, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(20, (u32)isr20, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(21, (u32)isr21, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(22, (u32)isr22, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(23, (u32)isr23, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(24, (u32)isr24, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(25, (u32)isr25, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(26, (u32)isr26, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(27, (u32)isr27, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(28, (u32)isr28, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(29, (u32)isr29, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(30, (u32)isr30, GDT_KERNEL_CODE_SEL, 0x8E);
    idt_set_gate(31, (u32)isr31, GDT_KERNEL_CODE_SEL, 0x8E);
}

/* Вызывается из isr_common_stub (isr.S) для каждого исключения CPU */
void isr_handler(registers_t regs) {
    const char *msg = "Unknown Exception";
    if (regs.int_no < 32) {
        msg = exception_messages[regs.int_no];
    }
    kprintf("\n[EXCEPTION] #%u: %s (err_code=%x)\n", regs.int_no, msg, regs.err_code);
    panic("Unhandled CPU exception");
}
