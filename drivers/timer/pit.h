#ifndef NEXUSOS_PIT_H
#define NEXUSOS_PIT_H

#include <stdint.h>

/* Настраивает PIT channel 0 на генерацию IRQ0 с заданной частотой. */
void pit_init(uint32_t frequency_hz);

/* Вызывается из isr_handler на каждый тик (IRQ0 -> вектор 32). */
void pit_handle_irq(void);

uint64_t pit_get_ticks(void);
uint64_t pit_get_uptime_seconds(void);
uint64_t pit_get_uptime_ms(void);

/* Частота, на которую реально настроен channel 0 (то, что передали в
 * pit_init()) — нужна как опорная для калибровки TSC (cpu.c). */
uint32_t pit_get_frequency_hz(void);

#endif
