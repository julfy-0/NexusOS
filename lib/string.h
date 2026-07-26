#ifndef NEXUS_LIB_STRING_H
#define NEXUS_LIB_STRING_H

#include <nexus/types.h>

void   *memset(void *dest, int value, size_t count);
void   *memcpy(void *dest, const void *src, size_t count);
void   *memmove(void *dest, const void *src, size_t count);
int     memcmp(const void *a, const void *b, size_t count);

size_t  strlen(const char *str);
int     strcmp(const char *a, const char *b);
char   *strcpy(char *dest, const char *src);
char   *strcat(char *dest, const char *src);
void    itoa(int value, char *buf, int base);
void    utoa(unsigned int value, char *buf, int base);

#endif /* NEXUS_LIB_STRING_H */
