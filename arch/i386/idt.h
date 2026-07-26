#ifndef NEXUS_ARCH_I386_IDT_H
#define NEXUS_ARCH_I386_IDT_H

#include <nexus/types.h>

#define IDT_ENTRIES 256

void idt_init(void);
void idt_set_gate(u8 num, u32 handler, u16 selector, u8 flags);

#endif /* NEXUS_ARCH_I386_IDT_H */
