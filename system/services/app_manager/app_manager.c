#include "app_manager.h"
#include "system.h"

#define NEXUS_APP_MAX 16

static nexus_app_t g_apps[NEXUS_APP_MAX];
static nexus_app_runtime_t g_runtime[NEXUS_APP_MAX];
static int g_count;
static int g_active = -1;

static int eq(const char *a, const char *b) {
    int i = 0;
    if (!a || !b) return 0;
    while (a[i] && b[i] && a[i] == b[i]) ++i;
    return a[i] == 0 && b[i] == 0;
}

static int index_of(const char *id) {
    if (!id) return -1;
    for (int i = 0; i < g_count; ++i)
        if (eq(g_apps[i].id, id)) return i;
    return -1;
}

int nexus_app_manager_init(void) {
    g_count = 0;
    g_active = -1;
    for (int i = 0; i < NEXUS_APP_MAX; ++i) {
        g_apps[i].id = 0;
        g_apps[i].name = 0;
        g_apps[i].entry = 0;
        g_apps[i].builtin = 0;
        g_apps[i].source = NEXUS_APP_SOURCE_BUILTIN;
        g_runtime[i].launch_count = 0;
        g_runtime[i].state = NEXUS_APP_STOPPED;
        g_runtime[i].source = NEXUS_APP_SOURCE_BUILTIN;
    }
    return 1;
}

int nexus_app_register(const nexus_app_t *app) {
    if (!app || !app->id || !app->name || !app->entry || g_count >= NEXUS_APP_MAX)
        return -1;
    if (index_of(app->id) >= 0) return -1;

    g_apps[g_count] = *app;
    g_runtime[g_count].launch_count = 0;
    g_runtime[g_count].state = NEXUS_APP_STOPPED;
    g_runtime[g_count].source = app->source;
    return g_count++;
}

int nexus_app_register_builtin(const char *id, const char *name, const char *entry) {
    nexus_app_t app = { id, name, entry, 1, NEXUS_APP_SOURCE_BUILTIN };
    return nexus_app_register(&app);
}

int nexus_app_count(void) { return g_count; }

const nexus_app_t *nexus_app_at(int index) {
    return (index >= 0 && index < g_count) ? &g_apps[index] : 0;
}

const nexus_app_t *nexus_app_find(const char *id) {
    int i = index_of(id);
    return i >= 0 ? &g_apps[i] : 0;
}

const nexus_app_runtime_t *nexus_app_runtime(const char *id) {
    int i = index_of(id);
    return i >= 0 ? &g_runtime[i] : 0;
}

int nexus_app_launch(const char *id) {
    int i = index_of(id);
    if (i < 0) return 0;

    /* The current milestone manages application state only. Actual ELF/user
     * process execution remains a later userspace milestone. */
    if (g_active >= 0 && g_active != i)
        g_runtime[g_active].state = NEXUS_APP_STOPPED;

    g_active = i;
    g_runtime[i].state = NEXUS_APP_RUNNING;
    ++g_runtime[i].launch_count;
    nexus_system_enter_application();
    return 1;
}

const nexus_app_t *nexus_app_active(void) {
    return g_active >= 0 ? &g_apps[g_active] : 0;
}

void nexus_app_stop_id(const char *id) {
    int i = index_of(id);
    if (i < 0) return;
    g_runtime[i].state = NEXUS_APP_STOPPED;
    if (g_active == i) {
        g_active = -1;
        nexus_system_enter_desktop();
    }
}

void nexus_app_stop(void) {
    if (g_active >= 0) g_runtime[g_active].state = NEXUS_APP_STOPPED;
    g_active = -1;
    nexus_system_enter_desktop();
}
