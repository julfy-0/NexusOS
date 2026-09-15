#ifndef NEXUSOS_ELF_LOADER_H
#define NEXUSOS_ELF_LOADER_H

#include <stdint.h>

/* Loads a small, statically readable ELF64 executable into an existing
 * process. The loader installs user mappings into the process-owned private CR3 and
 * initializes image data through CR3-aware physical translations. */
int nexus_elf_load_process(uint64_t pid, const char *path);

/* Returns the default user stack top used by the loader. */
uint64_t nexus_elf_user_stack_top(void);

#endif
