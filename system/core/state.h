#ifndef NEXUSOS_SYSTEM_STATE_H
#define NEXUSOS_SYSTEM_STATE_H

typedef enum {
    NEXUS_SYSTEM_BOOTING = 0,
    NEXUS_SYSTEM_CLI,
    NEXUS_SYSTEM_DESKTOP,
    NEXUS_SYSTEM_APPLICATION,
    NEXUS_SYSTEM_SHUTDOWN,
    NEXUS_SYSTEM_REBOOT
} nexus_system_state_t;

nexus_system_state_t nexus_system_state(void);
void nexus_system_set_state(nexus_system_state_t state);
const char *nexus_system_state_name(nexus_system_state_t state);

#endif
