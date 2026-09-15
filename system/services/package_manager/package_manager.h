#ifndef NEXUSOS_PACKAGE_MANAGER_H
#define NEXUSOS_PACKAGE_MANAGER_H

#define NEXUS_PACKAGE_EXT ".nx"

int nexus_package_manager_init(void);
int nexus_package_is_package_path(const char *path);
int nexus_package_discover(const char *root);
int nexus_package_count(void);

/* Installs one package directory into an application directory.
 * The source must contain manifest.nxm and may contain the Entry payload.
 * The destination is published only after validation and payload writes
 * succeed; on failure the newly-created destination is rolled back. */
int nexus_package_install(const char *source_dir, const char *destination_dir);

#endif
