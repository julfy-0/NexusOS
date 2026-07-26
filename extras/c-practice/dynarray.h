#ifndef DYNARRAY_H
#define DYNARRAY_H

#include <stddef.h>

/* Простой динамический массив указателей (void*).
 * Подходит для хранения любых данных через указатели. */
typedef struct {
    void **items;
    size_t size;      /* текущее количество элементов */
    size_t capacity;   /* выделенная ёмкость */
} DynArray;

/* Создаёт новый массив с начальной ёмкостью initial_capacity (0 = по умолчанию). */
DynArray *da_create(size_t initial_capacity);

/* Освобождает массив. Если free_items != 0, дополнительно вызывает free() для каждого элемента. */
void da_free(DynArray *arr, int free_items);

/* Добавляет элемент в конец массива. Возвращает 0 при успехе, -1 при ошибке. */
int da_push(DynArray *arr, void *item);

/* Извлекает и удаляет последний элемент. Возвращает NULL, если массив пуст. */
void *da_pop(DynArray *arr);

/* Возвращает элемент по индексу или NULL, если индекс вне диапазона. */
void *da_get(const DynArray *arr, size_t index);

/* Устанавливает элемент по индексу. Возвращает 0 при успехе, -1 при ошибке. */
int da_set(DynArray *arr, size_t index, void *item);

/* Удаляет элемент по индексу, сдвигая последующие элементы влево. */
int da_remove(DynArray *arr, size_t index);

/* Возвращает количество элементов. */
size_t da_size(const DynArray *arr);

#endif /* DYNARRAY_H */
