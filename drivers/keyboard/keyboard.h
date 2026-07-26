#ifndef NEXUS_DRIVERS_KEYBOARD_H
#define NEXUS_DRIVERS_KEYBOARD_H

void keyboard_init(void);

/* Возвращает последний считанный печатаемый символ, либо 0 если
 * с прошлого вызова ничего не нажималось. Простейший неблокирующий API. */
char keyboard_getchar(void);

#endif /* NEXUS_DRIVERS_KEYBOARD_H */
