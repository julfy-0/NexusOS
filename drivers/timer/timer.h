#ifndef NEXUS_DRIVERS_TIMER_H
#define NEXUS_DRIVERS_TIMER_H

#include <nexus/types.h>

/* Инициализирует PIT (Programmable Interval Timer) на заданную частоту в Гц */
void timer_init(u32 frequency);

/* Число тиков таймера с момента загрузки */
u32 timer_ticks(void);

#endif /* NEXUS_DRIVERS_TIMER_H */
