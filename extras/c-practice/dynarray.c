#include "dynarray.h"
#include <stdlib.h>
#include <string.h>

#define DA_DEFAULT_CAPACITY 8

DynArray *da_create(size_t initial_capacity) {
    if (initial_capacity == 0) initial_capacity = DA_DEFAULT_CAPACITY;

    DynArray *arr = (DynArray *)malloc(sizeof(DynArray));
    if (!arr) return NULL;

    arr->items = (void **)malloc(initial_capacity * sizeof(void *));
    if (!arr->items) {
        free(arr);
        return NULL;
    }

    arr->size = 0;
    arr->capacity = initial_capacity;
    return arr;
}

void da_free(DynArray *arr, int free_items) {
    if (!arr) return;

    if (free_items) {
        for (size_t i = 0; i < arr->size; i++) {
            free(arr->items[i]);
        }
    }

    free(arr->items);
    free(arr);
}

static int da_grow(DynArray *arr) {
    size_t new_capacity = arr->capacity * 2;
    void **new_items = (void **)realloc(arr->items, new_capacity * sizeof(void *));
    if (!new_items) return -1;

    arr->items = new_items;
    arr->capacity = new_capacity;
    return 0;
}

int da_push(DynArray *arr, void *item) {
    if (!arr) return -1;

    if (arr->size >= arr->capacity) {
        if (da_grow(arr) != 0) return -1;
    }

    arr->items[arr->size++] = item;
    return 0;
}

void *da_pop(DynArray *arr) {
    if (!arr || arr->size == 0) return NULL;
    return arr->items[--arr->size];
}

void *da_get(const DynArray *arr, size_t index) {
    if (!arr || index >= arr->size) return NULL;
    return arr->items[index];
}

int da_set(DynArray *arr, size_t index, void *item) {
    if (!arr || index >= arr->size) return -1;
    arr->items[index] = item;
    return 0;
}

int da_remove(DynArray *arr, size_t index) {
    if (!arr || index >= arr->size) return -1;

    for (size_t i = index; i < arr->size - 1; i++) {
        arr->items[i] = arr->items[i + 1];
    }
    arr->size--;
    return 0;
}

size_t da_size(const DynArray *arr) {
    if (!arr) return 0;
    return arr->size;
}
