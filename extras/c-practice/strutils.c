#include "strutils.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char *str_trim(char *s) {
    if (!s) return s;
    char *end;

    while (isspace((unsigned char)*s)) s++;

    if (*s == '\0') return s;

    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    return s;
}

int str_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return 0;
    size_t lp = strlen(prefix);
    size_t ls = strlen(s);
    if (lp > ls) return 0;
    return strncmp(s, prefix, lp) == 0;
}

int str_ends_with(const char *s, const char *suffix) {
    if (!s || !suffix) return 0;
    size_t ls = strlen(s);
    size_t lsuf = strlen(suffix);
    if (lsuf > ls) return 0;
    return strcmp(s + (ls - lsuf), suffix) == 0;
}

char *str_dup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (!copy) return NULL;
    memcpy(copy, s, len);
    return copy;
}

char **str_split(const char *s, char delim, size_t *count) {
    if (!s || !count) return NULL;

    size_t parts = 1;
    for (const char *p = s; *p; p++) {
        if (*p == delim) parts++;
    }

    char **result = (char **)malloc(parts * sizeof(char *));
    if (!result) return NULL;

    size_t idx = 0;
    const char *start = s;
    const char *p = s;

    while (1) {
        if (*p == delim || *p == '\0') {
            size_t len = (size_t)(p - start);
            char *token = (char *)malloc(len + 1);
            if (!token) {
                str_split_free(result, idx);
                return NULL;
            }
            memcpy(token, start, len);
            token[len] = '\0';
            result[idx++] = token;

            if (*p == '\0') break;
            start = p + 1;
        }
        p++;
    }

    *count = idx;
    return result;
}

void str_split_free(char **parts, size_t count) {
    if (!parts) return;
    for (size_t i = 0; i < count; i++) {
        free(parts[i]);
    }
    free(parts);
}

char *str_replace(const char *s, const char *from, const char *to) {
    if (!s || !from || !to) return NULL;

    size_t from_len = strlen(from);
    size_t to_len = strlen(to);
    if (from_len == 0) return str_dup(s);

    /* Считаем количество вхождений */
    size_t count = 0;
    const char *p = s;
    while ((p = strstr(p, from)) != NULL) {
        count++;
        p += from_len;
    }

    size_t result_len = strlen(s) + count * (to_len - from_len);
    char *result = (char *)malloc(result_len + 1);
    if (!result) return NULL;

    char *dst = result;
    const char *src = s;
    while ((p = strstr(src, from)) != NULL) {
        size_t chunk = (size_t)(p - src);
        memcpy(dst, src, chunk);
        dst += chunk;
        memcpy(dst, to, to_len);
        dst += to_len;
        src = p + from_len;
    }
    strcpy(dst, src);

    return result;
}

void str_tolower(char *s) {
    if (!s) return;
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

void str_toupper(char *s) {
    if (!s) return;
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

int str_is_palindrome(const char *s) {
    if (!s) return 0;

    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (!buf) return 0;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (isalnum((unsigned char)s[i])) {
            buf[j++] = (char)tolower((unsigned char)s[i]);
        }
    }
    buf[j] = '\0';

    int result = 1;
    for (size_t i = 0, k = j; i < k / 2; i++) {
        if (buf[i] != buf[k - 1 - i]) {
            result = 0;
            break;
        }
    }

    free(buf);
    return result;
}
