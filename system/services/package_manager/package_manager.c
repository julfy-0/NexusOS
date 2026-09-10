#include "package_manager.h"
#include "package_manifest.h"

static int g_initialized;
static int g_count;

int nexus_package_manager_init(void) { g_initialized = 1; g_count = 0; return 1; }
int nexus_package_is_package_path(const char *path) {
    int n=0; while(path && path[n]) ++n;
    return n >= 3 && path[n-3]=='.' && path[n-2]=='n' && path[n-1]=='x';
}
int nexus_package_discover(const char *root) { (void)root; if (!g_initialized) nexus_package_manager_init(); return g_count; }
int nexus_package_count(void) { return g_count; }
