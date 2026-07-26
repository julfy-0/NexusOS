/* NexusOS: 8259 PIC.
 *
 * По умолчанию после сброса PIC шлёт IRQ0-7 на векторы 0x08-0x0F — это
 * прямо поверх CPU-исключений (#DE, #DB, ...). Первым делом после
 * настройки IDT его нужно перепрограммировать (remap) на 32-47.
 */
#include "pic.h"
#include "io.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01

void pic_remap(void) {
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4); io_wait();

    outb(PIC1_DATA, PIC1_OFFSET); io_wait();   /* ICW2: базовый вектор master */
    outb(PIC2_DATA, PIC2_OFFSET); io_wait();   /* ICW2: базовый вектор slave  */

    outb(PIC1_DATA, 4); io_wait();  /* ICW3: слейв сидит на IRQ2 у мастера */
    outb(PIC2_DATA, 2); io_wait();  /* ICW3: номер каскада у слейва        */

    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}

void pic_set_mask(uint8_t irq_line, int masked) {
    uint16_t port = irq_line < 8 ? PIC1_DATA : PIC2_DATA;
    uint8_t line = irq_line < 8 ? irq_line : irq_line - 8;
    uint8_t value = inb(port);
    if (masked) value |= (1 << line);
    else value &= ~(1 << line);
    outb(port, value);
}
