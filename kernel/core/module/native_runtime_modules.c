#include "module.h"
#include "runtime.h"
#include "ipc.h"

static void module_nop_exit(void) { }
static const char *const runtime_dependencies[] = { "ipc" };
static int module_ipc_init(void) { return nexus_ipc_init(); }
static int module_runtime_init(void) { return nexus_runtime_init() && nexus_ipc_ready(); }

NEXUS_MODULE("ipc", "1.0", NEXUS_MODULE_SERVICE, 520, module_ipc_init, module_nop_exit);
NEXUS_MODULE_WITH_DEPS("runtime", "1.0", NEXUS_MODULE_SERVICE, 530, module_runtime_init, module_nop_exit, runtime_dependencies);
