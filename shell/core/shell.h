#ifndef NEXUSOS_SHELL_H
#define NEXUSOS_SHELL_H

void shell_init(void);

/* Called by the keyboard driver for each printable input character.
 * The shell handles echo, line buffering and command dispatch on Enter. */
void shell_input_char(char c);

/* Restore the command line after the graphical desktop exits. */
void shell_return_from_desktop(void);

/* Вызываются драйвером клавиатуры по стрелкам Вверх/Вниз — листают историю
 * команд прямо в строке ввода (как в обычных шеллах: bash, PowerShell и т.п.). */
void shell_history_prev(void);
void shell_history_next(void);

/* Print the stored command history. */
void shell_history_print(void);

#endif
