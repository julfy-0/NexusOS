#include "system.h"
#include "init.h"
#include "state.h"
#include "app_manager.h"
#include "package_manager.h"
#include "session.h"
#include "power.h"
#include "system_info.h"

int nexus_system_init(void) {
    nexus_system_set_state(NEXUS_SYSTEM_BOOTING);
    if (!nexus_system_bootstrap()) return 0;
    nexus_session_init();
    nexus_power_init();
    nexus_system_info_init();
    nexus_app_manager_init();
    nexus_app_register(&(nexus_app_t){"nexus.files", "Files", "builtin:files", 1});
    nexus_app_register(&(nexus_app_t){"nexus.terminal", "Terminal", "builtin:terminal", 1});
    nexus_app_register(&(nexus_app_t){"nexus.settings", "Settings", "builtin:settings", 1});
    nexus_package_manager_init();
    nexus_system_set_state(NEXUS_SYSTEM_CLI);
    return 1;
}
void nexus_system_enter_cli(void){nexus_system_set_state(NEXUS_SYSTEM_CLI);}
void nexus_system_enter_desktop(void){nexus_system_set_state(NEXUS_SYSTEM_DESKTOP);}
void nexus_system_enter_application(void){nexus_system_set_state(NEXUS_SYSTEM_APPLICATION);}
void nexus_system_request_shutdown(void){nexus_system_set_state(NEXUS_SYSTEM_SHUTDOWN);}
void nexus_system_request_reboot(void){nexus_system_set_state(NEXUS_SYSTEM_REBOOT);}
nexus_system_state_t nexus_system_get_state(void){return nexus_system_state();}
