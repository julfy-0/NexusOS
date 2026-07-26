#include <stdio.h>
#include <stdlib.h>
#include "strutils.h"
#include "dynarray.h"
#include "fileutils.h"
#include "mathutils.h"

int main(void) {
    /* --- strutils --- */
    char text[] = "   Привет, мир!   ";
    printf("trim: '%s'\n", str_trim(text));

    printf("starts_with: %d\n", str_starts_with("Hello, world", "Hello"));
    printf("is_palindrome('А роза упала на лапу Азора'): %d\n",
           str_is_palindrome("А роза упала на лапу Азора"));

    size_t parts_count;
    char **parts = str_split("one,two,three", ',', &parts_count);
    printf("split: ");
    for (size_t i = 0; i < parts_count; i++) printf("[%s] ", parts[i]);
    printf("\n");
    str_split_free(parts, parts_count);

    char *replaced = str_replace("foo bar foo", "foo", "baz");
    printf("replace: %s\n", replaced);
    free(replaced);

    /* --- dynarray --- */
    DynArray *arr = da_create(0);
    da_push(arr, "первый");
    da_push(arr, "второй");
    da_push(arr, "третий");
    printf("dynarray size: %zu\n", da_size(arr));
    for (size_t i = 0; i < da_size(arr); i++) {
        printf("  [%zu] = %s\n", i, (char *)da_get(arr, i));
    }
    da_free(arr, 0); /* элементы — строковые литералы, освобождать не нужно */

    /* --- fileutils --- */
    const char *tmp_path = "/tmp/clib_demo.txt";
    file_write_all(tmp_path, "строка 1\n", 9);
    file_append_line(tmp_path, "строка 2");
    printf("file exists: %d, size: %ld, lines: %ld\n",
           file_exists(tmp_path), file_size(tmp_path), file_count_lines(tmp_path));

    /* --- mathutils --- */
    double values[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    printf("gcd(48, 18) = %ld\n", gcd(48, 18));
    printf("is_prime(97) = %d\n", is_prime(97));
    printf("mean = %.2f, stddev = %.2f\n", mean(values, 5), stddev(values, 5));
    printf("clamp(15, 0, 10) = %.1f\n", clamp(15, 0, 10));

    return 0;
}
