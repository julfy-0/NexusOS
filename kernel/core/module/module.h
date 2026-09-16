#ifndef NEXUSOS_MODULE_H
#define NEXUSOS_MODULE_H

#include <stdint.h>

#define NEXUS_MODULE_MAGIC 0x4D4F4458u /* 'MODX' */
#define NEXUS_MODULE_NAME_MAX 32
#define NEXUS_MODULE_VERSION_MAX 16
#define NEXUS_MODULE_MAX 32

typedef int (*nexus_module_init_fn)(void);
typedef void (*nexus_module_exit_fn)(void);

typedef enum {
    NEXUS_MODULE_BUILTIN = 0,
    NEXUS_MODULE_DRIVER = 1,
    NEXUS_MODULE_SERVICE = 2
} nexus_module_type_t;

typedef enum {
    NEXUS_MODULE_STATE_DECLARED = 0,
    NEXUS_MODULE_STATE_LOADING,
    NEXUS_MODULE_STATE_LOADED,
    NEXUS_MODULE_STATE_FAILED
} nexus_module_state_t;

typedef struct nexus_module_descriptor {
    uint32_t magic;
    const char *name;
    const char *version;
    nexus_module_type_t type;
    uint16_t priority;
    nexus_module_init_fn init;
    nexus_module_exit_fn exit;
} nexus_module_descriptor_t;

typedef struct nexus_module_runtime {
    const nexus_module_descriptor_t *descriptor;
    nexus_module_state_t state;
    int result;
} nexus_module_runtime_t;

#define NEXUS_MODULE_SECTION __attribute__((used, section(".nexus_modules"), aligned(8)))
#define NEXUS_MODULE(name_, version_, type_, priority_, init_, exit_) \
    static const nexus_module_descriptor_t nexus_module_##init_ NEXUS_MODULE_SECTION = { \
        .magic = NEXUS_MODULE_MAGIC, \
        .name = (name_), \
        .version = (version_), \
        .type = (type_), \
        .priority = (priority_), \
        .init = (init_), \
        .exit = (exit_) \
    }

void module_manager_init(void);
int module_load(const char *name);
int module_load_all(void);
int module_is_loaded(const char *name);
const nexus_module_runtime_t *module_find(const char *name);
int module_count(void);
int module_loaded_count(void);
int module_failed_count(void);

#endif
