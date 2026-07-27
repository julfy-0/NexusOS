#ifndef NEXUSOS_SHELL_H
#define NEXUSOS_SHELL_H

void shell_init(void);

/* Вызывается драйвером клавиатуры на каждый напечатанный символ.
 * Шелл сам занимается эхом на экран, буферизацией строки и построчным
 * разбором команд по Enter. */
void shell_input_char(char c);

/* Вызываются драйвером клавиатуры по стрелкам Вверх/Вниз — листают историю
 * команд прямо в строке ввода (как в обычных шеллах: bash, PowerShell и т.п.). */
void shell_history_prev(void);
void shell_history_next(void);

#endif
