#include "system_info.h"
#include "nexus_version.h"
int nexus_system_info_init(void){return 1;}
const char *nexus_system_name(void){return "NexusOS";}
uint32_t nexus_system_major(void){return NEXUS_VERSION_MAJOR;}
uint32_t nexus_system_minor(void){return NEXUS_VERSION_MINOR;}
uint32_t nexus_system_patch(void){return NEXUS_VERSION_PATCH;}
