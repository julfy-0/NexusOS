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
#define CMD1C(name, fn) static void command_##name(char *args) { fn##_run(args); }

CMD0(neofetch, neofetch)
CMD0(sysinfo, sysinfo)
CMD0(meminfo, meminfo)
CMD0(about, about)
CMD0(whoami, whoami)
CMD0(version, version)
CMD0(date, date)
CMD0(colors, colors)
CMD0(reboot, reboot)
CMD0(halt, halt)
CMD0(beep, beep)
CMD0(shutdown, shutdown)
CMD0(lspci, lspci)
CMD0(uptime, uptime)
CMD1(diskls, diskls)
CMD1(diskcat, diskcat)
CMD0(hardware, hardware)

CMD1C(echo, echo)
CMD1(reverse, reverse)
CMD1C(len, len)
CMD1(upper, upper)
CMD1(lower, lower)
CMD1(title, title)
CMD1C(calc, calc)
CMD1(sum, sum)
CMD1(hex, hex)
CMD1(dec, dec)
CMD1(isprime, isprime)
CMD1(fib, fib)
CMD1(man, man)

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
        console_print("Usage: mount <source> <target> <fstype>\n");
    } else if (vfs_mount(source, target, fstype, 0) < 0) {
        console_print("mount: failed\n");
    } else {
        console_print("mounted "); console_print(source); console_print(" on ");
        console_print(target); console_print(" ["); console_print(fstype); console_print("]\n");
    }
}

static void command_umount(char *args) {
    if (args[0] == '\0') {
        console_print("Usage: umount <target>\n");
    } else if (vfs_umount(args) < 0) {
        console_print("umount: mount point not found or busy\n");
    } else {
        console_print("unmounted "); console_print(args); console_print("\n");
    }
}

static void command_mounts(char *args) { (void)args; vfs_mount_list(); }
static void command_desktop_run(char *args) {
    (void)args;
    console_print("Starting NexusOS " NEXUS_VERSION_DISPLAY " Desktop...\n\n");
    gui_start();
}
static void command_clear(char *args) { (void)args; console_clear(); }
static void command_help(char *args) { (void)args; shell_command_help(); }
static void command_history(char *args) { (void)args; shell_history_print(); }

#define COMMAND_TABLE(X) \
    X("neofetch", command_neofetch, "neofetch          - system info") \
    X("sysinfo", command_sysinfo, "sysinfo            - shell status") \
    X("meminfo", command_meminfo, "meminfo            - paging / memory map status") \
    X("about", command_about, "about              - about NexusOS") \
    X("whoami", command_whoami, "whoami             - who you are") \
    X("version", command_version, "version            - shell version") \
    X("date", command_date, "date               - date/time from CMOS RTC") \
    X("history", command_history, "history            - show recent commands") \
    X("colors", command_colors, "colors             - show console color demo") \
    X("echo", command_echo, "echo <text>       - print text") \
    X("reverse", command_reverse, "reverse <text>    - reverse text") \
    X("len", command_len, "len <text>        - print text length") \
    X("upper", command_upper, "upper <text>      - UPPERCASE text") \
    X("lower", command_lower, "lower <text>      - lowercase text") \
    X("title", command_title, "title <text>      - Title Case Text") \
    X("calc", command_calc, "calc <a> <op> <b> - integer calculator (+ - * /)") \
    X("sum", command_sum, "sum <n1> <n2> ..  - sum a list of integers") \
    X("hex", command_hex, "hex <n>           - decimal to hex") \
    X("dec", command_dec, "dec <n>           - hex to decimal") \
    X("isprime", command_isprime, "isprime <n>       - check if n is prime") \
    X("fib", command_fib, "fib <n>           - n-th Fibonacci number") \
    X("ls", command_ls, "ls                - list current directory") \
    X("pwd", command_pwd, "pwd               - print current directory") \
    X("cd", command_cd, "cd <dir>          - change directory (.. for up)") \
    X("mkdir", command_mkdir, "mkdir <name>      - create directory") \
    X("rmdir", command_rmdir, "rmdir <name>      - remove empty directory") \
    X("touch", command_touch, "touch <name>      - create empty file") \
    X("rm", command_rm, "rm <name>         - remove file or empty dir") \
    X("cp", command_cp, "cp <src> <dst>    - copy a file") \
    X("mv", command_mv, "mv <src> <dst>    - rename file or directory") \
    X("cat", command_cat, "cat <file>        - print file contents") \
    X("less", command_less, "less <file>       - same as cat (files are short here)") \
    X("head", command_head, "head <file> [n]   - first n chars (default 20)") \
    X("tail", command_tail, "tail <file> [n]   - last n chars (default 20)") \
    X("grep", command_grep, "grep <w> <file>   - print file if it contains w") \
    X("diff", command_diff, "diff <f1> <f2>    - compare two files") \
    X("find", command_find, "find <name>       - search whole filesystem") \
    X("write", command_write, "write <file> <t>  - write text to file") \
    X("append", command_append, "append <file> <t> - append text to file") \
    X("wc", command_wc, "wc <file>         - count chars/words in file") \
    X("df", command_df, "df                - ramfs node usage") \
    X("du", command_du, "du                - size of current dir subtree") \
    X("lspci", command_lspci, "lspci             - list PCI devices") \
    X("uptime", command_uptime, "uptime            - time since boot") \
    X("diskls", command_diskls, "diskls [path]     - list dir on real FAT32 disk") \
    X("diskcat", command_diskcat, "diskcat <path>    - print file from real FAT32 disk") \
    X("hardware", command_hardware, "hardware          - automatic PC hardware detection") \
    X("mount", command_mount, "mount             - list or create VFS mount point") \
    X("umount", command_umount, "umount <target>   - remove VFS mount point") \
    X("mounts", command_mounts, "mounts            - list VFS mount points") \
    X("beep", command_beep, "beep              - beep the PC speaker") \
    X("reboot", command_reboot, "reboot            - restart the machine") \
    X("shutdown", command_shutdown, "shutdown          - power off (ACPI/QEMU fallback)") \
    X("desktop-run", command_desktop_run, "desktop-run       - start NexusOS Desktop (Esc returns here)") \
    X("halt", command_halt, "halt              - stop the CPU") \
    X("clear", command_clear, "clear             - clear the screen") \
    X("man", command_man, "man <command>     - short manual entry") \
    X("help", command_help, "help              - this message")

typedef void (*command_fn_t)(char *args);
typedef struct { const char *name; command_fn_t run; const char *help; } command_entry_t;

#define MAKE_ENTRY(name, fn, help) { name, fn, help },
static const command_entry_t g_commands[] = { COMMAND_TABLE(MAKE_ENTRY) };
#undef MAKE_ENTRY

void shell_command_help(void) {
    console_print("Available commands:\n");
    for (unsigned int i = 0; i < sizeof(g_commands) / sizeof(g_commands[0]); ++i) {
        console_print("  ");
        console_print(g_commands[i].help);
        console_print("\n");
    }
}

int shell_command_execute(const char *name, char *args) {
    for (unsigned int i = 0; i < sizeof(g_commands) / sizeof(g_commands[0]); ++i) {
        if (strcmp(name, g_commands[i].name) == 0) {
            g_commands[i].run(args);
            return 1;
        }
    }
    return 0;
}
