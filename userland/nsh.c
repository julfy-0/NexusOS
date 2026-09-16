#include <stdint.h>

enum {
    SYS_EXIT = 1,
    SYS_GETPID = 2,
    SYS_WRITE = 3,
    SYS_READ = 6,
    SYS_COMMAND = 7
};

static inline uint64_t syscall3(uint64_t n, uint64_t a0, uint64_t a1, uint64_t a2) {
    uint64_t r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "D"(a0), "S"(a1), "d"(a2) : "rcx", "r11", "memory");
    return r;
}

static uint64_t slen(const char *s) {
    uint64_t n = 0;
    while (s && s[n]) ++n;
    return n;
}

static void write_out(const char *s) {
    uint64_t n = slen(s);
    if (n) (void)syscall3(SYS_WRITE, 1, (uint64_t)(uintptr_t)s, n);
}

static void write_buf(const char *s, uint64_t n) {
    if (n) (void)syscall3(SYS_WRITE, 1, (uint64_t)(uintptr_t)s, n);
}

static uint64_t copy_command(const char *line, char *name, uint64_t cap, char **args_out) {
    uint64_t i = 0;
    while (line[i] == ' ' || line[i] == '\t') ++i;
    uint64_t n = 0;
    while (line[i] && line[i] != ' ' && line[i] != '\t' && line[i] != '\n') {
        if (n + 1 >= cap) return 0;
        name[n++] = line[i++];
    }
    name[n] = 0;
    while (line[i] == ' ' || line[i] == '\t') ++i;
    *args_out = (char *)(line + i);
    return n;
}

static void run_line(char *line) {
    char *args = 0;
    char name[64];
    if (!copy_command(line, name, sizeof(name), &args)) return;

    if (name[0] == 'e' && name[1] == 'x' && name[2] == 'i' && name[3] == 't' && name[4] == 0) {
        (void)syscall3(SYS_EXIT, 0, 0, 0);
        return;
    }
    if (name[0] == 'c' && name[1] == 'l' && name[2] == 'e' && name[3] == 'a' && name[4] == 'r' && name[5] == 0) {
        write_out("\033[2J\033[H");
        return;
    }

    char command[320];
    uint64_t i = 0;
    while (name[i] && i + 1 < sizeof(command)) { command[i] = name[i]; ++i; }
    if (*args) {
        if (i + 1 < sizeof(command)) command[i++] = ' ';
        uint64_t j = 0;
        while (args[j] && i + 1 < sizeof(command)) command[i++] = args[j++];
    }
    command[i] = 0;

    char output[4096];
    uint64_t rc = syscall3(SYS_COMMAND, (uint64_t)(uintptr_t)command,
                           (uint64_t)(uintptr_t)output, sizeof(output) - 1);
    if ((int64_t)rc < 0) {
        write_out("command failed or is unavailable\n");
        return;
    }
    if (rc > sizeof(output) - 1) rc = sizeof(output) - 1;
    output[rc] = 0;
    write_buf(output, rc);
}

void _start(void) {
    static char input[256];
    write_out("NexusOS Userspace Shell\nType 'help' for available commands.\n\n");
    for (;;) {
        write_out("nsh> ");
        uint64_t got = syscall3(SYS_READ, 0, (uint64_t)(uintptr_t)input, sizeof(input) - 1);
        if ((int64_t)got < 0) {
            write_out("stdin unavailable\n");
            break;
        }
        if (got == 0) continue;
        input[got] = 0;
        run_line(input);
    }
    (void)syscall3(SYS_EXIT, 0, 0, 0);
    for (;;) __asm__ volatile("hlt");
}
