#define AVEN_MAX_ALIGNMENT
#define AVEN_NO_FUNCTIONS
#include "aven.h"

void *arena_alloc(Arena *arena, size_t size, size_t align) {
    assert(
        align == 1 ||
        align == 2 ||
        align == 4 ||
        align == 8 ||
        align == 16 ||
        align == 32
    );
    size_t padding = (size_t)((uintptr_t)arena->top & (align - 1));
    if ((ptrdiff_t)size >= (arena->top - arena->base - (ptrdiff_t)padding)) {
        return NULL;
    }
    arena->top -= padding + size;
    return arena->top;
}
