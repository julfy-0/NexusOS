#ifndef NEXUS_LIB_STDIO_H
#define NEXUS_LIB_STDIO_H

/*
 * kprintf — упрощённый printf для ядра.
 * Поддерживает: %d %u %x %s %c %%
 * Вывод одновременно идёт на VGA-экран и в серийный порт (COM1),
 * чтобы лог не терялся даже когда экран уже забит паникой.
 */
void kprintf(const char *fmt, ...);

#endif /* NEXUS_LIB_STDIO_H */
