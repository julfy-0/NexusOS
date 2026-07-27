/* NexusOS: 8253/8254 Programmable Interval Timer, канал 0 -> IRQ0. */
#include "pit.h"
#include "io.h"

#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND        0x43
#define PIT_BASE_FREQUENCY 1193182u /* Гц, частота внутреннего генератора PIT */

static volatile uint64_t g_ticks = 0;
static uint32_t g_frequency = 100;

void pit_init(uint32_t frequency_hz) {
    if (frequency_hz == 0) frequency_hz = 100;
    g_frequency = frequency_hz;

    uint32_t divisor = PIT_BASE_FREQUENCY / frequency_hz;
    if (divisor == 0) divisor = 1;
    if (divisor > 0xFFFF) divisor = 0xFFFF; /* 16-битный делитель, 0 == 65536 */

    outb(PIT_COMMAND, 0x36); /* channel 0, lobyte/hibyte, mode 3 (square wave), binary */
    outb(PIT_CHANNEL0_DATA, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_DATA, (uint8_t)((divisor >> 8) & 0xFF));

    g_ticks = 0;
}

void pit_handle_irq(void) {
    g_ticks++;
}

uint64_t pit_get_ticks(void) {
    return g_ticks;
}

uint64_t pit_get_uptime_seconds(void) {
    return g_ticks / g_frequency;
}

uint64_t pit_get_uptime_ms(void) {
    return (g_ticks * 1000ULL) / g_frequency;
}

uint32_t pit_get_frequency_hz(void) {
    return g_frequency;
}
