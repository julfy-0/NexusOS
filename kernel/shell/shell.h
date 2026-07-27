#ifndef NEXUSOS_SHELL_H
#define NEXUSOS_SHELL_H

void shell_init(void);

/* Вызывается драйвером клавиатуры на каждый напечатанный символ.
 * Шелл сам занимается эхом на экран, буферизацией строки и построчным
 * разбором команд по Enter. */
void shell_input_char(char c);

#endif
