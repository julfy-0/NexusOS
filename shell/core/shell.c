/* NexusOS: примитивный построчный шелл.
 * Живёт полностью в контексте прерывания клавиатуры (пока нет ни
 * процессов, ни очереди событий) — команды должны быть быстрыми и не
 * блокирующими. */
#include "shell.h"
#include "nexus_version.h"
#include "console.h"
#include "command_registry.h"
#include "vfs.h"
#include "mount.h"
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
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("NexusOS> ");
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

void shell_history_print(void) {
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
    if (!shell_command_execute(cmd, args)) {
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
    vfs_mount_init();
    console_clear();
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("NexusOS Command Line — " NEXUS_VERSION_DISPLAY "\n");
    console_print("Type 'help' for available commands.\n\n");
    print_prompt();
}

void shell_return_from_desktop(void) {
    console_clear();
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    g_len = 0;
    g_history_pos = -1;
    console_print("NexusOS Command Line — " NEXUS_VERSION_DISPLAY "\n");
    console_print("Returned from NexusOS Desktop.\n\n");
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
