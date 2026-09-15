#ifndef NEXUSOS_APP_MANAGER_H
#define NEXUSOS_APP_MANAGER_H

#include <stdint.h>

typedef enum {
    NEXUS_APP_SOURCE_BUILTIN = 0,
    NEXUS_APP_SOURCE_SYSTEM,
    NEXUS_APP_SOURCE_USERDATA
} nexus_app_source_t;

typedef enum {
    NEXUS_APP_STOPPED = 0,
    NEXUS_APP_RUNNING
} nexus_app_state_t;

typedef struct {
    const char *id;
    const char *name;
    const char *entry;
    int builtin;
    nexus_app_source_t source;
} nexus_app_t;

typedef struct {
    uint32_t launch_count;
    nexus_app_state_t state;
    nexus_app_source_t source;
} nexus_app_runtime_t;

int nexus_app_manager_init(void);
int nexus_app_register(const nexus_app_t *app);
int nexus_app_register_builtin(const char *id, const char *name, const char *entry);
int nexus_app_count(void);
const nexus_app_t *nexus_app_at(int index);
const nexus_app_t *nexus_app_find(const char *id);
const nexus_app_runtime_t *nexus_app_runtime(const char *id);
int nexus_app_launch(const char *id);
const nexus_app_t *nexus_app_active(void);
void nexus_app_stop(void);
void nexus_app_stop_id(const char *id);

#endif
