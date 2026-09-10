#ifndef NEXUSOS_PACKAGE_MANAGER_H
#define NEXUSOS_PACKAGE_MANAGER_H

#define NEXUS_PACKAGE_EXT ".nx"
int nexus_package_manager_init(void);
int nexus_package_is_package_path(const char *path);
int nexus_package_discover(const char *root);
int nexus_package_count(void);

#endif
