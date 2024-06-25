# A basic Vulkan application in C99 (or later)

A C implementation of a simple rotating anti-aliased square based on
the [Vulkan Tutorial][10]. There are a few significant changes from the
tutorial:

 - Vulkan 1.3 direct rendering is used in lieu of render pass objects;
 - the [volk][11] dynamic loader is used in lieu of directly linking
   the `libvulkan.so` shared library.

The coding style and project organization was inpsired by [Zig][21],
[Chris Wellons' blog][18], and [raylib][19].

## Requirements

To build the project you will need a C compiler that supports C99 or later,
as well as a [GLSL][5] to [SPIR-V][6] compiler.
The project supports Linux and Windows targets.
There are **ZERO** build dependencies beyond a `libc` for your target
that supports [pthreads][23].

The project vendors and builds [volk][11] and a slightly modified version
of [GLFW][2]. Also included are headers for [Vulkan][3], [xkbcommon][17],
[X11][22], [Wayland][13], and the [Wayland protocols][14].

To run the application you will need the [Vulkan loader][4]
and a Vulkan driver for your graphics card. To run executables built with
`ENABLE_VALIDATION_LAYERS` defined (which is the case in the default `CFLAGS`)
you will need to have the [Vulkan validation layers][12] installed as well.

To run on Linux you will also need either an [X11][22] server or a
[Wayland][13] compositor, along with the corresponding shared libraries.

## Building

The project builds with a single tiny `Makefile`.

### Building on Linux

By default `make` will build a debug app with [gcc][7] as the C compiler and
[glslc][8] as the shader compiler.

```
make -j5
./vulkan_app
```

To use a different compiler you can modify the appropriate environment
variable.

```
make CC="clang" -j5
./vulkan_app
```

With a little define hack to wrangle the glibc headers, you can even
build an application using the awesome and simple [cproc][1] compiler[^1].

```
make CC="cproc" CFLAGS="-D\"__attribute__(x)=\"" \
    GLFW_FLAGS="-D\"__attribute__(x)=\"" -j5
./vulkan_app
```

You can easily cross-compile a Windows application from Linux with the
[Mingw-w64][9] toolchain.

```
make CC="x86_64-w64-mingw32-gcc" CFLAGS="-std=c99 -O2" LDFLAGS="-mwindows" \
    GLFW_CFLAGS="-std=c99 -O2 -DNDEBUG" \
    SHADERFLAGS="--target-env=vulkan1.3 -O" -j5
```

You can also cross-compile a Windows application with [zig cc][21] (or any
clang toolchain with a Windows Mingw target).

```
LOCALWINPTHREADS="YES" make CC="zig cc -target x86_64-windows-gnu" \
    LDFLAGS="-lkernel32 -luser32 -lgdi32 -Wl,--subsystem,windows"
```

### Building on Windows

On a Windows machine you can compile an application with [Mingw-w64][9].

```
make CFLAGS="-std=c99 -O2" GLFW_CFLAGS="-std=c99 -O2 -DNDEBUG" \
    LDFLAGS="-mwindows" SHADERFLAGS="--target-env=vulkan1.3 -O" -j5
./vulkan_app.exe
```

You can also build a Windows app with [Zig][21].

```
LOCALWINPTHREADS="YES" make CC="zig cc" \
    LDLFAGS="-lkernel32 -luser32 -lgdi32 -Wl,--subsystem,windows"
mv vulkan_app vulkan_app.exe
./vulkan_app.exe
```

You can also cross-compile a Linux application from Windows.

```
make CC="zig cc -target x86_64-linux-gnu" CFLAGS="-std=c99 -O2" \
    GLFW_CFLAGS="-std=c99 -O2 -DNDEBUG" \
    SHADERFLAGS="--target-env=vulkan1.3 -O" -j5
```

[^1]: A [small patch][15] to [QBE][16] is required to increase the maximum
    identifier length.

[1]: https://sr.ht/~mcf/cproc/
[2]: https://github.com/glfw/glfw
[3]: https://github.com/KhronosGroup/Vulkan-Headers
[4]: https://github.com/KhronosGroup/Vulkan-Loader
[5]: https://www.khronos.org/opengl/wiki/Core_Language_(GLSL)
[6]: https://registry.khronos.org/SPIR-V/
[7]: https://gcc.gnu.org/
[8]: https://github.com/google/shaderc
[9]: https://www.mingw-w64.org/
[10]: https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Base_code
[11]: https://github.com/zeux/volk
[12]: https://github.com/KhronosGroup/Vulkan-ValidationLayers
[13]: https://gitlab.freedesktop.org/wayland/wayland
[14]: https://gitlab.freedesktop.org/wayland/wayland-protocols
[15]: https://musing.permutationlock.com/static/qbe_identifier_len_expansion.patch
[16]: https://c9x.me/compile/
[17]: https://github.com/xkbcommon/libxkbcommon
[18]: https://nullprogram.com
[19]: https://github.com/raysan5/raylib
[20]: https://clang.llvm.org/
[21]: https://ziglang.org/
[22]: https://www.x.org/wiki/
