/* NexusOS: примитивный построчный шелл.
 * Живёт полностью в контексте прерывания клавиатуры (пока нет ни
 * процессов, ни очереди событий) — команды должны быть быстрыми и не
 * блокирующими. */
#include "shell.h"
#include "console.h"
#include "neofetch.h"
#include "sysinfo.h"
#include "meminfo.h"
#include "echo.h"
#include "reverse.h"
#include "len.h"
#include "calc.h"
#include "about.h"
#include "vfs.h"
#include "ls.h"
#include "pwd.h"
#include "cd.h"
#include "mkdir.h"
#include "touch.h"
#include "rm.h"
#include "cat.h"
#include "write.h"
#include "append.h"
#include "wc.h"
#include "reboot.h"
#include "halt.h"
#include "colors.h"
#include "upper.h"
#include "lower.h"
#include "title.h"
#include "hex.h"
#include "dec.h"
#include "isprime.h"
#include "fib.h"
#include "sum.h"
#include "whoami.h"
#include "version.h"
#include "beep.h"
#include "date.h"
#include "rmdir.h"
#include "cp.h"
#include "mv.h"
#include "head.h"
#include "tail.h"
#include "grep.h"
#include "diff.h"
#include "uname.h"
#include "df.h"
#include "du.h"
#include "find.h"
#include "less.h"
#include "man.h"
#include "shutdown.h"
#include "lspci.h"
#include "uptime.h"
#include "diskls.h"
#include "diskcat.h"

extern int strcmp(const char *a, const char *b);

#define SHELL_BUF_SIZE 256
#define HISTORY_SIZE 8

static char g_buf[SHELL_BUF_SIZE];
static int g_len;

static char g_history[HISTORY_SIZE][SHELL_BUF_SIZE];
static int g_history_count;
static int g_history_next;

/* Пролистывание истории стрелками Вверх/Вниз прямо в строке ввода.
 * g_history_pos: -1 = не листаем (набирается новая строка), 0 = самая
 * свежая команда из истории, 1 = следующая по старшинству, и т.д.
 * g_draft_buf — черновик строки, который был на экране до первого нажатия
 * Вверх, чтобы Вниз мог его вернуть (как в bash/PowerShell). */
static int g_history_pos = -1;
static char g_draft_buf[SHELL_BUF_SIZE];

static void print_prompt(void) {
    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print("nexus");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("> ");
}

static void history_add(const char *cmd) {
    if (cmd[0] == '\0') {
        return;
    }

    int i = 0;
    while (cmd[i] != '\0' && i < SHELL_BUF_SIZE - 1) {
        g_history[g_history_next][i] = cmd[i];
        i++;
    }
    g_history[g_history_next][i] = '\0';

    g_history_next = (g_history_next + 1) % HISTORY_SIZE;
    if (g_history_count < HISTORY_SIZE) {
        g_history_count++;
    }
}

static void history_print(void) {
    if (g_history_count == 0) {
        console_print("(no history)\n");
        return;
    }

    int start = (g_history_next - g_history_count + HISTORY_SIZE) % HISTORY_SIZE;
    for (int i = 0; i < g_history_count; i++) {
        int idx = (start + i) % HISTORY_SIZE;
        console_print(g_history[idx]);
        console_print("\n");
    }
}

/* idx_from_recent: 0 = самая свежая команда, 1 = предыдущая, ... NULL, если
 * такого индекса в истории нет (вышли за её пределы). */
static const char *history_get(int idx_from_recent) {
    if (idx_from_recent < 0 || idx_from_recent >= g_history_count) {
        return 0;
    }
    int idx = (g_history_next - 1 - idx_from_recent + 2 * HISTORY_SIZE) % HISTORY_SIZE;
    return g_history[idx];
}

/* Стирает то, что сейчас на экране в строке ввода (g_len символов, через
 * backspace — так же, как обычный ввод), и печатает вместо этого new_cmd. */
static void redraw_line(const char *new_cmd) {
    while (g_len > 0) {
        g_len--;
        console_putchar('\b');
    }
    int i = 0;
    while (new_cmd[i] != '\0' && i < SHELL_BUF_SIZE - 1) {
        g_buf[i] = new_cmd[i];
        console_putchar(new_cmd[i]);
        i++;
    }
    g_buf[i] = '\0';
    g_len = i;
}

void shell_history_prev(void) {
    if (g_history_pos + 1 >= g_history_count) {
        return; /* уже на самой старой команде (или истории вообще нет) */
    }
    if (g_history_pos == -1) {
        /* Первый шаг вверх — запоминаем то, что было недописано в строке. */
        int i = 0;
        while (i < g_len) { g_draft_buf[i] = g_buf[i]; i++; }
        g_draft_buf[i] = '\0';
    }
    g_history_pos++;
    redraw_line(history_get(g_history_pos));
}

