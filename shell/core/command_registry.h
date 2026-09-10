#ifndef NEXUSOS_SHELL_COMMAND_REGISTRY_H
#define NEXUSOS_SHELL_COMMAND_REGISTRY_H

/* Central command table. Command implementations remain in shell/commands/;
 * this module only owns registration, dispatch and help metadata. */
int shell_command_execute(const char *name, char *args);
void shell_command_help(void);

#endif
