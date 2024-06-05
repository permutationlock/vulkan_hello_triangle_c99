CC ?= gcc
CFLAGS ?= -std=c99 -g3 -pedantic -fstrict-aliasing \
	 -fsanitize-trap -fsanitize=unreachable -fsanitize=undefined \
	 -Wcast-align -Wstrict-prototypes -Wold-style-definition \
	 -Werror -Wall -Wextra -Wconversion -Wdouble-promotion \
	 -Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion

SHADERC ?= glslc
SHADERFLAGS ?= 

all: vulkan_hello_triangle shaders/vert.spv shaders/frag.spv

clean:
	rm -f vulkan_hello_triangle main.o aven.o \
		shaders/vert.spv shaders/frag.spv

vulkan_hello_triangle: main.c aven.c
	$(CC) $(CFLAGS) -o vulkan_hello_triangle main.c aven.c \
		-lm -lglfw3 -lvulkan

shaders/vert.spv: shaders/base.vert
	$(SHADERC) $(SHADERFLAGS) -o shaders/vert.spv shaders/base.vert

shaders/frag.spv: shaders/base.frag
	$(SHADERC) $(SHADERFLAGS) -o shaders/frag.spv shaders/base.frag
