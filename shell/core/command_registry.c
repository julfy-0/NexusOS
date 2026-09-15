#include "command_registry.h"
#include "console.h"
#include "nexus_version.h"
#include "gui.h"
#include "vfs.h"
#include "mount.h"
#include "shell.h"

#include "neofetch.h"
#include "sysinfo.h"
#include "meminfo.h"
#include "about.h"
#include "whoami.h"
#include "version.h"
#include "date.h"
#include "colors.h"
#include "reboot.h"
#include "halt.h"
#include "beep.h"
#include "shutdown.h"
#include "lspci.h"
#include "uptime.h"
#include "diskls.h"
#include "diskcat.h"
#include "hardware.h"
#include "gpuinfo.h"
#include "inputinfo.h"
#include "uname.h"
#include "elf_loader.h"
#include "process.h"
#include "usermode.h"
#include "heap.h"
#include "scheduler.h"

#include "echo.h"
#include "reverse.h"
#include "len.h"
#include "upper.h"
#include "lower.h"
#include "title.h"
#include "calc.h"
#include "sum.h"
#include "hex.h"
#include "dec.h"
#include "isprime.h"
#include "fib.h"
#include "man.h"

#include "ls.h"
#include "pwd.h"
#include "cd.h"
#include "mkdir.h"
#include "rmdir.h"
#include "touch.h"
#include "rm.h"
#include "cp.h"
#include "mv.h"
#include "cat.h"
#include "less.h"
#include "head.h"
#include "tail.h"
#include "grep.h"
#include "diff.h"
#include "find.h"
#include "write.h"
#include "append.h"
#include "wc.h"
#include "df.h"
#include "du.h"

extern int strcmp(const char *a, const char *b);

#define CMD0(name, fn) static void command_##name(char *args) { (void)args; fn##_run(); }
#define CMD1(name, fn) static void command_##name(char *args) { fn##_run(args); }

typedef enum {
    CMD_SYSTEM,
    CMD_HARDWARE,
    CMD_STORAGE,
    CMD_FILESYSTEM,
    CMD_TEXT,
    CMD_MATH,
    CMD_SHELL,
    CMD_POWER
} command_category_t;

typedef struct {
    const char *name;
    const char *alias;
    command_category_t category;
    const char *usage;
    const char *description;
    void (*run)(char *args);
} command_entry_t;

CMD0(neofetch, neofetch)
CMD0(sysinfo, sysinfo)
CMD0(meminfo, meminfo)
CMD0(about, about)
CMD0(whoami, whoami)
CMD0(version, version)
CMD0(date, date)
CMD0(colors, colors)
CMD0(uptime, uptime)
CMD0(uname, uname)
CMD0(hardware, hardware)
CMD0(gpuinfo, gpuinfo)
CMD0(inputinfo, inputinfo)
CMD0(lspci, lspci)
CMD0(beep, beep)
CMD0(reboot, reboot)
CMD0(shutdown, shutdown)
CMD0(halt, halt)
CMD1(diskls, diskls)
CMD1(diskcat, diskcat)

CMD1(echo, echo)
CMD1(reverse, reverse)
CMD1(len, len)
CMD1(upper, upper)
CMD1(lower, lower)
CMD1(title, title)
CMD1(man, man)

CMD1(calc, calc)
CMD1(sum, sum)
CMD1(hex, hex)
CMD1(dec, dec)
CMD1(isprime, isprime)
CMD1(fib, fib)

CMD0(ls, ls)
CMD0(pwd, pwd)
CMD1(cd, cd)
CMD1(mkdir, mkdir)
CMD1(rmdir, rmdir)
CMD1(touch, touch)
CMD1(rm, rm)
CMD1(cp, cp)
CMD1(mv, mv)
CMD1(cat, cat)
CMD1(less, less)
CMD1(head, head)
CMD1(tail, tail)
CMD1(grep, grep)
CMD1(diff, diff)
CMD1(find, find)
CMD1(write, write)
CMD1(append, append)
CMD1(wc, wc)
CMD0(df, df)
CMD0(du, du)

static void command_mount(char *args) {
    if (args[0] == '\0') {
        vfs_mount_list();
        return;
    }

    char *source = args;
    char *target = vfs_split_word(source);
    char *fstype = vfs_split_word(target);

    if (target[0] == '\0' || fstype[0] == '\0') {
        console_print("mount: missing arguments\n");
        console_print("usage: mount <source> <target> <fstype>\n");
        return;
    }

    if (vfs_mount(source, target, fstype, 0) < 0) {
        console_print("mount: failed to create mount point\n");
        return;
    }

    console_print("mounted ");
    console_print(source);
    console_print(" on ");
    console_print(target);
    console_print(" [");
    console_print(fstype);
    console_print("]\n");
}

