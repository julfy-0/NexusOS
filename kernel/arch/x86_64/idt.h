#ifndef NEXUSOS_IDT_H
#define NEXUSOS_IDT_H

#include <stdint.h>

/* Порядок полей соответствует тому, как регистры складываются на стек
 * в isr_common_stub (kernel/isr.S). Не переставлять без пересмотра asm! */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector, err_code;
    uint64_t rip, cs, rflags, user_rsp, ss;
} __attribute__((packed)) interrupt_frame_t;

void idt_init(void);

/* Вызывается из isr_common_stub для любого прерывания/исключения */
void isr_handler(interrupt_frame_t *frame);

#endif