void shell_history_next(void) {
    if (g_history_pos == -1) {
        return; /* и так внизу, листать дальше некуда */
    }
    g_history_pos--;
    if (g_history_pos == -1) {
        redraw_line(g_draft_buf); /* вернулись к черновику, который набирали */
    } else {
        redraw_line(history_get(g_history_pos));
    }
}

static char *split_args(char *cmd) {
    char *args = cmd;
    while (*args != '\0' && *args != ' ') {
        args++;
    }
    if (*args == ' ') {
        *args = '\0';
        args++;
        while (*args == ' ') {
            args++;
        }
    }
    return args;
}

static void execute(char *cmd) {
    if (cmd[0] == '\0') {
        return;
    }

    char *args = split_args(cmd);

    if (strcmp(cmd, "neofetch") == 0) {
        neofetch_run();
    } else if (strcmp(cmd, "sysinfo") == 0) {
        sysinfo_run();
    } else if (strcmp(cmd, "meminfo") == 0) {
        meminfo_run();
    } else if (strcmp(cmd, "echo") == 0) {
        echo_run(args);
    } else if (strcmp(cmd, "reverse") == 0) {
        reverse_run(args);
    } else if (strcmp(cmd, "len") == 0) {
        len_run(args);
    } else if (strcmp(cmd, "calc") == 0) {
        calc_run(args);
    } else if (strcmp(cmd, "about") == 0) {
        about_run();
    } else if (strcmp(cmd, "ls") == 0) {
        ls_run();
    } else if (strcmp(cmd, "pwd") == 0) {
        pwd_run();
    } else if (strcmp(cmd, "cd") == 0) {
        cd_run(args);
    } else if (strcmp(cmd, "mkdir") == 0) {
        mkdir_run(args);
    } else if (strcmp(cmd, "touch") == 0) {
        touch_run(args);
    } else if (strcmp(cmd, "rm") == 0) {
        rm_run(args);
    } else if (strcmp(cmd, "cat") == 0) {
        cat_run(args);
    } else if (strcmp(cmd, "write") == 0) {
        write_run(args);
    } else if (strcmp(cmd, "append") == 0) {
        append_run(args);
    } else if (strcmp(cmd, "wc") == 0) {
        wc_run(args);
    } else if (strcmp(cmd, "history") == 0) {
        history_print();
    } else if (strcmp(cmd, "colors") == 0) {
        colors_run();
    } else if (strcmp(cmd, "upper") == 0) {
        upper_run(args);
    } else if (strcmp(cmd, "lower") == 0) {
        lower_run(args);
    } else if (strcmp(cmd, "title") == 0) {
        title_run(args);
    } else if (strcmp(cmd, "hex") == 0) {
        hex_run(args);
    } else if (strcmp(cmd, "dec") == 0) {
        dec_run(args);
    } else if (strcmp(cmd, "isprime") == 0) {
        isprime_run(args);
    } else if (strcmp(cmd, "fib") == 0) {
        fib_run(args);
    } else if (strcmp(cmd, "sum") == 0) {
        sum_run(args);
    } else if (strcmp(cmd, "whoami") == 0) {
        whoami_run();
    } else if (strcmp(cmd, "version") == 0) {
        version_run();
    } else if (strcmp(cmd, "beep") == 0) {
        beep_run();
    } else if (strcmp(cmd, "date") == 0) {
        date_run();
    } else if (strcmp(cmd, "rmdir") == 0) {
        rmdir_run(args);
    } else if (strcmp(cmd, "cp") == 0) {
        cp_run(args);
    } else if (strcmp(cmd, "mv") == 0) {
        mv_run(args);
    } else if (strcmp(cmd, "head") == 0) {
        head_run(args);
    } else if (strcmp(cmd, "tail") == 0) {
        tail_run(args);
    } else if (strcmp(cmd, "grep") == 0) {
        grep_run(args);
    } else if (strcmp(cmd, "diff") == 0) {
        diff_run(args);
    } else if (strcmp(cmd, "uname") == 0) {
        uname_run();
    } else if (strcmp(cmd, "df") == 0) {
        df_run();
    } else if (strcmp(cmd, "du") == 0) {
        du_run();
    } else if (strcmp(cmd, "find") == 0) {
        find_run(args);
    } else if (strcmp(cmd, "less") == 0) {
        less_run(args);
    } else if (strcmp(cmd, "man") == 0) {
        man_run(args);
    } else if (strcmp(cmd, "shutdown") == 0) {
        shutdown_run();
    } else if (strcmp(cmd, "lspci") == 0) {
        lspci_run();
    } else if (strcmp(cmd, "uptime") == 0) {
        uptime_run();
    } else if (strcmp(cmd, "diskls") == 0) {
        diskls_run(args);
    } else if (strcmp(cmd, "diskcat") == 0) {
        diskcat_run(args);
    } else if (strcmp(cmd, "reboot") == 0) {
        reboot_run();
    } else if (strcmp(cmd, "halt") == 0) {
        halt_run();
    } else if (strcmp(cmd, "help") == 0) {
        console_print("Available commands:\n");
        console_print("  --- info ---\n");
        console_print("  neofetch          - system info\n");
        console_print("  sysinfo           - shell status\n");
        console_print("  meminfo           - paging / memory map status\n");
        console_print("  about             - about NexusOS\n");
        console_print("  whoami            - who you are\n");
        console_print("  version           - shell version\n");
        console_print("  date              - date/time from CMOS RTC\n");
        console_print("  history           - show recent commands\n");
        console_print("  colors            - show console color demo\n");
        console_print("  --- text ---\n");
        console_print("  echo <text>       - print text\n");
        console_print("  reverse <text>    - reverse text\n");
        console_print("  len <text>        - print text length\n");
        console_print("  upper <text>      - UPPERCASE text\n");
        console_print("  lower <text>      - lowercase text\n");
        console_print("  title <text>      - Title Case Text\n");
        console_print("  --- math ---\n");
        console_print("  calc <a> <op> <b> - integer calculator (+ - * /)\n");
        console_print("  sum <n1> <n2> ..  - sum a list of integers\n");
        console_print("  hex <n>           - decimal to hex\n");
        console_print("  dec <n>           - hex to decimal\n");
        console_print("  isprime <n>       - check if n is prime\n");
        console_print("  fib <n>           - n-th Fibonacci number\n");
        console_print("  --- files (in-RAM, resets on reboot) ---\n");
        console_print("  ls                - list current directory\n");
        console_print("  pwd               - print current directory\n");
        console_print("  cd <dir>          - change directory (.. for up)\n");
        console_print("  mkdir <name>      - create directory\n");
        console_print("  rmdir <name>      - remove empty directory\n");
        console_print("  touch <name>      - create empty file\n");
        console_print("  rm <name>         - remove file or empty dir\n");
        console_print("  cp <src> <dst>    - copy a file\n");
        console_print("  mv <src> <dst>    - rename file or directory\n");
        console_print("  cat <file>        - print file contents\n");
        console_print("  less <file>       - same as cat (files are short here)\n");
        console_print("  head <file> [n]   - first n chars (default 20)\n");
        console_print("  tail <file> [n]   - last n chars (default 20)\n");
        console_print("  grep <w> <file>   - print file if it contains w\n");
        console_print("  diff <f1> <f2>    - compare two files\n");
        console_print("  find <name>       - search whole filesystem\n");
        console_print("  write <file> <t>  - write text to file\n");
        console_print("  append <file> <t> - append text to file\n");
        console_print("  wc <file>         - count chars/words in file\n");
        console_print("  df                - ramfs node usage\n");
        console_print("  du                - size of current dir subtree\n");
        console_print("  --- real disk (FAT32, read-only) ---\n");
        console_print("  lspci             - list PCI devices\n");
        console_print("  uptime            - time since boot\n");
        console_print("  diskls [path]     - list dir on real FAT32 disk\n");
        console_print("  diskcat <path>    - print file from real FAT32 disk\n");
        console_print("  --- power ---\n");
        console_print("  beep              - beep the PC speaker\n");
        console_print("  reboot            - restart the machine\n");
        console_print("  shutdown          - power off (QEMU ACPI trick)\n");
        console_print("  halt              - stop the CPU\n");
        console_print("  clear             - clear the screen\n");
        console_print("  man <command>     - short manual entry\n");
        console_print("  help              - this message\n");
    } else if (strcmp(cmd, "clear") == 0) {
        console_clear();
    } else {
        console_print("Unknown command: ");
        console_print(cmd);
        console_print("\nType 'help' for a list of commands.\n");
    }
}

void shell_init(void) {
    g_len = 0;
    g_history_count = 0;
    g_history_next = 0;
    g_history_pos = -1;
    vfs_init();
    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_print("Type 'help' to see available commands.\n\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    print_prompt();
}

void shell_input_char(char c) {
    if (c == '\n') {
        console_putchar('\n');
        g_buf[g_len] = '\0';
        history_add(g_buf);
        execute(g_buf);
        g_len = 0;
        g_history_pos = -1;
        print_prompt();
        return;
    }

    if (c == '\b') {
        if (g_len > 0) {
            g_len--;
            console_putchar('\b');
        }
        g_history_pos = -1; /* правка строки вручную — больше не "листаем" историю */
        return;
    }

    if (g_len < SHELL_BUF_SIZE - 1) {
        g_buf[g_len++] = c;
        console_putchar(c);
    }
    g_history_pos = -1; /* правка строки вручную — больше не "листаем" историю */
}