static void command_umount(char *args) {
    if (args[0] == '\0') {
        console_print("umount: missing target\nusage: umount <target>\n");
        return;
    }

    if (vfs_umount(args) < 0) {
        console_print("umount: mount point not found or busy\n");
        return;
    }

    console_print("unmounted ");
    console_print(args);
    console_print("\n");
}

static void command_mounts(char *args) { (void)args; vfs_mount_list(); }

static void command_elf_run(char *args) {
    if (!args || !args[0]) {
        console_print("elf-run: missing ELF path\nusage: elf-run <path>\n");
        return;
    }
    if (!usermode_ready() || !scheduler_is_ready()) {
        console_print("elf-run: user mode or scheduler is not ready\n");
        return;
    }

    uint64_t pid = process_create(0);
    if (!pid) {
        console_print("elf-run: process table is full\n");
        return;
    }
    if (!nexus_elf_load_process(pid, args)) {
        console_print("elf-run: invalid or unsupported ELF: ");
        console_print(args);
        console_print("\n");
        (void)process_exit(pid);
        return;
    }

    uint64_t tid = thread_create_user_process(pid);
    if (!tid) {
        console_print("elf-run: unable to create scheduler thread\n");
        (void)process_exit(pid);
        return;
    }

    nexus_process_t *p = process_get(pid);
    if (!p || !usermode_prepare(pid, p->user_entry,
                                p->user_stack_base, p->kernel_stack)) {
        console_print("elf-run: unable to prepare user context\n");
        (void)process_exit(pid);
        return;
    }

    console_print("Queued user ELF (PID ");
    console_print_dec(pid);
    console_print(", TID ");
    console_print_dec(tid);
    console_print(")\n");
    scheduler_request_reschedule();
}

static void command_desktop_run(char *args) {
    (void)args;
    console_print("Starting NexusOS " NEXUS_VERSION_DISPLAY " Desktop...\n\n");
    gui_start();
}

static void command_clear(char *args) { (void)args; console_clear(); }
static void command_help(char *args) { shell_command_help(args); }
static void command_history(char *args) { (void)args; shell_history_print(); }

static const char *category_name(command_category_t category) {
    switch (category) {
        case CMD_SYSTEM: return "system";
        case CMD_HARDWARE: return "hardware";
        case CMD_STORAGE: return "storage";
        case CMD_FILESYSTEM: return "filesystem";
        case CMD_TEXT: return "text";
        case CMD_MATH: return "math";
        case CMD_SHELL: return "shell";
        case CMD_POWER: return "power";
        default: return "other";
    }
}

