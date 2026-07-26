/*
 * NexusOS — драйвер программируемого интервального таймера (PIT, 8253/8254).
 * Настраивается на IRQ0 через arch/i386/irq.c.
 */
#include "timer.h"
#include <arch/i386/io.h>
#include <arch/i386/irq.h>

#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND       0x43
#define PIT_BASE_FREQ     1193182u

static volatile u32 ticks = 0;

static void timer_callback(registers_t *regs) {
    NX_UNUSED(regs);
    ticks++;
}

void timer_init(u32 frequency) {
    irq_install_handler(0, timer_callback);

    u32 divisor = PIT_BASE_FREQ / frequency;

    outb(PIT_COMMAND, 0x36); /* channel 0, lo/hi byte, mode 3 (square wave) */
    outb(PIT_CHANNEL0_DATA, (u8)(divisor & 0xFF));
    outb(PIT_CHANNEL0_DATA, (u8)((divisor >> 8) & 0xFF));
}

u32 timer_ticks(void) {
    return ticks;
}
