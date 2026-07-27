/* NexusOS: IDT + диспетчер прерываний.
 * Векторы 0-31: исключения CPU (паникуем и виснем).
 * Векторы 32-47: IRQ0-15 от PIC (обрабатываем нужные, шлём EOI). */
#include <stdint.h>
#include "idt.h"
#include "gdt.h"
#include "console.h"
#include "pic.h"
#include "keyboard.h"
#include "pit.h"

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[256];
static idt_ptr_t idtp;

#define IDT_TYPE_INTERRUPT_GATE 0x8E /* present, DPL0, 64-bit interrupt gate */

/* Заглушки из isr.S */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);
extern void isr32(void); extern void isr33(void); extern void isr34(void); extern void isr35(void);
extern void isr36(void); extern void isr37(void); extern void isr38(void); extern void isr39(void);
extern void isr40(void); extern void isr41(void); extern void isr42(void); extern void isr43(void);
extern void isr44(void); extern void isr45(void); extern void isr46(void); extern void isr47(void);

static void *isr_table[48] = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
    isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39,
    isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47,
};

static const char *exception_names[32] = {
    "Divide-by-zero", "Debug", "NMI", "Breakpoint",
    "Overflow", "Bound range exceeded", "Invalid opcode", "Device not available",
    "Double fault", "Coprocessor segment overrun", "Invalid TSS", "Segment not present",
    "Stack-segment fault", "General protection fault", "Page fault", "Reserved",
    "x87 FP exception", "Alignment check", "Machine check", "SIMD FP exception",
    "Virtualization exception", "Control protection exception", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor injection", "VMM communication", "Security exception", "Reserved",
};

static void idt_set_gate(int n, void *handler, uint16_t selector, uint8_t flags) {
    uint64_t addr = (uint64_t)(uintptr_t)handler;
    idt[n].offset_low  = addr & 0xFFFF;
    idt[n].selector    = selector;
    idt[n].ist         = 0;
    idt[n].type_attr   = flags;
    idt[n].offset_mid  = (addr >> 16) & 0xFFFF;
    idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[n].reserved    = 0;
}

void idt_init(void) {
    for (int i = 0; i < 48; i++) {
        idt_set_gate(i, isr_table[i], GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT_GATE);
    }

    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint64_t)(uintptr_t)idt;
    __asm__ volatile ("lidt (%0)" : : "r"(&idtp));
}

/* Page fault (vector 14) — единственное исключение, у которого error
 * code и CR2 реально расшифровываются на что-то полезное для человека,
 * поэтому у него отдельная диагностика, а не общий exception_names[]. */
#define PF_ERR_PRESENT   0x01 /* 0 = страницы не было вообще, 1 = была, но нарушение прав */
#define PF_ERR_WRITE     0x02 /* 0 = чтение, 1 = запись */
#define PF_ERR_USER      0x04 /* 0 = кольцо 0, 1 = кольцо 3 (у нас пока всегда 0, user mode ещё нет) */
#define PF_ERR_RESERVED  0x08 /* 1 = в самой page table записан мусор в reserved-битах */
#define PF_ERR_INSTR_FETCH 0x10 /* 1 = fault случился при выборке инструкции (NX), у нас NX не включён — всегда 0 */

static void print_page_fault_details(interrupt_frame_t *f) {
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

    console_print("Page fault (vector 14, error code ");
    console_print_hex(f->err_code);
    console_print(")\n\n  Faulting address (CR2): ");
    console_print_hex(cr2);
    console_print("\n  Cause: ");
    console_print((f->err_code & PF_ERR_PRESENT) ? "protection violation" : "page not present");
    console_print(", ");
    console_print((f->err_code & PF_ERR_WRITE) ? "write" : "read");
    console_print(", ");
    console_print((f->err_code & PF_ERR_USER) ? "user mode" : "kernel mode");
    if (f->err_code & PF_ERR_RESERVED) {
        console_print(", reserved bit set in page table entry (corrupt table!)");
    }
    if (f->err_code & PF_ERR_INSTR_FETCH) {
        console_print(", instruction fetch");
    }
    console_print("\n\n  RIP: ");
    console_print_hex(f->rip);
    console_print("   CS: ");
    console_print_hex(f->cs);
    console_print("\n  RFLAGS: ");
    console_print_hex(f->rflags);
    console_print("\n\n  System halted.\n");
}

static void panic_screen(interrupt_frame_t *f) {
    console_set_color(COLOR_WHITE, 0x1D1035); /* тёмно-фиолетовый фон, как BSOD/panic-экран */
    console_print("\n\n  *** NexusOS KERNEL PANIC ***\n\n  ");

    if (f->vector == 14) {
        print_page_fault_details(f);
        return;
    }

    if (f->vector < 32) {
        console_print(exception_names[f->vector]);
    } else {
        console_print("Unknown exception");
    }
    console_print(" (vector ");
    console_print_dec(f->vector);
    console_print(", error code ");
    console_print_hex(f->err_code);
    console_print(")\n\n  RIP: ");
    console_print_hex(f->rip);
    console_print("   CS: ");
    console_print_hex(f->cs);
    console_print("\n  RFLAGS: ");
    console_print_hex(f->rflags);
    console_print("\n\n  System halted.\n");
}

void isr_handler(interrupt_frame_t *frame) {
    if (frame->vector < 32) {
        panic_screen(frame);
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    if (frame->vector == 32) { /* IRQ0 = таймер */
        pit_handle_irq();
    }
    if (frame->vector == 33) { /* IRQ1 = клавиатура */
        keyboard_handle_irq();
    }

    pic_send_eoi((uint8_t)(frame->vector - 32));
}
