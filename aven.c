#define AVEN_MAX_ALIGNMENT
#include "aven.h"

void *aven_alloc(byte_slice_t *arena, size_t size, size_t align) {
    size_t padding = -(size_t)arena->ptr & (align - 1);
    size_t total = size + padding;
    if (total > arena->len) {
        return 0;
    }
    unsigned char *p = arena->ptr + padding;
    arena->ptr += total;
    arena->len -= total;
    return p;
}
