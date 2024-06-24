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
    unsigned char *mem = arena->top - size;
    size_t padding = (size_t)((uintptr_t)mem & (align - 1));
    if ((arena->top - arena->base) < (ptrdiff_t)(size + padding)) {
        return NULL;
    }
    arena->top = mem - padding;
    return arena->top;
}
