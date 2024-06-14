.POSIX:
.SUFFIXES:
CC = gcc
CFLAGS = -std=c99 -pedantic -fstrict-aliasing \
	 -Werror -Wall -Wextra -Wconversion -Wdouble-promotion \
	 -Wcast-align -Wstrict-prototypes -Wold-style-definition \
	 -g3 -fsanitize-trap -fsanitize=unreachable -fsanitize=undefined \
	 -D ENABLE_VALIDATION_LAYERS
GLFW_CFLAGS = -std=c99 -g3

SHADERC = glslc
SHADERFLAGS = --target-env=vulkan1.3 -Werror -g

INCLUDEFLAGS = -Ideps/glfw/include -Ideps/volk/include -Ideps/vulkan/include \
	-Ideps/wayland/include -Ideps/xkbcommon/include
LDFLAGS = -lm -ldl -lpthread

all: hello_triangle shaders

.PHONY: shaders
shaders: shaders/vert.spv shaders/frag.spv
.PHONY: clean
clean:
	rm -f hello_triangle* src/main.o src/aven.o deps/glfw/glfw.o \
		shaders/vert.spv shaders/frag.spv

hello_triangle: src/main.o src/aven.o deps/glfw/glfw.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDEFLAGS) -o $@ $^
src/main.o: src/main.c
	$(CC) $(CFLAGS) $(INCLUDEFLAGS) -c -o $@ $<
src/aven.o: src/aven.c
	$(CC) $(CFLAGS) $(INCLUDEFLAGS) -c -o $@ $<
deps/glfw/glfw.o: deps/glfw/glfw.c	
	$(CC) $(GLFW_CFLAGS) $(INCLUDEFLAGS) -c -o $@ $<

shaders/vert.spv: shaders/base.vert
	$(SHADERC) $(SHADERFLAGS) -o $@ $<
shaders/frag.spv: shaders/base.frag
	$(SHADERC) $(SHADERFLAGS) -o $@ $<
