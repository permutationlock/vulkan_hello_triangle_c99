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
    #define assert(c)
    #define __attribute(a)
#endif

#define unused_parameter(p) (void)(p)

#define countof(array) (sizeof(array) / sizeof(*array))

#if __STDC_VERSION >= 202000L
    #include <stdalign.h>
    #define NORETURN [[noreturn]]
#elif __STDC_VERSION__ >= 201112L
    #include <stdalign.h>
    #define NORETURN _Noreturn
#elif __STDC_VERSION__ >= 199901L
    #define NORETURN
    #ifndef AVEN_MAX_ALIGNMENT
        #error "define AVEN_MAX_ALIGNMENT to be the target's max alignment"
    #endif
    #define alignof(t) (AVEN_MAX_ALIGNMENT)
#else
    #error "C99 or later is required"
#endif

#define Optional(t) struct { t value; bool valid; }
#define Result(t) struct { t payload; int error; }
#define Slice(t) struct { t *ptr; size_t len; }

#ifndef AVEN_NO_FUNCTIONS
    #ifdef __GNUC__
        static size_t aven_verify_index_internal(size_t index, size_t len) {
            assert(index < len);
            return index;
        }

        #define slice_get(s, i) (s.ptr[aven_verify_index_internal(i, s.len)])
    #else
        #define slice_get(s, i) (s.ptr[i])
    #endif
#endif

typedef Slice(unsigned char) ByteSlice;

#define as_bytes(ptr) ((ByteSlice){ \
        .ptr = (unsigned char *)ptr, \
        .len = sizeof(*ptr) \
    })
#define array_as_bytes(ptr) ((ByteSlice){ \
        .ptr = (unsigned char *)ptr, \
        .len = sizeof(ptr)\
    })

typedef struct {
    unsigned char *base;
    unsigned char *top;
} Arena;

#ifndef AVEN_NO_FUNCTIONS
    static Arena arena_init(void *mem, size_t size) {
        return (Arena){ .base = mem, .top = (unsigned char *)mem + size };
    }

    __attribute((malloc, alloc_size(2), alloc_align(3)))
    void *arena_alloc(Arena *arena, size_t size, size_t align);

    #define arena_create(t, a) (t *)arena_alloc(a, sizeof(t), alignof(t))
    #define arena_create_array(t, a, n) (t *)arena_alloc( \
            a, \
            n * sizeof(t), \
            alignof(t) \
        )
#endif

#endif // AVEN_H
