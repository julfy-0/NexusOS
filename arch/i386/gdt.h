/*
 * NexusOS — Global Descriptor Table.
 * Используем «плоскую» модель памяти: два сегмента (код/данные),
 * каждый покрывает все 4 GiB адресного пространства. Реальную
 * изоляцию задач мы потом строим через paging, а не через сегменты.
 */
#ifndef NEXUS_ARCH_I386_GDT_H
#define NEXUS_ARCH_I386_GDT_H

#include <nexus/types.h>

#define GDT_ENTRIES 5

/* Селекторы сегментов, которые будет использовать остальной код ядра */
#define GDT_KERNEL_CODE_SEL 0x08
#define GDT_KERNEL_DATA_SEL 0x10
#define GDT_USER_CODE_SEL   0x18
#define GDT_USER_DATA_SEL   0x20

void gdt_init(void);

#endif /* NEXUS_ARCH_I386_GDT_H */