#define ENTRY(n, a, c, u, d, fn) { n, a, c, u, d, fn },
static const command_entry_t g_commands[] = {
    /* System */
    ENTRY("neofetch", 0, CMD_SYSTEM, "neofetch", "detailed system summary", command_neofetch)
    ENTRY("sysinfo", 0, CMD_SYSTEM, "sysinfo", "show shell and system status", command_sysinfo)
    ENTRY("meminfo", 0, CMD_SYSTEM, "meminfo", "show memory and paging information", command_meminfo)
    ENTRY("about", 0, CMD_SYSTEM, "about", "show NexusOS information", command_about)
    ENTRY("whoami", 0, CMD_SYSTEM, "whoami", "show current user", command_whoami)
    ENTRY("version", 0, CMD_SYSTEM, "version", "show NexusOS version", command_version)
    ENTRY("uname", 0, CMD_SYSTEM, "uname", "show kernel and architecture", command_uname)
    ENTRY("date", 0, CMD_SYSTEM, "date", "show CMOS RTC date and time", command_date)
    ENTRY("uptime", 0, CMD_SYSTEM, "uptime", "show time since boot", command_uptime)
    ENTRY("hardware", 0, CMD_SYSTEM, "hardware", "detect platform hardware", command_hardware)
    ENTRY("lspci", 0, CMD_SYSTEM, "lspci", "list PCI devices", command_lspci)
    ENTRY("colors", 0, CMD_SYSTEM, "colors", "show console color test", command_colors)

    /* Input / graphics */
    ENTRY("gpuinfo", 0, CMD_HARDWARE, "gpuinfo", "show detected GPU and PCI details", command_gpuinfo)
    ENTRY("inputinfo", 0, CMD_HARDWARE, "inputinfo", "show keyboard and mouse state", command_inputinfo)

    /* Storage */
    ENTRY("diskls", 0, CMD_STORAGE, "diskls [path]", "list a directory on FAT32 disk", command_diskls)
    ENTRY("diskcat", 0, CMD_STORAGE, "diskcat <path>", "print a file from FAT32 disk", command_diskcat)

    /* Text */
    ENTRY("echo", 0, CMD_TEXT, "echo <text>", "print text", command_echo)
    ENTRY("reverse", 0, CMD_TEXT, "reverse <text>", "reverse text", command_reverse)
    ENTRY("len", 0, CMD_TEXT, "len <text>", "print text length", command_len)
    ENTRY("upper", 0, CMD_TEXT, "upper <text>", "convert text to uppercase", command_upper)
    ENTRY("lower", 0, CMD_TEXT, "lower <text>", "convert text to lowercase", command_lower)
    ENTRY("title", 0, CMD_TEXT, "title <text>", "convert text to title case", command_title)

    /* Math */
    ENTRY("calc", 0, CMD_MATH, "calc <a> <op> <b>", "integer calculator: + - * /", command_calc)
    ENTRY("sum", 0, CMD_MATH, "sum <n1> <n2> ...", "sum integers", command_sum)
    ENTRY("hex", 0, CMD_MATH, "hex <n>", "decimal to hexadecimal", command_hex)
    ENTRY("dec", 0, CMD_MATH, "dec <n>", "hexadecimal to decimal", command_dec)
    ENTRY("isprime", 0, CMD_MATH, "isprime <n>", "test whether a number is prime", command_isprime)
    ENTRY("fib", 0, CMD_MATH, "fib <n>", "calculate the n-th Fibonacci number", command_fib)

    /* Filesystem */
    ENTRY("ls", "dir", CMD_FILESYSTEM, "ls", "list current directory", command_ls)
    ENTRY("pwd", 0, CMD_FILESYSTEM, "pwd", "print current directory", command_pwd)
    ENTRY("cd", 0, CMD_FILESYSTEM, "cd <dir>", "change directory", command_cd)
    ENTRY("mkdir", 0, CMD_FILESYSTEM, "mkdir <name>", "create directory", command_mkdir)
    ENTRY("rmdir", 0, CMD_FILESYSTEM, "rmdir <name>", "remove empty directory", command_rmdir)
    ENTRY("touch", 0, CMD_FILESYSTEM, "touch <name>", "create empty file", command_touch)
    ENTRY("rm", 0, CMD_FILESYSTEM, "rm <name>", "remove file or empty directory", command_rm)
    ENTRY("cp", 0, CMD_FILESYSTEM, "cp <src> <dst>", "copy a file", command_cp)
    ENTRY("mv", 0, CMD_FILESYSTEM, "mv <src> <dst>", "rename or move a file/directory", command_mv)
    ENTRY("cat", 0, CMD_FILESYSTEM, "cat <file>", "print file contents", command_cat)
    ENTRY("less", 0, CMD_FILESYSTEM, "less <file>", "view a file", command_less)
    ENTRY("head", 0, CMD_FILESYSTEM, "head <file> [n]", "show first n characters", command_head)
    ENTRY("tail", 0, CMD_FILESYSTEM, "tail <file> [n]", "show last n characters", command_tail)
    ENTRY("grep", 0, CMD_FILESYSTEM, "grep <word> <file>", "search for text in a file", command_grep)
    ENTRY("diff", 0, CMD_FILESYSTEM, "diff <file1> <file2>", "compare two files", command_diff)
    ENTRY("find", 0, CMD_FILESYSTEM, "find <name>", "search the filesystem", command_find)
    ENTRY("write", 0, CMD_FILESYSTEM, "write <file> <text>", "overwrite a file", command_write)
    ENTRY("append", 0, CMD_FILESYSTEM, "append <file> <text>", "append text to a file", command_append)
    ENTRY("wc", 0, CMD_FILESYSTEM, "wc <file>", "count characters and words", command_wc)
    ENTRY("df", 0, CMD_FILESYSTEM, "df", "show filesystem usage", command_df)
    ENTRY("du", 0, CMD_FILESYSTEM, "du", "show current directory size", command_du)
    ENTRY("mount", 0, CMD_FILESYSTEM, "mount [source target fstype]", "list or create a VFS mount", command_mount)
    ENTRY("umount", 0, CMD_FILESYSTEM, "umount <target>", "remove a VFS mount", command_umount)
    ENTRY("mounts", 0, CMD_FILESYSTEM, "mounts", "list VFS mount points", command_mounts)

    /* Shell */
    ENTRY("history", 0, CMD_SHELL, "history", "show command history", command_history)
    ENTRY("clear", "cls", CMD_SHELL, "clear", "clear the console", command_clear)
    ENTRY("help", "?", CMD_SHELL, "help [command|category]", "show command help", command_help)
    ENTRY("man", 0, CMD_SHELL, "man <command>", "show detailed command manual", command_man)
    ENTRY("desktop-run", 0, CMD_SHELL, "desktop-run", "start NexusOS Desktop", command_desktop_run)
    ENTRY("elf-run", 0, CMD_SHELL, "elf-run <path>", "load and enter a user ELF64 executable", command_elf_run)
    ENTRY("beep", 0, CMD_SHELL, "beep", "beep the PC speaker", command_beep)

    /* Power */
    ENTRY("reboot", 0, CMD_POWER, "reboot", "restart the machine", command_reboot)
    ENTRY("shutdown", 0, CMD_POWER, "shutdown", "power off the machine", command_shutdown)
    ENTRY("halt", 0, CMD_POWER, "halt", "stop the CPU", command_halt)
};
#undef ENTRY

