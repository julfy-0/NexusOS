#ifndef NEXUS_DRIVERS_SERIAL_H
#define NEXUS_DRIVERS_SERIAL_H

void serial_init(void);
void serial_putchar(char c);
void serial_puts(const char *s);

#endif /* NEXUS_DRIVERS_SERIAL_H */
