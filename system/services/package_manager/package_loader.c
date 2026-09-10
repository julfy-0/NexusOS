#include "package_loader.h"
#include "fat32.h"
int nexus_package_load_manifest(const char *path, char *buffer, unsigned int size, unsigned int *out_size) {
    if (!buffer || size == 0) return 0;
    return fat32_read_file(path, buffer, size, out_size);
}
