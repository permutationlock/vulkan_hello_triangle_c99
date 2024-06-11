#define AVEN_MAX_ALIGNMENT
#include "aven.h"

void *arena_alloc(Arena *arena, size_t size, size_t align) {
    unsigned char *p = (void *)((size_t)(arena->top - size) & ~(align - 1));
    if (p - arena->base < 0) {
        return NULL;
    }
    arena->top = p;
    return p;
}
