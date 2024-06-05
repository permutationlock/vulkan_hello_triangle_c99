#ifndef AVEN_H
#define AVEN_H

#include <iso646.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#ifdef __GNUC__
    #define assert(c) while (!(c)) { __builtin_unreachable(); }
#else
    #define assert(c) while (!(c)) { int s = *(int *)NULL; }
#endif

#define countof(array) (sizeof(array) / sizeof(*array))

#if __STDC_VERSION >= 202000L
    #define NORETURN [[noreturn]]
#elif __STDC_VERSION__ >= 201112L
    #define NORETURN _Noreturn
    #define alignof(t) _Alignof(t)
#elif __STDC_VERSION__ >= 199901L
    #define NORETURN
    #ifndef AVEN_MAX_ALIGNMENT
        #error "for C99 define AVEN_MAX_ALIGNMENT as the target's max alignment"
    #endif
    #define alignof(t) (AVEN_MAX_ALIGNMENT)
#else
    #error "requires C99 or later"
#endif

#define optional(t) struct { t value; bool valid; }
#define result(t) struct { t payload; int error; }
#define slice(t) struct { t *ptr; size_t len; }

#define slice_get(s, i) (s.ptr[aven_verify_index_internal(i, s.len)])

typedef slice(unsigned char) byte_slice_t;

#define as_bytes(ptr) ((byte_slice_t){ \
        .ptr = (unsigned char *)ptr, \
        .len = sizeof(*ptr) \
    })
#define array_as_bytes(ptr) ((byte_slice_t){ \
        .ptr = (unsigned char *)ptr, \
        .len = sizeof(ptr)\
    })

static size_t aven_verify_index_internal(size_t index, size_t len) {
    assert(index < len);
    return index;
}

#ifdef __GNUC__
__attribute((malloc, alloc_size(2), alloc_align(3)))
#endif
void *aven_alloc(byte_slice_t *arena, size_t size, size_t align);

#endif // AVEN_H
