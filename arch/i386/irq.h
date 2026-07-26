#ifndef NEXUS_ARCH_I386_IRQ_H
#define NEXUS_ARCH_I386_IRQ_H

#include <nexus/types.h>
#include "isr.h"

typedef void (*irq_handler_t)(registers_t *regs);

void irq_init(void);
void irq_install_handler(u8 irq, irq_handler_t handler);
void irq_uninstall_handler(u8 irq);

/* Ассемблерные стабы IRQ0-IRQ15, определены в irq.S-подобном месте isr.S-стиля */
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

#endif /* NEXUS_ARCH_I386_IRQ_H */
