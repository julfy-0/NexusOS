#include "watchdog.h"

/* This is a software stall detector, not a hardware watchdog. The PIT IRQ
 * remains capture-only; it only samples whether normal kernel context has
 * progressed recently. The main loop performs the actual stop transition. */
#define WATCHDOG_TIMEOUT_MS 3000ULL

static volatile uint64_t g_tick;
static volatile uint64_t g_last_heartbeat_tick;
static volatile uint64_t g_heartbeats;
static volatile uint64_t g_trips;
static volatile uint32_t g_timer_hz = 100;
static volatile int g_ready;
static volatile int g_trip_pending;

void nexus_watchdog_init(uint32_t timer_hz) {
    g_tick = 0;
    g_last_heartbeat_tick = 0;
    g_heartbeats = 0;
    g_trips = 0;
    g_timer_hz = timer_hz ? timer_hz : 100;
    g_ready = 1;
    g_trip_pending = 0;
}

void nexus_watchdog_heartbeat(void) {
    if (!g_ready) return;
    g_last_heartbeat_tick = g_tick;
    g_heartbeats++;
}

void nexus_watchdog_timer_tick(void) {
    if (!g_ready) return;
    g_tick++;

    uint64_t timeout_ticks = ((uint64_t)g_timer_hz * WATCHDOG_TIMEOUT_MS) / 1000ULL;
    if (timeout_ticks == 0) timeout_ticks = 1;

    if (!g_trip_pending && g_tick > g_last_heartbeat_tick + timeout_ticks) {
        g_trip_pending = 1;
        g_trips++;
    }
}

int nexus_watchdog_trip_pending(void) { return g_trip_pending != 0; }
void nexus_watchdog_clear_trip(void) { g_trip_pending = 0; }
uint64_t nexus_watchdog_heartbeats(void) { return g_heartbeats; }
uint64_t nexus_watchdog_last_heartbeat_tick(void) { return g_last_heartbeat_tick; }
uint64_t nexus_watchdog_trip_count(void) { return g_trips; }
