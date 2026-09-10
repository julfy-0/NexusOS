#ifndef NEXUSOS_SYSTEM_H
#define NEXUSOS_SYSTEM_H

#include "state.h"

int nexus_system_init(void);
void nexus_system_enter_cli(void);
void nexus_system_enter_desktop(void);
void nexus_system_enter_application(void);
void nexus_system_request_shutdown(void);
void nexus_system_request_reboot(void);
nexus_system_state_t nexus_system_get_state(void);

#endif
