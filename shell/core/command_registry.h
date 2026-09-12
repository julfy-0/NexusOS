#ifndef NEXUSOS_SHELL_COMMAND_REGISTRY_H
#define NEXUSOS_SHELL_COMMAND_REGISTRY_H

/* Central command table. Implementations stay in shell/commands/.
 * The registry owns dispatch, aliases, categories and command help. */
int shell_command_execute(const char *name, char *args);
void shell_command_help(const char *args);
void shell_command_manual(const char *name);

#endif