static unsigned int command_count(void) {
    return (unsigned int)(sizeof(g_commands) / sizeof(g_commands[0]));
}

static int is_category(const char *name, command_category_t *out) {
    for (int c = CMD_SYSTEM; c <= CMD_POWER; c++) {
        if (strcmp(name, category_name((command_category_t)c)) == 0) {
            *out = (command_category_t)c;
            return 1;
        }
    }
    return 0;
}

static const command_entry_t *find_command(const char *name) {
    for (unsigned int i = 0; i < command_count(); i++) {
        if (strcmp(name, g_commands[i].name) == 0 ||
            (g_commands[i].alias && strcmp(name, g_commands[i].alias) == 0)) {
            return &g_commands[i];
        }
    }
    return 0;
}

void shell_command_manual(const char *name) {
    const command_entry_t *cmd = find_command(name);
    if (!cmd) {
        console_print("No manual entry for '");
        console_print(name);
        console_print("'. Try 'help' or 'help <category>'.\n");
        return;
    }

    console_print("\n");
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print(cmd->usage);
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("\n");
    console_print("  ");
    console_print(cmd->description);
    console_print("\n");
    console_print("  category: ");
    console_print(category_name(cmd->category));
    if (cmd->alias) {
        console_print("\n  alias:    ");
        console_print(cmd->alias);
    }
    console_print("\n\n");
}

void shell_command_help(const char *args) {
    if (args && args[0] != '\0') {
        const command_entry_t *cmd = find_command(args);
        if (cmd) {
            shell_command_manual(args);
            return;
        }

        command_category_t category;
        if (is_category(args, &category)) {
            console_print("\n");
            console_set_color(COLOR_GREEN, COLOR_BLACK);
            console_print(category_name(category));
            console_print(" commands:\n");
            console_set_color(COLOR_WHITE, COLOR_BLACK);
            for (unsigned int i = 0; i < command_count(); i++) {
                if (g_commands[i].category == category) {
                    console_print("  ");
                    console_print(g_commands[i].usage);
                    console_print(" - ");
                    console_print(g_commands[i].description);
                    console_print("\n");
                }
            }
            console_print("\n");
            return;
        }

        console_print("help: unknown command or category '");
        console_print(args);
        console_print("'\n");
        console_print("Try: help, help system, help filesystem, help <command>\n");
        return;
    }

    console_print("\nNexusOS command center\n");
    console_print("Usage: help [command|category]\n\n");

    for (int c = CMD_SYSTEM; c <= CMD_POWER; c++) {
        console_set_color(COLOR_GREEN, COLOR_BLACK);
        console_print("[ ");
        console_print(category_name((command_category_t)c));
        console_print(" ]\n");
        console_set_color(COLOR_WHITE, COLOR_BLACK);

        for (unsigned int i = 0; i < command_count(); i++) {
            if (g_commands[i].category == (command_category_t)c) {
                console_print("  ");
                console_print(g_commands[i].usage);
                console_print(" - ");
                console_print(g_commands[i].description);
                if (g_commands[i].alias) {
                    console_print(" [");
                    console_print(g_commands[i].alias);
                    console_print("]");
                }
                console_print("\n");
            }
        }
        console_print("\n");
    }

    console_print("Tip: use 'man <command>' or 'help <command>' for details.\n");
}

int shell_command_execute(const char *name, char *args) {
    const command_entry_t *cmd = find_command(name);
    if (!cmd) {
        return 0;
    }
    cmd->run(args);
    return 1;
}
