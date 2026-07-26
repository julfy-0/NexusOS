#include "man.h"
#include "console.h"

extern int strcmp(const char *a, const char *b);

void man_run(char *args) {
    if (strcmp(args, "ls") == 0) {
        console_print("ls - list files and directories in the current directory\n");
    } else if (strcmp(args, "cd") == 0) {
        console_print("cd <dir> - change current directory. Use '..' to go up.\n");
    } else if (strcmp(args, "cat") == 0) {
        console_print("cat <file> - print the contents of a file\n");
    } else if (strcmp(args, "cp") == 0) {
        console_print("cp <src> <dst> - copy a file within the current directory\n");
    } else if (strcmp(args, "mv") == 0) {
        console_print("mv <src> <dst> - rename a file or directory\n");
    } else if (strcmp(args, "rm") == 0) {
        console_print("rm <name> - remove a file or an empty directory\n");
    } else if (strcmp(args, "grep") == 0) {
        console_print("grep <word> <file> - print a file if it contains <word>\n");
    } else if (strcmp(args, "find") == 0) {
        console_print("find <name> - search the whole filesystem for <name>\n");
    } else if (strcmp(args, "calc") == 0) {
        console_print("calc <a> <op> <b> - integer calculator, op is + - * /\n");
    } else if (strcmp(args, "write") == 0) {
        console_print("write <file> <text> - overwrite a file with text\n");
    } else if (strcmp(args, "append") == 0) {
        console_print("append <file> <text> - append text to the end of a file\n");
    } else if (strcmp(args, "reboot") == 0) {
        console_print("reboot - restart the machine via the 8042 controller\n");
    } else if (strcmp(args, "shutdown") == 0) {
        console_print("shutdown - power off via the QEMU ACPI trick, falls back to halt\n");
    } else if (args[0] == '\0') {
        console_print("usage: man <command>\n");
    } else {
        console_print("No manual entry for '");
        console_print(args);
        console_print("'. Try 'help' for the full command list.\n");
    }
}
