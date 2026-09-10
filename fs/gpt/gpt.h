#ifndef NEXUSOS_GPT_H
#define NEXUSOS_GPT_H

#include <stdint.h>

#define NEXUS_GPT_MAX_PARTITIONS 16
#define NEXUS_GPT_NAME_LEN 40

/* GUIDs used by the NexusOS image builder. Stored in GPT byte order. */
#define NEXUS_GPT_BOOT_TYPE    "C12A7328F81F11D2BA4B00A0C93EC93B"
#define NEXUS_GPT_SYSTEM_TYPE  "5953584E54534D455041525430303100"
#define NEXUS_GPT_USERDATA_TYPE "5553584E455352444154413030303100"

typedef struct {
    uint32_t index;              /* GPT entry index, 1-based for user output */
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t attributes;
    char type_guid[33];          /* normalized hexadecimal representation */
    char name[NEXUS_GPT_NAME_LEN];
} nexus_gpt_partition_t;

int gpt_scan(void);
int gpt_partition_count(void);
const nexus_gpt_partition_t *gpt_partition(int index);
const nexus_gpt_partition_t *gpt_find_type(const char *type_guid);
const nexus_gpt_partition_t *gpt_find_name(const char *name);

#endif
