/*
 * NexusOS — общие типы, используемые во всём ядре.
 * Базовые целочисленные типы берём из freestanding <stdint.h>,
 * который идёт вместе с кросс-компилятором (не зависит от libc хоста).
 */
#ifndef NEXUS_TYPES_H
#define NEXUS_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef enum {
    false = 0,
    true  = 1
} bool_t;

#define NX_UNUSED(x) ((void)(x))
#define NX_PACKED    __attribute__((packed))
#define NX_NORETURN  __attribute__((noreturn))
#define NX_ALIGNED(n) __attribute__((aligned(n)))

#endif /* NEXUS_TYPES_H */
