#ifndef NEXUSOS_WATCHDOG_H
#define NEXUSOS_WATCHDOG_H

#include <stdint.h>

void nexus_watchdog_init(uint32_t timer_hz);
void nexus_watchdog_heartbeat(void);
void nexus_watchdog_timer_tick(void);
int nexus_watchdog_trip_pending(void);
void nexus_watchdog_clear_trip(void);
uint64_t nexus_watchdog_heartbeats(void);
uint64_t nexus_watchdog_last_heartbeat_tick(void);
uint64_t nexus_watchdog_trip_count(void);

#endif
