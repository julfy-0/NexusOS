/*
 * NexusOS — реализация GDT.
 * Загрузка самой таблицы (инструкция LGDT) делается через встроенный
 * ассемблер напрямую тут же, отдельный .S файл для этого избыточен.
 */
#include "gdt.h"
#include <nexus/types.h>

struct NX_PACKED gdt_entry {
    u16 limit_low;
    u16 base_low;
    u8  base_middle;
    u8  access;
    u8  granularity;
    u8  base_high;
};

struct NX_PACKED gdt_ptr {
    u16 limit;
    u32 base;
};

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gdt_pointer;

static void gdt_set_entry(int idx, u32 base, u32 limit, u8 access, u8 gran) {
    gdt[idx].base_low    = (u16)(base & 0xFFFF);
    gdt[idx].base_middle  = (u8)((base >> 16) & 0xFF);
    gdt[idx].base_high    = (u8)((base >> 24) & 0xFF);

    gdt[idx].limit_low    = (u16)(limit & 0xFFFF);
    gdt[idx].granularity  = (u8)((limit >> 16) & 0x0F);

    gdt[idx].granularity |= (gran & 0xF0);
    gdt[idx].access       = access;
}

extern void gdt_flush(u32 gdt_ptr_addr);

void gdt_init(void) {
    gdt_pointer.limit = (u16)(sizeof(struct gdt_entry) * GDT_ENTRIES - 1);
    gdt_pointer.base  = (u32)&gdt;

    /* Нулевой дескриптор — обязательное требование архитектуры x86 */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* Kernel code: base=0, limit=4GiB, 32-bit, ring 0, executable/readable */
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    /* Kernel data: base=0, limit=4GiB, ring 0, read/write */
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    /* User code: ring 3, задел на будущий user-mode */
    gdt_set_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    /* User data: ring 3 */
    gdt_set_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    gdt_flush((u32)&gdt_pointer);
}

/*
 * gdt_flush — загружает GDTR через LGDT и обновляет сегментные регистры.
 * Написана на встроенном ассемблере: перезагрузка CS требует far jump /
 * ret, что не выразить чистым C.
 */
__asm__ (
    ".global gdt_flush\n"
    "gdt_flush:\n"
    "    mov 4(%esp), %eax\n"
    "    lgdt (%eax)\n"
    "    mov $0x10, %ax\n"      /* GDT_KERNEL_DATA_SEL */
    "    mov %ax, %ds\n"
    "    mov %ax, %es\n"
    "    mov %ax, %fs\n"
    "    mov %ax, %gs\n"
    "    mov %ax, %ss\n"
    "    ljmp $0x08, $gdt_flush_after\n"   /* перезагрузка CS */
    "gdt_flush_after:\n"
    "    ret\n"
);
