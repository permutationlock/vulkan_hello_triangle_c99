CC = gcc
CFLAGS = -std=c99 -pedantic -fstrict-aliasing \
	 -Werror -Wall -Wextra -Wconversion -Wdouble-promotion \
	 -Wcast-align -Wstrict-prototypes -Wold-style-definition
CDBGFLAGS = -g3 -fsanitize-trap -fsanitize=unreachable -fsanitize=undefined
COPTFLAGS = -O2

SHADERC = glslc
SHADERFLAGS = 

LIBFLAGS = -lm -lglfw3

all: debug release

.PHONY: debug
debug: hello_triangle_dbg shaders

.PHONY: release
release: hello_triangle shaders

.PHONY: shaders
shaders: shaders/vert.spv shaders/frag.spv

.PHONY: clean
clean:
	rm -f hello_triangle hello_triangle_dbg \
		shaders/vert.spv shaders/frag.spv

hello_triangle: main.c aven.c
	$(CC) $(CFLAGS) $(COPTFLAGS) -o hello_triangle main.c aven.c \
		$(LIBFLAGS)

hello_triangle_dbg: main.c aven.c
	$(CC) $(CFLAGS) $(CDBGFLAGS) -o hello_triangle_dbg main.c aven.c \
		-D ENABLE_VALIDATION_LAYERS $(LIBFLAGS)

shaders/vert.spv: shaders/base.vert
	$(SHADERC) $(SHADERFLAGS) -o shaders/vert.spv shaders/base.vert

shaders/frag.spv: shaders/base.frag
	$(SHADERC) $(SHADERFLAGS) -o shaders/frag.spv shaders/base.frag
