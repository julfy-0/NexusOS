/*
 * NexusOS — Interrupt Descriptor Table.
 * Заполняется обработчиками из isr.c (исключения CPU) и irq.c
 * (аппаратные прерывания через PIC).
 */
#include "idt.h"
#include "gdt.h"
#include <nexus/types.h>
#include <lib/string.h>

struct NX_PACKED idt_entry {
    u16 base_low;
    u16 selector;
    u8  zero;
    u8  flags;
    u16 base_high;
};

struct NX_PACKED idt_ptr {
    u16 limit;
    u32 base;
};

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

extern void idt_flush(u32 idt_ptr_addr);

void idt_set_gate(u8 num, u32 handler, u16 selector, u8 flags) {
    idt[num].base_low  = (u16)(handler & 0xFFFF);
    idt[num].base_high = (u16)((handler >> 16) & 0xFFFF);
    idt[num].selector  = selector;
    idt[num].zero      = 0;
    idt[num].flags     = flags;
}

void idt_init(void) {
    idtp.limit = (u16)(sizeof(struct idt_entry) * IDT_ENTRIES - 1);
    idtp.base  = (u32)&idt;

    memset(&idt, 0, sizeof(idt));

    idt_flush((u32)&idtp);
}

__asm__ (
    ".global idt_flush\n"
    "idt_flush:\n"
    "    mov 4(%esp), %eax\n"
    "    lidt (%eax)\n"
    "    ret\n"
);
