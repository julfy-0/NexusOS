#ifndef NEXUSOS_APP_MANAGER_H
#define NEXUSOS_APP_MANAGER_H

typedef struct {
    const char *id;
    const char *name;
    const char *entry;
    int builtin;
} nexus_app_t;

int nexus_app_manager_init(void);
int nexus_app_register(const nexus_app_t *app);
int nexus_app_count(void);
const nexus_app_t *nexus_app_at(int index);
const nexus_app_t *nexus_app_find(const char *id);
int nexus_app_launch(const char *id);

#endif
