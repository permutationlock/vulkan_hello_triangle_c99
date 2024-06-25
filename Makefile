.POSIX:
.SUFFIXES:
CC = gcc
CFLAGS = -std=c99 -pedantic -fstrict-aliasing \
	 -Werror -Wall -Wextra -Wconversion -Wdouble-promotion \
	 -Wcast-align -Wstrict-prototypes -Wold-style-definition \
	 -g3 -fsanitize-trap -fsanitize=unreachable -fsanitize=undefined \
	 -D ENABLE_VALIDATION_LAYERS
GLFW_CFLAGS = -std=c99 -g3
WINPTHREADS_CFLAGS = -std=c99 -g3

SHADERC = glslc
SHADERFLAGS = --target-env=vulkan1.3 -Werror -g

INCLUDEFLAGS = -Ideps/glfw/include -Ideps/volk/include -Ideps/vulkan/include \
	-Ideps/wayland/include -Ideps/xkbcommon/include -Ideps/X11/include
LDFLAGS = -lm -ldl -lpthread
OBJS = src/main.o src/aven.o deps/glfw/glfw.o

ifeq ($(LOCALWINPTHREADS),YES)
	INCLUDEFLAGS += -Ideps/winpthreads/include
	OBJS += deps/winpthreads/winpthreads.o
endif

.PHONY: all shaders clean cleanobj cleanshaders
all: vulkan_app shaders
clean: cleanobjects cleanshaders

vulkan_app: $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDEFLAGS) $(LDFLAGS) -o $@ $^
src/main.o: src/main.c
	$(CC) $(CFLAGS) $(INCLUDEFLAGS) -c -o $@ $<
src/aven.o: src/aven.c
	$(CC) $(CFLAGS) $(INCLUDEFLAGS) -c -o $@ $<
deps/glfw/glfw.o: deps/glfw/glfw.c
	$(CC) $(GLFW_CFLAGS) $(INCLUDEFLAGS) -c -o $@ $<
deps/winpthreads/winpthreads.o: deps/winpthreads/winpthreads.c
	$(CC) $(WINPTHREADS_CFLAGS) $(INCLUDEFLAGS) -c -o $@ $<
cleanobjects:
	rm -f vulkan_app* $(OBJS)

shaders: shaders/vert.spv shaders/frag.spv
shaders/vert.spv: shaders/base.vert
	$(SHADERC) $(SHADERFLAGS) -o $@ $<
shaders/frag.spv: shaders/base.frag
	$(SHADERC) $(SHADERFLAGS) -o $@ $<
cleanshaders:
	rm -f shaders/vert.spv shaders/frag.spv
