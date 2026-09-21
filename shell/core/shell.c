/* NexusOS command-line shell.
 * Input/history stay here; command registration and dispatch live in
 * command_registry.c so every command has one authoritative definition. */
#include "shell.h"
#include "nexus_version.h"
#include "console.h"
#include "gui.h"
#include "vfs.h"
#include "mount.h"
#include "command_registry.h"
#include "parser.h"
#include "target.h"

extern int strcmp(const char *a, const char *b);

#define SHELL_BUF_SIZE 256
#define HISTORY_SIZE 8
#define SHELL_COMMAND_QUEUE_SIZE 4

static char g_buf[SHELL_BUF_SIZE];
static int g_len;

static char g_history[HISTORY_SIZE][SHELL_BUF_SIZE];
static int g_history_count;
static int g_history_next;

/* Commands are executed outside the keyboard/xHCI event-drain path.  This
 * keeps input handling bounded even when a command performs heavy console,
 * filesystem, scheduler, or device work. */
static char g_pending_commands[SHELL_COMMAND_QUEUE_SIZE][SHELL_BUF_SIZE];
static uint32_t g_pending_head;
static uint32_t g_pending_tail;
static uint32_t g_pending_count;

/* Пролистывание истории стрелками Вверх/Вниз прямо в строке ввода.
 * g_history_pos: -1 = не листаем (набирается новая строка), 0 = самая
 * свежая команда из истории, 1 = следующая по старшинству, и т.д.
 * g_draft_buf — черновик строки, который был на экране до первого нажатия
 * Вверх, чтобы Вниз мог его вернуть (как в bash/PowerShell). */
static int g_history_pos = -1;
static char g_draft_buf[SHELL_BUF_SIZE];

