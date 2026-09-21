#include <stddef.h>
#include <stdint.h>
#include "module.h"
#include "console.h"

extern const nexus_module_descriptor_t __nexus_modules_start[];
extern const nexus_module_descriptor_t __nexus_modules_end[];

static nexus_module_runtime_t g_runtime[NEXUS_MODULE_MAX];
static int g_count;
static int g_loaded;
static int g_failed;
static uint8_t g_dependency_stack[NEXUS_MODULE_MAX];

static int name_equal(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a != *b) return 0;
        ++a; ++b;
    }
    return *a == '\0' && *b == '\0';
}

static void copy_registered_modules(void) {
    g_count = 0;
    g_loaded = 0;
    g_failed = 0;

    const nexus_module_descriptor_t *it = __nexus_modules_start;
    const nexus_module_descriptor_t *end = __nexus_modules_end;
    while (it < end && g_count < NEXUS_MODULE_MAX) {
        if (it->magic == NEXUS_MODULE_MAGIC && it->name && it->init) {
            g_runtime[g_count].descriptor = it;
            g_runtime[g_count].state = NEXUS_MODULE_STATE_DECLARED;
            g_runtime[g_count].result = 0;
            ++g_count;
        }
        ++it;
    }
}

void module_manager_init(void) {
    copy_registered_modules();
}

static nexus_module_runtime_t *find_runtime(const char *name) {
    for (int i = 0; i < g_count; ++i) {
        if (name_equal(g_runtime[i].descriptor->name, name)) return &g_runtime[i];
    }
    return NULL;
}

const nexus_module_runtime_t *module_find(const char *name) {
    return find_runtime(name);
}

static nexus_module_runtime_t *find_next_declared(void) {
    nexus_module_runtime_t *best = NULL;
    for (int i = 0; i < g_count; ++i) {
        nexus_module_runtime_t *r = &g_runtime[i];
        if (r->state != NEXUS_MODULE_STATE_DECLARED) continue;
        if (!best || r->descriptor->priority < best->descriptor->priority) best = r;
    }
    return best;
}

static int module_index(const char *name) {
    for (int i = 0; i < g_count; ++i)
        if (name_equal(g_runtime[i].descriptor->name, name)) return i;
    return -1;
}

static int module_load_index(int index) {
    if (index < 0 || index >= g_count) return 0;
    nexus_module_runtime_t *r = &g_runtime[index];
    if (r->state == NEXUS_MODULE_STATE_LOADED) return 1;
    if (r->state == NEXUS_MODULE_STATE_FAILED || r->state == NEXUS_MODULE_STATE_LOADING) return 0;

    g_dependency_stack[index] = 1;
    for (uint16_t d = 0; d < r->descriptor->dependency_count; ++d) {
        const char *dep = r->descriptor->dependencies[d];
        int dep_index = module_index(dep);
        if (!dep || dep_index < 0 || dep_index == index || g_dependency_stack[dep_index] || !module_load_index(dep_index)) {
            r->state = NEXUS_MODULE_STATE_FAILED;
            r->result = 0;
            ++g_failed;
            g_dependency_stack[index] = 0;
            console_component_status(r->descriptor->name,
                                     r->descriptor->type == NEXUS_MODULE_DRIVER ? "driver" : "service",
                                     r->descriptor->version ? r->descriptor->version : "unknown", 0);
            return 0;
        }
    }

    r->state = NEXUS_MODULE_STATE_LOADING;
    int rc = r->descriptor->init();
    r->result = rc;
    g_dependency_stack[index] = 0;
    if (rc) {
        r->state = NEXUS_MODULE_STATE_LOADED;
        ++g_loaded;
        console_component_status(r->descriptor->name,
                                 r->descriptor->type == NEXUS_MODULE_DRIVER ? "driver" : "service",
                                 r->descriptor->version ? r->descriptor->version : "unknown", 1);
        return 1;
    }

    r->state = NEXUS_MODULE_STATE_FAILED;
    ++g_failed;
    console_component_status(r->descriptor->name,
                             r->descriptor->type == NEXUS_MODULE_DRIVER ? "driver" : "service",
                             r->descriptor->version ? r->descriptor->version : "unknown", 0);
    return 0;
}

int module_load(const char *name) {
    nexus_module_runtime_t *r = find_runtime(name);
    if (!r) return 0;
    if (r->state == NEXUS_MODULE_STATE_LOADED) return 1;
    if (r->state == NEXUS_MODULE_STATE_LOADING) return 0;

    return module_load_index((int)(r - g_runtime));
}

int module_load_all(void) {
    int progress = 0;
    for (;;) {
        nexus_module_runtime_t *next = find_next_declared();
        if (!next) break;
        progress = 1;
        (void)module_load(next->descriptor->name);
    }
    return progress ? g_loaded : 0;
}

int module_is_loaded(const char *name) {
    const nexus_module_runtime_t *r = module_find(name);
    return r && r->state == NEXUS_MODULE_STATE_LOADED;
}

int module_count(void) { return g_count; }
int module_loaded_count(void) { return g_loaded; }
int module_failed_count(void) { return g_failed; }

int module_dependency_count(const char *name) {
    const nexus_module_runtime_t *r = module_find(name);
    return r ? (int)r->descriptor->dependency_count : -1;
}

int module_dependency_failed(const char *name) {
    const nexus_module_runtime_t *r = module_find(name);
    if (!r) return 1;
    for (uint16_t i = 0; i < r->descriptor->dependency_count; ++i) {
        const nexus_module_runtime_t *dep = module_find(r->descriptor->dependencies[i]);
        if (!dep || dep->state != NEXUS_MODULE_STATE_LOADED) return 1;
    }
    return 0;
}
