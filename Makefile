CC = gcc
CFLAGS = -std=c99 -pedantic -fstrict-aliasing \
	 -Werror -Wall -Wextra -Wconversion -Wdouble-promotion \
	 -Wcast-align -Wstrict-prototypes -Wold-style-definition
CDBGFLAGS = -g3 -fsanitize-trap -fsanitize=unreachable -fsanitize=undefined
COPTFLAGS = -O2

SHADERC = glslc
SHADERFLAGS = --target-env=vulkan1.3 -Werror -O

INCLUDEFLAGS = -Ideps/glfw/include -Ideps/volk/include -Ideps/vulkan/include \
	-Ideps/wayland/include -Ideps/xkbcommon/include
LIBFLAGS = -lm

GLFW_CFLAGS = -std=c99
GLFW_COPTFLAGS = -O2
GLFW_CDBGFLAGS = -g3

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
		hello_triangle.exe hello_triangle_dbg.exe \
		deps/glfw/glfw.o deps/glfw/glfw_dbg.o \
		shaders/vert.spv shaders/frag.spv

hello_triangle: main.c deps/glfw/glfw.o
	$(CC) $(CFLAGS) $(COPTFLAGS) -o hello_triangle main.c aven.c \
		deps/glfw/glfw.o $(LIBFLAGS) $(INCLUDEFLAGS)

hello_triangle_dbg: main.c deps/glfw/glfw_dbg.o
	$(CC) $(CFLAGS) $(CDBGFLAGS) -o hello_triangle_dbg main.c aven.c \
		deps/glfw/glfw_dbg.o -D ENABLE_VALIDATION_LAYERS \
		$(LIBFLAGS) $(INCLUDEFLAGS)

deps/glfw/glfw.o: deps/glfw/glfw.c	
	$(CC) $(GLFW_CFLAGS) $(GLFW_COPTFLAGS) -c -o deps/glfw/glfw.o \
		deps/glfw/glfw.c $(INCLUDEFLAGS)

deps/glfw/glfw_dbg.o: deps/glfw/glfw.c	
	$(CC) $(GLFW_CFLAGS) $(GLFW_CDBGFLAGS) -c -o deps/glfw/glfw_dbg.o \
		deps/glfw/glfw.c $(INCLUDEFLAGS)

shaders/vert.spv: shaders/base.vert
	$(SHADERC) $(SHADERFLAGS) -o shaders/vert.spv shaders/base.vert

shaders/frag.spv: shaders/base.frag
	$(SHADERC) $(SHADERFLAGS) -o shaders/frag.spv shaders/base.frag