static void print_prompt(void) {
    const char *user = "root";
    const char *host = target_baseboard_manufacturer();
    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print(user);
    console_print("@");
    console_print(host);
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
    console_cursor_hide();
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
    console_cursor_show();
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

#define SHELL_IO_BUF_SIZE 2048

static int queue_command(const char *cmd) {
    if (!cmd || !cmd[0] || g_pending_count >= SHELL_COMMAND_QUEUE_SIZE) {
        return 0;
    }

    int i = 0;
    while (cmd[i] && i < SHELL_BUF_SIZE - 1) {
        g_pending_commands[g_pending_head][i] = cmd[i];
        i++;
    }
    g_pending_commands[g_pending_head][i] = '\0';
    g_pending_head = (g_pending_head + 1u) % SHELL_COMMAND_QUEUE_SIZE;
    g_pending_count++;
    return 1;
}

static int dequeue_command(char *out) {
    if (!out || g_pending_count == 0) return 0;

    int i = 0;
    while (g_pending_commands[g_pending_tail][i] && i < SHELL_BUF_SIZE - 1) {
        out[i] = g_pending_commands[g_pending_tail][i];
        i++;
    }
    out[i] = '\0';
    g_pending_commands[g_pending_tail][0] = '\0';
    g_pending_tail = (g_pending_tail + 1u) % SHELL_COMMAND_QUEUE_SIZE;
    g_pending_count--;
    return 1;
}

/* Build the legacy command argument string from parsed argv plus the shell's
 * current stdin payload. Existing commands keep their current API while the
 * shell gains a small, bounded I/O transport. */
static void build_args(shell_parsed_command_t *c, const char *input,
                       char *args, int capacity) {
    int pos = 0;
    args[0] = '\0';
    for (int i = 1; i < c->argc; ++i) {
        int j = 0;
        if (pos && pos < capacity - 1) args[pos++] = ' ';
        while (c->argv[i][j] && pos < capacity - 1) args[pos++] = c->argv[i][j++];
    }
    if (input && input[0]) {
        if (pos && pos < capacity - 1) args[pos++] = ' ';
        int j = 0;
        while (input[j] && pos < capacity - 1) args[pos++] = input[j++];
    }
    args[pos] = '\0';
}

static int execute_one(shell_parsed_command_t *c, const char *stdin_text,
                       char *stdout_text, int capture) {
    static char args[SHELL_IO_BUF_SIZE];
    static char redirected_input[SHELL_IO_BUF_SIZE];
    const char *input = stdin_text;

    if (c->argc == 0) return 0;

    if (c->redirect_in) {
        if (vfs_read(c->redirect_in, redirected_input, SHELL_IO_BUF_SIZE) < 0) {
            console_print("shell: input file not found: ");
            console_print(c->redirect_in);
            console_print("\n");
            return 0;
        }
        input = redirected_input;
    }

    build_args(c, input, args, SHELL_IO_BUF_SIZE);
    if (capture) console_capture_begin(stdout_text, SHELL_IO_BUF_SIZE);

    int ok = shell_command_execute(c->argv[0], args);
    if (capture) console_capture_end();

    if (!ok) {
        if (!capture) {
            console_print("command not found: "); console_print(c->argv[0]);
            console_print("\nType 'help' for available commands.\n");
        }
        return 0;
    }
    return 1;
}

static int execute_pipeline(shell_parsed_command_t *commands, int first, int last) {
    static char pipe_a[SHELL_IO_BUF_SIZE];
    static char pipe_b[SHELL_IO_BUF_SIZE];
    const char *input = 0;
    char *out = pipe_a;
    int ok = 1;

    for (int i = first; i <= last; ++i) {
        int need_capture = (i < last) || commands[i].redirect_out;
        out[0] = '\0';
        ok = execute_one(&commands[i], input, out, need_capture);
        if (!ok) return 0;

        if (commands[i].redirect_out) {
            int rc = commands[i].append
                ? vfs_append(commands[i].redirect_out, out)
                : vfs_write(commands[i].redirect_out, out);
            if (rc != 0) {
                console_print("shell: cannot redirect output to ");
                console_print(commands[i].redirect_out);
                console_print("\n");
                return 0;
            }
        }

        if (i < last) {
            input = out;
            out = (out == pipe_a) ? pipe_b : pipe_a;
        }
    }
    return ok;
}

static void execute(char *cmd) {
    shell_parsed_command_t commands[SHELL_MAX_COMMANDS];
    int count = shell_parse_line(cmd, commands, SHELL_MAX_COMMANDS);
    if (count < 0) { console_print("shell: syntax error\n"); return; }

    int last_status = 1;
    int i = 0;
    while (i < count) {
        if (i > 0 && commands[i - 1].next == SHELL_OP_AND && !last_status) {
            i++;
            continue;
        }

        int end = i;
        while (end < count - 1 && commands[end].next == SHELL_OP_PIPE) end++;
        last_status = execute_pipeline(commands, i, end);
        i = end + 1;
    }
}

void shell_process_pending(void) {
    char command[SHELL_BUF_SIZE];
    if (!dequeue_command(command)) return;

    execute(command);
    if (!gui_is_active()) {
        print_prompt();
        console_cursor_show();
    }
}

void shell_init(void) {
    g_len = 0;
    g_history_count = 0;
    g_history_next = 0;
    g_history_pos = -1;
    g_pending_head = 0;
    g_pending_tail = 0;
    g_pending_count = 0;
    for (uint32_t i = 0; i < SHELL_COMMAND_QUEUE_SIZE; i++) g_pending_commands[i][0] = '\0';
    vfs_init();
    vfs_mount_init();
    console_clear();
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("NexusOS Command Line — " NEXUS_VERSION_DISPLAY "\n");
    console_print("Type 'help' for available commands.\n\n");
    print_prompt();
    console_cursor_enable();
}

void shell_return_from_desktop(void) {
    console_clear();
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    g_len = 0;
    g_history_pos = -1;
    console_print("NexusOS Command Line — " NEXUS_VERSION_DISPLAY "\n");
    console_print("Returned from NexusOS Desktop.\n\n");
    print_prompt();
    console_cursor_enable();
}

void shell_input_char(char c) {
    console_cursor_hide();
    if (c == '\n') {
        console_putchar('\n');
        g_buf[g_len] = '\0';
        history_add(g_buf);
        if (g_buf[0] != '\0') {
            if (!queue_command(g_buf)) {
                console_print("shell: command queue is full; try again\n");
                print_prompt();
            }
        } else {
            print_prompt();
        }
        g_len = 0;
        g_history_pos = -1;
        console_cursor_show();
        return;
    }

    if (c == '\b') {
        if (g_len > 0) {
            g_len--;
            console_putchar('\b');
        }
        g_history_pos = -1; /* правка строки вручную — больше не "листаем" историю */
        console_cursor_show();
        return;
    }

    if (g_len < SHELL_BUF_SIZE - 1) {
        g_buf[g_len++] = c;
        console_putchar(c);
    }
    g_history_pos = -1; /* правка строки вручную — больше не "листаем" историю */
    console_cursor_show();
}
