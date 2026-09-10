#include "state.h"

static nexus_system_state_t g_state = NEXUS_SYSTEM_BOOTING;

nexus_system_state_t nexus_system_state(void) { return g_state; }
void nexus_system_set_state(nexus_system_state_t state) { g_state = state; }

const char *nexus_system_state_name(nexus_system_state_t state) {
    switch (state) {
        case NEXUS_SYSTEM_BOOTING: return "booting";
        case NEXUS_SYSTEM_CLI: return "cli";
        case NEXUS_SYSTEM_DESKTOP: return "desktop";
        case NEXUS_SYSTEM_APPLICATION: return "application";
        case NEXUS_SYSTEM_SHUTDOWN: return "shutdown";
        case NEXUS_SYSTEM_REBOOT: return "reboot";
        default: return "unknown";
    }
}
