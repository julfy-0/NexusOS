#include "power.h"
#include "system.h"
int nexus_power_init(void){return 1;}
void nexus_power_shutdown(void){nexus_system_request_shutdown();}
void nexus_power_reboot(void){nexus_system_request_reboot();}
