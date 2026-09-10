#ifndef NEXUSOS_PACKAGE_MANIFEST_H
#define NEXUSOS_PACKAGE_MANIFEST_H

typedef struct {
    char name[64];
    char id[64];
    char version[32];
    char author[64];
    char type[32];
    char entry[96];
    char icon[96];
} nexus_package_manifest_t;

int nexus_package_manifest_parse(const char *text, nexus_package_manifest_t *out);

#endif
