#include "fileutils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

char *file_read_all(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);

    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(buf, 1, (size_t)size, f);
    fclose(f);

    if (read != (size_t)size) {
        free(buf);
        return NULL;
    }

    buf[size] = '\0';
    if (out_len) *out_len = (size_t)size;
    return buf;
}

int file_write_all(const char *path, const void *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    size_t written = fwrite(data, 1, len, f);
    fclose(f);

    return (written == len) ? 0 : -1;
}

int file_append_line(const char *path, const char *text) {
    FILE *f = fopen(path, "a");
    if (!f) return -1;

    int rc1 = fputs(text, f);
    int rc2 = fputc('\n', f);
    fclose(f);

    return (rc1 >= 0 && rc2 != EOF) ? 0 : -1;
}

int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

long file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long)st.st_size;
}

int file_copy(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;

    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }

    char buf[8192];
    size_t n;
    int ok = 1;

    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            ok = 0;
            break;
        }
    }

    fclose(in);
    fclose(out);

    return ok ? 0 : -1;
}

long file_count_lines(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    long count = 0;
    int c;
    int last_was_newline = 1;

    while ((c = fgetc(f)) != EOF) {
        last_was_newline = (c == '\n');
        if (c == '\n') count++;
    }

    /* Если файл не пуст и не заканчивается переводом строки, последняя строка тоже считается */
    if (!last_was_newline) count++;

    fclose(f);
    return count;
}
