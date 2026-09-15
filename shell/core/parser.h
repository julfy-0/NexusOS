#ifndef NEXUSOS_SHELL_PARSER_H
#define NEXUSOS_SHELL_PARSER_H

#define SHELL_MAX_TOKENS 32
#define SHELL_MAX_COMMANDS 8

typedef enum { SHELL_OP_END, SHELL_OP_SEQ, SHELL_OP_AND, SHELL_OP_PIPE } shell_op_t;
typedef struct { int argc; char *argv[SHELL_MAX_TOKENS]; shell_op_t next; char *redirect_in; char *redirect_out; int append; } shell_parsed_command_t;

/* Parses in-place. Quotes (' and ") and backslash escapes are removed. */
int shell_parse_line(char *line, shell_parsed_command_t *out, int max_commands);
#endif
