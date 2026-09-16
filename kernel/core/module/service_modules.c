#include "module.h"
#include "font_service.h"

static int module_font_init(void) { return nexus_font_service_init(); }
static void module_nop_exit(void) { }

NEXUS_MODULE("font", "1.0", NEXUS_MODULE_SERVICE, 500, module_font_init, module_nop_exit);
